#pragma once
// ═══════════════════════════════════════════════════════════════════════════
//  include/mono_image.hpp — Zero-dependency C++20 greyscale imaging engine
//
//  Provides:
//    • GrayImage: 8-bit pure monochrome canvas, AA drawing primitives,
//      3-pass sliding-window Gaussian blur, bilinear/bicubic rotation,
//      Lanczos-3 resampling, alpha compositing, clamped additive blend.
//    • ITU-T T.81 Baseline Grayscale JPEG encoder + SOF dimension reader.
//    • GIF89a 256-level pure greyscale looping LZW encoder + frame counter.
//    • RFC 4648 Base64 encoder for inline HTML data URIs.
// ═══════════════════════════════════════════════════════════════════════════

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace mono_img {

inline uint8_t clamp_u8(double v) {
    if (v <= 0.0) return 0;
    if (v >= 255.0) return 255;
    return static_cast<uint8_t>(v + 0.5);
}

inline uint8_t clamp_i32(int v) {
    if (v <= 0) return 0;
    if (v >= 255) return 255;
    return static_cast<uint8_t>(v);
}

struct GrayImage {
    int w = 0;
    int h = 0;
    std::vector<uint8_t> px;

    GrayImage() = default;
    GrayImage(int width, int height, uint8_t fill = 0)
        : w(width), h(height), px(static_cast<size_t>(width) * static_cast<size_t>(height), fill) {}

    uint8_t get_clamped(int x, int y) const {
        if (w <= 0 || h <= 0) return 0;
        x = std::clamp(x, 0, w - 1);
        y = std::clamp(y, 0, h - 1);
        return px[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)];
    }

    void set_px(int x, int y, uint8_t v) {
        if (x < 0 || x >= w || y < 0 || y >= h) return;
        px[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = v;
    }

    void blend_px(int x, int y, uint8_t col, double alpha) {
        if (x < 0 || x >= w || y < 0 || y >= h || alpha <= 0.0) return;
        size_t idx = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
        if (alpha >= 1.0) {
            px[idx] = col;
            return;
        }
        double cur = static_cast<double>(px[idx]);
        px[idx] = clamp_u8(cur + (static_cast<double>(col) - cur) * alpha);
    }

    double sample_bilinear(double x, double y) const {
        int x0 = static_cast<int>(std::floor(x));
        int y0 = static_cast<int>(std::floor(y));
        double fx = x - x0;
        double fy = y - y0;
        double p00 = get_clamped(x0, y0);
        double p10 = get_clamped(x0 + 1, y0);
        double p01 = get_clamped(x0, y0 + 1);
        double p11 = get_clamped(x0 + 1, y0 + 1);
        double top = p00 + (p10 - p00) * fx;
        double bot = p01 + (p11 - p01) * fx;
        return top + (bot - top) * fy;
    }

    void add_clamped(const GrayImage& other) {
        size_t n = std::min(px.size(), other.px.size());
        for (size_t i = 0; i < n; ++i) {
            int s = static_cast<int>(px[i]) + static_cast<int>(other.px[i]);
            px[i] = static_cast<uint8_t>(s > 255 ? 255 : s);
        }
    }

    static GrayImage composite(const GrayImage& fg, const GrayImage& bg, const GrayImage& mask) {
        GrayImage out(bg.w, bg.h, 0);
        size_t n = out.px.size();
        for (size_t i = 0; i < n; ++i) {
            uint32_t a = mask.px[i];
            uint32_t f = fg.px[i];
            uint32_t b = bg.px[i];
            out.px[i] = static_cast<uint8_t>((f * a + b * (255u - a) + 127u) / 255u);
        }
        return out;
    }

    GrayImage crop(int x0, int y0, int cw, int ch) const {
        GrayImage out(cw, ch, 0);
        for (int y = 0; y < ch; ++y) {
            for (int x = 0; x < cw; ++x) {
                int sx = x0 + x;
                int sy = y0 + y;
                if (sx >= 0 && sx < w && sy >= 0 && sy < h) {
                    out.px[static_cast<size_t>(y) * static_cast<size_t>(cw) + static_cast<size_t>(x)] =
                        px[static_cast<size_t>(sy) * static_cast<size_t>(w) + static_cast<size_t>(sx)];
                }
            }
        }
        return out;
    }

    GrayImage rotate_deg(double deg_ccw) const {
        GrayImage out(w, h, 0);
        double rad = deg_ccw * (std::numbers::pi / 180.0);
        double c = std::cos(rad);
        double s = std::sin(rad);
        double cx = (w - 1) * 0.5;
        double cy = (h - 1) * 0.5;
        for (int y = 0; y < h; ++y) {
            double dy = y - cy;
            for (int x = 0; x < w; ++x) {
                double dx = x - cx;
                // Inverse map from destination to source (CCW rotation of image)
                double sx = cx + dx * c + dy * s;
                double sy = cy - dx * s + dy * c;
                if (sx >= -0.5 && sx <= w - 0.5 && sy >= -0.5 && sy <= h - 0.5) {
                    out.px[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] =
                        clamp_u8(sample_bilinear(sx, sy));
                }
            }
        }
        return out;
    }

    GrayImage gaussian_blur(double sigma) const {
        if (sigma <= 0.25 || w <= 1 || h <= 1) return *this;
        // 3-pass separable sliding-window box blur approximating Gaussian within <1%
        double w_ideal = std::sqrt((12.0 * sigma * sigma / 3.0) + 1.0);
        int wl = static_cast<int>(std::floor(w_ideal));
        if (wl % 2 == 0) --wl;
        if (wl < 1) wl = 1;
        int wu = wl + 2;
        double m_ideal = (12.0 * sigma * sigma - 3.0 * wl * wl - 12.0 * wl - 9.0) / (-4.0 * wl - 4.0);
        int m = std::clamp(static_cast<int>(std::round(m_ideal)), 0, 3);

        std::vector<uint8_t> src = px;
        std::vector<uint8_t> tmp(px.size(), 0);
        const size_t W = static_cast<size_t>(w);
        const size_t H = static_cast<size_t>(h);

        auto box_h = [&](const std::vector<uint8_t>& in, std::vector<uint8_t>& out, int r) {
            if (r <= 0) {
                out = in;
                return;
            }
            int diam = 2 * r + 1;
            for (size_t y = 0; y < H; ++y) {
                const uint8_t* row = in.data() + y * W;
                uint8_t* dst = out.data() + y * W;
                int acc = (r + 1) * static_cast<int>(row[0]);
                for (int x = 1; x <= r; ++x) {
                    acc += static_cast<int>(row[std::min<int>(x, w - 1)]);
                }
                for (int x = 0; x < w; ++x) {
                    dst[x] = static_cast<uint8_t>((acc + diam / 2) / diam);
                    int add_idx = std::min(x + r + 1, w - 1);
                    int sub_idx = std::max(x - r, 0);
                    acc += static_cast<int>(row[add_idx]) - static_cast<int>(row[sub_idx]);
                }
            }
        };

        auto box_v = [&](const std::vector<uint8_t>& in, std::vector<uint8_t>& out, int r) {
            if (r <= 0) {
                out = in;
                return;
            }
            int diam = 2 * r + 1;
            for (int x = 0; x < w; ++x) {
                int acc = (r + 1) * static_cast<int>(in[static_cast<size_t>(x)]);
                for (int y = 1; y <= r; ++y) {
                    int cy = std::min(y, h - 1);
                    acc += static_cast<int>(in[static_cast<size_t>(cy) * W + static_cast<size_t>(x)]);
                }
                for (int y = 0; y < h; ++y) {
                    out[static_cast<size_t>(y) * W + static_cast<size_t>(x)] =
                        static_cast<uint8_t>((acc + diam / 2) / diam);
                    int add_y = std::min(y + r + 1, h - 1);
                    int sub_y = std::max(y - r, 0);
                    acc += static_cast<int>(in[static_cast<size_t>(add_y) * W + static_cast<size_t>(x)]) -
                           static_cast<int>(in[static_cast<size_t>(sub_y) * W + static_cast<size_t>(x)]);
                }
            }
        };

        for (int pass = 0; pass < 3; ++pass) {
            int bw = (pass < m) ? wl : wu;
            int r = (bw - 1) / 2;
            box_h(src, tmp, r);
            box_v(tmp, src, r);
        }

        GrayImage res(w, h, 0);
        res.px = std::move(src);
        return res;
    }

    GrayImage resize_lanczos(int nw, int nh) const {
        if (nw == w && nh == h) return *this;
        auto lanczos3 = [](double x) -> double {
            x = std::abs(x);
            if (x < 1e-9) return 1.0;
            if (x >= 3.0) return 0.0;
            double pix = std::numbers::pi * x;
            return (3.0 * std::sin(pix) * std::sin(pix / 3.0)) / (pix * pix);
        };

        // Horizontal pass -> double buffer (nw x h)
        std::vector<double> horiz(static_cast<size_t>(nw) * static_cast<size_t>(h), 0.0);
        double scale_x = static_cast<double>(w) / static_cast<double>(nw);
        double support_x = std::max(1.0, scale_x) * 3.0;
        double inv_filter_x = 1.0 / std::max(1.0, scale_x);

        struct Tap { int idx; double w; };
        std::vector<std::vector<Tap>> x_taps(static_cast<size_t>(nw));
        for (int dx = 0; dx < nw; ++dx) {
            double center = (dx + 0.5) * scale_x - 0.5;
            int left = static_cast<int>(std::floor(center - support_x));
            int right = static_cast<int>(std::ceil(center + support_x));
            double sum = 0.0;
            auto& taps = x_taps[static_cast<size_t>(dx)];
            for (int sx = left; sx <= right; ++sx) {
                double wt = lanczos3((sx - center) * inv_filter_x);
                if (wt != 0.0) {
                    taps.push_back({std::clamp(sx, 0, w - 1), wt});
                    sum += wt;
                }
            }
            if (sum != 0.0) {
                for (auto& t : taps) t.w /= sum;
            }
        }

        for (int y = 0; y < h; ++y) {
            const uint8_t* srow = px.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
            double* drow = horiz.data() + static_cast<size_t>(y) * static_cast<size_t>(nw);
            for (int dx = 0; dx < nw; ++dx) {
                double acc = 0.0;
                for (const auto& t : x_taps[static_cast<size_t>(dx)]) {
                    acc += static_cast<double>(srow[t.idx]) * t.w;
                }
                drow[dx] = acc;
            }
        }

        // Vertical pass -> GrayImage (nw x nh)
        GrayImage out(nw, nh, 0);
        double scale_y = static_cast<double>(h) / static_cast<double>(nh);
        double support_y = std::max(1.0, scale_y) * 3.0;
        double inv_filter_y = 1.0 / std::max(1.0, scale_y);

        for (int dy = 0; dy < nh; ++dy) {
            double center = (dy + 0.5) * scale_y - 0.5;
            int top = static_cast<int>(std::floor(center - support_y));
            int bot = static_cast<int>(std::ceil(center + support_y));
            std::vector<Tap> taps;
            double sum = 0.0;
            for (int sy = top; sy <= bot; ++sy) {
                double wt = lanczos3((sy - center) * inv_filter_y);
                if (wt != 0.0) {
                    taps.push_back({std::clamp(sy, 0, h - 1), wt});
                    sum += wt;
                }
            }
            if (sum != 0.0) {
                for (auto& t : taps) t.w /= sum;
            }
            uint8_t* drow = out.px.data() + static_cast<size_t>(dy) * static_cast<size_t>(nw);
            for (int dx = 0; dx < nw; ++dx) {
                double acc = 0.0;
                for (const auto& t : taps) {
                    acc += horiz[static_cast<size_t>(t.idx) * static_cast<size_t>(nw) + static_cast<size_t>(dx)] * t.w;
                }
                drow[dx] = clamp_u8(acc);
            }
        }
        return out;
    }

    // ── Drawing Primitives ────────────────────────────────────────────────

    void fill_rect(int x0, int y0, int x1, int y1, uint8_t col) {
        if (x0 > x1) std::swap(x0, x1);
        if (y0 > y1) std::swap(y0, y1);
        x0 = std::max(0, x0);
        y0 = std::max(0, y0);
        x1 = std::min(w - 1, x1);
        y1 = std::min(h - 1, y1);
        for (int y = y0; y <= y1; ++y) {
            uint8_t* row = px.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
            for (int x = x0; x <= x1; ++x) row[x] = col;
        }
    }

    void stroke_rect(int x0, int y0, int x1, int y1, uint8_t col, int width = 1) {
        if (x0 > x1) std::swap(x0, x1);
        if (y0 > y1) std::swap(y0, y1);
        int t = std::max(1, width);
        fill_rect(x0, y0, x1, y0 + t - 1, col);
        fill_rect(x0, y1 - t + 1, x1, y1, col);
        fill_rect(x0, y0, x0 + t - 1, y1, col);
        fill_rect(x1 - t + 1, y0, x1, y1, col);
    }

    void draw_line(double x0, double y0, double x1, double y1, uint8_t col, double width = 1.0) {
        double r = std::max(0.5, width * 0.5);
        int bx0 = std::max(0, static_cast<int>(std::floor(std::min(x0, x1) - r - 1.0)));
        int bx1 = std::min(w - 1, static_cast<int>(std::ceil(std::max(x0, x1) + r + 1.0)));
        int by0 = std::max(0, static_cast<int>(std::floor(std::min(y0, y1) - r - 1.0)));
        int by1 = std::min(h - 1, static_cast<int>(std::ceil(std::max(y0, y1) + r + 1.0)));

        double vx = x1 - x0;
        double vy = y1 - y0;
        double len2 = vx * vx + vy * vy;

        for (int y = by0; y <= by1; ++y) {
            for (int x = bx0; x <= bx1; ++x) {
                double t = 0.0;
                if (len2 > 1e-9) {
                    t = std::clamp(((x - x0) * vx + (y - y0) * vy) / len2, 0.0, 1.0);
                }
                double dx = x - (x0 + t * vx);
                double dy = y - (y0 + t * vy);
                double dist = std::sqrt(dx * dx + dy * dy);
                double cov = std::clamp(r + 0.5 - dist, 0.0, 1.0);
                if (cov > 0.0) blend_px(x, y, col, cov);
            }
        }
    }

    void draw_polyline(const std::vector<std::pair<double, double>>& pts, uint8_t col, double width = 1.0) {
        if (pts.size() < 2) return;
        double r = std::max(0.5, width * 0.5);
        double min_x = pts[0].first, max_x = pts[0].first;
        double min_y = pts[0].second, max_y = pts[0].second;
        for (const auto& p : pts) {
            min_x = std::min(min_x, p.first);
            max_x = std::max(max_x, p.first);
            min_y = std::min(min_y, p.second);
            max_y = std::max(max_y, p.second);
        }
        int bx0 = std::max(0, static_cast<int>(std::floor(min_x - r - 1.0)));
        int bx1 = std::min(w - 1, static_cast<int>(std::ceil(max_x + r + 1.0)));
        int by0 = std::max(0, static_cast<int>(std::floor(min_y - r - 1.0)));
        int by1 = std::min(h - 1, static_cast<int>(std::ceil(max_y + r + 1.0)));
        if (bx0 > bx1 || by0 > by1) return;

        int bw = bx1 - bx0 + 1;
        int bh = by1 - by0 + 1;
        std::vector<float> cov_buf(static_cast<size_t>(bw) * static_cast<size_t>(bh), 0.0f);

        for (size_t i = 1; i < pts.size(); ++i) {
            double x0 = pts[i - 1].first, y0 = pts[i - 1].second;
            double x1 = pts[i].first, y1 = pts[i].second;
            int sx0 = std::max(bx0, static_cast<int>(std::floor(std::min(x0, x1) - r - 1.0)));
            int sx1 = std::min(bx1, static_cast<int>(std::ceil(std::max(x0, x1) + r + 1.0)));
            int sy0 = std::max(by0, static_cast<int>(std::floor(std::min(y0, y1) - r - 1.0)));
            int sy1 = std::min(by1, static_cast<int>(std::ceil(std::max(y0, y1) + r + 1.0)));
            double vx = x1 - x0, vy = y1 - y0;
            double len2 = vx * vx + vy * vy;
            for (int y = sy0; y <= sy1; ++y) {
                for (int x = sx0; x <= sx1; ++x) {
                    double t = (len2 > 1e-9)
                        ? std::clamp(((x - x0) * vx + (y - y0) * vy) / len2, 0.0, 1.0)
                        : 0.0;
                    double dx = x - (x0 + t * vx);
                    double dy = y - (y0 + t * vy);
                    float cov = static_cast<float>(std::clamp(r + 0.5 - std::sqrt(dx * dx + dy * dy), 0.0, 1.0));
                    if (cov > 0.0f) {
                        float& cell = cov_buf[static_cast<size_t>(y - by0) * static_cast<size_t>(bw) + static_cast<size_t>(x - bx0)];
                        if (cov > cell) cell = cov;
                    }
                }
            }
        }

        for (int y = by0; y <= by1; ++y) {
            for (int x = bx0; x <= bx1; ++x) {
                float cov = cov_buf[static_cast<size_t>(y - by0) * static_cast<size_t>(bw) + static_cast<size_t>(x - bx0)];
                if (cov > 0.0f) blend_px(x, y, col, static_cast<double>(cov));
            }
        }
    }

    void fill_ellipse(double cx, double cy, double rx, double ry, uint8_t col) {
        if (rx <= 0.0 || ry <= 0.0) return;
        int x0 = std::max(0, static_cast<int>(std::floor(cx - rx - 1.0)));
        int x1 = std::min(w - 1, static_cast<int>(std::ceil(cx + rx + 1.0)));
        int y0 = std::max(0, static_cast<int>(std::floor(cy - ry - 1.0)));
        int y1 = std::min(h - 1, static_cast<int>(std::ceil(cy + ry + 1.0)));
        double avg_r = 0.5 * (rx + ry);
        for (int y = y0; y <= y1; ++y) {
            double ny = (y - cy) / ry;
            for (int x = x0; x <= x1; ++x) {
                double nx = (x - cx) / rx;
                double d = std::sqrt(nx * nx + ny * ny);
                double px_dist = (1.0 - d) * avg_r;
                double cov = std::clamp(px_dist + 0.5, 0.0, 1.0);
                if (cov > 0.0) blend_px(x, y, col, cov);
            }
        }
    }

    void stroke_ellipse(double cx, double cy, double rx, double ry, uint8_t col, double width = 1.0) {
        if (rx <= 0.0 || ry <= 0.0) return;
        double hw = std::max(0.5, width * 0.5);
        int x0 = std::max(0, static_cast<int>(std::floor(cx - rx - hw - 2.0)));
        int x1 = std::min(w - 1, static_cast<int>(std::ceil(cx + rx + hw + 2.0)));
        int y0 = std::max(0, static_cast<int>(std::floor(cy - ry - hw - 2.0)));
        int y1 = std::min(h - 1, static_cast<int>(std::ceil(cy + ry + hw + 2.0)));
        for (int y = y0; y <= y1; ++y) {
            double dy = y - cy;
            for (int x = x0; x <= x1; ++x) {
                double dx = x - cx;
                double nx = dx / rx;
                double ny = dy / ry;
                double f = std::sqrt(nx * nx + ny * ny);
                if (f < 1e-6) continue;
                // Gradient-normalized approximate Euclidean distance to ellipse boundary
                double gx = nx / rx;
                double gy = ny / ry;
                double gnorm = std::sqrt(gx * gx + gy * gy) * f;
                double dist = std::abs(f - 1.0) / std::max(1e-6, gnorm);
                double cov = std::clamp(hw + 0.5 - dist, 0.0, 1.0);
                if (cov > 0.0) blend_px(x, y, col, cov);
            }
        }
    }

    void stroke_arc(double cx, double cy, double rx, double ry,
                    double start_deg, double end_deg, uint8_t col, double width = 1.0) {
        if (rx <= 0.0 || ry <= 0.0) return;
        while (end_deg < start_deg) end_deg += 360.0;
        double span_deg = end_deg - start_deg;
        int steps = std::max(12, static_cast<int>(std::ceil(span_deg * std::max(rx, ry) * 0.035)));
        std::vector<std::pair<double, double>> pts;
        pts.reserve(static_cast<size_t>(steps + 1));
        for (int i = 0; i <= steps; ++i) {
            double deg = start_deg + span_deg * (static_cast<double>(i) / steps);
            double rad = deg * (std::numbers::pi / 180.0);
            pts.emplace_back(cx + std::cos(rad) * rx, cy + std::sin(rad) * ry);
        }
        draw_polyline(pts, col, width);
    }

    void fill_polygon(const std::vector<std::pair<double, double>>& poly, uint8_t col) {
        if (poly.size() < 3) return;
        double min_y = poly[0].second, max_y = poly[0].second;
        for (const auto& p : poly) {
            min_y = std::min(min_y, p.second);
            max_y = std::max(max_y, p.second);
        }
        int y0 = std::max(0, static_cast<int>(std::floor(min_y)));
        int y1 = std::min(h - 1, static_cast<int>(std::ceil(max_y)));
        std::vector<double> nodes;
        for (int y = y0; y <= y1; ++y) {
            double sy = y + 0.5;
            nodes.clear();
            size_t j = poly.size() - 1;
            for (size_t i = 0; i < poly.size(); ++i) {
                double yi = poly[i].second, yj = poly[j].second;
                if ((yi < sy && yj >= sy) || (yj < sy && yi >= sy)) {
                    double xi = poly[i].first, xj = poly[j].first;
                    nodes.push_back(xi + (sy - yi) / (yj - yi) * (xj - xi));
                }
                j = i;
            }
            std::sort(nodes.begin(), nodes.end());
            for (size_t i = 0; i + 1 < nodes.size(); i += 2) {
                int x_start = std::max(0, static_cast<int>(std::floor(nodes[i])));
                int x_end = std::min(w - 1, static_cast<int>(std::ceil(nodes[i + 1])));
                for (int x = x_start; x <= x_end; ++x) {
                    double left = std::max(static_cast<double>(x), nodes[i]);
                    double right = std::min(static_cast<double>(x + 1), nodes[i + 1]);
                    if (right > left) blend_px(x, y, col, right - left);
                }
            }
        }
    }
};

// ── ITU-T T.81 Baseline Grayscale JPEG Encoder ────────────────────────────

namespace detail {

inline constexpr std::array<uint8_t, 64> STD_LUMA_QUANT = {
    16, 11, 10, 16,  24,  40,  51,  61,
    12, 12, 14, 19,  26,  58,  60,  55,
    14, 13, 16, 24,  40,  57,  69,  56,
    14, 17, 22, 29,  51,  87,  80,  62,
    18, 22, 37, 56,  68, 109, 103,  77,
    24, 35, 55, 64,  81, 104, 113,  92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103,  99
};

inline constexpr std::array<uint8_t, 64> ZIGZAG = {
     0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

inline constexpr std::array<uint8_t, 16> BITS_DC_LUMA = {
    0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0
};
inline constexpr std::array<uint8_t, 12> VAL_DC_LUMA = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
};

inline constexpr std::array<uint8_t, 16> BITS_AC_LUMA = {
    0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d
};
inline constexpr std::array<uint8_t, 162> VAL_AC_LUMA = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
    0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
    0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
    0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
    0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
    0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
    0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
    0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
    0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
    0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
    0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
    0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa
};

struct HuffCode {
    uint16_t code = 0;
    uint8_t len = 0;
};

template <size_t N>
inline std::array<HuffCode, 256> build_huff_table(const std::array<uint8_t, 16>& bits,
                                                  const std::array<uint8_t, N>& vals) {
    std::array<HuffCode, 256> out{};
    uint16_t code = 0;
    size_t idx = 0;
    for (int len = 1; len <= 16; ++len) {
        for (int i = 0; i < bits[static_cast<size_t>(len - 1)]; ++i) {
            uint8_t sym = vals[idx++];
            out[sym] = {code, static_cast<uint8_t>(len)};
            ++code;
        }
        code <<= 1;
    }
    return out;
}

struct JpegBitWriter {
    std::vector<uint8_t>& buf;
    uint32_t acc = 0;
    int bits = 0;

    void put_bits(uint16_t val, int nbits) {
        acc = (acc << nbits) | (static_cast<uint32_t>(val) & ((1u << nbits) - 1u));
        bits += nbits;
        while (bits >= 8) {
            bits -= 8;
            uint8_t b = static_cast<uint8_t>((acc >> bits) & 0xFFu);
            buf.push_back(b);
            if (b == 0xFFu) buf.push_back(0x00u);
        }
    }

    void flush() {
        if (bits > 0) {
            put_bits(0x7Fu, 8 - bits);
            bits = 0;
            acc = 0;
        }
    }
};

}  // namespace detail

inline std::vector<uint8_t> encode_jpeg_mem(const GrayImage& img, int quality = 92) {
    quality = std::clamp(quality, 1, 100);
    int scale = (quality < 50) ? (5000 / quality) : (200 - 2 * quality);

    std::array<uint8_t, 64> qtable{};
    std::array<double, 64> inv_q{};
    for (size_t i = 0; i < 64; ++i) {
        int q = (static_cast<int>(detail::STD_LUMA_QUANT[i]) * scale + 50) / 100;
        q = std::clamp(q, 1, 255);
        qtable[i] = static_cast<uint8_t>(q);
        inv_q[i] = 1.0 / static_cast<double>(q);
    }

    // Precompute separable 8x8 DCT-II cosine matrix with orthonormal scaling
    std::array<std::array<double, 8>, 8> C{};
    for (int u = 0; u < 8; ++u) {
        double alpha = (u == 0) ? (1.0 / std::sqrt(2.0)) : 1.0;
        for (int x = 0; x < 8; ++x) {
            C[static_cast<size_t>(u)][static_cast<size_t>(x)] =
                0.5 * alpha * std::cos(((2.0 * x + 1.0) * u * std::numbers::pi) / 16.0);
        }
    }

    auto dc_tbl = detail::build_huff_table(detail::BITS_DC_LUMA, detail::VAL_DC_LUMA);
    auto ac_tbl = detail::build_huff_table(detail::BITS_AC_LUMA, detail::VAL_AC_LUMA);

    std::vector<uint8_t> out;
    out.reserve(static_cast<size_t>(img.w) * static_cast<size_t>(img.h) / 2 + 1024);

    auto emit_u16 = [&](uint16_t v) {
        out.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
        out.push_back(static_cast<uint8_t>(v & 0xFFu));
    };

    // SOI
    out.push_back(0xFF); out.push_back(0xD8);

    // APP0 (JFIF 1.01)
    out.push_back(0xFF); out.push_back(0xE0);
    emit_u16(16);
    out.insert(out.end(), {'J', 'F', 'I', 'F', 0, 1, 1, 0, 0, 1, 0, 1, 0, 0});

    // DQT (table 0, 8-bit precision, zig-zag order)
    out.push_back(0xFF); out.push_back(0xDB);
    emit_u16(67);
    out.push_back(0x00);
    for (size_t i = 0; i < 64; ++i) {
        out.push_back(qtable[detail::ZIGZAG[i]]);
    }

    // SOF0 (Baseline DCT, 1 greyscale component)
    out.push_back(0xFF); out.push_back(0xC0);
    emit_u16(11);
    out.push_back(8);
    emit_u16(static_cast<uint16_t>(img.h));
    emit_u16(static_cast<uint16_t>(img.w));
    out.push_back(1);     // Nf = 1 component (Y)
    out.push_back(1);     // Component ID = 1
    out.push_back(0x11);  // Sampling 1x1
    out.push_back(0);     // Quantization table 0

    // DHT (DC table 0)
    out.push_back(0xFF); out.push_back(0xC4);
    emit_u16(static_cast<uint16_t>(3 + detail::BITS_DC_LUMA.size() + detail::VAL_DC_LUMA.size()));
    out.push_back(0x00);
    out.insert(out.end(), detail::BITS_DC_LUMA.begin(), detail::BITS_DC_LUMA.end());
    out.insert(out.end(), detail::VAL_DC_LUMA.begin(), detail::VAL_DC_LUMA.end());

    // DHT (AC table 0)
    out.push_back(0xFF); out.push_back(0xC4);
    emit_u16(static_cast<uint16_t>(3 + detail::BITS_AC_LUMA.size() + detail::VAL_AC_LUMA.size()));
    out.push_back(0x10);
    out.insert(out.end(), detail::BITS_AC_LUMA.begin(), detail::BITS_AC_LUMA.end());
    out.insert(out.end(), detail::VAL_AC_LUMA.begin(), detail::VAL_AC_LUMA.end());

    // SOS
    out.push_back(0xFF); out.push_back(0xDA);
    emit_u16(8);
    out.push_back(1);     // Ns = 1
    out.push_back(1);     // Component 1
    out.push_back(0x00);  // Td=0, Ta=0
    out.push_back(0x00);  // Ss=0
    out.push_back(0x3F);  // Se=63
    out.push_back(0x00);  // Ah=0, Al=0

    detail::JpegBitWriter bw{out};
    int prev_dc = 0;

    auto bit_category = [](int v, uint16_t& mag_bits) -> int {
        if (v == 0) {
            mag_bits = 0;
            return 0;
        }
        int av = std::abs(v);
        int cat = 0;
        while ((1 << cat) <= av) ++cat;
        mag_bits = static_cast<uint16_t>((v > 0) ? v : ((1 << cat) - 1 + v));
        return cat;
    };

    std::array<double, 64> blk{};
    std::array<double, 64> tmp{};
    std::array<int, 64> zz{};

    for (int by = 0; by < img.h; by += 8) {
        for (int bx = 0; bx < img.w; bx += 8) {
            for (int r = 0; r < 8; ++r) {
                int sy = std::min(by + r, img.h - 1);
                const uint8_t* srow = img.px.data() + static_cast<size_t>(sy) * static_cast<size_t>(img.w);
                for (int c = 0; c < 8; ++c) {
                    int sx = std::min(bx + c, img.w - 1);
                    blk[static_cast<size_t>(r * 8 + c)] = static_cast<double>(srow[sx]) - 128.0;
                }
            }
            // Row 1D DCT
            for (int r = 0; r < 8; ++r) {
                for (int u = 0; u < 8; ++u) {
                    double s = 0.0;
                    for (int c = 0; c < 8; ++c) {
                        s += blk[static_cast<size_t>(r * 8 + c)] * C[static_cast<size_t>(u)][static_cast<size_t>(c)];
                    }
                    tmp[static_cast<size_t>(r * 8 + u)] = s;
                }
            }
            // Col 1D DCT + Quantize
            std::array<int, 64> qcoeff{};
            for (int v = 0; v < 8; ++v) {
                for (int u = 0; u < 8; ++u) {
                    double s = 0.0;
                    for (int r = 0; r < 8; ++r) {
                        s += tmp[static_cast<size_t>(r * 8 + u)] * C[static_cast<size_t>(v)][static_cast<size_t>(r)];
                    }
                    size_t idx = static_cast<size_t>(v * 8 + u);
                    qcoeff[idx] = static_cast<int>(std::round(s * inv_q[idx]));
                }
            }
            for (size_t i = 0; i < 64; ++i) {
                zz[i] = qcoeff[detail::ZIGZAG[i]];
            }

            // DC coefficient
            int diff = zz[0] - prev_dc;
            prev_dc = zz[0];
            uint16_t mbits = 0;
            int cat = bit_category(diff, mbits);
            bw.put_bits(dc_tbl[static_cast<size_t>(cat)].code, dc_tbl[static_cast<size_t>(cat)].len);
            if (cat > 0) bw.put_bits(mbits, cat);

            // AC coefficients
            int last_nz = 63;
            while (last_nz > 0 && zz[static_cast<size_t>(last_nz)] == 0) --last_nz;
            int zero_run = 0;
            for (int k = 1; k <= last_nz; ++k) {
                int v = zz[static_cast<size_t>(k)];
                if (v == 0) {
                    ++zero_run;
                } else {
                    while (zero_run >= 16) {
                        bw.put_bits(ac_tbl[0xF0].code, ac_tbl[0xF0].len);  // ZRL
                        zero_run -= 16;
                    }
                    int ac_cat = bit_category(v, mbits);
                    uint8_t sym = static_cast<uint8_t>((zero_run << 4) | ac_cat);
                    bw.put_bits(ac_tbl[sym].code, ac_tbl[sym].len);
                    bw.put_bits(mbits, ac_cat);
                    zero_run = 0;
                }
            }
            if (last_nz < 63) {
                bw.put_bits(ac_tbl[0x00].code, ac_tbl[0x00].len);  // EOB
            }
        }
    }

    bw.flush();
    // EOI
    out.push_back(0xFF); out.push_back(0xD9);
    return out;
}

inline void save_jpeg(const GrayImage& img, const std::filesystem::path& path, int quality = 92) {
    auto bytes = encode_jpeg_mem(img, quality);
    std::filesystem::create_directories(path.parent_path());
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) throw std::runtime_error("failed to open JPEG output: " + path.string());
    ofs.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

inline std::pair<int, int> read_jpeg_size(const std::filesystem::path& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return {0, 0};
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (b.size() < 4 || b[0] != 0xFF || b[1] != 0xD8) return {0, 0};
    size_t i = 2;
    while (i + 1 < b.size()) {
        if (b[i] != 0xFF) { ++i; continue; }
        while (i < b.size() && b[i] == 0xFF) ++i;
        if (i >= b.size()) break;
        uint8_t marker = b[i++];
        if (marker == 0xD8 || marker == 0xD9 || (marker >= 0xD0 && marker <= 0xD7) || marker == 0x01) continue;
        if (i + 1 >= b.size()) break;
        uint16_t seg_len = static_cast<uint16_t>((static_cast<uint16_t>(b[i]) << 8) | b[i + 1]);
        if (seg_len < 2 || i + seg_len > b.size()) break;
        if ((marker >= 0xC0 && marker <= 0xC3) || (marker >= 0xC5 && marker <= 0xC7) ||
            (marker >= 0xC9 && marker <= 0xCB) || (marker >= 0xCD && marker <= 0xCF)) {
            if (seg_len >= 7) {
                int h = (static_cast<int>(b[i + 3]) << 8) | b[i + 4];
                int w = (static_cast<int>(b[i + 5]) << 8) | b[i + 6];
                return {w, h};
            }
        }
        i += seg_len;
    }
    return {0, 0};
}

// ── GIF89a 256-Grey Variable-Width LZW Encoder & Frame Counter ───────────

inline std::vector<uint8_t> lzw_encode_gif8(const std::vector<uint8_t>& indices) {
    constexpr int CLEAR_CODE = 256;
    constexpr int END_CODE = 257;

    std::vector<int32_t> table(4096 * 256, -1);
    int next_code = 258;
    int code_size = 9;

    std::vector<uint8_t> packed;
    packed.reserve(indices.size() / 2 + 64);

    uint32_t bit_buf = 0;
    int bit_cnt = 0;

    auto emit_code = [&](int code) {
        bit_buf |= (static_cast<uint32_t>(code) << bit_cnt);
        bit_cnt += code_size;
        while (bit_cnt >= 8) {
            packed.push_back(static_cast<uint8_t>(bit_buf & 0xFFu));
            bit_buf >>= 8;
            bit_cnt -= 8;
        }
    };

    emit_code(CLEAR_CODE);
    if (indices.empty()) {
        emit_code(END_CODE);
    } else {
        int prefix = indices[0];
        for (size_t i = 1; i < indices.size(); ++i) {
            uint8_t k = indices[i];
            int32_t hit = table[static_cast<size_t>(prefix) * 256u + k];
            if (hit != -1) {
                prefix = hit;
            } else {
                emit_code(prefix);
                if (next_code < 4096) {
                    table[static_cast<size_t>(prefix) * 256u + k] = next_code;
                    if (next_code == (1 << code_size) && code_size < 12) {
                        ++code_size;
                    }
                    ++next_code;
                } else {
                    emit_code(CLEAR_CODE);
                    std::fill(table.begin(), table.end(), -1);
                    next_code = 258;
                    code_size = 9;
                }
                prefix = k;
            }
        }
        emit_code(prefix);
        emit_code(END_CODE);
    }
    if (bit_cnt > 0) {
        packed.push_back(static_cast<uint8_t>(bit_buf & 0xFFu));
    }
    return packed;
}

inline void save_gif_loop(const std::vector<GrayImage>& frames,
                          const std::filesystem::path& path,
                          int fps = 24) {
    if (frames.empty()) throw std::runtime_error("save_gif_loop: empty frame list");
    uint16_t w = static_cast<uint16_t>(frames[0].w);
    uint16_t h = static_cast<uint16_t>(frames[0].h);
    uint16_t delay_cs = static_cast<uint16_t>(std::max(2, static_cast<int>(std::round(100.0 / std::max(1, fps)))));

    std::vector<uint8_t> out;
    out.reserve(frames.size() * static_cast<size_t>(w) * static_cast<size_t>(h) / 4 + 2048);

    auto emit_le16 = [&](uint16_t v) {
        out.push_back(static_cast<uint8_t>(v & 0xFFu));
        out.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
    };

    // GIF89a Header + Logical Screen Descriptor (Global Color Table 256 entries -> 0xF7)
    out.insert(out.end(), {'G', 'I', 'F', '8', '9', 'a'});
    emit_le16(w);
    emit_le16(h);
    out.push_back(0xF7);  // GCT=1, color res=7 (8-bit), sort=0, size=7 (2^(7+1)=256)
    out.push_back(0x00);  // Background color index
    out.push_back(0x00);  // Pixel aspect ratio

    // 256-level pure greyscale Global Color Table (R == G == B == i)
    for (int i = 0; i < 256; ++i) {
        uint8_t v = static_cast<uint8_t>(i);
        out.push_back(v);
        out.push_back(v);
        out.push_back(v);
    }

    // Netscape 2.0 Infinite Loop Application Extension
    out.insert(out.end(), {
        0x21, 0xFF, 0x0B,
        'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0',
        0x03, 0x01, 0x00, 0x00, 0x00
    });

    for (const auto& fr : frames) {
        // Graphic Control Extension
        out.insert(out.end(), {0x21, 0xF9, 0x04, 0x04});  // Disposal 1 (do not dispose)
        emit_le16(delay_cs);
        out.push_back(0x00);  // Transparent color index (unused)
        out.push_back(0x00);  // Block terminator

        // Image Descriptor
        out.push_back(0x2C);
        emit_le16(0);
        emit_le16(0);
        emit_le16(w);
        emit_le16(h);
        out.push_back(0x00);  // No local color table, non-interlaced

        // Image Data: LZW min code size = 8 + sub-blocks of <= 255 bytes
        out.push_back(0x08);
        auto lzw = lzw_encode_gif8(fr.px);
        size_t pos = 0;
        while (pos < lzw.size()) {
            size_t chunk = std::min<size_t>(255, lzw.size() - pos);
            out.push_back(static_cast<uint8_t>(chunk));
            out.insert(out.end(), lzw.begin() + static_cast<std::ptrdiff_t>(pos),
                       lzw.begin() + static_cast<std::ptrdiff_t>(pos + chunk));
            pos += chunk;
        }
        out.push_back(0x00);  // Sub-block terminator
    }

    out.push_back(0x3B);  // GIF Trailer

    std::filesystem::create_directories(path.parent_path());
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) throw std::runtime_error("failed to open GIF output: " + path.string());
    ofs.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()));
}

inline int count_gif_frames(const std::filesystem::path& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return 0;
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (b.size() < 14 || b[0] != 'G' || b[1] != 'I' || b[2] != 'F') return 0;
    size_t i = 13;
    if (b[10] & 0x80u) {
        size_t gct_entries = 1u << ((b[10] & 0x07u) + 1u);
        i += 3u * gct_entries;
    }
    auto skip_subblocks = [&]() {
        while (i < b.size()) {
            uint8_t sz = b[i++];
            if (sz == 0) break;
            i += sz;
        }
    };
    int frames = 0;
    while (i < b.size()) {
        uint8_t intro = b[i++];
        if (intro == 0x3Bu) {
            break;
        } else if (intro == 0x21u) {
            if (i >= b.size()) break;
            ++i;  // extension label
            skip_subblocks();
        } else if (intro == 0x2Cu) {
            if (i + 9 > b.size()) break;
            uint8_t packed = b[i + 8];
            i += 9;
            if (packed & 0x80u) {
                size_t lct_entries = 1u << ((packed & 0x07u) + 1u);
                i += 3u * lct_entries;
            }
            if (i >= b.size()) break;
            ++i;  // LZW minimum code size
            skip_subblocks();
            ++frames;
        } else {
            break;
        }
    }
    return frames;
}

// ── RFC 4648 Base64 Encoder ───────────────────────────────────────────────

inline std::string base64_encode(const std::vector<uint8_t>& data) {
    static constexpr char TBL[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);
    size_t i = 0;
    while (i + 2 < data.size()) {
        uint32_t v = (static_cast<uint32_t>(data[i]) << 16) |
                     (static_cast<uint32_t>(data[i + 1]) << 8) |
                     static_cast<uint32_t>(data[i + 2]);
        out.push_back(TBL[(v >> 18) & 0x3Fu]);
        out.push_back(TBL[(v >> 12) & 0x3Fu]);
        out.push_back(TBL[(v >> 6) & 0x3Fu]);
        out.push_back(TBL[v & 0x3Fu]);
        i += 3;
    }
    if (i < data.size()) {
        uint32_t v = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < data.size()) v |= static_cast<uint32_t>(data[i + 1]) << 8;
        out.push_back(TBL[(v >> 18) & 0x3Fu]);
        out.push_back(TBL[(v >> 12) & 0x3Fu]);
        out.push_back((i + 1 < data.size()) ? TBL[(v >> 6) & 0x3Fu] : '=');
        out.push_back('=');
    }
    return out;
}

}  // namespace mono_img
