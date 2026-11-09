// ═══════════════════════════════════════════════════════════════════════════
//  wallpapers/animate.c — 20 Seamless Live Animated Wallpapers for `awww`
//
//  Synthesizes all 20 procedural monochrome compositions as zero-drift
//  greyscale GIF89a delta-compressed infinite loops in Pure C11:
//    - 3 Kinetic Instruments : orbital, pulse, lissajous
//    - 14 Studio Loops       : mono-01.gif .. mono-14.gif
//    - 3 Cinema UHD Loops    : mono-06-uhd.gif, mono-07-uhd.gif, mono-11-uhd.gif
//
//    ./bin/wallpaper-anim                            # render all 20 live loops
//    ./bin/wallpaper-anim 1280 720 --frames 30 --fps 24
//    ./bin/wallpaper-anim 160 90 --frames 6 --fps 12 --only orbital
//    ./bin/wallpaper-anim 160 90 --frames 6 --fps 12 --only mono-07
// ═══════════════════════════════════════════════════════════════════════════

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "generate.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static GrayImage render_orbital_frame(int w, int h, double phase) {
    double k = mono_imin(w, h) / 1080.0;
    double cx = w * 0.5, cy = h * 0.5;
    double tau = MONO_TAU * phase;

    GrayImage img = gray_new(w, h, 8);
    gray_draw_line(&img, w * 0.08, cy, w * 0.92, cy, 18, 1.0);
    gray_draw_line(&img, cx, h * 0.08, cx, h * 0.92, 18, 1.0);

    double r_core = mono_imin(w, h) * 0.17;
    GrayImage halo_raw = gray_new(w, h, 0);
    double hr = r_core * (1.26 + 0.06 * sin(tau));
    gray_fill_ellipse(&halo_raw, cx, cy, hr, hr, 38);
    GrayImage halo = gray_gaussian_blur(&halo_raw, 26.0 * k);
    gray_free(&halo_raw);
    gray_add_clamped(&img, &halo);
    gray_free(&halo);

    double orbits[3] = {r_core * 1.48, r_core * 1.98, r_core * 2.52};
    uint8_t ring_cols[3] = {42, 34, 26};
    for (size_t i = 0; i < 3; ++i) {
        gray_stroke_ellipse(&img, cx, cy, orbits[i], orbits[i], ring_cols[i], mono_dmax(1.0, 1.3 * k));
    }

    double r_tick = orbits[1];
    for (int idx = 0; idx < 36; ++idx) {
        double ang = (tau / 3.0) + idx * (MONO_TAU / 36.0);
        double r_in = r_tick - ((idx % 3 == 0) ? 5.0 * k : 2.5 * k);
        double r_out = r_tick + ((idx % 3 == 0) ? 5.0 * k : 2.5 * k);
        gray_draw_line(&img, cx + cos(ang) * r_in, cy + sin(ang) * r_in,
                       cx + cos(ang) * r_out, cy + sin(ang) * r_out,
                       (idx % 3 == 0) ? 58 : 32, 1.0);
    }

    GrayImage sphere = gray_new(w, h, 0);
    GrayImage smask = gray_new(w, h, 0);
    int ri = (int)r_core;
    double light_shift = 0.08 * sin(tau);
    for (int dy = -ri; dy <= ri; ++dy) {
        double span = sqrt(mono_dmax(0.0, r_core * r_core - dy * dy));
        double t = (dy + r_core) / (2.0 * r_core);
        uint8_t val = mono_clamp_u8(mono_dclamp(224.0 * pow(mono_dmax(0.0, 1.0 - t + light_shift), 1.35) + 14.0, 14.0, 232.0));
        if (dy > 0 && ((int)(dy / mono_dmax(1.0, 4.2 * k)) % 2 == 0)) val = 10;
        gray_draw_line(&sphere, cx - span, cy + dy, cx + span, cy + dy, val, 1.0);
        gray_draw_line(&smask, cx - span, cy + dy, cx + span, cy + dy, 255, 1.0);
    }
    GrayImage comp = gray_composite(&sphere, &img, &smask);
    gray_free(&sphere);
    gray_free(&smask);
    gray_free(&img);
    img = comp;
    gray_stroke_ellipse(&img, cx, cy, r_core, r_core, 150, mono_dmax(1.0, 1.4 * k));

    struct { double rad, ang, mr; uint8_t col; } moons[3] = {
        {orbits[0],  1.0 * tau - 0.6, 11.0 * k, 242},
        {orbits[1], -1.0 * tau + 1.8,  7.5 * k, 196},
        {orbits[2],  2.0 * tau + 0.4,  5.0 * k, 155},
    };
    GrayImage glow_raw = gray_new(w, h, 0);
    for (size_t i = 0; i < 3; ++i) {
        double mx = cx + cos(moons[i].ang) * moons[i].rad;
        double my = cy + sin(moons[i].ang) * moons[i].rad;
        gray_fill_ellipse(&glow_raw, mx, my, moons[i].mr * 2.4, moons[i].mr * 2.4, (uint8_t)(moons[i].col * 0.38));
        gray_fill_ellipse(&img, mx, my, moons[i].mr, moons[i].mr, moons[i].col);
    }
    GrayImage glow = gray_gaussian_blur(&glow_raw, 7.0 * k);
    gray_free(&glow_raw);
    gray_add_clamped(&img, &glow);
    gray_free(&glow);
    return img;
}

static GrayImage pulse_make_disc(int diag, double r0, double tau, double k, bool front) {
    GrayImage layer = gray_new(diag, diag, 0);
    double dc = diag * 0.5;
    for (int ring = 0; ring < 26; ++ring) {
        double f = ring / 25.0;
        double rx = r0 * (1.14 + f * 2.10);
        double ry = rx * 0.15;
        double s_wave = sin(f * 12.0 - tau);
        double base = 238.0 * exp(-f * 1.4) * (0.62 + 0.38 * s_wave * s_wave);
        double start_deg = front ? 0.0 : 180.0;
        for (int seg = 0; seg < 16; ++seg) {
            double a0 = start_deg + seg * 11.25;
            double a1 = a0 + 11.15;
            double mid = ((a0 + a1) * 0.5) * (MONO_PI / 180.0);
            double doppler = (0.92 - 0.24 * cos(mid)) * (0.86 + 0.14 * cos(2.0 * mid - tau));
            uint8_t lum = mono_clamp_u8(mono_dclamp(base * doppler, 0.0, 252.0));
            gray_stroke_arc(&layer, dc, dc, rx, ry, a0, a1, lum, mono_dmax(2.0, 2.6 * k));
        }
    }
    GrayImage blurred = gray_gaussian_blur(&layer, 1.5 * k);
    gray_free(&layer);
    GrayImage rot = gray_rotate_deg(&blurred, 14.0);
    gray_free(&blurred);
    return rot;
}

static GrayImage render_pulse_frame(int w, int h, double phase) {
    double k = mono_imin(w, h) / 1080.0;
    double cx = w * 0.5, cy = h * 0.5;
    double r0 = mono_imin(w, h) * 0.165;
    double tau = MONO_TAU * phase;

    GrayImage img = gray_new(w, h, 4);
    MonoMt19937 rng;
    mt_seed(&rng, 1101);
    for (int i = 0; i < 180; ++i) {
        int sx = (int)(mt_next(&rng) % (uint32_t)w);
        int sy = (int)(mt_next(&rng) % (uint32_t)h);
        double rphase = mt_unit(&rng) * MONO_TAU;
        int base_lum = 35 + (int)(mt_next(&rng) % 111u);
        double twinkle = 0.72 + 0.28 * sin(tau + rphase);
        gray_set_px(&img, sx, sy, (uint8_t)mono_imax(10, (int)(base_lum * twinkle)));
    }

    GrayImage halo_raw = gray_new(w, h, 0);
    double breath = 1.0 + 0.055 * sin(tau);
    for (int i = 28; i >= 1; --i) {
        double t = i / 28.0;
        double rad = r0 * (1.0 + t * 1.45 * breath);
        uint8_t lum = mono_clamp_u8(205.0 * pow(1.0 - t, 2.3) * (0.92 + 0.08 * sin(tau)));
        gray_fill_ellipse(&halo_raw, cx, cy, rad, rad * 0.93, lum);
    }
    GrayImage halo = gray_gaussian_blur(&halo_raw, 18.0 * k);
    gray_free(&halo_raw);

    int diag = (int)ceil(hypot(w, h));
    if (diag % 2 != 0) ++diag;
    int ox = (diag - w) / 2, oy = (diag - h) / 2;

    GrayImage bg_full = pulse_make_disc(diag, r0, tau, k, false);
    GrayImage bg = gray_crop(&bg_full, ox, oy, w, h);
    gray_free(&bg_full);

    GrayImage fg_full = pulse_make_disc(diag, r0, tau, k, true);
    GrayImage fg = gray_crop(&fg_full, ox, oy, w, h);
    gray_free(&fg_full);

    gray_add_clamped(&img, &halo);
    gray_add_clamped(&img, &bg);
    gray_free(&halo);
    gray_free(&bg);

    GrayImage mask_raw = gray_new(w, h, 0);
    gray_fill_ellipse(&mask_raw, cx, cy, r0, r0, 255);
    GrayImage mask = gray_gaussian_blur(&mask_raw, 1.2 * k);
    gray_free(&mask_raw);

    GrayImage hole = gray_new(w, h, 2);
    GrayImage comp = gray_composite(&hole, &img, &mask);
    gray_free(&hole);
    gray_free(&mask);
    gray_free(&img);
    img = comp;

    uint8_t ring_lum = mono_clamp_u8(218.0 + 24.0 * sin(tau));
    gray_stroke_ellipse(&img, cx, cy, r0 * 1.012, r0 * 1.012, ring_lum, mono_dmax(1.0, 2.0 * k));
    gray_add_clamped(&img, &fg);
    gray_free(&fg);
    return img;
}

static GrayImage render_lissajous_frame(int w, int h, double phase) {
    double k = mono_imin(w, h) / 1080.0;
    double cx = w * 0.5, cy = h * 0.5;
    double tau = MONO_TAU * phase;

    GrayImage img = gray_new(w, h, 6);
    int gx0 = (int)(w * 0.14), gx1 = (int)(w * 0.86);
    int gy0 = (int)(h * 0.12), gy1 = (int)(h * 0.88);
    gray_stroke_rect(&img, gx0, gy0, gx1, gy1, 22, 1);
    for (int div = 1; div < 8; ++div) {
        int x = gx0 + (gx1 - gx0) * div / 8;
        gray_draw_line(&img, x, gy0, x, gy1, (div == 4) ? 24 : 14, 1.0);
    }
    for (int div = 1; div < 6; ++div) {
        int y = gy0 + (gy1 - gy0) * div / 6;
        gray_draw_line(&img, gx0, y, gx1, y, (div == 3) ? 24 : 14, 1.0);
    }
    for (int t = -15; t <= 15; ++t) {
        if (t == 0) continue;
        int tx = (int)(cx + t * (gx1 - gx0) / 34.0);
        int ty = (int)(cy + t * (gy1 - gy0) / 34.0);
        gray_draw_line(&img, tx, cy - 3.0 * k, tx, cy + 3.0 * k, 34, 1.0);
        gray_draw_line(&img, cx - 3.0 * k, ty, cx + 3.0 * k, ty, 34, 1.0);
    }

    double rx = mono_imin(w, h) * 0.31, ry = rx;
    GrayImage glow_raw = gray_new(w, h, 0);
    GrayImage core = gray_new(w, h, 0);

    const int STEPS = 360;
    MonoPt ghost_pts[361], pts[361];
    for (int i = 0; i <= STEPS; ++i) {
        double s = MONO_TAU * ((double)i / STEPS);
        ghost_pts[i] = (MonoPt){cx + rx * 0.74 * sin(2.0 * s - tau), cy + ry * 0.74 * cos(3.0 * s)};
        pts[i] = (MonoPt){cx + rx * sin(3.0 * s + tau), cy + ry * sin(2.0 * s)};
    }
    gray_draw_polyline(&core, ghost_pts, STEPS + 1, 54, mono_dmax(1.0, 1.2 * k));
    gray_draw_polyline(&glow_raw, pts, STEPS + 1, 145, mono_dmax(3.0, 6.0 * k));
    gray_draw_polyline(&core, pts, STEPS + 1, 236, mono_dmax(1.0, 2.0 * k));

    double head_x = cx + rx * sin(tau);
    double hr = 5.5 * k;
    gray_fill_ellipse(&glow_raw, head_x, cy, hr * 3.0, hr * 3.0, 210);
    gray_fill_ellipse(&core, head_x, cy, hr, hr, 255);

    GrayImage glow = gray_gaussian_blur(&glow_raw, 8.5 * k);
    gray_free(&glow_raw);
    gray_add_clamped(&img, &glow);
    gray_add_clamped(&img, &core);
    gray_free(&glow);
    gray_free(&core);
    return img;
}

typedef struct {
    const char *key;
    const char *alt_num;
    const char *filename;
    const char *desc;
    int default_w;
    int default_h;
    GrayImage (*fn)(int w, int h, double phase);
} AnimSpec;

static const AnimSpec ANIMATIONS[20] = {
    {"orbital",     "",   "mono-live-01-orbital.gif",   "Bauhaus celestial orrery loop",                960,  540, render_orbital_frame},
    {"pulse",       "",   "mono-live-02-pulse.gif",     "Relativistic singularity pulse loop",          960,  540, render_pulse_frame},
    {"lissajous",   "",   "mono-live-03-lissajous.gif", "P4 white-phosphor Lissajous oscilloscope",     960,  540, render_lissajous_frame},
    {"mono-01",     "1",  "mono-01.gif",                "Noir Vignette live studio key-light loop",     960,  540, design_01_vignette},
    {"mono-02",     "2",  "mono-02.gif",                "Architectural Grid live telemetry scan loop",  960,  540, design_02_grid},
    {"mono-03",     "3",  "mono-03.gif",                "Concentric Arcs live acoustic wavefront loop", 960,  540, design_03_arcs},
    {"mono-04",     "4",  "mono-04.gif",                "Topographic Contours live isoline flow loop",  960,  540, design_04_contours},
    {"mono-05",     "5",  "mono-05.gif",                "Diagonal Strata live brushed graphite loop",   960,  540, design_05_diagonal},
    {"mono-06",     "6",  "mono-06.gif",                "Horizon Mist live sea fog & ripple loop",      960,  540, design_06_horizon},
    {"mono-07",     "7",  "mono-07.gif",                "Solar Eclipse live diamond-ring flare loop",   960,  540, design_07_eclipse},
    {"mono-08",     "8",  "mono-08.gif",                "Alpine Ridgeline live valley mist loop",       960,  540, design_08_ridgeline},
    {"mono-09",     "9",  "mono-09.gif",                "Brutalist Monoliths live raking light loop",   960,  540, design_09_monoliths},
    {"mono-10",     "10", "mono-10.gif",                "Caustic Silk Ribbon live harmonic wave loop",  960,  540, design_10_ribbon},
    {"mono-11",     "11", "mono-11.gif",                "Gravitational Singularity live accretion loop",960,  540, design_11_singularity},
    {"mono-12",     "12", "mono-12.gif",                "Liquid Mercury live ferrofluid chrome loop",   960,  540, design_12_mercury},
    {"mono-13",     "13", "mono-13.gif",                "Rain & Bokeh Noir live nightfall loop",        960,  540, design_13_rain_bokeh},
    {"mono-14",     "14", "mono-14.gif",                "Bauhaus Compass live vernier orbit loop",      960,  540, design_14_bauhaus},
    {"mono-06-uhd", "",   "mono-06-uhd.gif",            "Horizon Mist Cinema Master live loop",         1280, 720, design_06_horizon},
    {"mono-07-uhd", "",   "mono-07-uhd.gif",            "Solar Eclipse Cinema Master live loop",        1280, 720, design_07_eclipse},
    {"mono-11-uhd", "",   "mono-11-uhd.gif",            "Singularity Cinema Master live loop",          1280, 720, design_11_singularity},
};

static void resolve_default_outdir(const char *argv0, char *out, size_t cap) {
    char exe[PATH_MAX];
    if (argv0 && realpath(argv0, exe)) {
        char *slash = strrchr(exe, '/');
        if (slash && slash != exe) {
            *slash = '\0';
            const char *base = strrchr(exe, '/');
            if (base && strcmp(base + 1, "bin") == 0) {
                char repo[PATH_MAX];
                size_t rlen = (size_t)(base - exe);
                if (rlen > 0 && rlen < sizeof(repo)) {
                    memcpy(repo, exe, rlen);
                    repo[rlen] = '\0';
                    char wall[PATH_MAX];
                    int n = snprintf(wall, sizeof(wall), "%s/wallpapers", repo);
                    struct stat st;
                    if (n > 0 && (size_t)n < sizeof(wall) && stat(wall, &st) == 0 && S_ISDIR(st.st_mode)) {
                        snprintf(out, cap, "%s", wall);
                        return;
                    }
                }
            }
            snprintf(out, cap, "%s", exe);
            return;
        }
    }
    if (!getcwd(out, cap)) snprintf(out, cap, ".");
}

static void mkdir_p_local(const char *dir) {
    char tmp[PATH_MAX];
    int n = snprintf(tmp, sizeof(tmp), "%s", dir);
    if (n <= 0 || (size_t)n >= sizeof(tmp)) return;
    for (char *p = tmp + 1; *p; ++p) {
        if (*p == '/') { *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
    }
    mkdir(tmp, 0755);
}

typedef struct {
    int tid;
    int nthreads;
    int nframes;
    int width;
    int height;
    const AnimSpec *spec;
    GrayImage *frames;
} AnimWorkerJob;

static void *anim_worker_entry(void *arg) {
    AnimWorkerJob *j = (AnimWorkerJob *)arg;
    *mono_in_frame_worker_ptr() = true;
    for (int idx = j->tid; idx < j->nframes; idx += j->nthreads) {
        j->frames[idx] = j->spec->fn(j->width, j->height, (double)idx / j->nframes);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int explicit_w = 0;
    int explicit_h = 0;
    int nframes = 24;
    int fps = 20;
    const char *only = "";
    char out_dir[PATH_MAX];
    resolve_default_outdir(argv[0], out_dir, sizeof(out_dir));

    int pos_dims[4];
    int npos = 0;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "--frames") == 0 && i + 1 < argc) {
            nframes = mono_imax(1, atoi(argv[++i]));
        } else if (strcmp(arg, "--fps") == 0 && i + 1 < argc) {
            fps = mono_imax(1, atoi(argv[++i]));
        } else if (strcmp(arg, "--only") == 0 && i + 1 < argc) {
            only = argv[++i];
        } else if (strcmp(arg, "--out-dir") == 0 && i + 1 < argc) {
            snprintf(out_dir, sizeof(out_dir), "%s", argv[++i]);
        } else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            printf("Usage: %s [width] [height] [--frames N] [--fps F] [--only key|1..14] [--out-dir DIR]\n", argv[0]);
            return 0;
        } else if (arg[0] != '\0' && arg[0] != '-') {
            if (npos < 4) pos_dims[npos++] = atoi(arg);
        } else {
            fprintf(stderr, "Unknown option: %s\n", arg);
            return 1;
        }
    }
    if (npos >= 1) explicit_w = pos_dims[0];
    if (npos >= 2) explicit_h = pos_dims[1];

    const AnimSpec *selected[20];
    size_t nsel = 0;
    if (only[0] != '\0') {
        const char *norm_num = only;
        if (strlen(only) == 2 && only[0] == '0' && only[1] >= '1' && only[1] <= '9') {
            norm_num = only + 1;
        }
        for (size_t i = 0; i < 20; ++i) {
            if (strcmp(only, ANIMATIONS[i].key) == 0 ||
                strcmp(only, ANIMATIONS[i].filename) == 0 ||
                (ANIMATIONS[i].alt_num[0] != '\0' && strcmp(norm_num, ANIMATIONS[i].alt_num) == 0)) {
                selected[nsel++] = &ANIMATIONS[i];
            }
        }
        if (nsel == 0) {
            fprintf(stderr, "unknown animation '%s'\n", only);
            return 1;
        }
    } else {
        for (size_t i = 0; i < 20; ++i) selected[nsel++] = &ANIMATIONS[i];
    }

    mkdir_p_local(out_dir);
    int hw = mono_cpu_count();

    for (size_t i = 0; i < nsel; ++i) {
        const AnimSpec *spec = selected[i];
        int width = (explicit_w > 0) ? explicit_w : spec->default_w;
        int height = (explicit_h > 0) ? explicit_h : spec->default_h;
        char out[PATH_MAX];
        int n = snprintf(out, sizeof(out), "%s/%s", out_dir, spec->filename);
        if (n < 0 || (size_t)n >= sizeof(out)) return 1;

        printf("Rendering %s (%dx%d, %d frames @ %dfps — %s)...\n",
               spec->filename, width, height, nframes, fps, spec->desc);

        GrayImage *frames = (GrayImage *)calloc((size_t)nframes, sizeof(GrayImage));
        if (!frames) abort();
        int nthreads = mono_imin(nframes, hw);
        pthread_t tids[16];
        AnimWorkerJob jobs[16];
        int spawned = 0;

        for (int t = 0; t < nthreads; ++t) {
            jobs[spawned] = (AnimWorkerJob){t, nthreads, nframes, width, height, spec, frames};
            if (pthread_create(&tids[spawned], NULL, anim_worker_entry, &jobs[spawned]) == 0) {
                spawned++;
            } else {
                anim_worker_entry(&jobs[spawned]);
            }
        }
        for (int t = 0; t < spawned; ++t) pthread_join(tids[t], NULL);

        if (!mono_save_gif_loop(frames, (size_t)nframes, out, fps, 1)) {
            for (int f = 0; f < nframes; ++f) gray_free(&frames[f]);
            free(frames);
            return 1;
        }
        for (int f = 0; f < nframes; ++f) gray_free(&frames[f]);
        free(frames);

        struct stat st;
        unsigned long kb = (stat(out, &st) == 0) ? (unsigned long)(st.st_size / 1024) : 0;
        printf("  wrote %s (%lu KB)\n", spec->filename, kb);
    }
    return 0;
}
