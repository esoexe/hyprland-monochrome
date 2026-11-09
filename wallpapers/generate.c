// ═══════════════════════════════════════════════════════════════════════════
//  wallpapers/generate.c — Procedural Monochrome Wallpaper Studio (Pure C11)
//
//  Renders all 14 pure-greyscale procedural wallpapers at any resolution
//  from 1080p up to 16K (15360x8640), multi-threaded across CPU cores with
//  zero external libraries.
//
//    ./bin/wallpaper-gen                              # 1920x1080, all 14
//    ./bin/wallpaper-gen 3840 2160                    # 4K UHD
//    ./bin/wallpaper-gen 15360 8640 --only 11 --suffix -16k --quality 90
// ═══════════════════════════════════════════════════════════════════════════

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "generate.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

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

int main(int argc, char *argv[]) {
    int width = 1920;
    int height = 1080;
    const char *only = "";
    const char *suffix = "";
    int quality = 92;
    char out_dir[PATH_MAX];
    resolve_default_outdir(argv[0], out_dir, sizeof(out_dir));

    int pos_dims[4];
    int npos = 0;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "--only") == 0 && i + 1 < argc) {
            only = argv[++i];
        } else if (strcmp(arg, "--suffix") == 0 && i + 1 < argc) {
            suffix = argv[++i];
        } else if (strcmp(arg, "--quality") == 0 && i + 1 < argc) {
            quality = atoi(argv[++i]);
        } else if (strcmp(arg, "--out-dir") == 0 && i + 1 < argc) {
            snprintf(out_dir, sizeof(out_dir), "%s", argv[++i]);
        } else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            printf("Usage: %s [width] [height] [--only N|stem] [--suffix S] [--quality Q] [--out-dir DIR]\n", argv[0]);
            return 0;
        } else if (arg[0] != '\0' && arg[0] != '-') {
            if (npos < 4) pos_dims[npos++] = atoi(arg);
        } else {
            fprintf(stderr, "Unknown option: %s\n", arg);
            return 1;
        }
    }
    if (npos >= 1) width = pos_dims[0];
    if (npos >= 2) height = pos_dims[1];
    if (width <= 0 || height <= 0) {
        fprintf(stderr, "Invalid dimensions: %dx%d\n", width, height);
        return 1;
    }

    const MonoDesignSpec *selected[14];
    size_t nsel = 0;
    if (only[0] != '\0') {
        const MonoDesignSpec *spec = mono_find_design(only);
        if (!spec) {
            fprintf(stderr, "No wallpaper design matched --only '%s'\n", only);
            return 1;
        }
        selected[nsel++] = spec;
    } else {
        for (size_t i = 0; i < 14; ++i) selected[nsel++] = &MONO_DESIGNS[i];
    }

    mkdir_p_local(out_dir);
    for (size_t i = 0; i < nsel; ++i) {
        const MonoDesignSpec *spec = selected[i];
        char fname[256], out[PATH_MAX];
        snprintf(fname, sizeof(fname), "%s%s.jpg", spec->stem, suffix);
        int n = snprintf(out, sizeof(out), "%s/%s", out_dir, fname);
        if (n < 0 || (size_t)n >= sizeof(out)) return 1;

        printf("Rendering %s (%dx%d — %s)...\n", fname, width, height, spec->title);
        GrayImage img = spec->fn(width, height, 0.0);
        if (!mono_save_jpeg(&img, out, quality)) {
            gray_free(&img);
            return 1;
        }
        gray_free(&img);
        struct stat st;
        unsigned long kb = (stat(out, &st) == 0) ? (unsigned long)(st.st_size / 1024) : 0;
        printf("  wrote %s (%lu KB)\n", fname, kb);
    }
    return 0;
}
