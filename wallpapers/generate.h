#ifndef MONO_WALLPAPER_GENERATE_H
#define MONO_WALLPAPER_GENERATE_H
// ═══════════════════════════════════════════════════════════════════════════
//  wallpapers/generate.h — 14 Procedural Kinetic Monochrome Designs (Pure C11)
//
//  Every design is a pure C-infinity periodic phase function `fn(w, h, phase)`
//  over `phase in [0, 1)` so all 14 compositions render both as seamless
//  looping live wallpapers (`wallpapers/animate.c` + `tools/build-preview.c`)
//  and as arbitrary-resolution stills (`wallpapers/generate.c` at phase=0).
// ═══════════════════════════════════════════════════════════════════════════

#include "../include/mono_image.h"

#include <pthread.h>
#include <unistd.h>

#define MONO_TAU (2.0 * MONO_PI)

static inline bool *mono_in_frame_worker_ptr(void) {
    static _Thread_local bool flag = false;
    return &flag;
}

// ── Exact MT19937 32-bit PRNG (matches deterministic starfield/rain seeds) ─

typedef struct {
    uint32_t mt[624];
    int index;
} MonoMt19937;

static inline void mt_seed(MonoMt19937 *r, uint32_t seed) {
    r->mt[0] = seed;
    for (int i = 1; i < 624; ++i) {
        r->mt[i] = 1812433253u * (r->mt[i - 1] ^ (r->mt[i - 1] >> 30)) + (uint32_t)i;
    }
    r->index = 624;
}

static inline uint32_t mt_next(MonoMt19937 *r) {
    if (r->index >= 624) {
        for (int i = 0; i < 624; ++i) {
            uint32_t y = (r->mt[i] & 0x80000000u) | (r->mt[(i + 1) % 624] & 0x7fffffffu);
            r->mt[i] = r->mt[(i + 397) % 624] ^ (y >> 1);
            if (y & 1u) r->mt[i] ^= 0x9908b0dfu;
        }
        r->index = 0;
    }
    uint32_t y = r->mt[r->index++];
    y ^= (y >> 11);
    y ^= (y << 7) & 0x9d2c5680u;
    y ^= (y << 15) & 0xefc60000u;
    y ^= (y >> 18);
    return y;
}

static inline double mt_unit(MonoMt19937 *r) {
    return (double)mt_next(r) / 4294967295.0;
}

// ── Deterministic Hash Noise & Pthread Row Parallelism ─────────────────────

static inline uint64_t mono_splitmix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

static inline double mono_rand01(uint64_t seed, int x, int y, uint64_t salt) {
    uint64_t h = seed ^ ((uint64_t)(uint32_t)x * 0x9e3779b185ebca87ULL) ^
                 ((uint64_t)(uint32_t)y * 0xc2b2ae3d27d4eb4fULL) ^ salt;
    return (double)(mono_splitmix64(h) >> 11) * (1.0 / 9007199254740992.0);
}

static inline double mono_gauss_px(uint64_t seed, int x, int y, double sigma) {
    if (*mono_in_frame_worker_ptr()) return 0.0;
    double u1 = mono_dmax(1e-12, mono_rand01(seed, x, y, 0x11ULL));
    double u2 = mono_rand01(seed, x, y, 0x22ULL);
    double z = sqrt(-2.0 * log(u1)) * cos(MONO_TAU * u2);
    return z * sigma;
}

static inline int mono_cpu_count(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    if (n < 1) return 1;
    if (n > 16) return 16;
    return (int)n;
}

typedef void (*MonoRowFn)(int y, void *ctx);

typedef struct {
    int y0;
    int y1;
    MonoRowFn fn;
    void *ctx;
} MonoRowJob;

static inline void *mono_row_worker_entry(void *arg) {
    MonoRowJob *j = (MonoRowJob *)arg;
    for (int y = j->y0; y < j->y1; ++y) j->fn(y, j->ctx);
    return NULL;
}

static inline void mono_parallel_rows(int h, MonoRowFn fn, void *ctx) {
    if (*mono_in_frame_worker_ptr() || h < 64) {
        for (int y = 0; y < h; ++y) fn(y, ctx);
        return;
    }
    int nthreads = mono_imin(h, mono_cpu_count());
    if (nthreads <= 1) {
        for (int y = 0; y < h; ++y) fn(y, ctx);
        return;
    }
    pthread_t tids[16];
    MonoRowJob jobs[16];
    int spawned = 0;
    int chunk = (h + nthreads - 1) / nthreads;
    for (int t = 0; t < nthreads; ++t) {
        int y0 = t * chunk;
        int y1 = mono_imin(h, y0 + chunk);
        if (y0 >= y1) break;
        jobs[spawned] = (MonoRowJob){y0, y1, fn, ctx};
        if (pthread_create(&tids[spawned], NULL, mono_row_worker_entry, &jobs[spawned]) == 0) {
            spawned++;
        } else {
            for (int y = y0; y < y1; ++y) fn(y, ctx);
        }
    }
    for (int i = 0; i < spawned; ++i) pthread_join(tids[i], NULL);
}

typedef struct { GrayImage *img; double sigma; uint64_t seed; } GrainCtx;
static inline void grain_row_fn(int y, void *p) {
    GrainCtx *c = (GrainCtx *)p;
    int w = c->img->w;
    uint8_t *row = c->img->px + (size_t)y * (size_t)w;
    for (int x = 0; x < w; ++x) {
        row[x] = mono_clamp_u8((double)row[x] + mono_gauss_px(c->seed, x, y, c->sigma));
    }
}
static inline void mono_add_grain(GrayImage *img, double sigma, uint64_t seed) {
    GrainCtx c = {img, sigma, seed};
    mono_parallel_rows(img->h, grain_row_fn, &c);
}

typedef struct { GrayImage *img; double strength, cx, cy, max_d; } VigCtx;
static inline void vig_row_fn(int y, void *p) {
    VigCtx *c = (VigCtx *)p;
    int w = c->img->w;
    double dy = y - c->cy;
    uint8_t *row = c->img->px + (size_t)y * (size_t)w;
    for (int x = 0; x < w; ++x) {
        double r = hypot(x - c->cx, dy) / c->max_d;
        double factor = mono_dmax(0.0, 1.0 - c->strength * (r * r));
        row[x] = mono_clamp_u8((double)row[x] * factor);
    }
}
static inline void mono_apply_vignette(GrayImage *img, double strength) {
    double cx = img->w * 0.5, cy = img->h * 0.5;
    VigCtx c = {img, strength, cx, cy, hypot(cx, cy)};
    mono_parallel_rows(img->h, vig_row_fn, &c);
}

static inline double mono_value_noise(double x, double y, uint64_t seed) {
    int xi = (int)floor(x);
    int yi = (int)floor(y);
    double xf = x - xi;
    double yf = y - yi;
    double u = xf * xf * (3.0 - 2.0 * xf);
    double v = yf * yf * (3.0 - 2.0 * yf);
    double n00 = mono_rand01(seed, xi, yi, 0);
    double n10 = mono_rand01(seed, xi + 1, yi, 0);
    double n01 = mono_rand01(seed, xi, yi + 1, 0);
    double n11 = mono_rand01(seed, xi + 1, yi + 1, 0);
    double nx0 = n00 + (n10 - n00) * u;
    double nx1 = n01 + (n11 - n01) * u;
    return nx0 + (nx1 - nx0) * v;
}

static inline double mono_fbm2d(double x, double y, uint64_t seed, int octaves) {
    double sum = 0.0, amp = 0.5, norm = 0.0, freq = 1.0;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * mono_value_noise(x * freq, y * freq, seed + (uint64_t)i * 131ULL);
        norm += amp;
        amp *= 0.5;
        freq *= 2.02;
    }
    return sum / norm;
}

static inline double mono_wrap_dist(double a, double b, double period) {
    double d = fabs(a - b);
    return mono_dmin(d, period - d);
}

// ── 01 · Noir Vignette ─────────────────────────────────────────────────────
typedef struct {
    GrayImage *img;
    double cx, cy, max_r, lx, ly, spot_r, ring_r1, ring_r2, ring_w;
} D01Ctx;

static inline void d01_row(int y, void *p) {
    D01Ctx *c = (D01Ctx *)p;
    int w = c->img->w;
    double dy = (y - c->cy) * 1.12;
    uint8_t *row = c->img->px + (size_t)y * (size_t)w;
    for (int x = 0; x < w; ++x) {
        double t = mono_dmin(1.0, hypot(x - c->cx, dy) / c->max_r);
        double s = t * t * (3.0 - 2.0 * t);
        double sd = hypot(x - c->lx, (y - c->ly) * 1.08) / c->spot_r;
        double spot = 26.0 * exp(-sd * sd * 1.85);
        double rc = hypot(x - c->cx, y - c->cy);
        double q1 = (rc - c->ring_r1) / c->ring_w;
        double q2 = (rc - c->ring_r2) / (c->ring_w * 1.3);
        double r1 = 18.0 * exp(-(q1 * q1));
        double r2 = 11.0 * exp(-(q2 * q2));
        row[x] = mono_clamp_u8(24.0 - 18.0 * s + spot + r1 + r2 + mono_gauss_px(101, x, y, 1.3));
    }
}

static inline GrayImage design_01_vignette(int w, int h, double phase) {
    GrayImage img = gray_new(w, h, 0);
    double cx = w * 0.5, cy = h * 0.5;
    double tau = MONO_TAU * phase;
    double mn = (double)mono_imin(w, h);
    D01Ctx c = {
        &img, cx, cy, hypot(cx, cy),
        cx + cos(tau) * (w * 0.17), cy + sin(tau) * (h * 0.13),
        mn * 0.44, mn * (0.28 + 0.035 * sin(tau)), mn * (0.41 - 0.025 * cos(tau)),
        mono_dmax(1.5, mn / 260.0)
    };
    mono_parallel_rows(h, d01_row, &c);
    return img;
}

// ── 02 · Architectural Grid ────────────────────────────────────────────────
typedef struct { GrayImage *img; double sx, sy, band_x, band_y; } D02Ctx;
static inline void d02_row(int y, void *p) {
    D02Ctx *c = (D02Ctx *)p;
    int w = c->img->w, h = c->img->h;
    double dy = mono_wrap_dist((double)y, c->sy, (double)h);
    double qy = dy / c->band_y;
    double wy = 8.0 * exp(-(qy * qy));
    uint8_t *row = c->img->px + (size_t)y * (size_t)w;
    for (int x = 0; x < w; ++x) {
        double dx = mono_wrap_dist((double)x, c->sx, (double)w);
        double qx = dx / c->band_x;
        row[x] = mono_clamp_u8(11.0 + 10.0 * exp(-(qx * qx)) + wy);
    }
}

static inline GrayImage design_02_grid(int w, int h, double phase) {
    GrayImage img = gray_new(w, h, 11);
    int mn = mono_imin(w, h);
    int step = mono_imax(24, (int)round(mn / 18.0));
    int major = step * 4;
    double sx = fmod(phase + 1.0, 1.0) * w;
    double sy = fmod(phase + 0.5, 1.0) * h;
    double band_x = mono_dmax(24.0, w * 0.085);
    double band_y = mono_dmax(20.0, h * 0.10);

    D02Ctx c = {&img, sx, sy, band_x, band_y};
    mono_parallel_rows(h, d02_row, &c);

    for (int x = 0; x < w; x += step) {
        double dx = mono_wrap_dist((double)x, sx, (double)w) / (band_x * 0.75);
        uint8_t col = mono_clamp_u8(((x % major == 0) ? 28.0 : 18.0) + 46.0 * exp(-(dx * dx)));
        gray_draw_line(&img, x, 0, x, h - 1, col, 1.0);
    }
    for (int y = 0; y < h; y += step) {
        double dy = mono_wrap_dist((double)y, sy, (double)h) / (band_y * 0.75);
        uint8_t col = mono_clamp_u8(((y % major == 0) ? 28.0 : 18.0) + 42.0 * exp(-(dy * dy)));
        gray_draw_line(&img, 0, y, w - 1, y, col, 1.0);
    }
    double cross_r = mono_dmax(3.0, mn / 240.0);
    for (int y = 0; y < h; y += major) {
        double dy = mono_wrap_dist((double)y, sy, (double)h);
        for (int x = 0; x < w; x += major) {
            double dx = mono_wrap_dist((double)x, sx, (double)w);
            double glow = 115.0 * exp(-(dx * dx) / (2.0 * band_x * band_x) - (dy * dy) / (2.0 * band_y * band_y));
            uint8_t ccol = mono_clamp_u8(52.0 + glow);
            gray_draw_line(&img, x - cross_r, y, x + cross_r, y, ccol, 1.0);
            gray_draw_line(&img, x, y - cross_r, x, y + cross_r, ccol, 1.0);
        }
    }
    mono_apply_vignette(&img, 0.52);
    mono_add_grain(&img, 1.1, 202);
    return img;
}

// ── 03 · Concentric Interference Arcs ──────────────────────────────────────
typedef struct { GrayImage *img; double cx, cy, max_r, spacing, phase, tau; } D03Ctx;
static inline void d03_row(int y, void *p) {
    D03Ctx *c = (D03Ctx *)p;
    int w = c->img->w;
    uint8_t *row = c->img->px + (size_t)y * (size_t)w;
    for (int x = 0; x < w; ++x) {
        double d = hypot(x - c->cx, y - c->cy);
        double ph = fmod(d / c->spacing - c->phase * 2.0 + 1024.0, 1.0);
        double q = (ph - 0.5) * 9.0;
        double line = exp(-(q * q));
        double pulse = 0.82 + 0.18 * sin(d / (c->spacing * 3.5) - c->tau);
        double falloff = mono_dclamp(1.0 - (d / c->max_r) * 0.70, 0.16, 1.0);
        double base = 11.0 + 9.0 * (1.0 - d / c->max_r);
        row[x] = mono_clamp_u8(base + 48.0 * line * falloff * pulse + mono_gauss_px(303, x, y, 1.0));
    }
}

static inline GrayImage design_03_arcs(int w, int h, double phase) {
    GrayImage img = gray_new(w, h, 11);
    D03Ctx c = {&img, w * 1.08, h * 1.12, hypot(w, h) * 1.25, mono_dmax(14.0, mono_imin(w, h) / 26.0), phase, MONO_TAU * phase};
    mono_parallel_rows(h, d03_row, &c);
    return img;
}

// ── 04 · Topographic Contours ──────────────────────────────────────────────
typedef struct { GrayImage *img; double scale, phase; } D04Ctx;
static inline void d04_row(int y, void *p) {
    D04Ctx *c = (D04Ctx *)p;
    int w = c->img->w;
    uint8_t *row = c->img->px + (size_t)y * (size_t)w;
    for (int x = 0; x < w; ++x) {
        double elev = mono_fbm2d(x * c->scale, y * c->scale, 404, 5);
        double bands = elev * 15.0 - c->phase * 4.0 + 16.0;
        double fl = floor(bands);
        double frac = bands - fl;
        bool major = ((int)fl % 4 == 0);
        double dist = mono_dmin(frac, 1.0 - frac);
        double q = dist * (major ? 22.0 : 30.0);
        double sharp = exp(-(q * q));
        row[x] = mono_clamp_u8(11.0 + 14.0 * elev + (major ? 50.0 : 27.0) * sharp + mono_gauss_px(405, x, y, 1.0));
    }
}

static inline GrayImage design_04_contours(int w, int h, double phase) {
    GrayImage img = gray_new(w, h, 12);
    D04Ctx c = {&img, 4.2 / mono_imin(w, h), phase};
    mono_parallel_rows(h, d04_row, &c);
    mono_apply_vignette(&img, 0.42);
    return img;
}

// ── 05 · Diagonal Brushed Graphite Strata ──────────────────────────────────
typedef struct { GrayImage *img; double period, phase, tau; } D05Ctx;
static inline void d05_row(int y, void *p) {
    D05Ctx *c = (D05Ctx *)p;
    int w = c->img->w;
    uint8_t *row = c->img->px + (size_t)y * (size_t)w;
    for (int x = 0; x < w; ++x) {
        double u = (x * 0.70710678 + y * 0.70710678) / c->period - c->phase + 64.0;
        double f = u - floor(u);
        double saw = 0.25 + 0.75 * pow(1.0 - f, 1.8);
        double sheen = 0.84 + 0.16 * sin((x - y) * 0.006 + c->tau);
        double streak = mono_value_noise((x + y) * 0.18, (x - y) * 0.015, 505);
        double q = f * 34.0;
        double edge = exp(-(q * q));
        row[x] = mono_clamp_u8(10.0 + 26.0 * saw * sheen + 7.5 * streak + 24.0 * edge + mono_gauss_px(506, x, y, 1.1));
    }
}

static inline GrayImage design_05_diagonal(int w, int h, double phase) {
    GrayImage img = gray_new(w, h, 12);
    D05Ctx c = {&img, mono_dmax(40.0, mono_imin(w, h) / 7.5), phase, MONO_TAU * phase};
    mono_parallel_rows(h, d05_row, &c);
    mono_apply_vignette(&img, 0.45);
    return img;
}

// ── 06 · Horizon Mist ──────────────────────────────────────────────────────
typedef struct { GrayImage *img; double hy, tau; } D06Ctx;
static inline void d06_row(int y, void *p) {
    D06Ctx *c = (D06Ctx *)p;
    int w = c->img->w, h = c->img->h;
    uint8_t *row = c->img->px + (size_t)y * (size_t)w;
    double dy = (y - c->hy) / (double)h;
    double sky_or_sea = (y < c->hy)
        ? (12.0 + 52.0 * pow(1.0 - mono_dmin(1.0, (c->hy - y) / c->hy), 2.8))
        : (9.0 + 36.0 * exp(-dy * 9.5));
    double mist_amp = 56.0 + 8.0 * sin(c->tau);
    double qm = dy * 18.0;
    double mist_band = mist_amp * exp(-(qm * qm));
    for (int x = 0; x < w; ++x) {
        double nx = (double)x / mono_imax(1, w);
        double qh = (nx - 0.5) * 2.0;
        double horiz_falloff = 1.0 - 0.34 * (qh * qh);
        double drift = 0.85 + 0.15 * sin(nx * MONO_TAU * 2.0 - c->tau);
        double ripple = 0.0;
        if (y >= c->hy) {
            double n = mono_value_noise(x * 0.02, y * 0.08, 606);
            ripple = 5.2 * sin(y * 0.30 - c->tau * 2.0 + n * 6.0) * exp(-dy * 5.2);
        }
        row[x] = mono_clamp_u8((sky_or_sea + mist_band * horiz_falloff * drift) + ripple + mono_gauss_px(607, x, y, 1.1));
    }
}

static inline GrayImage design_06_horizon(int w, int h, double phase) {
    GrayImage img = gray_new(w, h, 0);
    D06Ctx c = {&img, h * 0.62, MONO_TAU * phase};
    mono_parallel_rows(h, d06_row, &c);
    return img;
}

// ── 07 · Solar Eclipse ─────────────────────────────────────────────────────
typedef struct {
    GrayImage *img;
    double cx, cy, r, lx, ly, breath, fx, fy, flare_scale, tau;
} D07Ctx;

static inline void d07_row(int y, void *p) {
    D07Ctx *c = (D07Ctx *)p;
    int w = c->img->w;
    uint8_t *row = c->img->px + (size_t)y * (size_t)w;
    for (int x = 0; x < w; ++x) {
        double dx = x - c->cx, dy = y - c->cy;
        double dist = hypot(dx, dy);
        if (dist < c->r * 0.995) { row[x] = 4; continue; }
        double nd = (dist - c->r) / c->r;
        double dir = (dx * c->lx + dy * c->ly) / mono_dmax(1.0, dist);
        double bias = 0.52 + 0.48 * mono_dmax(0.0, dir);
        double corona = (178.0 * exp(-nd * 3.35) * bias + 42.0 * exp(-nd * 0.95)) * c->breath;
        double qr = nd * 48.0;
        double rim = 238.0 * exp(-(qr * qr)) * (0.26 + 0.74 * pow(mono_dmax(0.0, (dir + 0.35) / 1.35), 2.4));
        double fdist = hypot(x - c->fx, y - c->fy) / c->flare_scale;
        double flare = (132.0 + 18.0 * sin(c->tau * 2.0)) * exp(-fdist * fdist * 1.7);
        row[x] = mono_clamp_u8((double)row[x] + corona + rim + flare + mono_gauss_px(708, x, y, 0.95));
    }
}

static inline GrayImage design_07_eclipse(int w, int h, double phase) {
    double k = mono_imin(w, h) / 1080.0;
    double cx = w * 0.56, cy = h * 0.46;
    double r = mono_imin(w, h) * 0.225;
    double tau = MONO_TAU * phase;

    GrayImage img = gray_new(w, h, 5);
    MonoMt19937 rng;
    mt_seed(&rng, 707);
    for (int i = 0; i < 220; ++i) {
        int sx = (int)(mt_next(&rng) % (uint32_t)w);
        int sy = (int)(mt_next(&rng) % (uint32_t)h);
        double sp = mt_unit(&rng) * MONO_TAU;
        int base_l = 24 + (int)(mt_next(&rng) % 90u);
        if (hypot(sx - cx, sy - cy) > r * 1.12) {
            gray_set_px(&img, sx, sy, mono_clamp_u8(base_l * (0.72 + 0.28 * sin(tau + sp))));
        }
    }

    double sun_ang = (-32.0 * (MONO_PI / 180.0)) + 0.44 * sin(tau);
    double lx = cos(sun_ang), ly = sin(sun_ang);
    D07Ctx c = {&img, cx, cy, r, lx, ly, 0.92 + 0.08 * cos(tau), cx + lx * r, cy + ly * r, mono_dmax(1.0, 28.0 * k), tau};
    mono_parallel_rows(h, d07_row, &c);
    return img;
}

// ── 08 · Alpine Ridgeline ──────────────────────────────────────────────────
typedef struct { GrayImage *img; double moon_x, moon_y, moon_r; } D08SkyCtx;
static inline void d08_sky_row(int y, void *p) {
    D08SkyCtx *c = (D08SkyCtx *)p;
    int w = c->img->w, h = c->img->h;
    uint8_t *row = c->img->px + (size_t)y * (size_t)w;
    double t = (double)y / mono_imax(1, h - 1);
    double base_sky = 14.0 + 46.0 * pow(t, 1.65);
    for (int x = 0; x < w; ++x) {
        double md = hypot(x - c->moon_x, (y - c->moon_y) * 1.35) / c->moon_r;
        row[x] = mono_clamp_u8(base_sky + 14.0 * exp(-md * md * 1.8));
    }
}

typedef struct {
    GrayImage *img;
    const double *crest;
    double depth, dir, tau;
    int layer;
    uint8_t ridge_col;
} D08LayerCtx;

static inline void d08_layer_row(int y, void *p) {
    D08LayerCtx *c = (D08LayerCtx *)p;
    int w = c->img->w, h = c->img->h;
    uint8_t *row = c->img->px + (size_t)y * (size_t)w;
    for (int x = 0; x < w; ++x) {
        double cy = c->crest[x];
        if (y >= cy) {
            double below = (y - cy) / mono_dmax(1.0, h * 0.14);
            double nx_fog = ((double)x / mono_imax(1, w)) * MONO_TAU * 2.0;
            double fog_wave = 0.72 + 0.28 * sin(nx_fog - c->dir * c->tau + c->layer * 1.1);
            double mist = (1.0 - c->depth) * 32.0 * fog_wave * exp(-below * 3.0);
            row[x] = mono_clamp_u8(c->ridge_col + mist);
        }
    }
}

static inline GrayImage design_08_ridgeline(int w, int h, double phase) {
    GrayImage img = gray_new(w, h, 0);
    double tau = MONO_TAU * phase;
    D08SkyCtx sc = {&img, w * (0.50 + 0.22 * sin(tau)), h * 0.20, mono_imin(w, h) * 0.42};
    mono_parallel_rows(h, d08_sky_row, &sc);

    double *crest = (double *)malloc((size_t)w * sizeof(double));
    if (!crest) abort();
    const int LAYERS = 7;
    for (int layer = 0; layer < LAYERS; ++layer) {
        double depth = (double)layer / (LAYERS - 1);
        double base_y = h * (0.34 + 0.48 * depth);
        double amp = h * (0.14 - 0.055 * depth);
        uint8_t ridge_col = mono_clamp_u8(54.0 * pow(1.0 - depth, 1.25) + 6.0);
        double dir = (layer % 2 == 0) ? 1.0 : -1.0;

        for (int x = 0; x < w; ++x) {
            double nx = ((double)x / w) * (3.2 + layer * 0.9);
            double n = mono_fbm2d(nx, layer * 7.3 + 1.1, 808 + (uint64_t)layer * 37ULL, 5);
            double jagged = fabs(mono_value_noise(nx * 4.5, layer * 3.1, 899) - 0.5) * 0.35;
            crest[x] = base_y - (n - 0.35 + jagged) * amp;
        }
        D08LayerCtx lc = {&img, crest, depth, dir, tau, layer, ridge_col};
        mono_parallel_rows(h, d08_layer_row, &lc);
    }
    free(crest);
    mono_add_grain(&img, 1.05, 818);
    return img;
}

// ── 09 · Brutalist Monoliths ───────────────────────────────────────────────
typedef struct { GrayImage *img; double sun_shift, tau; } D09Ctx;
static inline void d09_sky_row(int y, void *p) {
    D09Ctx *c = (D09Ctx *)p;
    int w = c->img->w, h = c->img->h;
    uint8_t *row = c->img->px + (size_t)y * (size_t)w;
    double t = (double)y / mono_imax(1, h - 1);
    double beam_x = w * (0.50 + 0.24 * c->sun_shift);
    for (int x = 0; x < w; ++x) {
        double bd = fabs(x - beam_x) / mono_dmax(1.0, w * 0.38);
        row[x] = mono_clamp_u8(10.0 + 18.0 * (1.0 - t) + 9.0 * exp(-bd * bd * 2.0) * (1.0 - t));
    }
}
static inline void d09_fog_row(int y, void *p) {
    D09Ctx *c = (D09Ctx *)p;
    int w = c->img->w, h = c->img->h;
    if (y < h * 0.56) return;
    double q = (y - h * 0.56) / (h * 0.44);
    double m = q * q;
    uint8_t *row = c->img->px + (size_t)y * (size_t)w;
    for (int x = 0; x < w; ++x) {
        double nx = (double)x / mono_imax(1, w);
        double wave = 0.80 + 0.20 * sin(nx * MONO_TAU * 2.0 - c->tau);
        double fog_lum = 12.0 + 14.0 * wave * (1.0 - m * 0.5);
        row[x] = mono_clamp_u8(row[x] * (1.0 - 0.68 * m) + fog_lum * m);
    }
}

static inline GrayImage design_09_monoliths(int w, int h, double phase) {
    GrayImage img = gray_new(w, h, 0);
    double tau = MONO_TAU * phase;
    D09Ctx c = {&img, sin(tau), tau};
    mono_parallel_rows(h, d09_sky_row, &c);

    static const struct { double cx, top_y, bw, skew; uint8_t front, side; } slabs[5] = {
        {0.23, 0.24, 0.11,  0.035, 34, 14},
        {0.76, 0.28, 0.13,  0.038, 30, 12},
        {0.40, 0.16, 0.12,  0.040, 48, 17},
        {0.61, 0.20, 0.10,  0.034, 42, 15},
        {0.50, 0.11, 0.115, 0.044, 68, 20},
    };

    double glint_y = h * (0.52 - 0.34 * cos(tau));
    double glint_span = mono_dmax(18.0, h * 0.14);

    for (size_t idx = 0; idx < 5; ++idx) {
        double x0 = (slabs[idx].cx - slabs[idx].bw * 0.5) * w;
        double x1 = (slabs[idx].cx + slabs[idx].bw * 0.5) * w;
        double x2 = x1 + slabs[idx].skew * w;
        double y0 = slabs[idx].top_y * h;
        double drop = slabs[idx].skew * h * 0.65;

        uint8_t front_l = mono_clamp_u8(slabs[idx].front + 10.0 * sin(tau + idx * 0.55));
        uint8_t side_l  = mono_clamp_u8(slabs[idx].side  -  3.0 * sin(tau + idx * 0.55));

        MonoPt pf[4] = {{x0, y0 + drop}, {x1, y0}, {x1, (double)h}, {x0, (double)h}};
        MonoPt ps[4] = {{x1, y0}, {x2, y0 + drop * 1.3}, {x2, (double)h}, {x1, (double)h}};
        gray_fill_polygon(&img, pf, 4, front_l);
        gray_fill_polygon(&img, ps, 4, side_l);
        gray_draw_line(&img, x1, y0, x1, h, mono_clamp_u8(front_l + 48.0), 1.3);

        double gy0 = mono_dmax(y0, glint_y - glint_span);
        double gy1 = mono_dmin((double)h, glint_y + glint_span);
        if (gy1 > gy0) gray_draw_line(&img, x1, gy0, x1, gy1, mono_clamp_u8(front_l + 108.0), 1.7);
    }

    mono_parallel_rows(h, d09_fog_row, &c);
    mono_apply_vignette(&img, 0.40);
    mono_add_grain(&img, 1.05, 909);
    return img;
}

// ── 10 · Caustic Silk Ribbon ───────────────────────────────────────────────
static inline GrayImage design_10_ribbon(int w, int h, double phase) {
    double k = mono_imin(w, h) / 1080.0;
    double tau = MONO_TAU * phase;
    GrayImage img = gray_new(w, h, 8);
    GrayImage accum = gray_new(w, h, 0);

    const int STRANDS = 76;
    int steps = mono_imax(240, w / 3);
    MonoPt *pts = (MonoPt *)malloc((size_t)(steps + 1) * sizeof(MonoPt));
    if (!pts) abort();

    for (int s = 0; s < STRANDS; ++s) {
        double u = (double)s / (STRANDS - 1);
        double env = sin(u * MONO_PI);
        uint8_t lum = mono_clamp_u8(18.0 + 185.0 * pow(env, 1.7));
        for (int i = 0; i <= steps; ++i) {
            double t = (double)i / steps;
            double x = t * w;
            double wave1 = sin(t * 5.2 + u * 2.35 - tau) * h * 0.19;
            double wave2 = cos(t * 8.4 - u * 3.80 + tau) * h * 0.085;
            double twist = (u - 0.5) * h * (0.28 * cos(t * 3.6 + 0.6 - tau));
            pts[i] = (MonoPt){x, h * 0.52 + wave1 + wave2 + twist};
        }
        GrayImage strand = gray_new(w, h, 0);
        gray_draw_polyline(&strand, pts, (size_t)(steps + 1), (uint8_t)(lum / 5), mono_dmax(1.0, 1.35 * k));
        gray_add_clamped(&accum, &strand);
        gray_free(&strand);
    }
    free(pts);

    GrayImage bloom = gray_gaussian_blur(&accum, 14.0 * k);
    size_t np = (size_t)w * (size_t)h;
    for (size_t i = 0; i < np; ++i) bloom.px[i] = (uint8_t)(bloom.px[i] * 0.45);
    gray_add_clamped(&img, &bloom);
    gray_add_clamped(&img, &accum);
    gray_free(&bloom);
    gray_free(&accum);

    mono_apply_vignette(&img, 0.35);
    mono_add_grain(&img, 0.9, 1010);
    return img;
}

// ── 11 · Gravitational Singularity ─────────────────────────────────────────
static inline GrayImage mono_singularity_disc(int diag, double r0, double tau, double k, bool front) {
    GrayImage layer = gray_new(diag, diag, 0);
    double dc = diag * 0.5;
    for (int ring = 0; ring < 28; ++ring) {
        double f = ring / 27.0;
        double rx = r0 * (1.14 + f * 2.12);
        double ry = rx * 0.15;
        double wave = sin(f * 13.0 - tau);
        double base = 242.0 * exp(-f * 1.38) * (0.62 + 0.38 * wave * wave);
        double start_deg = front ? 0.0 : 180.0;
        for (int seg = 0; seg < 16; ++seg) {
            double a0 = start_deg + seg * 11.25;
            double a1 = a0 + 11.15;
            double mid = ((a0 + a1) * 0.5) * (MONO_PI / 180.0);
            double doppler = (0.94 - 0.26 * cos(mid)) * (0.86 + 0.14 * cos(2.0 * mid - tau));
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

static inline GrayImage design_11_singularity(int w, int h, double phase) {
    double k = mono_imin(w, h) / 1080.0;
    double cx = w * 0.5, cy = h * 0.5;
    double r0 = mono_imin(w, h) * 0.165;
    double tau = MONO_TAU * phase;

    GrayImage img = gray_new(w, h, 4);
    MonoMt19937 rng;
    mt_seed(&rng, 1111);
    for (int i = 0; i < 240; ++i) {
        int sx = (int)(mt_next(&rng) % (uint32_t)w);
        int sy = (int)(mt_next(&rng) % (uint32_t)h);
        double sp = mt_unit(&rng) * MONO_TAU;
        int base_l = 28 + (int)(mt_next(&rng) % 110u);
        gray_set_px(&img, sx, sy, mono_clamp_u8(base_l * (0.72 + 0.28 * sin(tau + sp))));
    }

    GrayImage halo_raw = gray_new(w, h, 0);
    double breath = 1.0 + 0.05 * sin(tau);
    for (int i = 28; i >= 1; --i) {
        double t = i / 28.0;
        double rad = r0 * (1.0 + t * 1.48 * breath);
        uint8_t lum = mono_clamp_u8(210.0 * pow(1.0 - t, 2.35) * (0.93 + 0.07 * sin(tau)));
        gray_fill_ellipse(&halo_raw, cx, cy, rad, rad * 0.93, lum);
    }
    GrayImage halo = gray_gaussian_blur(&halo_raw, 18.0 * k);
    gray_free(&halo_raw);

    int diag = (int)ceil(hypot(w, h));
    if (diag % 2 != 0) ++diag;
    int ox = (diag - w) / 2, oy = (diag - h) / 2;

    GrayImage bg_full = mono_singularity_disc(diag, r0, tau, k, false);
    GrayImage bg = gray_crop(&bg_full, ox, oy, w, h);
    gray_free(&bg_full);

    GrayImage fg_full = mono_singularity_disc(diag, r0, tau, k, true);
    GrayImage fg = gray_crop(&fg_full, ox, oy, w, h);
    gray_free(&fg_full);

    gray_add_clamped(&img, &halo);
    gray_add_clamped(&img, &bg);
    gray_free(&halo);
    gray_free(&bg);

    GrayImage mask_raw = gray_new(w, h, 0);
    gray_fill_ellipse(&mask_raw, cx, cy, r0, r0, 255);
    GrayImage mask = gray_gaussian_blur(&mask_raw, 1.3 * k);
    gray_free(&mask_raw);

    GrayImage hole = gray_new(w, h, 2);
    GrayImage comp = gray_composite(&hole, &img, &mask);
    gray_free(&hole);
    gray_free(&mask);
    gray_free(&img);
    img = comp;

    uint8_t ring_lum = mono_clamp_u8(228.0 + 20.0 * sin(tau));
    gray_stroke_ellipse(&img, cx, cy, r0 * 1.012, r0 * 1.012, ring_lum, mono_dmax(1.2, 2.0 * k));
    gray_add_clamped(&img, &fg);
    gray_free(&fg);
    mono_add_grain(&img, 0.85, 1112);
    return img;
}

// ── 12 · Liquid Mercury ────────────────────────────────────────────────────
typedef struct {
    GrayImage *img;
    double inv, tau, cx_n, cy_n, eps, lx, ly, lz, hx, hy, hz;
} D12Ctx;

static inline double d12_height(const D12Ctx *c, double nx, double ny) {
    double wx = nx + 0.24 * sin(ny * 7.2 + c->tau) + 0.12 * cos((nx + ny) * 11.0 - c->tau);
    double wy = ny + 0.24 * cos(nx * 6.8 - c->tau) + 0.12 * sin((nx - ny) * 10.5 + c->tau);
    double r = hypot(wx - c->cx_n, wy - c->cy_n);
    return 0.55 * sin(r * 22.0 - 0.8 - c->tau * 2.0) * exp(-r * 1.4) +
           0.45 * mono_fbm2d(wx * 5.2, wy * 5.2, 1212, 4);
}

static inline void d12_row(int y, void *p) {
    D12Ctx *c = (D12Ctx *)p;
    int w = c->img->w;
    uint8_t *row = c->img->px + (size_t)y * (size_t)w;
    double ny = y * c->inv;
    for (int x = 0; x < w; ++x) {
        double nx = x * c->inv;
        double dhx = (d12_height(c, nx + c->eps, ny) - d12_height(c, nx - c->eps, ny)) / (2.0 * c->eps);
        double dhy = (d12_height(c, nx, ny + c->eps) - d12_height(c, nx, ny - c->eps)) / (2.0 * c->eps);
        double sx = -dhx * 0.42, sy = -dhy * 0.42, sz = 1.0;
        double slen = sqrt(sx * sx + sy * sy + sz * sz);
        sx /= slen; sy /= slen; sz /= slen;
        double ndotl = mono_dmax(0.0, sx * c->lx + sy * c->ly + sz * c->lz);
        double ndoth = mono_dmax(0.0, sx * c->hx + sy * c->hy + sz * c->hz);
        double qf = 1.0 - sz;
        double fresnel = qf * qf * qf;
        double env = 0.5 + 0.5 * sin((sx * 3.4 + sy * 2.2) * 3.14159 + c->tau);
        double lum = 8.0 + 38.0 * ndotl + 46.0 * env * (0.35 + 0.65 * fresnel) +
                     168.0 * pow(ndoth, 28.0) + mono_gauss_px(1213, x, y, 0.80);
        row[x] = mono_clamp_u8(lum);
    }
}

static inline GrayImage design_12_mercury(int w, int h, double phase) {
    GrayImage img = gray_new(w, h, 0);
    double inv = 1.0 / mono_imin(w, h);
    double tau = MONO_TAU * phase;
    double lx = -0.45 + 0.12 * sin(tau), ly = -0.55 + 0.10 * cos(tau), lz = 0.703;
    double llen = sqrt(lx * lx + ly * ly + lz * lz);
    lx /= llen; ly /= llen; lz /= llen;
    double hx = lx, hy = ly, hz = lz + 1.0;
    double hlen = sqrt(hx * hx + hy * hy + hz * hz);
    hx /= hlen; hy /= hlen; hz /= hlen;

    D12Ctx c = {&img, inv, tau, 0.52 + 0.04 * cos(tau), 0.48 + 0.04 * sin(tau), 2.0 * inv, lx, ly, lz, hx, hy, hz};
    mono_parallel_rows(h, d12_row, &c);
    mono_apply_vignette(&img, 0.38);
    return img;
}

// ── 13 · Rain & Bokeh Noir ─────────────────────────────────────────────────
static inline GrayImage design_13_rain_bokeh(int w, int h, double phase) {
    double k = mono_imin(w, h) / 1080.0;
    double tau = MONO_TAU * phase;
    GrayImage img = gray_new(w, h, 7);

    double sway = w * 0.025 * sin(tau);
    GrayImage shafts_raw = gray_new(w, h, 0);
    MonoPt s1[4] = {{w * 0.18, 0.0}, {w * 0.34, 0.0}, {w * 0.62 + sway, (double)h}, {w * 0.36 + sway, (double)h}};
    MonoPt s2[4] = {{w * 0.58, 0.0}, {w * 0.72, 0.0}, {w * 0.96 - sway, (double)h}, {w * 0.74 - sway, (double)h}};
    gray_fill_polygon(&shafts_raw, s1, 4, 18);
    gray_fill_polygon(&shafts_raw, s2, 4, 14);
    GrayImage shafts = gray_gaussian_blur(&shafts_raw, 36.0 * k);
    gray_free(&shafts_raw);
    gray_add_clamped(&img, &shafts);
    gray_free(&shafts);

    GrayImage bokeh_raw = gray_new(w, h, 0);
    MonoMt19937 rng;
    mt_seed(&rng, 1313);
    for (int i = 0; i < 34; ++i) {
        double bx0 = (0.06 + 0.88 * mt_unit(&rng)) * w;
        double by0 = (0.14 + 0.72 * mt_unit(&rng)) * h;
        double br0 = (14.0 + 54.0 * pow(mt_unit(&rng), 1.6)) * k;
        uint8_t fill_base = (uint8_t)(10 + (mt_next(&rng) % 28u));
        double bphase = i * 0.73;
        double bx = bx0 + cos(tau + bphase) * (9.0 * k);
        double by = by0 + sin(tau + bphase) * (6.0 * k);
        double br = br0 * (0.94 + 0.06 * sin(tau + bphase * 1.7));
        uint8_t fill_lum = mono_clamp_u8(fill_base * (0.82 + 0.28 * sin(tau + bphase)));
        GrayImage disc = gray_new(w, h, 0);
        gray_fill_ellipse(&disc, bx, by, br, br, fill_lum);
        gray_stroke_ellipse(&disc, bx, by, br, br, mono_clamp_u8(fill_lum + 18), mono_dmax(1.2, 2.0 * k));
        gray_add_clamped(&bokeh_raw, &disc);
        gray_free(&disc);
    }
    GrayImage bokeh = gray_gaussian_blur(&bokeh_raw, 4.5 * k);
    gray_free(&bokeh_raw);
    gray_add_clamped(&img, &bokeh);
    gray_free(&bokeh);

    GrayImage rain = gray_new(w, h, 0);
    double dx_unit = sin(-11.0 * MONO_PI / 180.0);
    double dy_unit = cos(-11.0 * MONO_PI / 180.0);
    double span_y = h + 140.0 * k;
    double travel_x = dx_unit * (span_y / dy_unit);
    for (int i = 0; i < 190; ++i) {
        double rx0 = mt_unit(&rng) * w;
        double p0  = mt_unit(&rng);
        double len = (24.0 + (mt_next(&rng) % 85u)) * k;
        uint8_t col = (uint8_t)(26 + (mt_next(&rng) % 72u));
        int speed = (i % 3 == 0) ? 2 : 1;
        double prog = fmod(p0 + phase * speed, 1.0);
        double ry = -70.0 * k + prog * span_y;
        double rx = fmod(rx0 + prog * travel_x + w * 4.0, (double)w);
        gray_draw_line(&rain, rx, ry, rx + dx_unit * len, ry + dy_unit * len, col, mono_dmax(1.0, 1.15 * k));
    }
    gray_add_clamped(&img, &rain);
    gray_free(&rain);

    mono_apply_vignette(&img, 0.42);
    mono_add_grain(&img, 1.0, 1314);
    return img;
}

// ── 14 · Bauhaus Compass ───────────────────────────────────────────────────
static inline GrayImage design_14_bauhaus(int w, int h, double phase) {
    double k = mono_imin(w, h) / 1080.0;
    double cx = w * 0.5, cy = h * 0.5;
    double r = mono_imin(w, h) * 0.28;
    double tau = MONO_TAU * phase;

    GrayImage img = gray_new(w, h, 10);
    int step = mono_imax(24, (int)round(64.0 * k));
    for (int x = (int)cx % step; x < w; x += step) gray_draw_line(&img, x, 0, x, h - 1, 15, 1.0);
    for (int y = (int)cy % step; y < h; y += step) gray_draw_line(&img, 0, y, w - 1, y, 15, 1.0);
    gray_draw_line(&img, w * 0.06, cy, w * 0.94, cy, 34, 1.0);
    gray_draw_line(&img, cx, h * 0.06, cx, h * 0.94, 34, 1.0);
    gray_draw_line(&img, cx - r * 1.45, cy - r * 1.45, cx + r * 1.45, cy + r * 1.45, 24, 1.0);

    double phi = 1.61803398875;
    gray_stroke_rect(&img, (int)(cx - r * phi * 0.78), (int)(cy - r * 0.78),
                     (int)(cx + r * phi * 0.78), (int)(cy + r * 0.78), 32, 1);

    gray_stroke_ellipse(&img, cx, cy, r * 1.28, r * 1.28, 46, mono_dmax(1.0, 1.3 * k));
    gray_stroke_ellipse(&img, cx, cy, r * 0.98, r * 0.98, 72, mono_dmax(1.0, 1.5 * k));

    double arc_off = 360.0 * phase;
    gray_stroke_arc(&img, cx - r * 0.38, cy, r * 1.15, r * 1.15, -62.0 + arc_off, 62.0 + arc_off, 68, mono_dmax(1.0, 1.4 * k));

    double dial_deg_off = 30.0 * phase;
    for (int deg = 0; deg < 360; deg += 5) {
        double rad = (deg + dial_deg_off) * (MONO_PI / 180.0);
        bool major = (deg % 30 == 0);
        double r_in = r * 1.28 - (major ? 8.0 * k : 3.5 * k);
        double r_out = r * 1.28 + (major ? 8.0 * k : 3.5 * k);
        gray_draw_line(&img, cx + cos(rad) * r_in, cy + sin(rad) * r_in,
                       cx + cos(rad) * r_out, cy + sin(rad) * r_out, major ? 96 : 42, 1.0);
    }

    double scx = cx - r * 0.22, scy = cy - r * 0.04, sr = r * 0.56;
    int sri = (int)ceil(sr);
    double light_shift = 0.09 * sin(tau);
    for (int dy = -sri; dy <= sri; ++dy) {
        double span = sqrt(mono_dmax(0.0, sr * sr - dy * dy));
        double t = (dy + sr) / (2.0 * sr);
        uint8_t val = mono_clamp_u8(16.0 + 214.0 * pow(mono_dclamp(1.0 - t + light_shift, 0.0, 1.0), 1.35));
        if (dy > 0 && ((int)(dy / mono_dmax(1.0, 4.5 * k)) % 2 == 0)) continue;
        gray_draw_line(&img, scx - span, scy + dy, scx + span, scy + dy, val, 1.0);
    }
    gray_stroke_ellipse(&img, scx, scy, sr, sr, 168, mono_dmax(1.0, 1.4 * k));

    double orbit_ang = -0.55 + tau;
    double ncx = cx + cos(orbit_ang) * (r * 0.98);
    double ncy = cy + sin(orbit_ang) * (r * 0.98);
    gray_draw_line(&img, cx, cy, ncx, ncy, 44, 1.0);
    gray_fill_ellipse(&img, ncx, ncy, 15.0 * k, 15.0 * k, 12);
    gray_stroke_ellipse(&img, ncx, ncy, 15.0 * k, 15.0 * k, 224, mono_dmax(1.0, 1.6 * k));
    gray_fill_ellipse(&img, ncx, ncy, 4.4 * k, 4.4 * k, 248);

    mono_apply_vignette(&img, 0.35);
    mono_add_grain(&img, 0.95, 1414);
    return img;
}

typedef struct {
    const char *stem;
    const char *title;
    const char *subtitle;
    GrayImage (*fn)(int w, int h, double phase);
} MonoDesignSpec;

static const MonoDesignSpec MONO_DESIGNS[14] = {
    {"mono-01", "Noir Vignette",             "Orbiting studio key-light & breathing aperture", design_01_vignette},
    {"mono-02", "Architectural Grid",        "Sub-pixel drafting grid & telemetry scanlines",  design_02_grid},
    {"mono-03", "Concentric Arcs",           "Propagating off-axis interference wavefronts",   design_03_arcs},
    {"mono-04", "Topographic Contours",      "Harmonic flowing elevation isolines",            design_04_contours},
    {"mono-05", "Diagonal Strata",           "Gliding 45-degree brushed graphite bands",       design_05_diagonal},
    {"mono-06", "Horizon Mist",              "Drifting sea fog & rolling silver ripples",      design_06_horizon},
    {"mono-07", "Solar Eclipse",             "Orbiting diamond-ring flare & corona",           design_07_eclipse},
    {"mono-08", "Alpine Ridgeline",          "7-layer peaks & drifting valley mist",           design_08_ridgeline},
    {"mono-09", "Brutalist Monoliths",       "Orbiting raking light & arris glint",            design_09_monoliths},
    {"mono-10", "Caustic Silk Ribbon",       "76-strand undulating harmonic silver wave",      design_10_ribbon},
    {"mono-11", "Gravitational Singularity", "Spiralling relativistic accretion & photon ring",design_11_singularity},
    {"mono-12", "Liquid Mercury",            "Propagating Blinn-Phong ferrofluid chrome",      design_12_mercury},
    {"mono-13", "Rain & Bokeh Noir",         "Falling silver rain & breathing optical bokeh",  design_13_rain_bokeh},
    {"mono-14", "Bauhaus Compass",           "Rotating vernier dial & orbiting golden node",   design_14_bauhaus},
};

static inline const MonoDesignSpec *mono_find_design(const char *query) {
    if (!query || query[0] == '\0') return NULL;
    bool all_digits = true;
    for (size_t i = 0; query[i]; ++i) {
        if (query[i] < '0' || query[i] > '9') { all_digits = false; break; }
    }
    if (all_digits) {
        int idx = atoi(query);
        if (idx >= 1 && idx <= 14) return &MONO_DESIGNS[idx - 1];
        return NULL;
    }
    for (size_t i = 0; i < 14; ++i) {
        if (strcmp(MONO_DESIGNS[i].stem, query) == 0 || strstr(MONO_DESIGNS[i].stem, query) != NULL) {
            return &MONO_DESIGNS[i];
        }
    }
    return NULL;
}

#endif
