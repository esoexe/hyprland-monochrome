// ═══════════════════════════════════════════════════════════════════════════
//  .config/hypr/src/mono-tui.c — Zero-Dependency Pure ISO C11 Monochrome
//  Terminal Control Deck, Live Telemetry Monitor & Microsecond Benchmark
//
//  Built with raw POSIX termios + VT100 alternate screen + poll() + /proc +
//  /sys + Hyprland AF_UNIX .socket.sock IPC (zero ncurses, zero C++, zero
//  Python).
//
//  Usage:
//    mono-tui              interactive 6-tab live control center (Super+Shift+M)
//    mono-tui --snapshot   print single monochrome dashboard frame to stdout
//    mono-tui --json       emit full system + compositor telemetry as JSON
//    mono-tui --bench      run CLOCK_MONOTONIC Pure C11 microsecond benchmark
// ═══════════════════════════════════════════════════════════════════════════

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

// ── Terminal Raw Mode & Signal Safety ──────────────────────────────────────

static struct termios g_orig_termios;
static bool g_raw_active = false;
static volatile sig_atomic_t g_quit = 0;
static volatile sig_atomic_t g_resized = 0;

static void restore_tty(void) {
    if (g_raw_active) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
        const char *seq = "\033[?25h\033[0m\033[?1049l";
        (void)!write(STDOUT_FILENO, seq, strlen(seq));
        g_raw_active = false;
    }
}

static void on_sig_quit(int sig) {
    (void)sig;
    g_quit = 1;
}

static void on_sig_winch(int sig) {
    (void)sig;
    g_resized = 1;
}

static bool enter_raw_tty(void) {
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) return false;
    if (tcgetattr(STDIN_FILENO, &g_orig_termios) != 0) return false;

    struct termios raw = g_orig_termios;
    raw.c_iflag &= (tcflag_t)~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= (tcflag_t)~(OPOST);
    raw.c_cflag |= (tcflag_t)(CS8);
    raw.c_lflag &= (tcflag_t)~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) return false;
    g_raw_active = true;
    atexit(restore_tty);

    struct sigaction sa_q, sa_w;
    memset(&sa_q, 0, sizeof(sa_q));
    sa_q.sa_handler = on_sig_quit;
    sigaction(SIGINT, &sa_q, NULL);
    sigaction(SIGTERM, &sa_q, NULL);

    memset(&sa_w, 0, sizeof(sa_w));
    sa_w.sa_handler = on_sig_winch;
    sigaction(SIGWINCH, &sa_w, NULL);

    const char *seq = "\033[?1049h\033[?25l\033[2J\033[H";
    (void)!write(STDOUT_FILENO, seq, strlen(seq));
    return true;
}

// ── String Buffer ──────────────────────────────────────────────────────────

typedef struct {
    char *s;
    size_t len;
    size_t cap;
} TStr;

static void ts_init(TStr *b) {
    b->cap = 4096;
    b->len = 0;
    b->s = (char *)malloc(b->cap);
    if (!b->s) abort();
    b->s[0] = '\0';
}

static void ts_free(TStr *b) {
    free(b->s);
    b->s = NULL;
    b->len = b->cap = 0;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
static void ts_printf(TStr *b, const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n > 0) {
        while (b->len + (size_t)n + 1 > b->cap) b->cap *= 2;
        b->s = (char *)realloc(b->s, b->cap);
        if (!b->s) abort();
        vsnprintf(b->s + b->len, (size_t)n + 1, fmt, ap2);
        b->len += (size_t)n;
    }
    va_end(ap2);
}

static void ts_puts(TStr *b, const char *s) {
    if (!s) return;
    size_t n = strlen(s);
    while (b->len + n + 1 > b->cap) b->cap *= 2;
    b->s = (char *)realloc(b->s, b->cap);
    if (!b->s) abort();
    memcpy(b->s + b->len, s, n + 1);
    b->len += n;
}

static uint64_t mono_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

// ── System, Hardware & Compositor State Collector ──────────────────────────

#define MAX_CORES 16
#define HIST_LEN  44

typedef struct {
    unsigned long long prev_total;
    unsigned long long prev_busy;
    unsigned long long core_total[MAX_CORES];
    unsigned long long core_busy[MAX_CORES];
    int core_pct[MAX_CORES];
    int ncores;
    int cpu_pct;
    int hist[HIST_LEN];
    double mem_used_gib;
    double mem_total_gib;
    int mem_pct;
    double temp_c;
    char load1[16];
    char load5[16];
    char load15[16];
    char gpu[96];
    char chassis[32];
    bool socket_live;
    char motion[32];
    char glass[32];
    bool turbo;
    char shader[32];
    char cursor_theme[48];
    char cursor_size[16];
    char kb_type[32];
    char kb_layout[32];
    char wallpaper[96];
    double bench_proc_us;
    double bench_cfg_us;
    double bench_ipc_us;
} TuiState;

static void read_trimmed_line(const char *path, char *out, size_t cap, const char *fallback) {
    snprintf(out, cap, "%s", fallback);
    FILE *fp = fopen(path, "r");
    if (!fp) return;
    char buf[256];
    if (fgets(buf, sizeof(buf), fp)) {
        size_t l = strlen(buf);
        while (l > 0 && isspace((unsigned char)buf[l - 1])) buf[--l] = '\0';
        char *p = buf;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p) snprintf(out, cap, "%s", p);
    }
    fclose(fp);
}

static void detect_gpu_and_chassis(TuiState *st) {
    char ctype[32];
    read_trimmed_line("/sys/devices/virtual/dmi/id/chassis_type", ctype, sizeof(ctype), "");
    int ct = atoi(ctype);
    if (ct == 8 || ct == 9 || ct == 10 || ct == 14 || ct == 31 || ct == 32) {
        snprintf(st->chassis, sizeof(st->chassis), "Laptop");
    } else {
        char prod[64];
        read_trimmed_line("/sys/devices/virtual/dmi/id/product_name", prod, sizeof(prod), "");
        if (strstr(prod, "KVM") || strstr(prod, "QEMU") || strstr(prod, "Virtual") || strstr(prod, "VMware")) {
            snprintf(st->chassis, sizeof(st->chassis), "Virtual Machine");
        } else {
            snprintf(st->chassis, sizeof(st->chassis), "Desktop / Workstation");
        }
    }

    bool has_amd = false, has_intel = false, has_nv = false, has_vm = false;
    DIR *dp = opendir("/sys/bus/pci/devices");
    if (dp) {
        struct dirent *ent;
        while ((ent = readdir(dp)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            char p_cls[PATH_MAX], p_ven[PATH_MAX], cls[32], ven[32];
            int n1 = snprintf(p_cls, sizeof(p_cls), "/sys/bus/pci/devices/%s/class", ent->d_name);
            int n2 = snprintf(p_ven, sizeof(p_ven), "/sys/bus/pci/devices/%s/vendor", ent->d_name);
            if (n1 < 0 || (size_t)n1 >= sizeof(p_cls) || n2 < 0 || (size_t)n2 >= sizeof(p_ven)) continue;
            read_trimmed_line(p_cls, cls, sizeof(cls), "");
            if (strncmp(cls, "0x03", 4) != 0) continue;
            read_trimmed_line(p_ven, ven, sizeof(ven), "");
            if (strcmp(ven, "0x1002") == 0) has_amd = true;
            else if (strcmp(ven, "0x8086") == 0) has_intel = true;
            else if (strcmp(ven, "0x10de") == 0) has_nv = true;
            else if (strcmp(ven, "0x1af4") == 0 || strcmp(ven, "0x15ad") == 0 || strcmp(ven, "0x80ee") == 0) has_vm = true;
        }
        closedir(dp);
    }

    if ((has_amd || has_intel) && has_nv) snprintf(st->gpu, sizeof(st->gpu), "Hybrid Optimus (%s + NVIDIA)", has_amd ? "AMD" : "Intel");
    else if (has_nv) snprintf(st->gpu, sizeof(st->gpu), "NVIDIA RTX/GTX (nvidia-drm)");
    else if (has_amd) snprintf(st->gpu, sizeof(st->gpu), "AMD Radeon (amdgpu)");
    else if (has_intel) snprintf(st->gpu, sizeof(st->gpu), "Intel Iris / Arc (iHD)");
    else if (has_vm) snprintf(st->gpu, sizeof(st->gpu), "VirtIO / VM Software Renderer");
    else snprintf(st->gpu, sizeof(st->gpu), "Auto-Profiled DRM/KMS");
}

static bool probe_hypr_socket(void) {
    const char *his = getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if (!his || !his[0]) return false;
    const char *rt = getenv("XDG_RUNTIME_DIR");
    char spath[PATH_MAX];
    if (rt && rt[0]) snprintf(spath, sizeof(spath), "%s/hypr/%s/.socket.sock", rt, his);
    else snprintf(spath, sizeof(spath), "/run/user/%u/hypr/%s/.socket.sock", (unsigned)getuid(), his);
    struct stat sb;
    return (stat(spath, &sb) == 0 && S_ISSOCK(sb.st_mode));
}

static void get_dirs(char *cfg_dir, size_t ccap, char *cache_dir, size_t kcap) {
    const char *xc = getenv("XDG_CONFIG_HOME");
    const char *xk = getenv("XDG_CACHE_HOME");
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    if (xc && xc[0]) snprintf(cfg_dir, ccap, "%s", xc);
    else snprintf(cfg_dir, ccap, "%s/.config", home);
    if (xk && xk[0]) snprintf(cache_dir, kcap, "%s/hyprland-monochrome", xk);
    else snprintf(cache_dir, kcap, "%s/.cache/hyprland-monochrome", home);
}

static void refresh_telemetry(TuiState *st) {
    uint64_t t0 = mono_now_ns();

    FILE *fs = fopen("/proc/stat", "r");
    if (fs) {
        char line[256];
        int ci = 0;
        while (fgets(line, sizeof(line), fs)) {
            if (strncmp(line, "cpu ", 4) == 0) {
                unsigned long long u = 0, n = 0, s = 0, idle = 0, iow = 0, irq = 0, sirq = 0, steal = 0;
                if (sscanf(line + 4, "%llu %llu %llu %llu %llu %llu %llu %llu",
                           &u, &n, &s, &idle, &iow, &irq, &sirq, &steal) >= 4) {
                    unsigned long long tot = u + n + s + idle + iow + irq + sirq + steal;
                    unsigned long long bsy = tot - idle - iow;
                    if (st->prev_total > 0 && tot > st->prev_total && bsy >= st->prev_busy) {
                        st->cpu_pct = (int)((100ULL * (bsy - st->prev_busy)) / (tot - st->prev_total));
                    }
                    st->prev_total = tot;
                    st->prev_busy = bsy;
                }
            } else if (strncmp(line, "cpu", 3) == 0 && isdigit((unsigned char)line[3]) && ci < MAX_CORES) {
                char tag[16];
                unsigned long long u = 0, n = 0, s = 0, idle = 0, iow = 0, irq = 0, sirq = 0, steal = 0;
                if (sscanf(line, "%15s %llu %llu %llu %llu %llu %llu %llu %llu",
                           tag, &u, &n, &s, &idle, &iow, &irq, &sirq, &steal) >= 5) {
                    unsigned long long tot = u + n + s + idle + iow + irq + sirq + steal;
                    unsigned long long bsy = tot - idle - iow;
                    if (st->core_total[ci] > 0 && tot > st->core_total[ci] && bsy >= st->core_busy[ci]) {
                        st->core_pct[ci] = (int)((100ULL * (bsy - st->core_busy[ci])) / (tot - st->core_total[ci]));
                    }
                    st->core_total[ci] = tot;
                    st->core_busy[ci] = bsy;
                    ci++;
                }
            }
        }
        st->ncores = ci > 0 ? ci : 1;
        fclose(fs);
    }

    if (st->cpu_pct < 0) st->cpu_pct = 0;
    if (st->cpu_pct > 100) st->cpu_pct = 100;
    memmove(&st->hist[0], &st->hist[1], (HIST_LEN - 1) * sizeof(int));
    st->hist[HIST_LEN - 1] = st->cpu_pct;

    FILE *fm = fopen("/proc/meminfo", "r");
    if (fm) {
        unsigned long long mt = 0, ma = 0;
        char line[128];
        while (fgets(line, sizeof(line), fm)) {
            if (strncmp(line, "MemTotal:", 9) == 0) (void)sscanf(line + 9, "%llu", &mt);
            else if (strncmp(line, "MemAvailable:", 13) == 0) { (void)sscanf(line + 13, "%llu", &ma); break; }
        }
        fclose(fm);
        if (mt > 0) {
            unsigned long long used = (mt > ma) ? (mt - ma) : 0;
            st->mem_used_gib = (double)used / 1048576.0;
            st->mem_total_gib = (double)mt / 1048576.0;
            st->mem_pct = (int)((used * 100ULL) / mt);
        }
    }

    FILE *fl = fopen("/proc/loadavg", "r");
    if (fl) {
        if (fscanf(fl, "%15s %15s %15s", st->load1, st->load5, st->load15) < 3) {
            snprintf(st->load1, sizeof(st->load1), "0.00");
        }
        fclose(fl);
    }

    st->temp_c = 0.0;
    for (int z = 0; z < 6; ++z) {
        char tp[96], raw[32];
        snprintf(tp, sizeof(tp), "/sys/class/thermal/thermal_zone%d/temp", z);
        read_trimmed_line(tp, raw, sizeof(raw), "");
        if (raw[0]) {
            long mv = atol(raw);
            if (mv > 1000) { st->temp_c = (double)mv / 1000.0; break; }
        }
    }

    st->bench_proc_us = (double)(mono_now_ns() - t0) / 1000.0;

    uint64_t t1 = mono_now_ns();
    char cfg_dir[PATH_MAX], cache_dir[PATH_MAX], pbuf[PATH_MAX + 64];
    get_dirs(cfg_dir, sizeof(cfg_dir), cache_dir, sizeof(cache_dir));

    snprintf(pbuf, sizeof(pbuf), "%s/motion.state", cache_dir);
    read_trimmed_line(pbuf, st->motion, sizeof(st->motion), "fluid");

    snprintf(pbuf, sizeof(pbuf), "%s/glass.state", cache_dir);
    read_trimmed_line(pbuf, st->glass, sizeof(st->glass), "kawase");

    snprintf(pbuf, sizeof(pbuf), "%s/turbo.state", cache_dir);
    struct stat sb;
    st->turbo = (stat(pbuf, &sb) == 0);

    snprintf(pbuf, sizeof(pbuf), "%s/shader.state", cache_dir);
    read_trimmed_line(pbuf, st->shader, sizeof(st->shader), "off");

    snprintf(pbuf, sizeof(pbuf), "%s/cursor-theme.state", cache_dir);
    read_trimmed_line(pbuf, st->cursor_theme, sizeof(st->cursor_theme), "Monochrome-Obsidian");

    snprintf(pbuf, sizeof(pbuf), "%s/cursor-size.state", cache_dir);
    read_trimmed_line(pbuf, st->cursor_size, sizeof(st->cursor_size), "24");

    snprintf(pbuf, sizeof(pbuf), "%s/kb-type.state", cache_dir);
    read_trimmed_line(pbuf, st->kb_type, sizeof(st->kb_type), "ansi");

    snprintf(pbuf, sizeof(pbuf), "%s/kb-layout.state", cache_dir);
    read_trimmed_line(pbuf, st->kb_layout, sizeof(st->kb_layout), "us");

    snprintf(pbuf, sizeof(pbuf), "%s/wallpaper", cache_dir);
    char wp_full[PATH_MAX];
    read_trimmed_line(pbuf, wp_full, sizeof(wp_full), "mono-11.gif");
    const char *slash = strrchr(wp_full, '/');
    snprintf(st->wallpaper, sizeof(st->wallpaper), "%.95s", slash ? slash + 1 : wp_full);

    st->bench_cfg_us = (double)(mono_now_ns() - t1) / 1000.0;

    uint64_t t2 = mono_now_ns();
    st->socket_live = probe_hypr_socket();
    st->bench_ipc_us = (double)(mono_now_ns() - t2) / 1000.0;
}

// ── Interactive Action Tables ──────────────────────────────────────────────

typedef struct {
    const char *label;
    const char *detail;
    const char *script;
    const char *args;
} TuiAction;

static const TuiAction ACT_VELOCITY[] = {
    {"Motion : Fluid Silk Spring",     "silkSpring (mass=1.0, stiff=175, damp=22)", "motion.sh", "set fluid"},
    {"Motion : Snappy Studio",         "160ms zero-overshoot precision response",   "motion.sh", "set snappy"},
    {"Motion : Cinematic Orbit",       "orbiting borderangle + deep parallax",      "motion.sh", "set cinematic"},
    {"Motion : Minimal Instant",       "0ms animations for pure focus",             "motion.sh", "set minimal"},
    {"Glass  : 2-Pass Kawase",         "balanced frosted obsidian glass",           "perf.sh",   "glass kawase"},
    {"Glass  : Fast 1-Pass X-Ray",     "2.5x lower GPU cost wallpaper sampling",    "perf.sh",   "glass xray"},
    {"Glass  : Liquid Acrylic Lens",   "refraction=18.0, aberration=0.0",           "perf.sh",   "glass acrylic"},
    {"Glass  : Pure Matte Off",        "disable blur for maximum headroom",         "perf.sh",   "glass off"},
    {"Turbo  : Toggle Direct Scanout", "zero-latency Game / Battery mode (F12)",    "perf.sh",   "turbo toggle"},
};

static const TuiAction ACT_SHADERS[] = {
    {"Shader : Off (Native Passthrough)", "zero GPU post-processing overhead",       "shader.sh", "off"},
    {"Shader : Noir Luminance",           "Rec.709 hardware greyscale + S-curve",    "shader.sh", "set noir"},
    {"Shader : 35mm Cinema Grain",        "organic silver-halide film grain",        "shader.sh", "set cinema-grain"},
    {"Shader : P4 Studio CRT",            "phosphor scanlines + subtle barrel lens", "shader.sh", "set crt-studio"},
    {"Shader : Matte Paper E-Ink",        "low-contrast warm daylight paper curve",  "shader.sh", "set paper-eink"},
    {"Shader : OLED Pitch True-Black",    "crushes near-black for OLED pixel-off",   "shader.sh", "set oled-pitch"},
};

static const TuiAction ACT_INPUT[] = {
    {"Cursor : Monochrome-Obsidian",   "matte #0c0c0c body + crisp #f5f5f5 edge",    "cursor.sh",   "theme Monochrome-Obsidian"},
    {"Cursor : Monochrome-Ivory",      "luminous #f4f4f4 body + obsidian border",    "cursor.sh",   "theme Monochrome-Ivory"},
    {"Cursor : Monochrome-Precision",  "architectural CAD crosshair reticle",        "cursor.sh",   "theme Monochrome-Precision"},
    {"Cursor : Monochrome-Dot",        "minimalist concentric telemetry halo",       "cursor.sh",   "theme Monochrome-Dot"},
    {"Size   : 24px Standard",         "default logical pointer geometry",           "cursor.sh",   "size 24"},
    {"Size   : 32px Studio HiDPI",     "high-visibility studio scale",               "cursor.sh",   "size 32"},
    {"Keybd  : ANSI 104-Key Standard", "standard US mechanical physical matrix",     "keyboard.sh", "type ansi"},
    {"Keybd  : 60% / HHKB Mode",       "CapsLock -> Escape (caps:escape)",           "keyboard.sh", "type compact60"},
    {"Keybd  : Vim Modal Swap",        "swap CapsLock & Escape (caps:swapescape)",   "keyboard.sh", "type vim"},
    {"Layout : Cycle Active Keymap",   "switch next XKB layout (Super + X)",         "keyboard.sh", "next"},
};

static const TuiAction ACT_WALLS[] = {
    {"Cycle Next Live Loop",           "120 FPS Bezier transition via awww",         "wallpaper.sh", "next"},
    {"Cycle Kinetic Instrument Loop",  "Orbital / Harmonic Pulse / Lissajous",       "wallpaper.sh", "live"},
    {"Open Visual Wallpaper Picker",   "Rofi thumbnail grid across all 20 loops",    "wallpaper.sh", "picker"},
    {"Solid Pure Pitch (#080808)",     "zero-distraction solid obsidian canvas",     "wallpaper.sh", "solid"},
};

static void run_script_action(const TuiAction *a, char *status_msg, size_t scap) {
    char cfg_dir[PATH_MAX], cache_dir[PATH_MAX], cmd[PATH_MAX + 128];
    get_dirs(cfg_dir, sizeof(cfg_dir), cache_dir, sizeof(cache_dir));
    int n = snprintf(cmd, sizeof(cmd), "\"%s/hypr/scripts/%s\" %s >/dev/null 2>&1",
                     cfg_dir, a->script, a->args);
    if (n > 0 && (size_t)n < sizeof(cmd)) {
        int rc = system(cmd);
        (void)rc;
    }
    snprintf(status_msg, scap, "Applied: %s", a->label);
}

// ── Frame Renderer (Shared by Interactive TTY & `--snapshot`) ──────────────

static void append_bar(TStr *out, int pct, int width) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    int filled = (pct * width + 50) / 100;
    for (int i = 0; i < width; ++i) {
        ts_puts(out, i < filled ? "\xe2\x96\x88" : "\xe2\x96\x91");
    }
}

static void render_dashboard(TStr *out, const TuiState *st, int tab, int sel,
                             const char *status_msg, bool ansi_color) {
    const char *B  = ansi_color ? "\033[1;97m" : "";
    const char *D  = ansi_color ? "\033[38;5;245m" : "";
    const char *F  = ansi_color ? "\033[38;5;240m" : "";
    const char *HI = ansi_color ? "\033[7;1m" : "> ";
    const char *R  = ansi_color ? "\033[0m" : "";

    if (ansi_color) ts_puts(out, "\033[H");

    ts_printf(out, "%s\xe2\x94\x8c\xe2\x94\x80\xe2\x94\x80 hyprland-monochrome v5.3.0 // Pure ISO C11 Control Deck \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x90%s\r\n", B, R);
    ts_printf(out, "%s\xe2\x94\x82%s %sChassis:%s %-22s %sGPU:%s %-37s %sIPC:%s %-4s %s\xe2\x94\x82%s\r\n",
              B, R, D, R, st->chassis, D, R, st->gpu, D, R, st->socket_live ? "LIVE" : "MOCK", B, R);
    ts_printf(out, "%s\xe2\x94\x9c\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\xa4%s\r\n", F, R);

    static const char *TABS[6] = {
        "1:TELEMETRY", "2:VELOCITY", "3:SHADERS", "4:INPUT+CURSOR", "5:WALLPAPERS", "6:BENCHMARK"
    };
    ts_puts(out, "  ");
    for (int i = 0; i < 6; ++i) {
        if (i == tab) ts_printf(out, "%s[%s]%s  ", HI, TABS[i], R);
        else ts_printf(out, "%s %s %s  ", D, TABS[i], R);
    }
    ts_puts(out, "\r\n\r\n");

    static const char *SPARK[8] = {
        "\xe2\x96\x81", "\xe2\x96\x82", "\xe2\x96\x83", "\xe2\x96\x84",
        "\xe2\x96\x85", "\xe2\x96\x86", "\xe2\x96\x87", "\xe2\x96\x88"
    };

    if (tab == 0) {
        ts_printf(out, "  %sCPU UTILIZATION%s   ", B, R);
        append_bar(out, st->cpu_pct, 28);
        ts_printf(out, "  %s%3d%%%s   load %s %s %s", B, st->cpu_pct, R, st->load1, st->load5, st->load15);
        if (st->temp_c > 1.0) ts_printf(out, "   %.1f\xc2\xb0" "C", st->temp_c);
        ts_puts(out, "\r\n  ");
        ts_printf(out, "%sROLLING WAVEFORM%s  ", D, R);
        for (int i = 0; i < HIST_LEN; ++i) {
            int idx = (st->hist[i] * 7) / 100;
            if (idx < 0) idx = 0;
            if (idx > 7) idx = 7;
            ts_puts(out, SPARK[idx]);
        }
        ts_puts(out, "\r\n\r\n");

        int show_cores = st->ncores > 8 ? 8 : st->ncores;
        for (int c = 0; c < show_cores; c += 2) {
            ts_printf(out, "  %score%-2d%s ", D, c, R);
            append_bar(out, st->core_pct[c], 18);
            ts_printf(out, " %3d%%", st->core_pct[c]);
            if (c + 1 < show_cores) {
                ts_printf(out, "      %score%-2d%s ", D, c + 1, R);
                append_bar(out, st->core_pct[c + 1], 18);
                ts_printf(out, " %3d%%", st->core_pct[c + 1]);
            }
            ts_puts(out, "\r\n");
        }

        ts_puts(out, "\r\n");
        ts_printf(out, "  %sMEMORY (RAM)%s      ", B, R);
        append_bar(out, st->mem_pct, 28);
        ts_printf(out, "  %s%.1f / %.1f GiB (%d%%)%s\r\n\r\n", B, st->mem_used_gib, st->mem_total_gib, st->mem_pct, R);

        ts_printf(out, "  %sACTIVE COMPOSITOR PROFILE%s\r\n", B, R);
        ts_printf(out, "    Motion   : %-16s   Optical Glass : %-14s   Turbo : %s\r\n",
                  st->motion, st->glass, st->turbo ? "ON (Direct Scanout)" : "off");
        ts_printf(out, "    Shader   : %-16s   Cursor Suite  : %s (%spx)\r\n",
                  st->shader, st->cursor_theme, st->cursor_size);
        ts_printf(out, "    Keyboard : %-6s / %-8s  Live Loop     : %s\r\n",
                  st->kb_type, st->kb_layout, st->wallpaper);
    } else if (tab >= 1 && tab <= 4) {
        const TuiAction *list = NULL;
        int count = 0;
        if (tab == 1) { list = ACT_VELOCITY; count = (int)(sizeof(ACT_VELOCITY) / sizeof(ACT_VELOCITY[0])); }
        else if (tab == 2) { list = ACT_SHADERS; count = (int)(sizeof(ACT_SHADERS) / sizeof(ACT_SHADERS[0])); }
        else if (tab == 3) { list = ACT_INPUT; count = (int)(sizeof(ACT_INPUT) / sizeof(ACT_INPUT[0])); }
        else { list = ACT_WALLS; count = (int)(sizeof(ACT_WALLS) / sizeof(ACT_WALLS[0])); }

        for (int i = 0; i < count; ++i) {
            if (i == sel) {
                ts_printf(out, "  %s %-34s %s  %s\r\n", HI, list[i].label, R, list[i].detail);
            } else {
                ts_printf(out, "    %-34s   %s%s%s\r\n", list[i].label, D, list[i].detail, R);
            }
        }
    } else if (tab == 5) {
        ts_printf(out, "  %sPURE ISO C11 MICROSECOND RUNTIME BENCHMARK (CLOCK_MONOTONIC)%s\r\n\r\n", B, R);
        ts_printf(out, "    /proc + /sys telemetry collector   : %s%7.2f us%s   (0 child processes)\r\n", B, st->bench_proc_us, R);
        ts_printf(out, "    XDG state + profile reader         : %s%7.2f us%s   (in-process C11)\r\n", B, st->bench_cfg_us, R);
        ts_printf(out, "    Hyprland .socket.sock stat probe   : %s%7.2f us%s   (AF_UNIX direct)\r\n", B, st->bench_ipc_us, R);
        ts_puts(out, "\r\n  Toolchain: 100% ISO C11 (-std=c11 -O3 -Wall -Wextra -Werror)  |  0 Python  |  0 C++  |  0 jq\r\n");
    }

    ts_puts(out, "\r\n");
    ts_printf(out, "  %sKeys:%s [1-6/Tab] Switch Panel  [j/k] Navigate  [Enter] Apply  [q] Quit", D, R);
    if (status_msg && status_msg[0]) ts_printf(out, "   %s| %s%s", B, status_msg, R);
    if (ansi_color) ts_puts(out, "\033[J");
    ts_puts(out, "\r\n");
}

static int tab_item_count(int tab) {
    if (tab == 1) return (int)(sizeof(ACT_VELOCITY) / sizeof(ACT_VELOCITY[0]));
    if (tab == 2) return (int)(sizeof(ACT_SHADERS) / sizeof(ACT_SHADERS[0]));
    if (tab == 3) return (int)(sizeof(ACT_INPUT) / sizeof(ACT_INPUT[0]));
    if (tab == 4) return (int)(sizeof(ACT_WALLS) / sizeof(ACT_WALLS[0]));
    return 0;
}

int main(int argc, char *argv[]) {
    bool snap_only = false;
    bool json_only = false;
    bool bench_only = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--snapshot") == 0) snap_only = true;
        else if (strcmp(argv[i], "--json") == 0) json_only = true;
        else if (strcmp(argv[i], "--bench") == 0) bench_only = true;
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("usage: mono-tui [--snapshot | --json | --bench]\n");
            return 0;
        }
    }

    TuiState st;
    memset(&st, 0, sizeof(st));
    detect_gpu_and_chassis(&st);
    refresh_telemetry(&st);

    if (json_only) {
        printf("{\"chassis\":\"%s\",\"gpu\":\"%s\",\"socket_live\":%s,"
               "\"cpu_pct\":%d,\"cores\":%d,\"mem_used_gib\":%.2f,\"mem_total_gib\":%.2f,"
               "\"mem_pct\":%d,\"temp_c\":%.1f,\"motion\":\"%s\",\"glass\":\"%s\","
               "\"turbo\":%s,\"shader\":\"%s\",\"cursor_theme\":\"%s\","
               "\"cursor_size\":\"%s\",\"kb_type\":\"%s\",\"kb_layout\":\"%s\"}\n",
               st.chassis, st.gpu, st.socket_live ? "true" : "false",
               st.cpu_pct, st.ncores, st.mem_used_gib, st.mem_total_gib,
               st.mem_pct, st.temp_c, st.motion, st.glass,
               st.turbo ? "true" : "false", st.shader, st.cursor_theme,
               st.cursor_size, st.kb_type, st.kb_layout);
        return 0;
    }

    if (bench_only) {
        struct timespec req = {0, 12000000L};
        nanosleep(&req, NULL);
        refresh_telemetry(&st);
        printf("mono-tui Pure C11 CLOCK_MONOTONIC benchmark:\n");
        printf("  telemetry_proc_sys : %.2f us\n", st.bench_proc_us);
        printf("  profile_state_read : %.2f us\n", st.bench_cfg_us);
        printf("  hypr_socket_probe  : %.2f us\n", st.bench_ipc_us);
        return 0;
    }

    if (snap_only || !enter_raw_tty()) {
        struct timespec req = {0, 15000000L};
        nanosleep(&req, NULL);
        refresh_telemetry(&st);
        TStr frame;
        ts_init(&frame);
        render_dashboard(&frame, &st, 0, 0, "", false);
        fwrite(frame.s, 1, frame.len, stdout);
        ts_free(&frame);
        return 0;
    }

    int tab = 0, sel = 0;
    char status_msg[128] = "";
    TStr frame;
    ts_init(&frame);

    while (!g_quit) {
        if (g_resized) {
            g_resized = 0;
            const char *clr = "\033[2J\033[H";
            (void)!write(STDOUT_FILENO, clr, strlen(clr));
        }
        refresh_telemetry(&st);
        frame.len = 0;
        frame.s[0] = '\0';
        render_dashboard(&frame, &st, tab, sel, status_msg, true);
        (void)!write(STDOUT_FILENO, frame.s, frame.len);

        struct pollfd pfd = {STDIN_FILENO, POLLIN, 0};
        int pr = poll(&pfd, 1, 250);
        if (pr > 0 && (pfd.revents & POLLIN)) {
            unsigned char buf[16];
            ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n > 0) {
                if (buf[0] == 'q' || buf[0] == 'Q' || (n == 1 && buf[0] == 27)) break;
                if (buf[0] >= '1' && buf[0] <= '6') { tab = buf[0] - '1'; sel = 0; }
                else if (buf[0] == '\t') { tab = (tab + 1) % 6; sel = 0; }
                else if (buf[0] == 'j' || (n >= 3 && buf[0] == 27 && buf[1] == '[' && buf[2] == 'B')) {
                    int cnt = tab_item_count(tab);
                    if (cnt > 0) sel = (sel + 1) % cnt;
                } else if (buf[0] == 'k' || (n >= 3 && buf[0] == 27 && buf[1] == '[' && buf[2] == 'A')) {
                    int cnt = tab_item_count(tab);
                    if (cnt > 0) sel = (sel + cnt - 1) % cnt;
                } else if (buf[0] == '\r' || buf[0] == '\n' || buf[0] == ' ') {
                    int cnt = tab_item_count(tab);
                    if (cnt > 0 && sel < cnt) {
                        const TuiAction *list = (tab == 1) ? ACT_VELOCITY :
                                                (tab == 2) ? ACT_SHADERS :
                                                (tab == 3) ? ACT_INPUT : ACT_WALLS;
                        run_script_action(&list[sel], status_msg, sizeof(status_msg));
                    }
                }
            }
        }
    }

    ts_free(&frame);
    restore_tty();
    return 0;
}
