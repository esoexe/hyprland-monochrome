// ===========================================================================
//  .config/hypr/src/mono-cursor-gen.c
//  Zero-dependency Pure C11 Bespoke Monochrome Cursor Compiler
//
//  Generates 4 architectural monochrome cursor suites in BOTH formats:
//    1. Native Hyprland `hyprcursor` (`manifest.hl` + `hyprcursors/*.hlc` ZIP
//       containers holding `meta.hl` + scalable 1:1 vector `.svg` frames)
//    2. Universal X11 / GTK / Qt / XWayland `Xcursor(3)` binary files
//       (`index.theme` + `cursors/` multi-DPI 24/32/48 premultiplied ARGB32
//       with 4x4 supersampled anti-aliasing + full freedesktop symlinks)
//
//  Themes produced:
//    - Monochrome-Obsidian   (Dark #0a0a0a body, luminous #f5f5f5 crisp edge)
//    - Monochrome-Ivory      (Pure #f7f7f7 body, obsidian #0a0a0a crisp edge)
//    - Monochrome-Precision  (Hollow CAD drafting reticle + pinpoint tip)
//    - Monochrome-Dot        (Minimalist tactile halo + core dot pointer)
//
//  Usage:
//    mono-cursor-gen [--out-dir <dir>] [--theme <name>] [--list]
// ===========================================================================

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define MONO_PI 3.14159265358979323846

typedef enum {
    STYLE_OBSIDIAN = 0,
    STYLE_IVORY,
    STYLE_PRECISION,
    STYLE_DOT
} CursorStyle;

typedef struct {
    const char *id;
    const char *name;
    const char *comment;
    CursorStyle style;
} ThemeSpec;

static const ThemeSpec THEMES[4] = {
    {"obsidian",  "Monochrome-Obsidian",  "Architectural obsidian pointer with high-contrast silver edge", STYLE_OBSIDIAN},
    {"ivory",     "Monochrome-Ivory",     "Luminous ivory pointer with deep obsidian perimeter stroke",    STYLE_IVORY},
    {"precision", "Monochrome-Precision", "Hollow Swiss drafting reticle cursor for sub-pixel accuracy",   STYLE_PRECISION},
    {"dot",       "Monochrome-Dot",       "Minimalist concentric halo and tactile dot cursor",             STYLE_DOT},
};

typedef enum {
    SHAPE_LEFT_PTR = 0,
    SHAPE_HAND,
    SHAPE_XTERM,
    SHAPE_CROSSHAIR,
    SHAPE_WATCH,
    SHAPE_FLEUR,
    SHAPE_RESIZE_H,
    SHAPE_RESIZE_V,
    SHAPE_RESIZE_NWSE,
    SHAPE_RESIZE_NESW,
    SHAPE_GRAB,
    SHAPE_GRABBING,
    SHAPE_NOT_ALLOWED
} ShapeKind;

typedef struct {
    const char *name;
    ShapeKind kind;
    double hot_x;
    double hot_y;
    int frames;
    uint32_t delay_ms;
    const char *aliases[10];
} ShapeSpec;

static const ShapeSpec SHAPES[13] = {
    {"left_ptr", SHAPE_LEFT_PTR, 0.16, 0.12, 1, 0,
     {"default", "arrow", "top_left_arrow", "left_ptr_arrow", NULL}},
    {"hand2", SHAPE_HAND, 0.36, 0.16, 1, 0,
     {"pointer", "pointing_hand", "hand1", "hand",
      "e29285e634086352946a0e7090d73106", "9d800788f1b08800ae810202380a0822", NULL}},
    {"xterm", SHAPE_XTERM, 0.50, 0.50, 1, 0,
     {"text", "ibeam", NULL}},
    {"crosshair", SHAPE_CROSSHAIR, 0.50, 0.50, 1, 0,
     {"cross", "tcross", "cell", "plus", NULL}},
    {"watch", SHAPE_WATCH, 0.50, 0.50, 8, 65,
     {"wait", "progress", "left_ptr_watch",
      "00000000000000020006000e7e9ffc3f", "08e8e1c95fe2fc01f976f1e063a24ccd", NULL}},
    {"fleur", SHAPE_FLEUR, 0.50, 0.50, 1, 0,
     {"move", "all-scroll", "size_all", NULL}},
    {"sb_h_double_arrow", SHAPE_RESIZE_H, 0.50, 0.50, 1, 0,
     {"ew-resize", "col-resize", "size_hor", "h_double_arrow", "w-resize", "e-resize", "left_side", "right_side", NULL}},
    {"sb_v_double_arrow", SHAPE_RESIZE_V, 0.50, 0.50, 1, 0,
     {"ns-resize", "row-resize", "size_ver", "v_double_arrow", "n-resize", "s-resize", "top_side", "bottom_side", NULL}},
    {"bd_double_arrow", SHAPE_RESIZE_NWSE, 0.50, 0.50, 1, 0,
     {"nwse-resize", "size_fdiag", "nw-resize", "se-resize", "top_left_corner", "bottom_right_corner", NULL}},
    {"fd_double_arrow", SHAPE_RESIZE_NESW, 0.50, 0.50, 1, 0,
     {"nesw-resize", "size_bdiag", "ne-resize", "sw-resize", "top_right_corner", "bottom_left_corner", NULL}},
    {"grab", SHAPE_GRAB, 0.50, 0.50, 1, 0,
     {"openhand", NULL}},
    {"grabbing", SHAPE_GRABBING, 0.50, 0.50, 1, 0,
     {"closedhand", "dnd-move", "dnd-copy", "dnd-none", NULL}},
    {"not_allowed", SHAPE_NOT_ALLOWED, 0.50, 0.50, 1, 0,
     {"not-allowed", "crossed_circle", "forbidden", "no-drop", "circle", NULL}}
};

typedef struct { double x, y; } Vec2;

static double d_min(double a, double b) { return a < b ? a : b; }
static double d_max(double a, double b) { return a > b ? a : b; }
static double d_clamp(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
static int i_clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static double dot2(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
static double len2(Vec2 v) { return sqrt(v.x * v.x + v.y * v.y); }

static double sd_segment(Vec2 p, Vec2 a, Vec2 b) {
    Vec2 pa = {p.x - a.x, p.y - a.y};
    Vec2 ba = {b.x - a.x, b.y - a.y};
    double h = d_clamp(dot2(pa, ba) / d_max(1e-9, dot2(ba, ba)), 0.0, 1.0);
    Vec2 d = {pa.x - ba.x * h, pa.y - ba.y * h};
    return len2(d);
}

static double sd_circle(Vec2 p, Vec2 c, double r) {
    Vec2 d = {p.x - c.x, p.y - c.y};
    return len2(d) - r;
}

static double sd_box(Vec2 p, Vec2 c, Vec2 half_ext) {
    double dx = fabs(p.x - c.x) - half_ext.x;
    double dy = fabs(p.y - c.y) - half_ext.y;
    Vec2 outside = {d_max(dx, 0.0), d_max(dy, 0.0)};
    double out = len2(outside);
    double in = d_min(d_max(dx, dy), 0.0);
    return out + in;
}

static double sd_polygon(Vec2 p, const Vec2 *v, size_t n) {
    Vec2 d0 = {p.x - v[0].x, p.y - v[0].y};
    double d = dot2(d0, d0);
    double s = 1.0;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        Vec2 e = {v[j].x - v[i].x, v[j].y - v[i].y};
        Vec2 w = {p.x - v[i].x, p.y - v[i].y};
        double h = d_clamp(dot2(w, e) / d_max(1e-9, dot2(e, e)), 0.0, 1.0);
        Vec2 b = {w.x - e.x * h, w.y - e.y * h};
        d = d_min(d, dot2(b, b));
        bool c1 = p.y >= v[i].y;
        bool c2 = p.y < v[j].y;
        bool c3 = (e.x * w.y > e.y * w.x);
        if ((c1 && c2 && c3) || (!c1 && !c2 && !c3)) s *= -1.0;
    }
    return s * sqrt(d);
}

static Vec2 rotate_around(Vec2 p, Vec2 c, double rad) {
    double cs = cos(rad), sn = sin(rad);
    double dx = p.x - c.x, dy = p.y - c.y;
    Vec2 r = {c.x + dx * cs - dy * sn, c.y + dx * sn + dy * cs};
    return r;
}

static double shape_sdf(ShapeKind kind, CursorStyle style, Vec2 p, int frame, int total_frames) {
    Vec2 center = {0.50, 0.50};
    if (style == STYLE_DOT && kind == SHAPE_LEFT_PTR) {
        double core = sd_circle(p, center, 0.115);
        double ring = fabs(sd_circle(p, center, 0.255)) - 0.024;
        return d_min(core, ring);
    }
    if (style == STYLE_DOT && kind == SHAPE_HAND) {
        double core = sd_circle(p, center, 0.155);
        double ring = fabs(sd_circle(p, center, 0.315)) - 0.030;
        return d_min(core, ring);
    }

    switch (kind) {
        case SHAPE_LEFT_PTR: {
            static const Vec2 arrow[7] = {
                {0.16, 0.12}, {0.16, 0.80}, {0.34, 0.63}, {0.46, 0.86},
                {0.55, 0.81}, {0.43, 0.58}, {0.66, 0.58}
            };
            double d = sd_polygon(p, arrow, 7);
            if (style == STYLE_PRECISION) {
                double shell = fabs(d + 0.018) - 0.022;
                Vec2 tip_pt = {0.16, 0.12};
                double tip = sd_circle(p, tip_pt, 0.032);
                return d_min(shell, tip);
            }
            return d;
        }
        case SHAPE_HAND: {
            static const Vec2 poly[9] = {
                {0.36, 0.14}, {0.44, 0.14}, {0.44, 0.42}, {0.72, 0.42}, {0.72, 0.78},
                {0.30, 0.78}, {0.22, 0.60}, {0.28, 0.52}, {0.36, 0.58}
            };
            double d = sd_polygon(p, poly, 9);
            return (style == STYLE_PRECISION) ? (fabs(d + 0.015) - 0.022) : d;
        }
        case SHAPE_XTERM: {
            double stem = sd_segment(p, (Vec2){0.50, 0.20}, (Vec2){0.50, 0.80}) - 0.028;
            double top_b = sd_segment(p, (Vec2){0.37, 0.20}, (Vec2){0.63, 0.20}) - 0.026;
            double bot_b = sd_segment(p, (Vec2){0.37, 0.80}, (Vec2){0.63, 0.80}) - 0.026;
            return d_min(stem, d_min(top_b, bot_b));
        }
        case SHAPE_CROSSHAIR: {
            double h1 = sd_segment(p, (Vec2){0.16, 0.50}, (Vec2){0.40, 0.50}) - 0.024;
            double h2 = sd_segment(p, (Vec2){0.60, 0.50}, (Vec2){0.84, 0.50}) - 0.024;
            double v1 = sd_segment(p, (Vec2){0.50, 0.16}, (Vec2){0.50, 0.40}) - 0.024;
            double v2 = sd_segment(p, (Vec2){0.50, 0.60}, (Vec2){0.50, 0.84}) - 0.024;
            double c = sd_circle(p, center, 0.032);
            return d_min(d_min(h1, h2), d_min(d_min(v1, v2), c));
        }
        case SHAPE_WATCH: {
            int tf = total_frames > 0 ? total_frames : 1;
            double ang = (2.0 * MONO_PI * frame) / tf;
            double ring = fabs(sd_circle(p, center, 0.27)) - 0.036;
            Vec2 sat = {0.50 + 0.27 * cos(ang - MONO_PI * 0.5), 0.50 + 0.27 * sin(ang - MONO_PI * 0.5)};
            double orb = sd_circle(p, sat, 0.075);
            double ctr = sd_circle(p, center, 0.045);
            return d_min(ring, d_min(orb, ctr));
        }
        case SHAPE_FLEUR: {
            double best = sd_segment(p, (Vec2){0.20, 0.50}, (Vec2){0.80, 0.50}) - 0.024;
            best = d_min(best, sd_segment(p, (Vec2){0.50, 0.20}, (Vec2){0.50, 0.80}) - 0.024);
            best = d_min(best, sd_segment(p, (Vec2){0.18, 0.50}, (Vec2){0.29, 0.40}) - 0.024);
            best = d_min(best, sd_segment(p, (Vec2){0.18, 0.50}, (Vec2){0.29, 0.60}) - 0.024);
            best = d_min(best, sd_segment(p, (Vec2){0.82, 0.50}, (Vec2){0.71, 0.40}) - 0.024);
            best = d_min(best, sd_segment(p, (Vec2){0.82, 0.50}, (Vec2){0.71, 0.60}) - 0.024);
            best = d_min(best, sd_segment(p, (Vec2){0.50, 0.18}, (Vec2){0.40, 0.29}) - 0.024);
            best = d_min(best, sd_segment(p, (Vec2){0.50, 0.18}, (Vec2){0.60, 0.29}) - 0.024);
            best = d_min(best, sd_segment(p, (Vec2){0.50, 0.82}, (Vec2){0.40, 0.71}) - 0.024);
            best = d_min(best, sd_segment(p, (Vec2){0.50, 0.82}, (Vec2){0.60, 0.71}) - 0.024);
            return best;
        }
        case SHAPE_RESIZE_H: {
            double best = sd_segment(p, (Vec2){0.20, 0.50}, (Vec2){0.80, 0.50}) - 0.026;
            best = d_min(best, sd_segment(p, (Vec2){0.50, 0.34}, (Vec2){0.50, 0.66}) - 0.022);
            best = d_min(best, sd_segment(p, (Vec2){0.18, 0.50}, (Vec2){0.31, 0.38}) - 0.026);
            best = d_min(best, sd_segment(p, (Vec2){0.18, 0.50}, (Vec2){0.31, 0.62}) - 0.026);
            best = d_min(best, sd_segment(p, (Vec2){0.82, 0.50}, (Vec2){0.69, 0.38}) - 0.026);
            best = d_min(best, sd_segment(p, (Vec2){0.82, 0.50}, (Vec2){0.69, 0.62}) - 0.026);
            return best;
        }
        case SHAPE_RESIZE_V:
            return shape_sdf(SHAPE_RESIZE_H, style, rotate_around(p, center, MONO_PI * 0.5), frame, total_frames);
        case SHAPE_RESIZE_NWSE:
            return shape_sdf(SHAPE_RESIZE_H, style, rotate_around(p, center, MONO_PI * 0.25), frame, total_frames);
        case SHAPE_RESIZE_NESW:
            return shape_sdf(SHAPE_RESIZE_H, style, rotate_around(p, center, -MONO_PI * 0.25), frame, total_frames);
        case SHAPE_GRAB: {
            double b = fabs(sd_box(p, (Vec2){0.50, 0.52}, (Vec2){0.20, 0.18})) - 0.028;
            double d = sd_circle(p, (Vec2){0.50, 0.52}, 0.045);
            return d_min(b, d);
        }
        case SHAPE_GRABBING:
            return sd_box(p, (Vec2){0.50, 0.52}, (Vec2){0.17, 0.15}) - 0.03;
        case SHAPE_NOT_ALLOWED: {
            double ring = fabs(sd_circle(p, center, 0.28)) - 0.036;
            double slash = sd_segment(p, (Vec2){0.31, 0.31}, (Vec2){0.69, 0.69}) - 0.034;
            return d_min(ring, slash);
        }
    }
    return 1.0;
}

static uint32_t *rasterize_shape(ShapeKind kind, CursorStyle style, int size, int frame, int total_frames) {
    uint32_t *pixels = (uint32_t *)calloc((size_t)(size * size), sizeof(uint32_t));
    if (!pixels) exit(1);

    double body_lum = 10.0, edge_lum = 246.0;
    if (style == STYLE_IVORY) { body_lum = 247.0; edge_lum = 10.0; }
    else if (style == STYLE_PRECISION) { body_lum = 245.0; edge_lum = 12.0; }
    else if (style == STYLE_DOT) { body_lum = 244.0; edge_lum = 14.0; }

    double edge_w = (style == STYLE_PRECISION || style == STYLE_DOT) ? 0.022 : 0.034;
    const int ss = 4;

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            double acc_a = 0.0, acc_c = 0.0;
            for (int sy = 0; sy < ss; ++sy) {
                for (int sx = 0; sx < ss; ++sx) {
                    double u = (x + (sx + 0.5) / ss) / size;
                    double v = (y + (sy + 0.5) / ss) / size;
                    double d = shape_sdf(kind, style, (Vec2){u, v}, frame, total_frames);
                    if (d <= 0.0) {
                        acc_a += 1.0;
                        acc_c += body_lum;
                    } else if (d <= edge_w) {
                        acc_a += 1.0;
                        acc_c += edge_lum;
                    } else {
                        double d_sh = shape_sdf(kind, style, (Vec2){u - 0.02, v - 0.025}, frame, total_frames);
                        if (d_sh <= edge_w) acc_a += 0.28;
                    }
                }
            }
            double samples = (double)(ss * ss);
            double alpha = d_clamp(acc_a / samples, 0.0, 1.0);
            double premul_lum = (acc_a > 1e-6) ? (acc_c / samples) : 0.0;
            uint8_t a8 = (uint8_t)round(alpha * 255.0);
            uint8_t l8 = (uint8_t)d_clamp(round(premul_lum), 0.0, (double)a8);
            pixels[(size_t)(y * size + x)] =
                ((uint32_t)a8 << 24) | ((uint32_t)l8 << 16) | ((uint32_t)l8 << 8) | (uint32_t)l8;
        }
    }
    return pixels;
}

static void render_svg_frame(ShapeKind kind, CursorStyle style, int frame, int total_frames, char *out, size_t cap) {
    const char *fill = "#0a0a0a";
    const char *stroke = "#f5f5f5";
    if (style == STYLE_IVORY) { fill = "#f7f7f7"; stroke = "#0a0a0a"; }
    else if (style == STYLE_PRECISION || style == STYLE_DOT) { fill = "#f4f4f4"; stroke = "#0c0c0c"; }

    int n = snprintf(out, cap, "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 64 64\" width=\"64\" height=\"64\">\n");

    if (style == STYLE_DOT && kind == SHAPE_LEFT_PTR) {
        snprintf(out + n, cap - (size_t)n,
            "  <circle cx=\"32\" cy=\"32\" r=\"16.5\" fill=\"none\" stroke=\"%s\" stroke-width=\"4\" opacity=\"0.55\"/>\n"
            "  <circle cx=\"32\" cy=\"32\" r=\"16.5\" fill=\"none\" stroke=\"%s\" stroke-width=\"2\"/>\n"
            "  <circle cx=\"32\" cy=\"32\" r=\"7.2\" fill=\"%s\" stroke=\"%s\" stroke-width=\"1.6\"/>\n"
            "</svg>\n", stroke, fill, fill, stroke);
        return;
    }
    if (style == STYLE_DOT && kind == SHAPE_HAND) {
        snprintf(out + n, cap - (size_t)n,
            "  <circle cx=\"32\" cy=\"32\" r=\"20\" fill=\"none\" stroke=\"%s\" stroke-width=\"2.4\"/>\n"
            "  <circle cx=\"32\" cy=\"32\" r=\"9.5\" fill=\"%s\" stroke=\"%s\" stroke-width=\"1.6\"/>\n"
            "</svg>\n", fill, fill, stroke);
        return;
    }

    switch (kind) {
        case SHAPE_LEFT_PTR:
            if (style == STYLE_PRECISION) {
                n += snprintf(out + n, cap - (size_t)n,
                    "  <polygon points=\"10,8 10,51 22,40 30,55 35,52 27,37 42,37\" fill=\"none\" stroke=\"%s\" stroke-width=\"4.2\" stroke-linejoin=\"round\"/>\n"
                    "  <polygon points=\"10,8 10,51 22,40 30,55 35,52 27,37 42,37\" fill=\"none\" stroke=\"%s\" stroke-width=\"2.0\" stroke-linejoin=\"round\"/>\n"
                    "  <circle cx=\"10\" cy=\"8\" r=\"2.3\" fill=\"#ffffff\"/>\n", stroke, fill);
            } else {
                n += snprintf(out + n, cap - (size_t)n,
                    "  <polygon points=\"10,8 10,51 22,40 30,55 35,52 27,37 42,37\" fill=\"%s\" stroke=\"%s\" stroke-width=\"2.4\" stroke-linejoin=\"round\"/>\n",
                    fill, stroke);
            }
            break;
        case SHAPE_HAND:
            n += snprintf(out + n, cap - (size_t)n,
                "  <polygon points=\"23,9 28,9 28,27 46,27 46,50 19,50 14,38 18,33 23,37\" fill=\"%s\" stroke=\"%s\" stroke-width=\"2.4\" stroke-linejoin=\"round\"/>\n",
                (style == STYLE_PRECISION ? "none" : fill), stroke);
            if (style == STYLE_PRECISION) {
                n += snprintf(out + n, cap - (size_t)n,
                    "  <polygon points=\"23,9 28,9 28,27 46,27 46,50 19,50 14,38 18,33 23,37\" fill=\"none\" stroke=\"%s\" stroke-width=\"1.5\" stroke-linejoin=\"round\"/>\n",
                    fill);
            }
            break;
        case SHAPE_XTERM:
            n += snprintf(out + n, cap - (size_t)n,
                "  <g fill=\"none\" stroke=\"%s\" stroke-width=\"5\" stroke-linecap=\"round\">\n"
                "    <path d=\"M32 13V51M24 13H40M24 51H40\"/>\n"
                "  </g>\n"
                "  <g fill=\"none\" stroke=\"%s\" stroke-width=\"2.4\" stroke-linecap=\"round\">\n"
                "    <path d=\"M32 13V51M24 13H40M24 51H40\"/>\n"
                "  </g>\n", stroke, fill);
            break;
        case SHAPE_CROSSHAIR:
            n += snprintf(out + n, cap - (size_t)n,
                "  <g fill=\"none\" stroke=\"%s\" stroke-width=\"4.5\" stroke-linecap=\"round\">\n"
                "    <path d=\"M10 32H25M39 32H54M32 10V25M32 39V54\"/>\n"
                "  </g>\n"
                "  <g fill=\"none\" stroke=\"%s\" stroke-width=\"2.2\" stroke-linecap=\"round\">\n"
                "    <path d=\"M10 32H25M39 32H54M32 10V25M32 39V54\"/>\n"
                "  </g>\n"
                "  <circle cx=\"32\" cy=\"32\" r=\"2.2\" fill=\"%s\" stroke=\"%s\" stroke-width=\"1.2\"/>\n",
                stroke, fill, fill, stroke);
            break;
        case SHAPE_WATCH: {
            int tf = total_frames > 0 ? total_frames : 1;
            double deg = (360.0 * frame) / tf;
            n += snprintf(out + n, cap - (size_t)n,
                "  <g transform=\"rotate(%.1f 32 32)\">\n"
                "    <circle cx=\"32\" cy=\"32\" r=\"17.5\" fill=\"none\" stroke=\"%s\" stroke-width=\"5\" opacity=\"0.55\"/>\n"
                "    <circle cx=\"32\" cy=\"32\" r=\"17.5\" fill=\"none\" stroke=\"%s\" stroke-width=\"2.6\"/>\n"
                "    <circle cx=\"32\" cy=\"14.5\" r=\"4.8\" fill=\"%s\" stroke=\"%s\" stroke-width=\"1.8\"/>\n"
                "  </g>\n", deg, stroke, fill, fill, stroke);
            break;
        }
        case SHAPE_FLEUR:
            n += snprintf(out + n, cap - (size_t)n,
                "  <g fill=\"none\" stroke=\"%s\" stroke-width=\"4.6\" stroke-linecap=\"round\" stroke-linejoin=\"round\">\n"
                "    <path d=\"M12 32H52M32 12V52M19 25L12 32L19 39M45 25L52 32L45 39M25 19L32 12L39 19M25 45L32 52L39 45\"/>\n"
                "  </g>\n"
                "  <g fill=\"none\" stroke=\"%s\" stroke-width=\"2.2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">\n"
                "    <path d=\"M12 32H52M32 12V52M19 25L12 32L19 39M45 25L52 32L45 39M25 19L32 12L39 19M25 45L32 52L39 45\"/>\n"
                "  </g>\n", stroke, fill);
            break;
        case SHAPE_RESIZE_H:
        case SHAPE_RESIZE_V:
        case SHAPE_RESIZE_NWSE:
        case SHAPE_RESIZE_NESW: {
            int rot = 0;
            if (kind == SHAPE_RESIZE_V) rot = 90;
            else if (kind == SHAPE_RESIZE_NWSE) rot = 45;
            else if (kind == SHAPE_RESIZE_NESW) rot = -45;
            n += snprintf(out + n, cap - (size_t)n,
                "  <g transform=\"rotate(%d 32 32)\" fill=\"none\" stroke-linecap=\"round\" stroke-linejoin=\"round\">\n"
                "    <path stroke=\"%s\" stroke-width=\"4.8\" d=\"M12 32H52M32 22V42M20 24L12 32L20 40M44 24L52 32L44 40\"/>\n"
                "    <path stroke=\"%s\" stroke-width=\"2.3\" d=\"M12 32H52M32 22V42M20 24L12 32L20 40M44 24L52 32L44 40\"/>\n"
                "  </g>\n", rot, stroke, fill);
            break;
        }
        case SHAPE_GRAB:
            n += snprintf(out + n, cap - (size_t)n,
                "  <rect x=\"19\" y=\"21\" width=\"26\" height=\"24\" rx=\"5\" fill=\"none\" stroke=\"%s\" stroke-width=\"4.6\"/>\n"
                "  <rect x=\"19\" y=\"21\" width=\"26\" height=\"24\" rx=\"5\" fill=\"none\" stroke=\"%s\" stroke-width=\"2.2\"/>\n",
                stroke, fill);
            break;
        case SHAPE_GRABBING:
            n += snprintf(out + n, cap - (size_t)n,
                "  <rect x=\"21\" y=\"23\" width=\"22\" height=\"20\" rx=\"5\" fill=\"%s\" stroke=\"%s\" stroke-width=\"2.4\"/>\n",
                fill, stroke);
            break;
        case SHAPE_NOT_ALLOWED:
            n += snprintf(out + n, cap - (size_t)n,
                "  <g fill=\"none\" stroke-linecap=\"round\">\n"
                "    <circle cx=\"32\" cy=\"32\" r=\"18\" stroke=\"%s\" stroke-width=\"5\"/>\n"
                "    <line x1=\"20\" y1=\"20\" x2=\"44\" y2=\"44\" stroke=\"%s\" stroke-width=\"5\"/>\n"
                "    <circle cx=\"32\" cy=\"32\" r=\"18\" stroke=\"%s\" stroke-width=\"2.4\"/>\n"
                "    <line x1=\"20\" y1=\"20\" x2=\"44\" y2=\"44\" stroke=\"%s\" stroke-width=\"2.4\"/>\n"
                "  </g>\n", stroke, stroke, fill, fill);
            break;
    }
    snprintf(out + n, cap - (size_t)n, "</svg>\n");
}

/* ── Minimal Byte Buffer + IEEE 802.3 CRC32 + Uncompressed ZIP (.hlc) ─── */

typedef struct {
    uint8_t *data;
    size_t len;
    size_t cap;
} ByteBuf;

static void bbuf_append(ByteBuf *b, const void *src, size_t n) {
    if (b->len + n > b->cap) {
        size_t ncap = b->cap ? b->cap * 2 : 4096;
        while (ncap < b->len + n) ncap *= 2;
        uint8_t *nd = (uint8_t *)realloc(b->data, ncap);
        if (!nd) exit(1);
        b->data = nd;
        b->cap = ncap;
    }
    memcpy(b->data + b->len, src, n);
    b->len += n;
}

static void put_u16(ByteBuf *b, uint16_t v) {
    uint8_t tmp[2] = { (uint8_t)(v & 0xFFu), (uint8_t)((v >> 8) & 0xFFu) };
    bbuf_append(b, tmp, 2);
}

static void put_u32(ByteBuf *b, uint32_t v) {
    uint8_t tmp[4] = {
        (uint8_t)(v & 0xFFu),
        (uint8_t)((v >> 8) & 0xFFu),
        (uint8_t)((v >> 16) & 0xFFu),
        (uint8_t)((v >> 24) & 0xFFu)
    };
    bbuf_append(b, tmp, 4);
}

static uint32_t crc32_compute(const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= p[i];
        for (int k = 0; k < 8; ++k) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1u)));
        }
    }
    return ~crc;
}

typedef struct {
    char name[64];
    char *content;
    size_t len;
} ZipEntry;

static void write_binary_file(const char *path, const void *data, size_t len) {
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "cannot write %s: %s\n", path, strerror(errno));
        exit(1);
    }
    if (len > 0 && fwrite(data, 1, len, fp) != len) {
        fclose(fp);
        exit(1);
    }
    fclose(fp);
}

static void write_zip_archive(const char *path, const ZipEntry *entries, size_t count) {
    ByteBuf blob = {NULL, 0, 0};
    typedef struct { uint32_t crc, size, offset; uint16_t nlen; const char *name; } CdRec;
    CdRec cds[16];

    for (size_t i = 0; i < count; ++i) {
        uint32_t crc = crc32_compute(entries[i].content, entries[i].len);
        uint32_t sz = (uint32_t)entries[i].len;
        uint32_t off = (uint32_t)blob.len;
        uint16_t nlen = (uint16_t)strlen(entries[i].name);
        cds[i] = (CdRec){crc, sz, off, nlen, entries[i].name};

        put_u32(&blob, 0x04034b50u);
        put_u16(&blob, 20);
        put_u16(&blob, 0);
        put_u16(&blob, 0);
        put_u16(&blob, 0);
        put_u16(&blob, 0);
        put_u32(&blob, crc);
        put_u32(&blob, sz);
        put_u32(&blob, sz);
        put_u16(&blob, nlen);
        put_u16(&blob, 0);
        bbuf_append(&blob, entries[i].name, nlen);
        bbuf_append(&blob, entries[i].content, sz);
    }

    uint32_t cd_start = (uint32_t)blob.len;
    for (size_t i = 0; i < count; ++i) {
        put_u32(&blob, 0x02014b50u);
        put_u16(&blob, 20);
        put_u16(&blob, 20);
        put_u16(&blob, 0);
        put_u16(&blob, 0);
        put_u16(&blob, 0);
        put_u16(&blob, 0);
        put_u32(&blob, cds[i].crc);
        put_u32(&blob, cds[i].size);
        put_u32(&blob, cds[i].size);
        put_u16(&blob, cds[i].nlen);
        put_u16(&blob, 0);
        put_u16(&blob, 0);
        put_u16(&blob, 0);
        put_u16(&blob, 0);
        put_u32(&blob, 0);
        put_u32(&blob, cds[i].offset);
        bbuf_append(&blob, cds[i].name, cds[i].nlen);
    }
    uint32_t cd_size = (uint32_t)blob.len - cd_start;

    put_u32(&blob, 0x06054b50u);
    put_u16(&blob, 0);
    put_u16(&blob, 0);
    put_u16(&blob, (uint16_t)count);
    put_u16(&blob, (uint16_t)count);
    put_u32(&blob, cd_size);
    put_u32(&blob, cd_start);
    put_u16(&blob, 0);

    write_binary_file(path, blob.data, blob.len);
    free(blob.data);
}

static void write_xcursor_file(const char *path, const ShapeSpec *shape, CursorStyle style) {
    static const int SIZES[3] = {24, 32, 48};
    double hx = shape->hot_x;
    double hy = shape->hot_y;
    if (style == STYLE_DOT && (shape->kind == SHAPE_LEFT_PTR || shape->kind == SHAPE_HAND)) {
        hx = 0.50;
        hy = 0.50;
    }

    typedef struct {
        uint32_t nominal, w, h, xhot, yhot, delay;
        uint32_t *pixels;
    } ImgChunk;

    ImgChunk chunks[32];
    size_t nchunks = 0;

    for (size_t s = 0; s < 3; ++s) {
        int sz = SIZES[s];
        for (int f = 0; f < shape->frames; ++f) {
            ImgChunk *ch = &chunks[nchunks++];
            ch->nominal = (uint32_t)sz;
            ch->w = (uint32_t)sz;
            ch->h = (uint32_t)sz;
            ch->xhot = (uint32_t)i_clamp((int)round(hx * sz), 0, sz - 1);
            ch->yhot = (uint32_t)i_clamp((int)round(hy * sz), 0, sz - 1);
            ch->delay = (shape->frames > 1) ? shape->delay_ms : 0u;
            ch->pixels = rasterize_shape(shape->kind, style, sz, f, shape->frames);
        }
    }

    ByteBuf buf = {NULL, 0, 0};
    bbuf_append(&buf, "Xcur", 4);
    put_u32(&buf, 16u);
    put_u32(&buf, 0x00010000u);
    put_u32(&buf, (uint32_t)nchunks);

    uint32_t offset = (uint32_t)(16 + nchunks * 12);
    for (size_t i = 0; i < nchunks; ++i) {
        put_u32(&buf, 0xfffd0002u);
        put_u32(&buf, chunks[i].nominal);
        put_u32(&buf, offset);
        offset += (uint32_t)(36 + (size_t)chunks[i].w * (size_t)chunks[i].h * 4);
    }

    for (size_t i = 0; i < nchunks; ++i) {
        put_u32(&buf, 36u);
        put_u32(&buf, 0xfffd0002u);
        put_u32(&buf, chunks[i].nominal);
        put_u32(&buf, 1u);
        put_u32(&buf, chunks[i].w);
        put_u32(&buf, chunks[i].h);
        put_u32(&buf, chunks[i].xhot);
        put_u32(&buf, chunks[i].yhot);
        put_u32(&buf, chunks[i].delay);
        size_t npix = (size_t)chunks[i].w * (size_t)chunks[i].h;
        for (size_t p = 0; p < npix; ++p) {
            put_u32(&buf, chunks[i].pixels[p]);
        }
        free(chunks[i].pixels);
    }

    write_binary_file(path, buf.data, buf.len);
    free(buf.data);
}

static void mkdir_p(const char *dir) {
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", dir);
    size_t len = strlen(tmp);
    if (len == 0) return;
    if (tmp[len - 1] == '/') tmp[len - 1] = '\0';
    for (char *p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

static void copy_file_fallback(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return;
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return; }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) break;
    }
    fclose(out);
    fclose(in);
}

static void path_join2(char *dst, size_t cap, const char *a, const char *b) {
    int n = snprintf(dst, cap, "%s/%s", a, b);
    if (n < 0 || (size_t)n >= cap) exit(1);
}

static void path_join_ext(char *dst, size_t cap, const char *a, const char *b, const char *ext) {
    int n = snprintf(dst, cap, "%s/%s%s", a, b, ext);
    if (n < 0 || (size_t)n >= cap) exit(1);
}

static void build_theme(const char *out_root, const ThemeSpec *theme) {
    char root[PATH_MAX], hc_dir[PATH_MAX], xc_dir[PATH_MAX], path[PATH_MAX];
    path_join2(root, sizeof(root), out_root, theme->name);
    path_join2(hc_dir, sizeof(hc_dir), root, "hyprcursors");
    path_join2(xc_dir, sizeof(xc_dir), root, "cursors");
    mkdir_p(hc_dir);
    mkdir_p(xc_dir);

    char idx[1024];
    int ilen = snprintf(idx, sizeof(idx),
        "[Icon Theme]\n"
        "Name=%s\n"
        "Comment=%s\n"
        "Inherits=Adwaita\n", theme->name, theme->comment);
    path_join2(path, sizeof(path), root, "index.theme");
    write_binary_file(path, idx, (size_t)ilen);
    path_join2(path, sizeof(path), root, "cursor.theme");
    write_binary_file(path, idx, (size_t)ilen);

    char mf[1024];
    int mlen = snprintf(mf, sizeof(mf),
        "name = %s\n"
        "description = %s\n"
        "version = 5.3\n"
        "cursors_directory = hyprcursors\n", theme->name, theme->comment);
    path_join2(path, sizeof(path), root, "manifest.hl");
    write_binary_file(path, mf, (size_t)mlen);

    for (size_t s = 0; s < 13; ++s) {
        const ShapeSpec *shape = &SHAPES[s];
        double hx = shape->hot_x;
        double hy = shape->hot_y;
        if (theme->style == STYLE_DOT && (shape->kind == SHAPE_LEFT_PTR || shape->kind == SHAPE_HAND)) {
            hx = 0.50;
            hy = 0.50;
        }

        char meta[4096];
        int mp = snprintf(meta, sizeof(meta),
            "resize_algorithm = bilinear\n"
            "hotspot_x = %.2f\n"
            "hotspot_y = %.2f\n", hx, hy);
        for (size_t a = 0; shape->aliases[a] != NULL; ++a) {
            mp += snprintf(meta + mp, sizeof(meta) - (size_t)mp,
                "define_override = %s\n", shape->aliases[a]);
        }

        ZipEntry entries[12];
        size_t nent = 1; /* slot 0 reserved for meta.hl */

        if (shape->frames <= 1) {
            mp += snprintf(meta + mp, sizeof(meta) - (size_t)mp, "define_size = 0, cursor.svg\n");
            char *svg = (char *)malloc(4096);
            render_svg_frame(shape->kind, theme->style, 0, 1, svg, 4096);
            snprintf(entries[nent].name, sizeof(entries[nent].name), "cursor.svg");
            entries[nent].content = svg;
            entries[nent].len = strlen(svg);
            nent++;
        } else {
            for (int f = 0; f < shape->frames; ++f) {
                char fname[32];
                snprintf(fname, sizeof(fname), "frame%d.svg", f);
                mp += snprintf(meta + mp, sizeof(meta) - (size_t)mp,
                    "define_size = 0, %s, %u\n", fname, shape->delay_ms);
                char *svg = (char *)malloc(4096);
                render_svg_frame(shape->kind, theme->style, f, shape->frames, svg, 4096);
                snprintf(entries[nent].name, sizeof(entries[nent].name), "%s", fname);
                entries[nent].content = svg;
                entries[nent].len = strlen(svg);
                nent++;
            }
        }

        snprintf(entries[0].name, sizeof(entries[0].name), "meta.hl");
        entries[0].content = meta;
        entries[0].len = (size_t)mp;

        path_join_ext(path, sizeof(path), hc_dir, shape->name, ".hlc");
        write_zip_archive(path, entries, nent);
        for (size_t e = 1; e < nent; ++e) free(entries[e].content);

        char xc_main[PATH_MAX];
        path_join2(xc_main, sizeof(xc_main), xc_dir, shape->name);
        write_xcursor_file(xc_main, shape, theme->style);

        for (size_t a = 0; shape->aliases[a] != NULL; ++a) {
            char link_path[PATH_MAX];
            path_join2(link_path, sizeof(link_path), xc_dir, shape->aliases[a]);
            unlink(link_path);
            if (symlink(shape->name, link_path) != 0) {
                copy_file_fallback(xc_main, link_path);
            }
        }
    }
}

int main(int argc, char **argv) {
    const char *out_dir = "cursors";
    const char *only_theme = "";

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--list") == 0) {
            for (size_t t = 0; t < 4; ++t) {
                printf("%s\t%s\t%s\n", THEMES[t].id, THEMES[t].name, THEMES[t].comment);
            }
            return 0;
        }
        if ((strcmp(argv[i], "--out-dir") == 0 || strcmp(argv[i], "-o") == 0) && i + 1 < argc) {
            out_dir = argv[++i];
        } else if ((strcmp(argv[i], "--theme") == 0 || strcmp(argv[i], "-t") == 0) && i + 1 < argc) {
            only_theme = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: mono-cursor-gen [--out-dir <dir>] [--theme <obsidian|ivory|precision|dot>] [--list]\n");
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return 1;
        }
    }

    int built = 0;
    for (size_t t = 0; t < 4; ++t) {
        if (only_theme[0] != '\0' && strcmp(only_theme, THEMES[t].id) != 0 && strcmp(only_theme, THEMES[t].name) != 0) {
            continue;
        }
        build_theme(out_dir, &THEMES[t]);
        built++;
    }

    if (built == 0) {
        fprintf(stderr, "No matching cursor theme for '%s'\n", only_theme);
        return 1;
    }

    printf("built %d monochrome cursor theme(s) in %s\n", built, out_dir);
    return 0;
}
