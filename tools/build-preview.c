// ═══════════════════════════════════════════════════════════════════════════
//  tools/build-preview.c — Build self-contained docs/preview.html in Pure C11
//
//  Reads the live configuration (`binds.lua`, `user.lua`, `waybar/config.jsonc`,
//  `rofi/theme.rasi`, `kitty/theme.conf`, `starship.toml`, `packages.txt`,
//  `aur-packages.txt`, `docs/preview.svg`, and `wallpapers/`) and emits a
//  zero-external-asset HTML preview with inline base64 GIF loops and
//  the 60 FPS interactive Velocity & Liquid Glass desktop simulator.
//
//    ./bin/build-preview            # write docs/preview.html
//    ./bin/build-preview --check    # exit 1 if out of date (ignores date stamp)
// ═══════════════════════════════════════════════════════════════════════════

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "../include/keybind_table.h"
#include "../include/mono_config.h"
#include "../include/mono_image.h"
#include "../wallpapers/generate.h"

#include <dirent.h>
#include <time.h>

static void today_iso(char *out, size_t cap) {
    time_t now = time(NULL);
    struct tm tm_val;
    memset(&tm_val, 0, sizeof(tm_val));
    localtime_r(&now, &tm_val);
    strftime(out, cap, "%Y-%m-%d", &tm_val);
}

static void append_swatch(MonoStr *out, const char *name, const char *val) {
    char *en = mono_html_escape(name);
    char *ev = mono_html_escape(val);
    mstr_printf(out,
        "<div class=\"sw\"><div class=\"chip\" style=\"background:%s\"></div>"
        "<div class=\"meta\"><b>%s</b><span>%s</span></div></div>",
        val, en, ev);
    free(en);
    free(ev);
}

typedef struct {
    int f;
    int nframes;
    const MonoDesignSpec *spec;
    GrayImage *frames;
} PreviewFrameJob;

static void *preview_frame_worker(void *arg) {
    PreviewFrameJob *j = (PreviewFrameJob *)arg;
    *mono_in_frame_worker_ptr() = true;
    j->frames[j->f] = j->spec->fn(320, 180, (double)j->f / j->nframes);
    return NULL;
}

static void build_wallpaper_cards(const char *repo, char **hero_uri_out, char **cards_html_out) {
    char wall_dir[PATH_MAX];
    mono_path_join(wall_dir, sizeof(wall_dir), repo, "wallpapers");

    char *hero = mono_strdup("");
    MonoStr items;
    mstr_init(&items);

    const int PREVIEW_FRAMES = 10;
    for (size_t i = 0; i < 14; ++i) {
        const MonoDesignSpec *d = &MONO_DESIGNS[i];
        char p_gif[PATH_MAX], p_jpg[PATH_MAX], p_uhd[PATH_MAX];
        int n1 = snprintf(p_gif, sizeof(p_gif), "%s/%s.gif", wall_dir, d->stem);
        int n2 = snprintf(p_jpg, sizeof(p_jpg), "%s/%s.jpg", wall_dir, d->stem);
        int n3 = snprintf(p_uhd, sizeof(p_uhd), "%s/%s-uhd.gif", wall_dir, d->stem);
        if (n1 < 0 || n2 < 0 || n3 < 0) continue;
        if (!mono_file_exists(p_gif) && !mono_file_exists(p_jpg)) continue;

        GrayImage frames[10];
        pthread_t tids[10];
        PreviewFrameJob jobs[10];
        bool ok_th[10];
        for (int f = 0; f < PREVIEW_FRAMES; ++f) {
            jobs[f] = (PreviewFrameJob){f, PREVIEW_FRAMES, d, frames};
            ok_th[f] = (pthread_create(&tids[f], NULL, preview_frame_worker, &jobs[f]) == 0);
            if (!ok_th[f]) preview_frame_worker(&jobs[f]);
        }
        for (int f = 0; f < PREVIEW_FRAMES; ++f) {
            if (ok_th[f]) pthread_join(tids[f], NULL);
        }

        MonoByteBuf gif = mono_encode_gif_loop_mem(frames, PREVIEW_FRAMES, 16, 2);
        for (int f = 0; f < PREVIEW_FRAMES; ++f) gray_free(&frames[f]);

        char *b64 = mono_base64_encode(gif.data, gif.len);
        mbuf_free(&gif);

        MonoStr uri;
        mstr_init(&uri);
        mstr_printf(&uri, "data:image/gif;base64,%s", b64);
        free(b64);

        if (strcmp(d->stem, "mono-11") == 0 || hero[0] == '\0') {
            free(hero);
            hero = mono_strdup(uri.s);
        }

        const char *badge = mono_file_exists(p_uhd) ? "Cinema \xc2\xb7 Live Loop" : "60fps \xc2\xb7 Live Loop";
        const char *active = (strcmp(d->stem, "mono-11") == 0) ? " active" : "";

        char *e_stem = mono_html_escape(d->stem);
        char *e_title = mono_html_escape(d->title);
        char *e_sub = mono_html_escape(d->subtitle);
        char *e_badge = mono_html_escape(badge);

        mstr_printf(&items,
            "<button type=\"button\" class=\"wcard%s\" data-uri=\"%s\""
            " data-title=\"%s \xe2\x80\x94 %s\" data-desc=\"%s (%s)\">"
            "<img src=\"%s\" alt=\"%s\">"
            "<div class=\"wmeta\"><b>%s \xc2\xb7 %s</b><span>%s</span></div></button>",
            active, uri.s, e_stem, e_title, e_sub, e_badge, uri.s, e_stem, e_stem, e_title, e_badge);

        free(e_stem);
        free(e_title);
        free(e_sub);
        free(e_badge);
        mstr_free(&uri);
    }

    *hero_uri_out = hero;
    *cards_html_out = items.s;
}

static void render_bar_group(MonoStr *out, const MonoStrList *mods) {
    static const struct { const char *mod; const char *icon; const char *val; } SAMPLE[] = {
        {"custom/launcher",     "\xf3\xb0\xa3\x87", ""},
        {"hyprland/workspaces", "", ""},
        {"hyprland/submap",     "RESIZE", ""},
        {"hyprland/window",     "", "~/projects/monochrome \xe2\x80\x94 kitty"},
        {"mpris",               "\xf3\xb0\x8e\x88", "Midnight City \xe2\x80\x94 M83"},
        {"custom/sysgraph",     "\xe2\x96\x81\xe2\x96\x82\xe2\x96\x84\xe2\x96\x86\xe2\x96\x83\xe2\x96\x82", "14%"},
        {"cpu",                 "", "12%"},
        {"memory",              "", "42%"},
        {"temperature",         "", "48\xc2\xb0" "C"},
        {"custom/updates",      "\xf3\xb0\x9a\xb0", "7"},
        {"network",             "\xf3\xb0\x96\xa9", "wlan0"},
        {"bluetooth",           "\xf3\xb0\x82\xaf", ""},
        {"pulseaudio",          "\xf3\xb0\x95\xbe", "65%"},
        {"battery",             "\xf3\xb0\x81\xb9", "88%"},
        {"custom/keyboard",     "US", ""},
        {"custom/perf",         "\xe2\x9a\xa1", "TURBO"},
        {"custom/focus",        "\xe2\x97\x8e", "24:58"},
        {"custom/recording",    "\xe2\x97\x8f", "00:42"},
        {"tray",                "", ""},
        {"clock",               "", "21:42"},
        {"custom/settings",     "\xe2\x9a\x99", ""},
        {"custom/power",        "\xe2\x8f\xbb", ""},
        {NULL, NULL, NULL}
    };

    mstr_puts(out, "<div class=\"group \">");
    for (size_t i = 0; i < mods->len; ++i) {
        const char *mod = mods->items[i];
        if (strcmp(mod, "hyprland/workspaces") == 0) {
            mstr_puts(out, "<span class=\"ws active\">1</span><span class=\"ws\">2</span>"
                           "<span class=\"ws\">3</span><span class=\"ws\">4</span><span class=\"ws\">5</span>");
            continue;
        }
        if (strcmp(mod, "tray") == 0) {
            mstr_puts(out, "<span class=\"mod tray\"><span class=\"dot\"></span><span class=\"dot\"></span></span>");
            continue;
        }
        const char *icon = "", *val = "";
        for (size_t s = 0; SAMPLE[s].mod != NULL; ++s) {
            if (strcmp(SAMPLE[s].mod, mod) == 0) { icon = SAMPLE[s].icon; val = SAMPLE[s].val; break; }
        }
        char *css = mono_strdup(mod);
        for (size_t k = 0; css[k]; ++k) if (css[k] == '/') css[k] = '-';
        char *e_icon = mono_html_escape(icon);
        char *e_val = mono_html_escape(val);
        if (val[0] == '\0') {
            mstr_printf(out, "<span class=\"mod %s\">%s</span>", css, e_icon);
        } else {
            mstr_printf(out, "<span class=\"mod %s\">%s<b>%s</b></span>", css, e_icon, e_val);
        }
        free(e_icon);
        free(e_val);
        free(css);
    }
    mstr_puts(out, "</div>");
}

static char *build_bar_html(const JsonVal *config) {
    MonoStrList left = json_get_string_array(config, "modules-left");
    MonoStrList center = json_get_string_array(config, "modules-center");
    MonoStrList right = json_get_string_array(config, "modules-right");

    MonoStr body;
    mstr_init(&body);
    mstr_puts(&body, "<div class=\"brow\"><span class=\"blabel\">left</span>");
    render_bar_group(&body, &left);
    mstr_puts(&body, "</div><div class=\"brow\"><span class=\"blabel\">centre</span>");
    render_bar_group(&body, &center);
    mstr_puts(&body, "</div><div class=\"brow\"><span class=\"blabel\">right</span>");
    render_bar_group(&body, &right);
    mstr_puts(&body, "</div>");

    MonoStr names;
    mstr_init(&names);
    for (size_t i = 0; i < right.len; ++i) {
        if (i > 0) mstr_puts(&names, ", ");
        mstr_puts(&names, right.items[i]);
    }
    char *e_names = mono_html_escape(names.s);
    mstr_free(&names);

    MonoStr res;
    mstr_init(&res);
    mstr_printf(&res,
        "<div class=\"bar\">%s</div>\n"
        "<p class=\"caption\">The three floating groups of the bar, exactly as listed in\n"
        "<code>waybar/config.jsonc</code> \xe2\x80\x94 on screen they sit in one row, left \xc2\xb7 centre \xc2\xb7 right.\n"
        "Module values are sample data; the right group is ordered\n"
        "<code>%s</code>.</p>",
        body.s, e_names);

    free(e_names);
    mstr_free(&body);
    mslist_free(&left);
    mslist_free(&center);
    mslist_free(&right);
    return res.s;
}

static void walk_repo_files(const char *root, const char *rel_prefix, MonoStrList *rels) {
    char dir_path[PATH_MAX];
    if (rel_prefix[0] == '\0') snprintf(dir_path, sizeof(dir_path), "%s", root);
    else mono_path_join(dir_path, sizeof(dir_path), root, rel_prefix);

    DIR *dp = opendir(dir_path);
    if (!dp) return;
    struct dirent *ent;
    while ((ent = readdir(dp)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        char rel[PATH_MAX];
        if (rel_prefix[0] == '\0') snprintf(rel, sizeof(rel), "%s", ent->d_name);
        else {
            int n = snprintf(rel, sizeof(rel), "%s/%s", rel_prefix, ent->d_name);
            if (n < 0 || (size_t)n >= sizeof(rel)) continue;
        }
        if (strcmp(rel, ".git") == 0 || mono_starts_with(rel, ".git/") ||
            strcmp(rel, "bin") == 0 || mono_starts_with(rel, "bin/") ||
            mono_starts_with(rel, ".config/hypr/bin")) {
            continue;
        }
        if (mono_starts_with(rel, "cursors/") &&
            (strstr(rel, "/cursors") != NULL || strstr(rel, "/hyprcursors") != NULL)) {
            continue;
        }

        char full[PATH_MAX];
        mono_path_join(full, sizeof(full), root, rel);
        struct stat st;
        if (lstat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            walk_repo_files(root, rel, rels);
        } else if (S_ISREG(st.st_mode)) {
            mslist_push(rels, rel);
        }
    }
    closedir(dp);
}

static int cmp_cstr_ptr(const void *a, const void *b) {
    const char *sa = *(const char *const *)a;
    const char *sb = *(const char *const *)b;
    return strcmp(sa, sb);
}

static char *build_tree_html(const char *repo) {
    MonoStrList rels;
    mslist_init(&rels);
    walk_repo_files(repo, "", &rels);
    if (rels.len > 1) qsort(rels.items, rels.len, sizeof(char *), cmp_cstr_ptr);

    MonoStrList seen_dirs;
    mslist_init(&seen_dirs);
    MonoStr html;
    mstr_init(&html);

    for (size_t i = 0; i < rels.len; ++i) {
        const char *rel = rels.items[i];
        MonoStrList parts;
        mslist_init(&parts);
        size_t rlen = strlen(rel), start = 0;
        while (start <= rlen) {
            const char *sl = strchr(rel + start, '/');
            if (!sl) {
                mslist_push(&parts, rel + start);
                break;
            }
            mslist_push_owned(&parts, mono_strndup(rel + start, (size_t)(sl - (rel + start))));
            start = (size_t)(sl - rel) + 1;
        }

        MonoStr key;
        mstr_init(&key);
        for (size_t d = 0; d + 1 < parts.len; ++d) {
            if (d > 0) mstr_putc(&key, '/');
            mstr_puts(&key, parts.items[d]);
            if (!mslist_contains(&seen_dirs, key.s)) {
                mslist_push(&seen_dirs, key.s);
                mstr_puts(&html, "<div>");
                for (size_t p = 0; p < d; ++p) mstr_puts(&html, "\xe2\x94\x82  ");
                char *ed = mono_html_escape(parts.items[d]);
                mstr_printf(&html, "<span class=\"d\">%s/</span></div>", ed);
                free(ed);
            }
        }
        mstr_free(&key);

        if (parts.len > 0) {
            mstr_puts(&html, "<div>");
            for (size_t p = 0; p + 1 < parts.len; ++p) mstr_puts(&html, "\xe2\x94\x82  ");
            char *ef = mono_html_escape(parts.items[parts.len - 1]);
            mstr_printf(&html, "<span class=\"\">%s</span></div>", ef);
            free(ef);
        }
        mslist_free(&parts);
    }

    mslist_free(&seen_dirs);
    mslist_free(&rels);
    return html.s;
}

static const char PREVIEW_CSS[] =
    ":root{--ink:#080808;--surface:#0d0d0d;--surface-2:#141414;--surface-3:#1a1a1a;"
    "--text:#e6e6e6;--dim:#8a8a8a;--faint:#4a4a4a;--line:rgba(255,255,255,.08);"
    "--line-strong:rgba(255,255,255,.16);--mono:\"JetBrainsMono Nerd Font\",\"JetBrains Mono\",ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;}"
    "*{box-sizing:border-box}body{margin:0;background:var(--ink);color:var(--text);font-family:var(--mono);font-size:14px;line-height:1.6;-webkit-font-smoothing:antialiased;}"
    ".wrap{max-width:1180px;margin:0 auto;padding:56px 24px 96px}"
    "a{color:#fff;text-decoration:none;border-bottom:1px solid var(--line-strong)}a:hover{border-bottom-color:#fff}"
    "h1{font-size:34px;margin:0 0 6px;letter-spacing:-.5px;color:#fff}"
    "h2{font-size:19px;margin:64px 0 4px;color:#fff;letter-spacing:.2px}h2 .num{color:var(--faint);margin-right:10px;font-weight:400}"
    "h3{font-size:14px;margin:28px 0 8px;color:var(--dim);text-transform:uppercase;letter-spacing:1.4px;font-weight:500}"
    "p{margin:8px 0;color:var(--text)}.lead{color:var(--dim);max-width:74ch}"
    "code{font-family:var(--mono);background:var(--surface-2);border:1px solid var(--line);border-radius:5px;padding:1px 6px;font-size:12.5px;color:#fff}"
    ".badges{display:flex;flex-wrap:wrap;gap:8px;margin:18px 0 0}"
    ".badge{border:1px solid var(--line);background:var(--surface);border-radius:999px;padding:4px 12px;font-size:12px;color:var(--dim)}.badge b{color:#fff;font-weight:500}"
    ".figure{border:1px solid var(--line);border-radius:14px;overflow:hidden;margin-top:18px;background:#000}.figure svg{display:block;width:100%;height:auto}"
    ".caption{color:var(--faint);font-size:12px;margin:10px 0 0}"
    ".bar{display:flex;flex-direction:column;gap:7px;background:#050505;border:1px solid var(--line);border-radius:14px;padding:12px 14px;margin:18px 0 6px}"
    ".brow{display:flex;gap:10px;align-items:center;flex-wrap:wrap}.blabel{color:var(--dim);font-size:10px;letter-spacing:.16em;text-transform:uppercase;min-width:50px}"
    ".group{display:flex;gap:2px;align-items:center;background:rgba(10,10,10,.72);border:1px solid var(--line);border-radius:12px;padding:4px 8px;flex-wrap:wrap}"
    ".mod{color:var(--text);padding:0 8px;font-size:12.5px;white-space:nowrap}.mod b{color:#fff;font-weight:500;margin-left:5px}"
    ".ws{color:#6f6f6f;padding:2px 7px;border-radius:7px;font-size:12.5px}.ws.active{background:#e6e6e6;color:#0a0a0a;font-weight:600}"
    ".mod.custom-focus,.mod.hyprland-submap,.mod.custom-perf{background:#e6e6e6;color:#0a0a0a;border-radius:7px;padding:1px 8px;margin:0 3px;font-weight:600;font-size:11.5px}"
    ".mod.custom-focus b,.mod.hyprland-submap b,.mod.custom-perf b{color:#0a0a0a;font-weight:700}"
    ".wgrid{display:grid;grid-template-columns:repeat(auto-fill,minmax(195px,1fr));gap:10px;margin-top:12px}"
    ".simbar{display:flex;flex-wrap:wrap;gap:6px;align-items:center;margin:16px 0 10px}"
    ".simbtn{appearance:none;background:var(--surface);border:1px solid var(--line-strong);color:var(--dim);border-radius:8px;padding:5px 11px;font:inherit;font-size:11.5px;cursor:pointer;transition:all .15s ease}"
    ".simbtn:hover{color:#fff;border-color:#fff}.simbtn.on{background:#e6e6e6;color:#080808;border-color:#e6e6e6;font-weight:600}"
    ".simstage{position:relative;aspect-ratio:16/9;border:1px solid var(--line-strong);border-radius:14px;overflow:hidden;background:#050505;user-select:none;--dur:440ms;--ease:cubic-bezier(0.16,1,0.3,1)}"
    ".simstage.snappy{--dur:160ms;--ease:cubic-bezier(0.2,0,0,1)}.simstage.cinematic{--dur:680ms;--ease:cubic-bezier(0.12,1.04,0.28,1)}"
    ".simstage.minimal,.simstage.turbo{--dur:0ms;--ease:linear}"
    ".simstage.turbo .simgrid{inset:38px 0 0 0;gap:0}.simstage.turbo .simwin{border-radius:0;backdrop-filter:none;background:#090909}"
    ".simstage.glass-xray .simwin{backdrop-filter:blur(5px);background:rgba(10,10,10,.88)}"
    ".simstage.glass-acrylic .simwin{backdrop-filter:blur(18px) saturate(1.1) contrast(1.06);background:rgba(18,18,18,.68);border-color:rgba(255,255,255,.26)}"
    ".simstage.glass-off .simwin{backdrop-filter:none;background:#0b0b0b}"
    ".simstage.sh-noir{filter:grayscale(1) contrast(1.12)}.simstage.sh-grain{filter:grayscale(0.9) contrast(1.08) brightness(0.96)}"
    ".simstage.sh-crt{filter:grayscale(1) contrast(1.16) brightness(1.03)}.simstage.sh-eink{filter:grayscale(1) contrast(0.84) brightness(1.08)}.simstage.sh-oled{filter:grayscale(1) contrast(1.24) brightness(0.92)}"
    ".simtabs{display:flex;gap:4px;margin-bottom:7px}.simtab{appearance:none;background:rgba(24,24,24,.88);border:1px solid rgba(255,255,255,.10);color:#8a8a8a;border-radius:7px;padding:2px 8px;font:inherit;font-size:10px;cursor:pointer}.simtab.active{background:#383838;color:#fff;border-color:rgba(255,255,255,.32);font-weight:600}"
    ".simcrt{position:absolute;inset:0;pointer-events:none;opacity:0;z-index:6;background:repeating-linear-gradient(0deg,rgba(0,0,0,.24) 0px,rgba(0,0,0,.24) 1px,transparent 1px,transparent 3px)}.simstage.sh-crt .simcrt{opacity:1}"
    ".simcanvas{position:absolute;inset:0;width:100%;height:100%;display:block}.simbg{position:absolute;inset:0;width:100%;height:100%;object-fit:cover;opacity:0;clip-path:circle(145% at 50% 50%);transition:opacity 360ms var(--ease),clip-path 680ms var(--ease)}.simbg.show{opacity:1}"
    ".simtop{position:absolute;top:8px;left:10px;right:10px;height:28px;display:flex;align-items:center;justify-content:space-between;padding:0 10px;background:rgba(10,10,10,.82);backdrop-filter:blur(10px);border:1px solid var(--line-strong);border-radius:9px;font-size:11px;z-index:3;transition:transform var(--dur) var(--ease),opacity var(--dur) var(--ease)}"
    ".simstage.zen .simtop{transform:translateY(-44px);opacity:0}"
    ".simws{display:inline-block;color:#6f6f6f;padding:1px 6px;min-width:18px;text-align:center;border-radius:6px;cursor:pointer}.simws.active{background:#e6e6e6;color:#080808;font-weight:700;padding:1px 11px;min-width:26px}"
    ".simgrid{position:absolute;inset:46px 12px 12px 12px;display:grid;grid-template-columns:1.15fr .85fr;gap:10px;z-index:2;transition:inset var(--dur) var(--ease),gap var(--dur) var(--ease),transform var(--dur) var(--ease),opacity var(--dur) var(--ease)}"
    ".simstage.zen .simgrid{inset:20px 26px 20px 26px;gap:18px}.simstage.airy .simgrid{inset:52px 24px 24px 24px;gap:18px}.simstage.compact .simgrid{inset:40px 5px 5px 5px;gap:4px}.simstage.zero .simgrid{inset:38px 0 0 0;gap:0}"
    ".simwin{background:rgba(10,10,10,.84);backdrop-filter:blur(10px);border:1px solid rgba(255,255,255,.14);border-radius:10px;padding:12px 14px;font-size:11.5px;line-height:1.55;overflow:hidden;display:flex;flex-direction:column}.simstage.zero .simwin{border-radius:0}.simwin.focus{border-color:rgba(255,255,255,.72);box-shadow:0 12px 36px rgba(0,0,0,.55)}"
    ".simdim{position:absolute;inset:0;background:rgba(0,0,0,.46);backdrop-filter:blur(3px);opacity:0;pointer-events:none;z-index:3}.simdim.on{opacity:1}"
    ".simtitle{display:flex;justify-content:space-between;color:var(--dim);font-size:10.5px;border-bottom:1px solid var(--line);padding-bottom:6px;margin-bottom:8px}"
    ".simdrawer{position:absolute;left:14%;right:14%;top:15%;bottom:17%;background:rgba(12,12,12,.94);border:1px solid #e6e6e6;border-radius:12px;padding:14px 16px;box-shadow:0 24px 64px rgba(0,0,0,.82);display:flex;flex-direction:column;z-index:4;opacity:0;pointer-events:none;transform:translateY(-28px) scale(0.95);transition:transform var(--dur) var(--ease),opacity var(--dur) var(--ease)}.simdrawer.open{opacity:1;pointer-events:auto;transform:translateY(0) scale(1)}"
    ".simpal{position:absolute;left:20%;right:20%;top:16%;background:rgba(10,10,10,.96);border:1px solid #ffffff;border-radius:12px;padding:12px;box-shadow:0 24px 64px rgba(0,0,0,.88);display:flex;flex-direction:column;gap:8px;z-index:5;opacity:0;pointer-events:none;transform:scale(0.92) translateY(-8px);transition:transform var(--dur) var(--ease),opacity var(--dur) var(--ease)}.simpal.open{opacity:1;pointer-events:auto;transform:scale(1) translateY(0)}"
    ".simpal input{width:100%;background:#141414;border:1px solid var(--line-strong);border-radius:8px;color:#fff;font:inherit;font-size:12px;padding:7px 10px;outline:none}"
    ".simpal ul{list-style:none;margin:0;padding:0;font-size:11.5px;max-height:150px;overflow:auto}.simpal li{padding:5px 8px;border-radius:6px;color:var(--text);cursor:pointer}.simpal li:first-child,.simpal li:hover{background:#e6e6e6;color:#080808;font-weight:600}"
    ".wcard{appearance:none;background:var(--surface);border:1px solid var(--line);border-radius:10px;padding:0;overflow:hidden;text-align:left;cursor:pointer;color:var(--text);font:inherit}.wcard:hover{border-color:var(--line-strong)}.wcard.active{border-color:#fff;box-shadow:0 0 0 1px #fff}"
    ".wcard img{display:block;width:100%;aspect-ratio:16/9;object-fit:cover}.wmeta{padding:6px 8px;font-size:10.5px}.wmeta b{display:block;color:#fff;font-weight:500;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.wmeta span{color:var(--faint)}"
    ".kbsearch{width:100%;background:var(--surface-2);border:1px solid var(--line-strong);border-radius:10px;padding:10px 14px;color:#fff;font-family:var(--mono);font-size:13px;margin:14px 0 6px;outline:none}.kbsearch:focus{border-color:#fff}"
    ".dot{display:inline-block;width:9px;height:9px;border-radius:2px;background:#3a3a3a;margin:0 3px}"
    "table{width:100%;border-collapse:collapse;margin:10px 0 0;font-size:13px}th,td{text-align:left;padding:8px 10px;border-bottom:1px solid var(--line);vertical-align:top}"
    "th{color:var(--faint);font-weight:500;font-size:11.5px;text-transform:uppercase;letter-spacing:1.2px}td:first-child{white-space:nowrap;color:#fff}tbody tr:hover{background:var(--surface)}"
    ".two{display:grid;grid-template-columns:repeat(auto-fit,minmax(320px,1fr));gap:28px}.swatches{display:flex;flex-wrap:wrap;gap:10px;margin-top:12px}"
    ".sw{border:1px solid var(--line);border-radius:10px;overflow:hidden;width:132px;background:var(--surface)}.sw .chip{height:44px}.sw .meta{padding:7px 10px;font-size:11.5px;color:var(--dim)}.sw .meta b{display:block;color:#fff;font-weight:500}.sw .meta span{color:var(--faint)}"
    "pre{background:var(--surface);border:1px solid var(--line);border-radius:10px;padding:14px 16px;overflow-x:auto;font-size:12.5px;color:var(--text);margin:10px 0}pre .c{color:var(--faint)}pre .k{color:#fff}"
    ".tree{columns:2;column-gap:32px;font-size:12.5px;color:var(--dim);margin-top:12px}.tree div{break-inside:avoid;white-space:pre}.tree .d{color:#fff}"
    ".grid3{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:14px;margin-top:14px}.card{border:1px solid var(--line);background:var(--surface);border-radius:12px;padding:14px 16px}.card h4{margin:0 0 6px;font-size:13px;color:#fff;font-weight:600}.card p{margin:0;font-size:12.5px;color:var(--dim)}"
    ".card kbd{font-family:var(--mono);background:#fff;color:#0a0a0a;border-radius:5px;padding:1px 7px;font-size:11.5px;font-weight:600}"
    "footer{margin-top:72px;padding-top:20px;border-top:1px solid var(--line);color:var(--faint);font-size:12px}"
    "@media (max-width:760px){.wgrid{grid-template-columns:repeat(2,minmax(0,1fr))}.tree{columns:1}h1{font-size:26px}}";

static const char SIM_JS[] =
    "<script>\n"
    "(function(){\n"
    "  var stage=document.getElementById('sim-stage'),sgrid=document.getElementById('sim-grid'),sdim=document.getElementById('sim-dim'),"
    "dbox=document.getElementById('sim-drawer-box'),pbox=document.getElementById('sim-pal-box'),bgImg=document.getElementById('wall-stage-img'),"
    "liveBtn=document.getElementById('sim-live-btn'),cvs=document.getElementById('sim-live-canvas'),liveMode=true,liveScene=0;\n"
    "  if(cvs&&cvs.getContext){var ctx=cvs.getContext('2d'),startT=performance.now();\n"
    "    function drawOrrery(now){if(liveMode){var t=(now-startT)*0.00055,w=cvs.width,h=cvs.height,cx=w*0.5,cy=h*0.5;"
    "ctx.fillStyle='#070707';ctx.fillRect(0,0,w,h);"
    "if(liveScene===0){ctx.strokeStyle='rgba(255,255,255,0.04)';ctx.lineWidth=1;ctx.beginPath();ctx.moveTo(w*0.06,cy);ctx.lineTo(w*0.94,cy);ctx.moveTo(cx,h*0.06);ctx.lineTo(cx,h*0.94);ctx.stroke();"
    "var rc=h*0.165,orbs=[rc*1.48,rc*1.98,rc*2.52];orbs.forEach(function(r,i){ctx.strokeStyle='rgba(255,255,255,'+(0.15-i*0.035)+')';ctx.beginPath();ctx.arc(cx,cy,r,0,Math.PI*2);ctx.stroke();});"
    "var sg=ctx.createLinearGradient(cx,cy-rc,cx,cy+rc);sg.addColorStop(0,'#dcdcdc');sg.addColorStop(0.55,'#545454');sg.addColorStop(1,'#101010');ctx.fillStyle=sg;ctx.beginPath();ctx.arc(cx,cy,rc,0,Math.PI*2);ctx.fill();"
    "[[orbs[0],t*1.3,6.5,'#f4f4f4'],[orbs[1],-t*0.85+1.8,4.5,'#c4c4c4'],[orbs[2],t*1.9+0.5,3.2,'#9a9a9a']].forEach(function(m){ctx.fillStyle=m[3];ctx.beginPath();ctx.arc(cx+Math.cos(m[1])*m[0],cy+Math.sin(m[1])*m[0],m[2],0,Math.PI*2);ctx.fill();});"
    "}else{var rx=h*0.31,ry=h*0.31;ctx.strokeStyle='#f0f0f0';ctx.lineWidth=2;ctx.beginPath();"
    "for(var i=0;i<=300;i++){var s=(i/300)*Math.PI*2,px=cx+rx*Math.sin(3*s+t*2.4),py=cy+ry*Math.sin(2*s);if(i===0)ctx.moveTo(px,py);else ctx.lineTo(px,py);}ctx.stroke();}}"
    "requestAnimationFrame(drawOrrery);}requestAnimationFrame(drawOrrery);}\n"
    "  function syncDim(){if(sdim)sdim.classList.toggle('on',dbox.classList.contains('open')||pbox.classList.contains('open'));}\n"
    "  var grid=document.getElementById('wall-grid');if(grid)grid.addEventListener('click',function(ev){var btn=ev.target.closest('.wcard');if(!btn)return;"
    "liveMode=false;if(liveBtn)liveBtn.classList.remove('on');document.querySelectorAll('.wcard').forEach(function(b){b.classList.remove('active');});btn.classList.add('active');"
    "bgImg.src=btn.getAttribute('data-uri');bgImg.classList.add('show');document.getElementById('wall-stage-title').textContent='Active: '+btn.getAttribute('data-title');"
    "document.getElementById('wall-stage-desc').textContent=btn.getAttribute('data-desc');});\n"
    "  if(liveBtn)liveBtn.addEventListener('click',function(){if(liveMode)liveScene=(liveScene+1)%2;liveMode=true;liveBtn.classList.add('on');bgImg.classList.remove('show');});\n"
    "  var turboBtn=document.getElementById('sim-turbo-btn'),turboPill=document.getElementById('sim-turbo-pill');"
    "if(turboBtn)turboBtn.addEventListener('click',function(){var on=stage.classList.toggle('turbo');turboBtn.classList.toggle('on',on);if(turboPill)turboPill.style.display=on?'inline-block':'none';});\n"
    "  document.querySelectorAll('[data-gap]').forEach(function(btn){btn.addEventListener('click',function(){document.querySelectorAll('[data-gap]').forEach(function(b){b.classList.remove('on');});btn.classList.add('on');stage.classList.remove('airy','compact','zero');var g=btn.getAttribute('data-gap');if(g!=='default')stage.classList.add(g);});});\n"
    "  document.querySelectorAll('[data-mot]').forEach(function(btn){btn.addEventListener('click',function(){document.querySelectorAll('[data-mot]').forEach(function(b){b.classList.remove('on');});btn.classList.add('on');stage.classList.remove('snappy','cinematic','minimal');var m=btn.getAttribute('data-mot');if(m!=='fluid')stage.classList.add(m);});});\n"
    "  var zenBtn=document.getElementById('sim-zen');if(zenBtn)zenBtn.addEventListener('click',function(){zenBtn.classList.toggle('on');stage.classList.toggle('zen');});\n"
    "  var drwBtn=document.getElementById('sim-drawer');if(drwBtn)drwBtn.addEventListener('click',function(){drwBtn.classList.toggle('on');dbox.classList.toggle('open');syncDim();});\n"
    "  var palBtn=document.getElementById('sim-pal-btn'),palIn=document.getElementById('sim-pal-in'),palMath=document.getElementById('sim-pal-math');"
    "if(palBtn)palBtn.addEventListener('click',function(){palBtn.classList.toggle('on');pbox.classList.toggle('open');syncDim();if(pbox.classList.contains('open'))palIn.focus();});\n"
    "  if(palIn)palIn.addEventListener('input',function(){var raw=palIn.value.trim(),expr=raw.charAt(0)==='='?raw.slice(1).trim():raw;"
    "if(expr&&/^[0-9+\\-*/().%\\s]+$/.test(expr)){try{var v=Function('\"use strict\";return ('+expr+')')();if(typeof v==='number'&&isFinite(v)){palMath.textContent='= '+v.toLocaleString()+'  (live C11 calculator)';return;}}catch(_e){}}"
    "palMath.textContent='Tip: type \"= 3840 * 2160\" or click an action below';});\n"
    "  var input=document.getElementById('kb-filter'),box=document.getElementById('kb-container');"
    "if(input&&box){input.addEventListener('input',function(){var q=input.value.trim().toLowerCase();box.querySelectorAll('table').forEach(function(tbl){var shown=0;"
    "tbl.querySelectorAll('tbody tr').forEach(function(tr){var ok=!q||tr.textContent.toLowerCase().indexOf(q)!==-1;tr.style.display=ok?'':'none';if(ok)shown++;});"
    "var h3=tbl.previousElementSibling;tbl.style.display=shown?'':'none';if(h3&&h3.tagName==='H3')h3.style.display=shown?'':'none';});});}\n"
    "})();\n</script>";

static char *build_page(const char *repo) {
    char p_binds[PATH_MAX], p_wb[PATH_MAX], p_rofi[PATH_MAX], p_kitty[PATH_MAX];
    char p_toml[PATH_MAX], p_wbcss[PATH_MAX], p_pkgs[PATH_MAX], p_aur[PATH_MAX];
    char p_svg[PATH_MAX], p_lua[PATH_MAX];

    mono_path_join(p_binds, sizeof(p_binds), repo, ".config/hypr/modules/binds.lua");
    mono_path_join(p_wb, sizeof(p_wb), repo, ".config/waybar/config.jsonc");
    mono_path_join(p_rofi, sizeof(p_rofi), repo, ".config/rofi/theme.rasi");
    mono_path_join(p_kitty, sizeof(p_kitty), repo, ".config/kitty/theme.conf");
    mono_path_join(p_toml, sizeof(p_toml), repo, ".config/starship.toml");
    mono_path_join(p_wbcss, sizeof(p_wbcss), repo, ".config/waybar/style.css");
    mono_path_join(p_pkgs, sizeof(p_pkgs), repo, "packages.txt");
    mono_path_join(p_aur, sizeof(p_aur), repo, "aur-packages.txt");
    mono_path_join(p_svg, sizeof(p_svg), repo, "docs/preview.svg");
    mono_path_join(p_lua, sizeof(p_lua), repo, ".config/hypr/modules/user.lua");

    char *binds_txt = mono_read_file(p_binds);
    KbDoc raw_kb = kb_parse(binds_txt, repo);
    free(binds_txt);
    KbDoc doc_kb = kb_documented_rows(&raw_kb);
    char *main_label = kb_mod_label(raw_kb.main_mod);

    char *wb_txt = mono_read_file(p_wb);
    JsonVal *waybar_cfg = mono_parse_jsonc(wb_txt, NULL);
    free(wb_txt);

    char *rofi_txt = mono_read_file(p_rofi);
    MonoKVList rofi = mono_parse_rofi_palette(rofi_txt);
    free(rofi_txt);

    char *kitty_txt = mono_read_file(p_kitty);
    MonoKVList kitty = mono_parse_kitty_palette(kitty_txt);
    free(kitty_txt);

    char *toml_txt = mono_read_file(p_toml);
    TomlDoc toml = mono_parse_and_validate_toml(toml_txt, NULL);
    free(toml_txt);
    const MonoKVList *starship = toml_get_section(&toml, "palettes.mono");

    char *wbcss_txt = mono_read_file(p_wbcss);
    MonoStrList wb_colours = mono_parse_waybar_colours(wbcss_txt);
    free(wbcss_txt);

    char *pkgs_txt = mono_read_file(p_pkgs);
    MonoPkgList pkgs = mono_parse_packages_txt(pkgs_txt);
    free(pkgs_txt);

    char *aur_txt = mono_read_file(p_aur);
    MonoKVList aur = mono_parse_aur_packages_txt(aur_txt);
    free(aur_txt);

    char *tree_html = build_tree_html(repo);

    char *svg_raw = mono_read_file(p_svg);
    char *svg_trimmed = svg_raw;
    if (mono_starts_with(svg_raw, "<?xml")) {
        char *end_pi = strstr(svg_raw, "?>");
        if (end_pi) svg_trimmed = mono_trim_dup(end_pi + 2);
    }
    if (svg_trimmed == svg_raw) svg_trimmed = mono_strdup(svg_raw);
    free(svg_raw);
    char *desktop_svg = mono_replace_all_dup(svg_trimmed, "width=\"1600\" height=\"900\"", "preserveAspectRatio=\"xMidYMid meet\"");
    free(svg_trimmed);

    char *hero_wall_uri = NULL, *wall_cards_html = NULL;
    build_wallpaper_cards(repo, &hero_wall_uri, &wall_cards_html);

    size_t total_binds = 0;
    for (size_t i = 0; i < doc_kb.len; ++i) total_binds += doc_kb.secs[i].rows.len;
    size_t total_pkgs = 0;
    for (size_t i = 0; i < pkgs.len; ++i) total_pkgs += pkgs.groups[i].entries.len;

    MonoStr bind_tables; mstr_init(&bind_tables);
    for (size_t i = 0; i < doc_kb.len; ++i) {
        char *et = mono_html_escape(doc_kb.secs[i].title);
        mstr_printf(&bind_tables, "<h3>%s</h3><table><thead><tr><th>Keys</th><th>Action</th></tr></thead><tbody>", et);
        free(et);
        for (size_t j = 0; j < doc_kb.secs[i].rows.len; ++j) {
            char *ek = mono_html_escape(doc_kb.secs[i].rows.items[j].key);
            char *ed = mono_html_escape(doc_kb.secs[i].rows.items[j].val);
            mstr_printf(&bind_tables, "<tr><td><kbd>%s</kbd></td><td>%s</td></tr>", ek, ed);
            free(ek); free(ed);
        }
        mstr_puts(&bind_tables, "</tbody></table>");
    }

    MonoStr rofi_sw; mstr_init(&rofi_sw);
    for (size_t i = 0; i < rofi.len; ++i) append_swatch(&rofi_sw, rofi.items[i].key, rofi.items[i].val);

    MonoStr kitty_sw; mstr_init(&kitty_sw);
    static const char *k_main[3] = {"background", "foreground", "cursor"};
    for (int i = 0; i < 3; ++i) {
        const char *v = mkv_get(&kitty, k_main[i], NULL);
        if (v) append_swatch(&kitty_sw, k_main[i], v);
    }
    MonoStr kitty_ramp; mstr_init(&kitty_ramp);
    for (int i = 0; i < 16; ++i) {
        char ck[16]; snprintf(ck, sizeof(ck), "color%d", i);
        const char *v = mkv_get(&kitty, ck, NULL);
        if (v) append_swatch(&kitty_ramp, ck, v);
    }

    MonoStr star_sw; mstr_init(&star_sw);
    if (starship) {
        for (size_t i = 0; i < starship->len; ++i) append_swatch(&star_sw, starship->items[i].key, starship->items[i].val);
    }

    MonoStr wb_sw; mstr_init(&wb_sw);
    for (size_t i = 0; i < wb_colours.len && i < 8; ++i) {
        char lbl[32]; snprintf(lbl, sizeof(lbl), "style.css #%zu", i + 1);
        append_swatch(&wb_sw, lbl, wb_colours.items[i]);
    }

    MonoStr pkg_html; mstr_init(&pkg_html);
    for (size_t i = 0; i < pkgs.len; ++i) {
        char *et = mono_html_escape(pkgs.groups[i].title);
        mstr_printf(&pkg_html, "<h3>%s</h3><table><tbody>", et);
        free(et);
        for (size_t j = 0; j < pkgs.groups[i].entries.len; ++j) {
            char *en = mono_html_escape(pkgs.groups[i].entries.items[j].key);
            char *ec = mono_html_escape(pkgs.groups[i].entries.items[j].val);
            mstr_printf(&pkg_html, "<tr><td>%s</td><td>%s</td></tr>", en, ec);
            free(en); free(ec);
        }
        mstr_puts(&pkg_html, "</tbody></table>");
    }

    MonoStr aur_rows; mstr_init(&aur_rows);
    for (size_t i = 0; i < aur.len; ++i) {
        char *en = mono_html_escape(aur.items[i].key);
        char *ec = mono_html_escape(aur.items[i].val);
        mstr_printf(&aur_rows, "<tr><td>%s</td><td>%s</td></tr>", en, ec);
        free(en); free(ec);
    }

    char *user_lua = mono_read_file(p_lua);
    char *gaps_in = mono_lua_value(user_lua, "gaps_in", "5");
    char *gaps_out = mono_lua_value(user_lua, "gaps_out", "10");
    char *rounding = mono_lua_value(user_lua, "rounding", "10");
    char *browser = mono_lua_value(user_lua, "browser", "firefox");
    char *terminal = mono_lua_value(user_lua, "terminal", "kitty");
    char *files_app = mono_lua_value(user_lua, "file_manager", "thunar");
    free(user_lua);

    char *e_main = mono_html_escape(main_label);
    char *e_term = mono_html_escape(terminal);
    char *e_files = mono_html_escape(files_app);
    char *e_browser = mono_html_escape(browser);
    char *bar_html = build_bar_html(waybar_cfg);
    char date_buf[32];
    today_iso(date_buf, sizeof(date_buf));

    MonoStr o;
    mstr_init(&o);
    mstr_printf(&o,
        "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        "<title>hyprland-monochrome — preview</title>\n"
        "<style>%s</style>\n</head>\n<body>\n<div class=\"wrap\">\n\n"
        "  <h1>hyprland-monochrome</h1>\n"
        "  <p class=\"lead\">A universal monochrome Hyprland desktop engineered in Pure C11, Lua 5.4, and POSIX shell "
        "that runs on any PC (AMD, Intel, NVIDIA, hybrid Optimus laptops, VMs &amp; Arch/Debian/Fedora/openSUSE), "
        "opens any Linux or Windows app, and ships an <b>OpenCode</b> browser theme across Firefox &amp; Chromium.</p>\n"
        "  <div class=\"badges\">\n"
        "    <span class=\"badge\"><b>Hyprland</b> 0.56 · Lua config · Pure C11 tools</span>\n"
        "    <span class=\"badge\"><b>Any PC</b> AMD · Intel · NVIDIA · Optimus · VM</span>\n"
        "    <span class=\"badge\"><b>Any App</b> Wayland · X11 · Flatpak · AppImage · Wine</span>\n"
        "    <span class=\"badge\"><b>OpenCode</b> Browser Theme + Startpage</span>\n"
        "    <span class=\"badge\"><b>%zu</b> keybinds</span>\n"
        "    <span class=\"badge\"><b>%zu</b> official packages</span>\n"
        "    <span class=\"badge\"><b>%s</b> · <b>%s</b> · Zsh + Starship</span>\n"
        "  </div>\n\n"
        "  <h2><span class=\"num\">01</span>Desktop layout &amp; Wallpaper Studio</h2>\n"
        "  <p class=\"lead\">Tiled windows with %spx inner and %spx outer gaps, %spx rounding, "
        "a focused-window border that goes white-to-grey, bespoke monochrome vector cursors, and a frosted top bar.</p>\n"
        "  <div class=\"figure\">%s</div>\n"
        "  <p class=\"caption\">Vector layout drawn to the real gap, rounding and bar geometry from <code>modules/appearance.lua</code> (<code>docs/preview.svg</code>).</p>\n\n"
        "  <h3>60 FPS Velocity &amp; Kinetic Simulator, Squircle Liquid Glass, GLSL Shaders &amp; 20-Wallpaper Studio</h3>\n"
        "  <div class=\"simbar\" id=\"sim-controls\">\n"
        "    <button type=\"button\" class=\"simbtn on\" id=\"sim-live-btn\">✦ Live: Orrery (60fps)</button>\n"
        "    <button type=\"button\" class=\"simbtn\" id=\"sim-turbo-btn\">⚡ Turbo (Super+F12)</button>\n"
        "    <button type=\"button\" class=\"simbtn on\" data-gap=\"default\">Gaps: Default</button>\n"
        "    <button type=\"button\" class=\"simbtn\" data-gap=\"airy\">Airy</button>\n"
        "    <button type=\"button\" class=\"simbtn\" data-gap=\"compact\">Compact</button>\n"
        "    <button type=\"button\" class=\"simbtn\" data-gap=\"zero\">Zero</button>\n"
        "    <button type=\"button\" class=\"simbtn on\" data-mot=\"fluid\">Motion: Fluid</button>\n"
        "    <button type=\"button\" class=\"simbtn\" data-mot=\"snappy\">Snappy</button>\n"
        "    <button type=\"button\" class=\"simbtn\" data-mot=\"cinematic\">Cinematic</button>\n"
        "    <button type=\"button\" class=\"simbtn\" id=\"sim-zen\">Zen (Super+Z)</button>\n"
        "    <button type=\"button\" class=\"simbtn\" id=\"sim-drawer\">btop++ Drawer</button>\n"
        "    <button type=\"button\" class=\"simbtn\" id=\"sim-pal-btn\">Palette (Super+;)</button>\n"
        "  </div>\n"
        "  <div class=\"simstage\" id=\"sim-stage\">\n"
        "    <canvas class=\"simcanvas\" id=\"sim-live-canvas\" width=\"960\" height=\"540\"></canvas>\n"
        "    <img class=\"simbg\" id=\"wall-stage-img\" src=\"%s\" alt=\"Selected monochrome wallpaper\">\n"
        "    <div class=\"simcrt\"></div><div class=\"simdim\" id=\"sim-dim\"></div>\n"
        "    <div class=\"simtop\"><div><b style=\"color:#fff;margin-right:7px\">◆</b><span class=\"simws active\">1</span> <span class=\"simws\">2</span> <span class=\"simws\">3</span></div>"
        "<div style=\"color:var(--dim)\">♫ Midnight City — M83</div><div><span id=\"sim-turbo-pill\" style=\"display:none;background:#fff;color:#080808;padding:1px 7px;border-radius:6px;font-weight:700;margin-right:7px\">⚡ TURBO</span><b style=\"color:#fff\">21:42</b></div></div>\n"
        "    <div class=\"simgrid\" id=\"sim-grid\">\n"
        "      <div class=\"simwin focus\"><div class=\"simtitle\"><span>NORMAL · ~/.config/hypr/modules/animations.lua</span><span>silkSpring · 29 leaves</span></div>"
        "<div>hl.curve(\"silkSpring\", { type = \"spring\", mass = 1.0, stiffness = 175, damping = 22 })</div>"
        "<div style=\"margin-top:auto;color:var(--dim)\">GROUP Super+G tab stack · squircle=4.0 · vfr+vrr</div></div>\n"
        "      <div class=\"simwin\"><div class=\"simtitle\"><span>kitty --single-instance · doctor.sh</span><span>Pure C11 + 0-fork</span></div>"
        "<div>✓ Lua config · 12/12 modules · squircle=4.0</div>"
        "<div>✓ Native C11 · 9 tools · 0 Python · 4 cursor themes</div>"
        "<div>✓ Wallpapers · 20/20 live animated GIF89a loops</div>"
        "<div style=\"margin-top:auto;color:var(--dim)\" id=\"wall-stage-title\">Active: mono-live-01 — Bauhaus Celestial Orrery (60fps Live)</div>"
        "<div style=\"color:var(--faint);font-size:10.5px\" id=\"wall-stage-desc\">Click any wallpaper card below to preview its live loop</div></div>\n"
        "    </div>\n"
        "    <div class=\"simdrawer\" id=\"sim-drawer-box\"><div class=\"simtitle\"><b style=\"color:#fff\">▣ special:btop — Floating Telemetry Drawer</b><span>Esc to hide</span></div>"
        "<div>CPU  ██████████░░░░░░░░░░  34%%   48°C</div><div>MEM  ████████░░░░░░░░░░░░  4.1 / 16.0 GiB</div></div>\n"
        "    <div class=\"simpal\" id=\"sim-pal-box\"><input id=\"sim-pal-in\" type=\"text\" value=\"= 1920 * 1080\">"
        "<div id=\"sim-pal-math\" style=\"color:#fff;font-size:11.5px;padding:0 4px\">= 2,073,600 (live C11 calculator preview)</div></div>\n"
        "  </div>\n"
        "  <div class=\"wgrid\" id=\"wall-grid\">%s</div>\n\n"
        "  <h2><span class=\"num\">02</span>The bar</h2>\n"
        "  %s\n\n"
        "  <h2><span class=\"num\">03</span>Keybinds</h2>\n"
        "  <p class=\"lead\">The main modifier is <kbd>%s</kbd>. Filter all %zu binds live below:</p>\n"
        "  <input id=\"kb-filter\" class=\"kbsearch\" type=\"search\" placeholder=\"Filter %zu keybinds (try: palette, zen, cursor, keyboard, record)…\">\n"
        "  <div id=\"kb-container\">%s</div>\n\n"
        "  <h2><span class=\"num\">04</span>Palette</h2>\n"
        "  <h3>Menus — rofi/theme.rasi</h3><div class=\"swatches\">%s</div>\n"
        "  <div class=\"two\"><div><h3>Terminal — kitty/theme.conf</h3><div class=\"swatches\">%s</div><div class=\"swatches\">%s</div></div>"
        "<div><h3>Prompt — starship.toml</h3><div class=\"swatches\">%s</div><h3>Bar — waybar/style.css</h3><div class=\"swatches\">%s</div></div></div>\n\n"
        "  <h2><span class=\"num\">05</span>What gets installed</h2>\n"
        "  %s\n"
        "  <h3>Optional AUR extras</h3><table><tbody>%s</tbody></table>\n\n"
        "  <h2><span class=\"num\">06</span>Project layout</h2>\n"
        "  <p class=\"lead\">Native Pure C11 tools build the wallpapers, cursors, docs, HTTP server, and expression engine; Lua 5.4 configures Hyprland; POSIX/Bash scripts drive the menus and bar helpers.</p>\n"
        "  <div class=\"tree\">%s</div>\n\n"
        "  <h2><span class=\"num\">07</span>Try it</h2>\n"
        "  <div class=\"grid3\">\n"
        "    <div class=\"card\"><h4>Install</h4><p><code>./install.sh</code><br>Packages, backups, configs, Pure C11 helpers, cursors, and 20 live wallpapers.</p></div>\n"
        "    <div class=\"card\"><h4>Make it yours</h4><p><code>modules/user.lua</code><br>Right now: browser <b>%s</b>, terminal <b>%s</b>.</p></div>\n"
        "    <div class=\"card\"><h4>Command Palette</h4><p><kbd>%s + Shift + P</kbd> or <kbd>%s + ;</kbd><br>Jump to windows, switch layouts/cursors, or type <code>= 1920*1080</code> for C11 math.</p></div>\n"
        "  </div>\n\n"
        "  <footer>Generated from this repository's config on %s by <code>tools/build-preview.c</code> · %zu binds, %zu packages, MIT licensed.</footer>\n"
        "</div>\n%s\n</body>\n</html>\n",
        PREVIEW_CSS, total_binds, total_pkgs, e_term, e_files,
        gaps_in, gaps_out, rounding, desktop_svg,
        hero_wall_uri, wall_cards_html, bar_html,
        e_main, total_binds, total_binds, bind_tables.s,
        rofi_sw.s, kitty_sw.s, kitty_ramp.s, star_sw.s, wb_sw.s,
        pkg_html.s, aur_rows.s, tree_html,
        e_browser, e_term, e_main, e_main,
        date_buf, total_binds, total_pkgs, SIM_JS);

    free(bar_html);
    free(e_browser); free(e_files); free(e_term); free(e_main);
    free(files_app); free(terminal); free(browser); free(rounding); free(gaps_out); free(gaps_in);
    mstr_free(&aur_rows); mstr_free(&pkg_html); mstr_free(&wb_sw); mstr_free(&star_sw);
    mstr_free(&kitty_ramp); mstr_free(&kitty_sw); mstr_free(&rofi_sw); mstr_free(&bind_tables);
    free(wall_cards_html); free(hero_wall_uri); free(desktop_svg); free(tree_html);
    mkv_free(&aur); mpkg_free(&pkgs); mslist_free(&wb_colours); toml_free(&toml);
    mkv_free(&kitty); mkv_free(&rofi); json_free(waybar_cfg);
    free(main_label); kb_doc_free(&doc_kb); kb_doc_free(&raw_kb);

    return o.s;
}

static char *strip_date_stamp(const char *page) {
    MonoStr out;
    mstr_init(&out);
    const char *p = page;
    while (*p) {
        if (strncmp(p, "on ", 3) == 0 &&
            isdigit((unsigned char)p[3]) && isdigit((unsigned char)p[4]) &&
            isdigit((unsigned char)p[5]) && isdigit((unsigned char)p[6]) &&
            p[7] == '-' && isdigit((unsigned char)p[8]) && isdigit((unsigned char)p[9]) &&
            p[10] == '-' && isdigit((unsigned char)p[11]) && isdigit((unsigned char)p[12]) &&
            strncmp(p + 13, " by", 3) == 0) {
            mstr_puts(&out, "on <date> by");
            p += 16;
            continue;
        }
        mstr_putc(&out, *p++);
    }
    return out.s;
}

int main(int argc, char *argv[]) {
    bool check_only = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--check") == 0) check_only = true;
    }

    char repo[PATH_MAX], out[PATH_MAX];
    mono_find_repo_root(argv[0], repo, sizeof(repo));
    mono_path_join(out, sizeof(out), repo, "docs/preview.html");

    char *page = build_page(repo);

    if (check_only) {
        int rc = 1;
        if (mono_file_exists(out)) {
            char *cur = mono_read_file(out);
            char *s_cur = strip_date_stamp(cur);
            char *s_new = strip_date_stamp(page);
            if (strcmp(s_cur, s_new) == 0) {
                printf("docs/preview.html is up to date.\n");
                rc = 0;
            }
            free(s_cur);
            free(s_new);
            free(cur);
        }
        if (rc != 0) {
            fprintf(stderr, "docs/preview.html is OUT OF DATE — run make docs (./bin/build-preview)\n");
        }
        free(page);
        return rc;
    }

    mono_write_file(out, page);
    printf("wrote docs/preview.html (%zu KB)\n", strlen(page) / 1024);
    free(page);
    return 0;
}
