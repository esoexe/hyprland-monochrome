#ifndef MONO_IMAGE_H
#define MONO_IMAGE_H
// ═══════════════════════════════════════════════════════════════════════════
//  include/mono_image.h — Zero-dependency Pure C11 greyscale imaging engine
//
//  Provides:
//    - GrayImage: 8-bit pure monochrome canvas, AA drawing primitives,
//      3-pass sliding-window Gaussian blur, bilinear rotation,
//      Lanczos-3 resampling, alpha compositing, clamped additive blend.
//    - ITU-T T.81 Baseline Grayscale JPEG encoder + SOF dimension reader.
//    - GIF89a 64-grey Bayer-dithered looping LZW encoder + frame counter.
//    - RFC 4648 Base64 encoder for inline HTML data URIs.
// ═══════════════════════════════════════════════════════════════════════════

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef MONO_PI
#define MONO_PI 3.14159265358979323846264338327950288
#endif

typedef struct {
    double x;
    double y;
} MonoPt;

typedef struct {
    int w;
    int h;
    uint8_t *px;
} GrayImage;

typedef struct {
    uint8_t *data;
    size_t len;
    size_t cap;
} MonoByteBuf;

static inline int mono_imin(int a, int b) { return a < b ? a : b; }
static inline int mono_imax(int a, int b) { return a > b ? a : b; }
static inline int mono_iclamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline double mono_dmin(double a, double b) { return a < b ? a : b; }
static inline double mono_dmax(double a, double b) { return a > b ? a : b; }
static inline double mono_dclamp(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

static inline uint8_t mono_clamp_u8(double v) {
    if (v <= 0.0) return 0;
    if (v >= 255.0) return 255;
    return (uint8_t)(v + 0.5);
}

static inline void mbuf_init(MonoByteBuf *b, size_t initial_cap) {
    b->len = 0;
    b->cap = initial_cap;
    b->data = initial_cap ? (uint8_t *)malloc(initial_cap) : NULL;
}

static inline void mbuf_free(MonoByteBuf *b) {
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static inline void mbuf_reserve(MonoByteBuf *b, size_t need) {
    if (need <= b->cap) return;
    size_t ncap = b->cap ? b->cap * 2 : 1024;
    while (ncap < need) ncap *= 2;
    uint8_t *nd = (uint8_t *)realloc(b->data, ncap);
    if (!nd) abort();
    b->data = nd;
    b->cap = ncap;
}

static inline void mbuf_push(MonoByteBuf *b, uint8_t v) {
    if (b->len + 1 > b->cap) mbuf_reserve(b, b->len + 1);
    b->data[b->len++] = v;
}

static inline void mbuf_append(MonoByteBuf *b, const void *src, size_t n) {
    if (n == 0) return;
    if (b->len + n > b->cap) mbuf_reserve(b, b->len + n);
    memcpy(b->data + b->len, src, n);
    b->len += n;
}

static inline void mono_ensure_parent_dirs(const char *path) {
    char tmp[4096];
    int n = snprintf(tmp, sizeof(tmp), "%s", path);
    if (n <= 0 || (size_t)n >= sizeof(tmp)) return;
    char *slash = strrchr(tmp, '/');
    if (!slash || slash == tmp) return;
    *slash = '\0';
    for (char *p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

// ── GrayImage Lifecycle & Pixel Operations ─────────────────────────────────

static inline GrayImage gray_new(int w, int h, uint8_t fill) {
    GrayImage img;
    img.w = w;
    img.h = h;
    size_t total = (w > 0 && h > 0) ? ((size_t)w * (size_t)h) : 0;
    img.px = total ? (uint8_t *)malloc(total) : NULL;
    if (total && !img.px) abort();
    if (total) memset(img.px, fill, total);
    return img;
}

static inline void gray_free(GrayImage *img) {
    if (!img) return;
    free(img->px);
    img->px = NULL;
    img->w = 0;
    img->h = 0;
}

static inline GrayImage gray_clone(const GrayImage *src) {
    GrayImage out = gray_new(src->w, src->h, 0);
    size_t total = (size_t)src->w * (size_t)src->h;
    if (total && src->px) memcpy(out.px, src->px, total);
    return out;
}

static inline uint8_t gray_get_clamped(const GrayImage *img, int x, int y) {
    if (img->w <= 0 || img->h <= 0 || !img->px) return 0;
    x = mono_iclamp(x, 0, img->w - 1);
    y = mono_iclamp(y, 0, img->h - 1);
    return img->px[(size_t)y * (size_t)img->w + (size_t)x];
}

static inline void gray_set_px(GrayImage *img, int x, int y, uint8_t v) {
    if (x < 0 || x >= img->w || y < 0 || y >= img->h) return;
    img->px[(size_t)y * (size_t)img->w + (size_t)x] = v;
}

static inline void gray_blend_px(GrayImage *img, int x, int y, uint8_t col, double alpha) {
    if (x < 0 || x >= img->w || y < 0 || y >= img->h || alpha <= 0.0) return;
    size_t idx = (size_t)y * (size_t)img->w + (size_t)x;
    if (alpha >= 1.0) {
        img->px[idx] = col;
        return;
    }
    double cur = (double)img->px[idx];
    img->px[idx] = mono_clamp_u8(cur + ((double)col - cur) * alpha);
}

static inline double gray_sample_bilinear(const GrayImage *img, double x, double y) {
    int x0 = (int)floor(x);
    int y0 = (int)floor(y);
    double fx = x - x0;
    double fy = y - y0;
    double p00 = gray_get_clamped(img, x0, y0);
    double p10 = gray_get_clamped(img, x0 + 1, y0);
    double p01 = gray_get_clamped(img, x0, y0 + 1);
    double p11 = gray_get_clamped(img, x0 + 1, y0 + 1);
    double top = p00 + (p10 - p00) * fx;
    double bot = p01 + (p11 - p01) * fx;
    return top + (bot - top) * fy;
}

static inline void gray_add_clamped(GrayImage *dst, const GrayImage *other) {
    size_t n1 = (size_t)dst->w * (size_t)dst->h;
    size_t n2 = (size_t)other->w * (size_t)other->h;
    size_t n = n1 < n2 ? n1 : n2;
    for (size_t i = 0; i < n; ++i) {
        int s = (int)dst->px[i] + (int)other->px[i];
        dst->px[i] = (uint8_t)(s > 255 ? 255 : s);
    }
}

static inline GrayImage gray_composite(const GrayImage *fg, const GrayImage *bg, const GrayImage *mask) {
    GrayImage out = gray_new(bg->w, bg->h, 0);
    size_t n = (size_t)out.w * (size_t)out.h;
    for (size_t i = 0; i < n; ++i) {
        uint32_t a = mask->px[i];
        uint32_t f = fg->px[i];
        uint32_t b = bg->px[i];
        out.px[i] = (uint8_t)((f * a + b * (255u - a) + 127u) / 255u);
    }
    return out;
}

static inline GrayImage gray_crop(const GrayImage *img, int x0, int y0, int cw, int ch) {
    GrayImage out = gray_new(cw, ch, 0);
    for (int y = 0; y < ch; ++y) {
        for (int x = 0; x < cw; ++x) {
            int sx = x0 + x;
            int sy = y0 + y;
            if (sx >= 0 && sx < img->w && sy >= 0 && sy < img->h) {
                out.px[(size_t)y * (size_t)cw + (size_t)x] =
                    img->px[(size_t)sy * (size_t)img->w + (size_t)sx];
            }
        }
    }
    return out;
}

static inline GrayImage gray_rotate_deg(const GrayImage *img, double deg_ccw) {
    GrayImage out = gray_new(img->w, img->h, 0);
    double rad = deg_ccw * (MONO_PI / 180.0);
    double c = cos(rad);
    double s = sin(rad);
    double cx = (img->w - 1) * 0.5;
    double cy = (img->h - 1) * 0.5;
    for (int y = 0; y < img->h; ++y) {
        double dy = y - cy;
        for (int x = 0; x < img->w; ++x) {
            double dx = x - cx;
            double sx = cx + dx * c + dy * s;
            double sy = cy - dx * s + dy * c;
            if (sx >= -0.5 && sx <= img->w - 0.5 && sy >= -0.5 && sy <= img->h - 0.5) {
                out.px[(size_t)y * (size_t)img->w + (size_t)x] =
                    mono_clamp_u8(gray_sample_bilinear(img, sx, sy));
            }
        }
    }
    return out;
}

static inline void gray_box_h(const uint8_t *in, uint8_t *out, int w, int h, int r) {
    size_t W = (size_t)w;
    size_t H = (size_t)h;
    if (r <= 0) {
        memcpy(out, in, W * H);
        return;
    }
    int diam = 2 * r + 1;
    for (size_t y = 0; y < H; ++y) {
        const uint8_t *row = in + y * W;
        uint8_t *dst = out + y * W;
        int acc = (r + 1) * (int)row[0];
        for (int x = 1; x <= r; ++x) {
            acc += (int)row[mono_imin(x, w - 1)];
        }
        for (int x = 0; x < w; ++x) {
            dst[x] = (uint8_t)((acc + diam / 2) / diam);
            int add_idx = mono_imin(x + r + 1, w - 1);
            int sub_idx = mono_imax(x - r, 0);
            acc += (int)row[add_idx] - (int)row[sub_idx];
        }
    }
}

static inline void gray_box_v(const uint8_t *in, uint8_t *out, int w, int h, int r) {
    size_t W = (size_t)w;
    size_t H = (size_t)h;
    if (r <= 0) {
        memcpy(out, in, W * H);
        return;
    }
    int diam = 2 * r + 1;
    for (int x = 0; x < w; ++x) {
        int acc = (r + 1) * (int)in[(size_t)x];
        for (int y = 1; y <= r; ++y) {
            int cy = mono_imin(y, h - 1);
            acc += (int)in[(size_t)cy * W + (size_t)x];
        }
        for (int y = 0; y < h; ++y) {
            out[(size_t)y * W + (size_t)x] = (uint8_t)((acc + diam / 2) / diam);
            int add_y = mono_imin(y + r + 1, h - 1);
            int sub_y = mono_imax(y - r, 0);
            acc += (int)in[(size_t)add_y * W + (size_t)x] - (int)in[(size_t)sub_y * W + (size_t)x];
        }
    }
}

static inline GrayImage gray_gaussian_blur(const GrayImage *img, double sigma) {
    if (sigma <= 0.25 || img->w <= 1 || img->h <= 1) return gray_clone(img);
    double w_ideal = sqrt((12.0 * sigma * sigma / 3.0) + 1.0);
    int wl = (int)floor(w_ideal);
    if (wl % 2 == 0) --wl;
    if (wl < 1) wl = 1;
    int wu = wl + 2;
    double m_ideal = (12.0 * sigma * sigma - 3.0 * wl * wl - 12.0 * wl - 9.0) / (-4.0 * wl - 4.0);
    int m = mono_iclamp((int)round(m_ideal), 0, 3);

    size_t total = (size_t)img->w * (size_t)img->h;
    GrayImage res = gray_clone(img);
    uint8_t *tmp = (uint8_t *)malloc(total);
    if (!tmp) abort();

    for (int pass = 0; pass < 3; ++pass) {
        int bw = (pass < m) ? wl : wu;
        int r = (bw - 1) / 2;
        gray_box_h(res.px, tmp, img->w, img->h, r);
        gray_box_v(tmp, res.px, img->w, img->h, r);
    }
    free(tmp);
    return res;
}

// ── Drawing Primitives ─────────────────────────────────────────────────────

static inline void gray_fill_rect(GrayImage *img, int x0, int y0, int x1, int y1, uint8_t col) {
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    x0 = mono_imax(0, x0);
    y0 = mono_imax(0, y0);
    x1 = mono_imin(img->w - 1, x1);
    y1 = mono_imin(img->h - 1, y1);
    for (int y = y0; y <= y1; ++y) {
        uint8_t *row = img->px + (size_t)y * (size_t)img->w;
        for (int x = x0; x <= x1; ++x) row[x] = col;
    }
}

static inline void gray_stroke_rect(GrayImage *img, int x0, int y0, int x1, int y1, uint8_t col, int width) {
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    int t = mono_imax(1, width);
    gray_fill_rect(img, x0, y0, x1, y0 + t - 1, col);
    gray_fill_rect(img, x0, y1 - t + 1, x1, y1, col);
    gray_fill_rect(img, x0, y0, x0 + t - 1, y1, col);
    gray_fill_rect(img, x1 - t + 1, y0, x1, y1, col);
}

static inline void gray_draw_line(GrayImage *img, double x0, double y0, double x1, double y1, uint8_t col, double width) {
    double r = mono_dmax(0.5, width * 0.5);
    int bx0 = mono_imax(0, (int)floor(mono_dmin(x0, x1) - r - 1.0));
    int bx1 = mono_imin(img->w - 1, (int)ceil(mono_dmax(x0, x1) + r + 1.0));
    int by0 = mono_imax(0, (int)floor(mono_dmin(y0, y1) - r - 1.0));
    int by1 = mono_imin(img->h - 1, (int)ceil(mono_dmax(y0, y1) + r + 1.0));

    double vx = x1 - x0;
    double vy = y1 - y0;
    double len2 = vx * vx + vy * vy;

    for (int y = by0; y <= by1; ++y) {
        for (int x = bx0; x <= bx1; ++x) {
            double t = (len2 > 1e-9) ? mono_dclamp(((x - x0) * vx + (y - y0) * vy) / len2, 0.0, 1.0) : 0.0;
            double dx = x - (x0 + t * vx);
            double dy = y - (y0 + t * vy);
            double dist = sqrt(dx * dx + dy * dy);
            double cov = mono_dclamp(r + 0.5 - dist, 0.0, 1.0);
            if (cov > 0.0) gray_blend_px(img, x, y, col, cov);
        }
    }
}

static inline void gray_draw_polyline(GrayImage *img, const MonoPt *pts, size_t npts, uint8_t col, double width) {
    if (npts < 2) return;
    double r = mono_dmax(0.5, width * 0.5);
    double min_x = pts[0].x, max_x = pts[0].x;
    double min_y = pts[0].y, max_y = pts[0].y;
    for (size_t i = 1; i < npts; ++i) {
        min_x = mono_dmin(min_x, pts[i].x);
        max_x = mono_dmax(max_x, pts[i].x);
        min_y = mono_dmin(min_y, pts[i].y);
        max_y = mono_dmax(max_y, pts[i].y);
    }
    int bx0 = mono_imax(0, (int)floor(min_x - r - 1.0));
    int bx1 = mono_imin(img->w - 1, (int)ceil(max_x + r + 1.0));
    int by0 = mono_imax(0, (int)floor(min_y - r - 1.0));
    int by1 = mono_imin(img->h - 1, (int)ceil(max_y + r + 1.0));
    if (bx0 > bx1 || by0 > by1) return;

    int bw = bx1 - bx0 + 1;
    int bh = by1 - by0 + 1;
    float *cov_buf = (float *)calloc((size_t)bw * (size_t)bh, sizeof(float));
    if (!cov_buf) abort();

    for (size_t i = 1; i < npts; ++i) {
        double x0 = pts[i - 1].x, y0 = pts[i - 1].y;
        double x1 = pts[i].x, y1 = pts[i].y;
        int sx0 = mono_imax(bx0, (int)floor(mono_dmin(x0, x1) - r - 1.0));
        int sx1 = mono_imin(bx1, (int)ceil(mono_dmax(x0, x1) + r + 1.0));
        int sy0 = mono_imax(by0, (int)floor(mono_dmin(y0, y1) - r - 1.0));
        int sy1 = mono_imin(by1, (int)ceil(mono_dmax(y0, y1) + r + 1.0));
        double vx = x1 - x0, vy = y1 - y0;
        double len2 = vx * vx + vy * vy;
        for (int y = sy0; y <= sy1; ++y) {
            for (int x = sx0; x <= sx1; ++x) {
                double t = (len2 > 1e-9) ? mono_dclamp(((x - x0) * vx + (y - y0) * vy) / len2, 0.0, 1.0) : 0.0;
                double dx = x - (x0 + t * vx);
                double dy = y - (y0 + t * vy);
                float cov = (float)mono_dclamp(r + 0.5 - sqrt(dx * dx + dy * dy), 0.0, 1.0);
                if (cov > 0.0f) {
                    size_t cidx = (size_t)(y - by0) * (size_t)bw + (size_t)(x - bx0);
                    if (cov > cov_buf[cidx]) cov_buf[cidx] = cov;
                }
            }
        }
    }

    for (int y = by0; y <= by1; ++y) {
        for (int x = bx0; x <= bx1; ++x) {
            float cov = cov_buf[(size_t)(y - by0) * (size_t)bw + (size_t)(x - bx0)];
            if (cov > 0.0f) gray_blend_px(img, x, y, col, (double)cov);
        }
    }
    free(cov_buf);
}

static inline void gray_fill_ellipse(GrayImage *img, double cx, double cy, double rx, double ry, uint8_t col) {
    if (rx <= 0.0 || ry <= 0.0) return;
    int x0 = mono_imax(0, (int)floor(cx - rx - 1.0));
    int x1 = mono_imin(img->w - 1, (int)ceil(cx + rx + 1.0));
    int y0 = mono_imax(0, (int)floor(cy - ry - 1.0));
    int y1 = mono_imin(img->h - 1, (int)ceil(cy + ry + 1.0));
    double avg_r = 0.5 * (rx + ry);
    for (int y = y0; y <= y1; ++y) {
        double ny = (y - cy) / ry;
        for (int x = x0; x <= x1; ++x) {
            double nx = (x - cx) / rx;
            double d = sqrt(nx * nx + ny * ny);
            double px_dist = (1.0 - d) * avg_r;
            double cov = mono_dclamp(px_dist + 0.5, 0.0, 1.0);
            if (cov > 0.0) gray_blend_px(img, x, y, col, cov);
        }
    }
}

static inline void gray_stroke_ellipse(GrayImage *img, double cx, double cy, double rx, double ry, uint8_t col, double width) {
    if (rx <= 0.0 || ry <= 0.0) return;
    double hw = mono_dmax(0.5, width * 0.5);
    int x0 = mono_imax(0, (int)floor(cx - rx - hw - 2.0));
    int x1 = mono_imin(img->w - 1, (int)ceil(cx + rx + hw + 2.0));
    int y0 = mono_imax(0, (int)floor(cy - ry - hw - 2.0));
    int y1 = mono_imin(img->h - 1, (int)ceil(cy + ry + hw + 2.0));
    for (int y = y0; y <= y1; ++y) {
        double dy = y - cy;
        for (int x = x0; x <= x1; ++x) {
            double dx = x - cx;
            double nx = dx / rx;
            double ny = dy / ry;
            double f = sqrt(nx * nx + ny * ny);
            if (f < 1e-6) continue;
            double gx = nx / rx;
            double gy = ny / ry;
            double gnorm = sqrt(gx * gx + gy * gy) * f;
            double dist = fabs(f - 1.0) / mono_dmax(1e-6, gnorm);
            double cov = mono_dclamp(hw + 0.5 - dist, 0.0, 1.0);
            if (cov > 0.0) gray_blend_px(img, x, y, col, cov);
        }
    }
}

static inline void gray_stroke_arc(GrayImage *img, double cx, double cy, double rx, double ry,
                                   double start_deg, double end_deg, uint8_t col, double width) {
    if (rx <= 0.0 || ry <= 0.0) return;
    while (end_deg < start_deg) end_deg += 360.0;
    double span_deg = end_deg - start_deg;
    int steps = mono_imax(12, (int)ceil(span_deg * mono_dmax(rx, ry) * 0.035));
    MonoPt *pts = (MonoPt *)malloc((size_t)(steps + 1) * sizeof(MonoPt));
    if (!pts) abort();
    for (int i = 0; i <= steps; ++i) {
        double deg = start_deg + span_deg * ((double)i / steps);
        double rad = deg * (MONO_PI / 180.0);
        pts[i].x = cx + cos(rad) * rx;
        pts[i].y = cy + sin(rad) * ry;
    }
    gray_draw_polyline(img, pts, (size_t)(steps + 1), col, width);
    free(pts);
}

static inline int mono_cmp_double(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    return (da < db) ? -1 : (da > db ? 1 : 0);
}

static inline void gray_fill_polygon(GrayImage *img, const MonoPt *poly, size_t npts, uint8_t col) {
    if (npts < 3) return;
    double min_y = poly[0].y, max_y = poly[0].y;
    for (size_t i = 1; i < npts; ++i) {
        min_y = mono_dmin(min_y, poly[i].y);
        max_y = mono_dmax(max_y, poly[i].y);
    }
    int y0 = mono_imax(0, (int)floor(min_y));
    int y1 = mono_imin(img->h - 1, (int)ceil(max_y));
    double *nodes = (double *)malloc(npts * sizeof(double));
    if (!nodes) abort();

    for (int y = y0; y <= y1; ++y) {
        double sy = y + 0.5;
        size_t nn = 0;
        size_t j = npts - 1;
        for (size_t i = 0; i < npts; ++i) {
            double yi = poly[i].y, yj = poly[j].y;
            if ((yi < sy && yj >= sy) || (yj < sy && yi >= sy)) {
                double xi = poly[i].x, xj = poly[j].x;
                nodes[nn++] = xi + (sy - yi) / (yj - yi) * (xj - xi);
            }
            j = i;
        }
        qsort(nodes, nn, sizeof(double), mono_cmp_double);
        for (size_t i = 0; i + 1 < nn; i += 2) {
            int x_start = mono_imax(0, (int)floor(nodes[i]));
            int x_end = mono_imin(img->w - 1, (int)ceil(nodes[i + 1]));
            for (int x = x_start; x <= x_end; ++x) {
                double left = mono_dmax((double)x, nodes[i]);
                double right = mono_dmin((double)(x + 1), nodes[i + 1]);
                if (right > left) gray_blend_px(img, x, y, col, right - left);
            }
        }
    }
    free(nodes);
}

// ── ITU-T T.81 Baseline Grayscale JPEG Encoder + SOF Reader ───────────────

typedef struct { uint16_t code; uint8_t len; } MonoHuffCode;

typedef struct {
    MonoByteBuf *buf;
    uint32_t acc;
    int bits;
} MonoJpegBitWriter;

static inline void jbw_put_bits(MonoJpegBitWriter *bw, uint16_t val, int nbits) {
    bw->acc = (bw->acc << nbits) | ((uint32_t)val & ((1u << nbits) - 1u));
    bw->bits += nbits;
    while (bw->bits >= 8) {
        bw->bits -= 8;
        uint8_t b = (uint8_t)((bw->acc >> bw->bits) & 0xFFu);
        mbuf_push(bw->buf, b);
        if (b == 0xFFu) mbuf_push(bw->buf, 0x00u);
    }
}

static inline void jbw_flush(MonoJpegBitWriter *bw) {
    if (bw->bits > 0) {
        jbw_put_bits(bw, 0x7Fu, 8 - bw->bits);
        bw->bits = 0;
        bw->acc = 0;
    }
}

static inline void mono_build_huff(const uint8_t bits[16], const uint8_t *vals, MonoHuffCode out[256]) {
    memset(out, 0, sizeof(MonoHuffCode) * 256);
    uint16_t code = 0;
    size_t idx = 0;
    for (int len = 1; len <= 16; ++len) {
        for (int i = 0; i < bits[len - 1]; ++i) {
            uint8_t sym = vals[idx++];
            out[sym].code = code;
            out[sym].len = (uint8_t)len;
            ++code;
        }
        code <<= 1;
    }
}

static inline int mono_bit_category(int v, uint16_t *mag_bits) {
    if (v == 0) { *mag_bits = 0; return 0; }
    int av = abs(v);
    int cat = 0;
    while ((1 << cat) <= av) ++cat;
    *mag_bits = (uint16_t)((v > 0) ? v : ((1 << cat) - 1 + v));
    return cat;
}

static inline MonoByteBuf mono_encode_jpeg_mem(const GrayImage *img, int quality) {
    static const uint8_t STD_LUMA_QUANT[64] = {
        16, 11, 10, 16,  24,  40,  51,  61,
        12, 12, 14, 19,  26,  58,  60,  55,
        14, 13, 16, 24,  40,  57,  69,  56,
        14, 17, 22, 29,  51,  87,  80,  62,
        18, 22, 37, 56,  68, 109, 103,  77,
        24, 35, 55, 64,  81, 104, 113,  92,
        49, 64, 78, 87, 103, 121, 120, 101,
        72, 92, 95, 98, 112, 100, 103,  99
    };
    static const uint8_t ZIGZAG[64] = {
         0,  1,  8, 16,  9,  2,  3, 10,
        17, 24, 32, 25, 18, 11,  4,  5,
        12, 19, 26, 33, 40, 48, 41, 34,
        27, 20, 13,  6,  7, 14, 21, 28,
        35, 42, 49, 56, 57, 50, 43, 36,
        29, 22, 15, 23, 30, 37, 44, 51,
        58, 59, 52, 45, 38, 31, 39, 46,
        53, 60, 61, 54, 47, 55, 62, 63
    };
    static const uint8_t BITS_DC_LUMA[16] = {0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0};
    static const uint8_t VAL_DC_LUMA[12] = {0,1,2,3,4,5,6,7,8,9,10,11};
    static const uint8_t BITS_AC_LUMA[16] = {0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,0x7d};
    static const uint8_t VAL_AC_LUMA[162] = {
        0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,
        0x22,0x71,0x14,0x32,0x81,0x91,0xa1,0x08,0x23,0x42,0xb1,0xc1,0x15,0x52,0xd1,0xf0,
        0x24,0x33,0x62,0x72,0x82,0x09,0x0a,0x16,0x17,0x18,0x19,0x1a,0x25,0x26,0x27,0x28,
        0x29,0x2a,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,0x49,
        0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,0x69,
        0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x83,0x84,0x85,0x86,0x87,0x88,0x89,
        0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,
        0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,0xc4,0xc5,
        0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xe1,0xe2,
        0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,
        0xf9,0xfa
    };

    quality = mono_iclamp(quality, 1, 100);
    int scale = (quality < 50) ? (5000 / quality) : (200 - 2 * quality);

    uint8_t qtable[64];
    double inv_q[64];
    for (int i = 0; i < 64; ++i) {
        int q = mono_iclamp(((int)STD_LUMA_QUANT[i] * scale + 50) / 100, 1, 255);
        qtable[i] = (uint8_t)q;
        inv_q[i] = 1.0 / (double)q;
    }

    double C[8][8];
    for (int u = 0; u < 8; ++u) {
        double alpha = (u == 0) ? (1.0 / sqrt(2.0)) : 1.0;
        for (int x = 0; x < 8; ++x) {
            C[u][x] = 0.5 * alpha * cos(((2.0 * x + 1.0) * u * MONO_PI) / 16.0);
        }
    }

    MonoHuffCode dc_tbl[256], ac_tbl[256];
    mono_build_huff(BITS_DC_LUMA, VAL_DC_LUMA, dc_tbl);
    mono_build_huff(BITS_AC_LUMA, VAL_AC_LUMA, ac_tbl);

    MonoByteBuf out;
    mbuf_init(&out, (size_t)img->w * (size_t)img->h / 2 + 1024);

    #define EMIT_U16(v) do { mbuf_push(&out, (uint8_t)(((v) >> 8) & 0xFFu)); mbuf_push(&out, (uint8_t)((v) & 0xFFu)); } while (0)

    mbuf_push(&out, 0xFF); mbuf_push(&out, 0xD8);
    mbuf_push(&out, 0xFF); mbuf_push(&out, 0xE0); EMIT_U16(16);
    static const uint8_t jfif[14] = {'J','F','I','F',0,1,1,0,0,1,0,1,0,0};
    mbuf_append(&out, jfif, 14);

    mbuf_push(&out, 0xFF); mbuf_push(&out, 0xDB); EMIT_U16(67); mbuf_push(&out, 0x00);
    for (int i = 0; i < 64; ++i) mbuf_push(&out, qtable[ZIGZAG[i]]);

    mbuf_push(&out, 0xFF); mbuf_push(&out, 0xC0); EMIT_U16(11);
    mbuf_push(&out, 8); EMIT_U16((uint16_t)img->h); EMIT_U16((uint16_t)img->w);
    mbuf_push(&out, 1); mbuf_push(&out, 1); mbuf_push(&out, 0x11); mbuf_push(&out, 0);

    mbuf_push(&out, 0xFF); mbuf_push(&out, 0xC4); EMIT_U16(3 + 16 + 12); mbuf_push(&out, 0x00);
    mbuf_append(&out, BITS_DC_LUMA, 16); mbuf_append(&out, VAL_DC_LUMA, 12);

    mbuf_push(&out, 0xFF); mbuf_push(&out, 0xC4); EMIT_U16(3 + 16 + 162); mbuf_push(&out, 0x10);
    mbuf_append(&out, BITS_AC_LUMA, 16); mbuf_append(&out, VAL_AC_LUMA, 162);

    mbuf_push(&out, 0xFF); mbuf_push(&out, 0xDA); EMIT_U16(8);
    mbuf_push(&out, 1); mbuf_push(&out, 1); mbuf_push(&out, 0x00);
    mbuf_push(&out, 0x00); mbuf_push(&out, 0x3F); mbuf_push(&out, 0x00);

    #undef EMIT_U16

    MonoJpegBitWriter bw = {&out, 0, 0};
    int prev_dc = 0;
    double blk[64], tmp[64];
    int qcoeff[64], zz[64];

    for (int by = 0; by < img->h; by += 8) {
        for (int bx = 0; bx < img->w; bx += 8) {
            for (int r = 0; r < 8; ++r) {
                int sy = mono_imin(by + r, img->h - 1);
                const uint8_t *srow = img->px + (size_t)sy * (size_t)img->w;
                for (int c = 0; c < 8; ++c) {
                    int sx = mono_imin(bx + c, img->w - 1);
                    blk[r * 8 + c] = (double)srow[sx] - 128.0;
                }
            }
            for (int r = 0; r < 8; ++r) {
                for (int u = 0; u < 8; ++u) {
                    double s = 0.0;
                    for (int c = 0; c < 8; ++c) s += blk[r * 8 + c] * C[u][c];
                    tmp[r * 8 + u] = s;
                }
            }
            for (int v = 0; v < 8; ++v) {
                for (int u = 0; u < 8; ++u) {
                    double s = 0.0;
                    for (int r = 0; r < 8; ++r) s += tmp[r * 8 + u] * C[v][r];
                    int idx = v * 8 + u;
                    qcoeff[idx] = (int)round(s * inv_q[idx]);
                }
            }
            for (int i = 0; i < 64; ++i) zz[i] = qcoeff[ZIGZAG[i]];

            int diff = zz[0] - prev_dc;
            prev_dc = zz[0];
            uint16_t mbits = 0;
            int cat = mono_bit_category(diff, &mbits);
            jbw_put_bits(&bw, dc_tbl[cat].code, dc_tbl[cat].len);
            if (cat > 0) jbw_put_bits(&bw, mbits, cat);

            int last_nz = 63;
            while (last_nz > 0 && zz[last_nz] == 0) --last_nz;
            int zero_run = 0;
            for (int k = 1; k <= last_nz; ++k) {
                int v = zz[k];
                if (v == 0) {
                    ++zero_run;
                } else {
                    while (zero_run >= 16) {
                        jbw_put_bits(&bw, ac_tbl[0xF0].code, ac_tbl[0xF0].len);
                        zero_run -= 16;
                    }
                    int ac_cat = mono_bit_category(v, &mbits);
                    uint8_t sym = (uint8_t)((zero_run << 4) | ac_cat);
                    jbw_put_bits(&bw, ac_tbl[sym].code, ac_tbl[sym].len);
                    jbw_put_bits(&bw, mbits, ac_cat);
                    zero_run = 0;
                }
            }
            if (last_nz < 63) jbw_put_bits(&bw, ac_tbl[0x00].code, ac_tbl[0x00].len);
        }
    }

    jbw_flush(&bw);
    mbuf_push(&out, 0xFF); mbuf_push(&out, 0xD9);
    return out;
}

static inline bool mono_save_jpeg(const GrayImage *img, const char *path, int quality) {
    MonoByteBuf bytes = mono_encode_jpeg_mem(img, quality);
    mono_ensure_parent_dirs(path);
    FILE *fp = fopen(path, "wb");
    if (!fp) { mbuf_free(&bytes); return false; }
    bool ok = (fwrite(bytes.data, 1, bytes.len, fp) == bytes.len);
    fclose(fp);
    mbuf_free(&bytes);
    return ok;
}

static inline bool mono_read_jpeg_size(const char *path, int *out_w, int *out_h) {
    *out_w = 0;
    *out_h = 0;
    FILE *fp = fopen(path, "rb");
    if (!fp) return false;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return false; }
    long sz = ftell(fp);
    if (sz < 4) { fclose(fp); return false; }
    rewind(fp);
    uint8_t *b = (uint8_t *)malloc((size_t)sz);
    if (!b) { fclose(fp); return false; }
    size_t got = fread(b, 1, (size_t)sz, fp);
    fclose(fp);
    if (got < 4 || b[0] != 0xFF || b[1] != 0xD8) { free(b); return false; }

    size_t i = 2;
    while (i + 1 < got) {
        if (b[i] != 0xFF) { ++i; continue; }
        while (i < got && b[i] == 0xFF) ++i;
        if (i >= got) break;
        uint8_t marker = b[i++];
        if (marker == 0xD8 || marker == 0xD9 || (marker >= 0xD0 && marker <= 0xD7) || marker == 0x01) continue;
        if (i + 1 >= got) break;
        uint16_t seg_len = (uint16_t)(((uint16_t)b[i] << 8) | b[i + 1]);
        if (seg_len < 2 || i + seg_len > got) break;
        if ((marker >= 0xC0 && marker <= 0xC3) || (marker >= 0xC5 && marker <= 0xC7) ||
            (marker >= 0xC9 && marker <= 0xCB) || (marker >= 0xCD && marker <= 0xCF)) {
            if (seg_len >= 7) {
                *out_h = ((int)b[i + 3] << 8) | b[i + 4];
                *out_w = ((int)b[i + 5] << 8) | b[i + 6];
                free(b);
                return true;
            }
        }
        i += seg_len;
    }
    free(b);
    return false;
}

// ── GIF89a 64-Grey Bayer-Dithered Variable-Width LZW Encoder ──────────────

static inline MonoByteBuf mono_lzw_encode_gif8(const uint8_t *indices, size_t nindices) {
    const int CLEAR_CODE = 256;
    const int END_CODE = 257;
    size_t tbl_len = 4096u * 256u;
    int32_t *table = (int32_t *)malloc(tbl_len * sizeof(int32_t));
    if (!table) abort();
    for (size_t i = 0; i < tbl_len; ++i) table[i] = -1;

    int next_code = 258;
    int code_size = 9;
    MonoByteBuf packed;
    mbuf_init(&packed, nindices / 2 + 64);

    uint32_t bit_buf = 0;
    int bit_cnt = 0;

    #define LZW_EMIT(c) do { \
        bit_buf |= ((uint32_t)(c) << bit_cnt); \
        bit_cnt += code_size; \
        while (bit_cnt >= 8) { \
            mbuf_push(&packed, (uint8_t)(bit_buf & 0xFFu)); \
            bit_buf >>= 8; \
            bit_cnt -= 8; \
        } \
    } while (0)

    LZW_EMIT(CLEAR_CODE);
    if (nindices == 0) {
        LZW_EMIT(END_CODE);
    } else {
        int prefix = indices[0];
        for (size_t i = 1; i < nindices; ++i) {
            uint8_t k = indices[i];
            int32_t hit = table[(size_t)prefix * 256u + k];
            if (hit != -1) {
                prefix = hit;
            } else {
                LZW_EMIT(prefix);
                if (next_code < 4096) {
                    table[(size_t)prefix * 256u + k] = next_code;
                    if (next_code == (1 << code_size) && code_size < 12) ++code_size;
                    ++next_code;
                } else {
                    LZW_EMIT(CLEAR_CODE);
                    for (size_t t = 0; t < tbl_len; ++t) table[t] = -1;
                    next_code = 258;
                    code_size = 9;
                }
                prefix = k;
            }
        }
        LZW_EMIT(prefix);
        LZW_EMIT(END_CODE);
    }
    if (bit_cnt > 0) mbuf_push(&packed, (uint8_t)(bit_buf & 0xFFu));
    #undef LZW_EMIT
    free(table);
    return packed;
}

static inline uint8_t mono_quant_bayer(uint8_t v, int x, int y) {
    static const int BAYER4[4][4] = {
        { 0,  8,  2, 10},
        {12,  4, 14,  6},
        { 3, 11,  1,  9},
        {15,  7, 13,  5},
    };
    double d = ((double)BAYER4[y & 3][x & 3] - 7.5) * (1.0 / 16.0);
    int q = (int)floor(((double)v / 255.0) * 63.0 + d + 0.5);
    return (uint8_t)mono_iclamp(q, 0, 63);
}

static inline MonoByteBuf mono_encode_gif_loop_mem(const GrayImage *frames, size_t nframes, int fps, int delta_tol) {
    MonoByteBuf out;
    if (nframes == 0) { mbuf_init(&out, 0); return out; }
    uint16_t w = (uint16_t)frames[0].w;
    uint16_t h = (uint16_t)frames[0].h;
    int eff_fps = fps > 0 ? fps : 24;
    uint16_t delay_cs = (uint16_t)mono_imax(2, (int)round(100.0 / eff_fps));

    mbuf_init(&out, nframes * (size_t)w * (size_t)h / 6 + 2048);

    #define GIF_LE16(v) do { mbuf_push(&out, (uint8_t)((v) & 0xFFu)); mbuf_push(&out, (uint8_t)(((v) >> 8) & 0xFFu)); } while (0)

    mbuf_append(&out, "GIF89a", 6);
    GIF_LE16(w);
    GIF_LE16(h);
    mbuf_push(&out, 0xF7);
    mbuf_push(&out, 0x00);
    mbuf_push(&out, 0x00);

    for (int i = 0; i < 64; ++i) {
        uint8_t v = (uint8_t)((i * 255 + 31) / 63);
        mbuf_push(&out, v); mbuf_push(&out, v); mbuf_push(&out, v);
    }
    for (int i = 64; i < 256; ++i) {
        mbuf_push(&out, 0); mbuf_push(&out, 0); mbuf_push(&out, 0);
    }

    static const uint8_t netscape_ext[19] = {
        0x21, 0xFF, 0x0B, 'N','E','T','S','C','A','P','E','2','.','0', 0x03, 0x01, 0x00, 0x00, 0x00
    };
    mbuf_append(&out, netscape_ext, 19);

    size_t npix = (size_t)w * (size_t)h;
    uint8_t *canvas_raw = (uint8_t *)calloc(npix, 1);
    uint8_t *canvas_q = (uint8_t *)calloc(npix, 1);
    if (!canvas_raw || !canvas_q) abort();
    int raw_tol = mono_imax(1, delta_tol * 2);

    for (size_t fi = 0; fi < nframes; ++fi) {
        const GrayImage *fr = &frames[fi];
        uint16_t bx0 = 0, by0 = 0, bw = w, bh = h;
        bool use_trans = (fi > 0);
        uint8_t *sub_indices = NULL;
        size_t sub_len = 0;

        if (fi == 0) {
            sub_len = npix;
            sub_indices = (uint8_t *)malloc(sub_len);
            if (!sub_indices) abort();
            for (int y = 0; y < (int)h; ++y) {
                size_t row_off = (size_t)y * (size_t)w;
                for (int x = 0; x < (int)w; ++x) {
                    uint8_t rv = fr->px[row_off + (size_t)x];
                    uint8_t q = mono_quant_bayer(rv, x, y);
                    canvas_raw[row_off + (size_t)x] = rv;
                    canvas_q[row_off + (size_t)x] = q;
                    sub_indices[row_off + (size_t)x] = q;
                }
            }
        } else {
            int min_x = w, min_y = h, max_x = -1, max_y = -1;
            for (int y = 0; y < (int)h; ++y) {
                size_t row_off = (size_t)y * (size_t)w;
                for (int x = 0; x < (int)w; ++x) {
                    uint8_t rv = fr->px[row_off + (size_t)x];
                    uint8_t q = mono_quant_bayer(rv, x, y);
                    if (q != canvas_q[row_off + (size_t)x] &&
                        abs((int)rv - (int)canvas_raw[row_off + (size_t)x]) > raw_tol) {
                        if (x < min_x) min_x = x;
                        if (x > max_x) max_x = x;
                        if (y < min_y) min_y = y;
                        if (y > max_y) max_y = y;
                    }
                }
            }
            if (max_x < min_x) {
                bx0 = 0; by0 = 0; bw = 1; bh = 1;
                sub_len = 1;
                sub_indices = (uint8_t *)malloc(1);
                sub_indices[0] = 255u;
            } else {
                bx0 = (uint16_t)min_x;
                by0 = (uint16_t)min_y;
                bw = (uint16_t)(max_x - min_x + 1);
                bh = (uint16_t)(max_y - min_y + 1);
                sub_len = (size_t)bw * (size_t)bh;
                sub_indices = (uint8_t *)malloc(sub_len);
                if (!sub_indices) abort();
                for (uint16_t dy = 0; dy < bh; ++dy) {
                    int py = (int)(by0 + dy);
                    size_t src_off = (size_t)py * (size_t)w + (size_t)bx0;
                    size_t dst_off = (size_t)dy * (size_t)bw;
                    for (uint16_t dx = 0; dx < bw; ++dx) {
                        int px_x = (int)(bx0 + dx);
                        uint8_t rv = fr->px[src_off + dx];
                        uint8_t q = mono_quant_bayer(rv, px_x, py);
                        if (q == canvas_q[src_off + dx] ||
                            abs((int)rv - (int)canvas_raw[src_off + dx]) <= raw_tol) {
                            sub_indices[dst_off + dx] = 255u;
                        } else {
                            canvas_raw[src_off + dx] = rv;
                            canvas_q[src_off + dx] = q;
                            sub_indices[dst_off + dx] = q;
                        }
                    }
                }
            }
        }

        mbuf_push(&out, 0x21);
        mbuf_push(&out, 0xF9);
        mbuf_push(&out, 0x04);
        mbuf_push(&out, (uint8_t)(use_trans ? 0x05u : 0x04u));
        GIF_LE16(delay_cs);
        mbuf_push(&out, use_trans ? 255u : 0x00u);
        mbuf_push(&out, 0x00);

        mbuf_push(&out, 0x2C);
        GIF_LE16(bx0); GIF_LE16(by0); GIF_LE16(bw); GIF_LE16(bh);
        mbuf_push(&out, 0x00);
        mbuf_push(&out, 0x08);

        MonoByteBuf lzw = mono_lzw_encode_gif8(sub_indices, sub_len);
        free(sub_indices);
        size_t pos = 0;
        while (pos < lzw.len) {
            size_t chunk = (lzw.len - pos < 255) ? (lzw.len - pos) : 255;
            mbuf_push(&out, (uint8_t)chunk);
            mbuf_append(&out, lzw.data + pos, chunk);
            pos += chunk;
        }
        mbuf_free(&lzw);
        mbuf_push(&out, 0x00);
    }

    #undef GIF_LE16
    free(canvas_raw);
    free(canvas_q);
    mbuf_push(&out, 0x3B);
    return out;
}

static inline bool mono_save_gif_loop(const GrayImage *frames, size_t nframes, const char *path, int fps, int delta_tol) {
    MonoByteBuf out = mono_encode_gif_loop_mem(frames, nframes, fps, delta_tol);
    mono_ensure_parent_dirs(path);
    FILE *fp = fopen(path, "wb");
    if (!fp) { mbuf_free(&out); return false; }
    bool ok = (fwrite(out.data, 1, out.len, fp) == out.len);
    fclose(fp);
    mbuf_free(&out);
    return ok;
}

static inline int mono_count_gif_frames(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return 0; }
    long sz = ftell(fp);
    if (sz < 14) { fclose(fp); return 0; }
    rewind(fp);
    uint8_t *b = (uint8_t *)malloc((size_t)sz);
    if (!b) { fclose(fp); return 0; }
    size_t len = fread(b, 1, (size_t)sz, fp);
    fclose(fp);
    if (len < 14 || b[0] != 'G' || b[1] != 'I' || b[2] != 'F') { free(b); return 0; }

    size_t i = 13;
    if (b[10] & 0x80u) {
        size_t gct_entries = 1u << ((b[10] & 0x07u) + 1u);
        i += 3u * gct_entries;
    }

    int frames = 0;
    while (i < len) {
        uint8_t intro = b[i++];
        if (intro == 0x3Bu) {
            break;
        } else if (intro == 0x21u) {
            if (i >= len) break;
            ++i;
            while (i < len) {
                uint8_t sub = b[i++];
                if (sub == 0) break;
                i += sub;
            }
        } else if (intro == 0x2Cu) {
            if (i + 9 > len) break;
            uint8_t packed = b[i + 8];
            i += 9;
            if (packed & 0x80u) {
                size_t lct_entries = 1u << ((packed & 0x07u) + 1u);
                i += 3u * lct_entries;
            }
            if (i >= len) break;
            ++i;
            while (i < len) {
                uint8_t sub = b[i++];
                if (sub == 0) break;
                i += sub;
            }
            ++frames;
        } else {
            break;
        }
    }
    free(b);
    return frames;
}

// ── RFC 4648 Base64 Encoder ────────────────────────────────────────────────

static inline char *mono_base64_encode(const uint8_t *data, size_t len) {
    static const char TBL[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t out_cap = ((len + 2) / 3) * 4 + 1;
    char *out = (char *)malloc(out_cap);
    if (!out) abort();
    size_t wp = 0, i = 0;
    while (i + 2 < len) {
        uint32_t v = ((uint32_t)data[i] << 16) | ((uint32_t)data[i + 1] << 8) | (uint32_t)data[i + 2];
        out[wp++] = TBL[(v >> 18) & 0x3Fu];
        out[wp++] = TBL[(v >> 12) & 0x3Fu];
        out[wp++] = TBL[(v >> 6) & 0x3Fu];
        out[wp++] = TBL[v & 0x3Fu];
        i += 3;
    }
    if (i < len) {
        uint32_t v = (uint32_t)data[i] << 16;
        if (i + 1 < len) v |= (uint32_t)data[i + 1] << 8;
        out[wp++] = TBL[(v >> 18) & 0x3Fu];
        out[wp++] = TBL[(v >> 12) & 0x3Fu];
        out[wp++] = (i + 1 < len) ? TBL[(v >> 6) & 0x3Fu] : '=';
        out[wp++] = '=';
    }
    out[wp] = '\0';
    return out;
}

#endif
