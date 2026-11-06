#pragma once
// ═══════════════════════════════════════════════════════════════════════════
//  wallpapers/generate.hpp — 14 Procedural Monochrome Designs in C++20
//
//  Shared between `wallpapers/generate.cpp` (standalone 1080p..16K renderer)
//  and `tools/build-preview.cpp` (deterministic Studio gallery thumbnails).
// ═══════════════════════════════════════════════════════════════════════════

#include "../include/mono_image.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <numbers>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace mono_wall {

using mono_img::GrayImage;
using mono_img::clamp_u8;

namespace detail {

inline uint64_t splitmix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

inline double rand01(uint64_t seed, int x, int y, uint64_t salt = 0) {
    uint64_t h = seed ^ (static_cast<uint64_t>(static_cast<uint32_t>(x)) * 0x9e3779b185ebca87ULL) ^
                 (static_cast<uint64_t>(static_cast<uint32_t>(y)) * 0xc2b2ae3d27d4eb4fULL) ^ salt;
    return static_cast<double>(splitmix64(h) >> 11) * (1.0 / 9007199254740992.0);
}

inline double gauss_px(uint64_t seed, int x, int y, double sigma) {
    double u1 = std::max(1e-12, rand01(seed, x, y, 0x11ULL));
    double u2 = rand01(seed, x, y, 0x22ULL);
    double z = std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * std::numbers::pi * u2);
    return z * sigma;
}

template <typename RowFn>
inline void parallel_rows(int h, RowFn&& fn) {
    unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    int nthreads = std::min(h, static_cast<int>(std::min(hw, 16u)));
    if (nthreads <= 1 || h < 64) {
        for (int y = 0; y < h; ++y) fn(y);
        return;
    }
    std::vector<std::thread> pool;
    pool.reserve(static_cast<size_t>(nthreads));
    int chunk = (h + nthreads - 1) / nthreads;
    for (int t = 0; t < nthreads; ++t) {
        int y0 = t * chunk;
        int y1 = std::min(h, y0 + chunk);
        if (y0 >= y1) break;
        pool.emplace_back([y0, y1, &fn]() {
            for (int y = y0; y < y1; ++y) fn(y);
        });
    }
    for (auto& th : pool) th.join();
}

inline void add_grain(GrayImage& img, double sigma, uint64_t seed = 42) {
    int w = img.w;
    parallel_rows(img.h, [&](int y) {
        uint8_t* row = img.px.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        for (int x = 0; x < w; ++x) {
            row[x] = clamp_u8(static_cast<double>(row[x]) + gauss_px(seed, x, y, sigma));
        }
    });
}

inline void apply_vignette(GrayImage& img, double strength = 0.45) {
    int w = img.w, h = img.h;
    double cx = w * 0.5, cy = h * 0.5;
    double max_d = std::hypot(cx, cy);
    parallel_rows(h, [&](int y) {
        double dy = y - cy;
        uint8_t* row = img.px.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        for (int x = 0; x < w; ++x) {
            double r = std::hypot(x - cx, dy) / max_d;
            double factor = std::max(0.0, 1.0 - strength * (r * r));
            row[x] = clamp_u8(static_cast<double>(row[x]) * factor);
        }
    });
}

inline double value_noise(double x, double y, uint64_t seed) {
    int xi = static_cast<int>(std::floor(x));
    int yi = static_cast<int>(std::floor(y));
    double xf = x - xi;
    double yf = y - yi;
    double u = xf * xf * (3.0 - 2.0 * xf);
    double v = yf * yf * (3.0 - 2.0 * yf);
    double n00 = rand01(seed, xi, yi);
    double n10 = rand01(seed, xi + 1, yi);
    double n01 = rand01(seed, xi, yi + 1);
    double n11 = rand01(seed, xi + 1, yi + 1);
    double nx0 = n00 + (n10 - n00) * u;
    double nx1 = n01 + (n11 - n01) * u;
    return nx0 + (nx1 - nx0) * v;
}

inline double fbm2d(double x, double y, uint64_t seed, int octaves = 5) {
    double sum = 0.0, amp = 0.5, norm = 0.0, freq = 1.0;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * value_noise(x * freq, y * freq, seed + static_cast<uint64_t>(i) * 131ULL);
        norm += amp;
        amp *= 0.5;
        freq *= 2.02;
    }
    return sum / norm;
}

}  // namespace detail

// ── 01 · Noir Vignette ────────────────────────────────────────────────────
inline GrayImage design_01_vignette(int w, int h) {
    GrayImage img(w, h, 0);
    double cx = w * 0.5, cy = h * 0.5;
    double max_r = std::hypot(cx, cy);
    detail::parallel_rows(h, [&](int y) {
        double dy = (y - cy) * 1.12;
        uint8_t* row = img.px.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        for (int x = 0; x < w; ++x) {
            double t = std::min(1.0, std::hypot(x - cx, dy) / max_r);
            double s = t * t * (3.0 - 2.0 * t);
            double v = 30.0 - 22.0 * s + detail::gauss_px(101, x, y, 1.9);
            row[x] = clamp_u8(v);
        }
    });
    return img;
}

// ── 02 · Architectural Grid ───────────────────────────────────────────────
inline GrayImage design_02_grid(int w, int h) {
    GrayImage img(w, h, 12);
    int step = std::max(28, static_cast<int>(std::round(std::min(w, h) / 18.0)));
    int major = step * 4;
    for (int x = 0; x < w; x += step) {
        uint8_t col = (x % major == 0) ? 28 : 18;
        img.draw_line(x, 0, x, h - 1, col, 1.0);
    }
    for (int y = 0; y < h; y += step) {
        uint8_t col = (y % major == 0) ? 28 : 18;
        img.draw_line(0, y, w - 1, y, col, 1.0);
    }
    for (int y = 0; y < h; y += major) {
        for (int x = 0; x < w; x += major) {
            img.draw_line(x - 3, y, x + 3, y, 52, 1.0);
            img.draw_line(x, y - 3, x, y + 3, 52, 1.0);
        }
    }
    detail::apply_vignette(img, 0.55);
    detail::add_grain(img, 1.3, 202);
    return img;
}

// ── 03 · Concentric Interference Arcs ─────────────────────────────────────
inline GrayImage design_03_arcs(int w, int h) {
    GrayImage img(w, h, 11);
    double cx = w * 1.08, cy = h * 1.12;
    double max_r = std::hypot(w, h) * 1.25;
    double spacing = std::max(14.0, std::min(w, h) / 26.0);
    detail::parallel_rows(h, [&](int y) {
        uint8_t* row = img.px.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        for (int x = 0; x < w; ++x) {
            double d = std::hypot(x - cx, y - cy);
            double phase = std::fmod(d, spacing) / spacing;
            double line = std::exp(-std::pow((phase - 0.5) * 9.5, 2.0));
            double falloff = std::clamp(1.0 - (d / max_r) * 0.72, 0.15, 1.0);
            double base = 11.0 + 9.0 * (1.0 - d / max_r);
            double v = base + 42.0 * line * falloff + detail::gauss_px(303, x, y, 1.2);
            row[x] = clamp_u8(v);
        }
    });
    return img;
}

// ── 04 · Topographic Contours ─────────────────────────────────────────────
inline GrayImage design_04_contours(int w, int h) {
    GrayImage img(w, h, 12);
    double scale = 4.2 / std::min(w, h);
    detail::parallel_rows(h, [&](int y) {
        uint8_t* row = img.px.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        for (int x = 0; x < w; ++x) {
            double elev = detail::fbm2d(x * scale, y * scale, 404, 5);
            double bands = elev * 15.0;
            double frac = bands - std::floor(bands);
            bool major = (static_cast<int>(std::floor(bands)) % 4 == 0);
            double dist = std::min(frac, 1.0 - frac);
            double sharp = std::exp(-std::pow(dist * (major ? 22.0 : 30.0), 2.0));
            double v = 11.0 + 14.0 * elev + (major ? 48.0 : 26.0) * sharp +
                       detail::gauss_px(405, x, y, 1.1);
            row[x] = clamp_u8(v);
        }
    });
    detail::apply_vignette(img, 0.42);
    return img;
}

// ── 05 · Diagonal Brushed Graphite Strata ─────────────────────────────────
inline GrayImage design_05_diagonal(int w, int h) {
    GrayImage img(w, h, 12);
    double period = std::max(40.0, std::min(w, h) / 7.5);
    detail::parallel_rows(h, [&](int y) {
        uint8_t* row = img.px.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        for (int x = 0; x < w; ++x) {
            double u = (x * 0.70710678 + y * 0.70710678) / period;
            double f = u - std::floor(u);
            double saw = 0.25 + 0.75 * std::pow(1.0 - f, 1.8);
            double streak = detail::value_noise((x + y) * 0.18, (x - y) * 0.015, 505);
            double edge = std::exp(-std::pow(f * 36.0, 2.0));
            double v = 10.0 + 24.0 * saw + 8.0 * streak + 22.0 * edge +
                       detail::gauss_px(506, x, y, 1.3);
            row[x] = clamp_u8(v);
        }
    });
    detail::apply_vignette(img, 0.45);
    return img;
}

// ── 06 · Horizon Mist (16K-ready atmospheric horizon) ─────────────────────
inline GrayImage design_06_horizon(int w, int h) {
    GrayImage img(w, h, 0);
    double hy = h * 0.62;
    detail::parallel_rows(h, [&](int y) {
        uint8_t* row = img.px.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        double dy = (y - hy) / static_cast<double>(h);
        double sky_or_sea = (y < hy)
            ? (12.0 + 52.0 * std::pow(1.0 - std::min(1.0, (hy - y) / hy), 2.8))
            : (9.0 + 36.0 * std::exp(-dy * 9.5));
        double mist_band = 58.0 * std::exp(-std::pow(dy * 18.0, 2.0));
        for (int x = 0; x < w; ++x) {
            double horiz_falloff = 1.0 - 0.34 * std::pow((x - w * 0.5) / (w * 0.5), 2.0);
            double ripple = (y >= hy)
                ? (3.5 * std::sin(y * 0.35 + detail::value_noise(x * 0.02, y * 0.08, 606) * 6.0) *
                   std::exp(-dy * 6.0))
                : 0.0;
            double v = (sky_or_sea + mist_band * horiz_falloff) + ripple +
                       detail::gauss_px(607, x, y, 1.35);
            row[x] = clamp_u8(v);
        }
    });
    return img;
}

// ── 07 · Solar Eclipse (off-axis corona & specular diamond rim) ───────────
inline GrayImage design_07_eclipse(int w, int h) {
    double k = std::min(w, h) / 1080.0;
    double cx = w * 0.56, cy = h * 0.46;
    double r = std::min(w, h) * 0.225;

    GrayImage img(w, h, 5);
    std::mt19937 rng(707);
    for (int i = 0; i < 220; ++i) {
        int sx = static_cast<int>(rng() % static_cast<uint32_t>(w));
        int sy = static_cast<int>(rng() % static_cast<uint32_t>(h));
        if (std::hypot(sx - cx, sy - cy) > r * 1.12) {
            img.set_px(sx, sy, static_cast<uint8_t>(24 + (rng() % 90)));
        }
    }

    // Broad atmospheric corona + off-axis sunward bias (-32 deg)
    double sun_ang = -32.0 * (std::numbers::pi / 180.0);
    double lx = std::cos(sun_ang), ly = std::sin(sun_ang);
    detail::parallel_rows(h, [&](int y) {
        uint8_t* row = img.px.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        for (int x = 0; x < w; ++x) {
            double dx = x - cx, dy = y - cy;
            double dist = std::hypot(dx, dy);
            if (dist < r * 0.995) {
                row[x] = 4;
                continue;
            }
            double nd = (dist - r) / r;
            double dir = (dx * lx + dy * ly) / std::max(1.0, dist);
            double bias = 0.55 + 0.45 * std::max(0.0, dir);
            double corona = 175.0 * std::exp(-nd * 3.4) * bias + 42.0 * std::exp(-nd * 0.95);
            double rim = 235.0 * std::exp(-std::pow(nd * 48.0, 2.0)) * (0.28 + 0.72 * std::pow(std::max(0.0, (dir + 0.35) / 1.35), 2.4));
            // Diamond-ring flare right at sun_ang
            double fx = cx + lx * r, fy = cy + ly * r;
            double fdist = std::hypot(x - fx, y - fy) / std::max(1.0, 26.0 * k);
            double flare = 120.0 * std::exp(-fdist * fdist * 1.8);
            double v = static_cast<double>(row[x]) + corona + rim + flare +
                       detail::gauss_px(708, x, y, 1.15);
            row[x] = clamp_u8(v);
        }
    });
    return img;
}

// ── 08 · Alpine Ridgeline (7-layer silhouette · silver mist) ──────────────
inline GrayImage design_08_ridgeline(int w, int h) {
    GrayImage img(w, h, 0);
    detail::parallel_rows(h, [&](int y) {
        uint8_t* row = img.px.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        double t = static_cast<double>(y) / std::max(1, h - 1);
        uint8_t sky = clamp_u8(14.0 + 48.0 * std::pow(t, 1.65));
        for (int x = 0; x < w; ++x) row[x] = sky;
    });

    constexpr int LAYERS = 7;
    for (int layer = 0; layer < LAYERS; ++layer) {
        double depth = static_cast<double>(layer) / (LAYERS - 1);  // 0 = far, 1 = near
        double base_y = h * (0.34 + 0.48 * depth);
        double amp = h * (0.14 - 0.055 * depth);
        uint8_t ridge_col = clamp_u8(54.0 * std::pow(1.0 - depth, 1.25) + 6.0);

        std::vector<double> crest(static_cast<size_t>(w));
        for (int x = 0; x < w; ++x) {
            double nx = (static_cast<double>(x) / w) * (3.2 + layer * 0.9);
            double n = detail::fbm2d(nx, layer * 7.3 + 1.1, 808 + static_cast<uint64_t>(layer) * 37ULL, 5);
            double jagged = std::abs(detail::value_noise(nx * 4.5, layer * 3.1, 899) - 0.5) * 0.35;
            crest[static_cast<size_t>(x)] = base_y - (n - 0.35 + jagged) * amp;
        }

        // Silver mist rising behind this ridge before painting the next closer layer
        detail::parallel_rows(h, [&](int y) {
            uint8_t* row = img.px.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
            for (int x = 0; x < w; ++x) {
                double cy = crest[static_cast<size_t>(x)];
                if (y >= cy) {
                    double below = (y - cy) / std::max(1.0, h * 0.14);
                    double mist = (1.0 - depth) * 26.0 * std::exp(-below * 3.2);
                    row[x] = clamp_u8(ridge_col + mist);
                }
            }
        });
    }
    detail::add_grain(img, 1.25, 818);
    return img;
}

// ── 09 · Brutalist Monoliths (side-lit architectural slabs) ───────────────
inline GrayImage design_09_monoliths(int w, int h) {
    GrayImage img(w, h, 0);
    detail::parallel_rows(h, [&](int y) {
        uint8_t* row = img.px.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        double t = static_cast<double>(y) / std::max(1, h - 1);
        uint8_t bg = clamp_u8(10.0 + 18.0 * (1.0 - t));
        for (int x = 0; x < w; ++x) row[x] = bg;
    });

    struct Slab { double cx, top_y, bw, skew; uint8_t front, side; };
    std::array<Slab, 5> slabs = {{
        {0.23, 0.24, 0.11, 0.035, 34, 14},
        {0.76, 0.28, 0.13, 0.038, 30, 12},
        {0.40, 0.16, 0.12, 0.040, 48, 17},
        {0.61, 0.20, 0.10, 0.034, 42, 15},
        {0.50, 0.11, 0.115, 0.044, 68, 20},
    }};

    for (const auto& s : slabs) {
        double x0 = (s.cx - s.bw * 0.5) * w;
        double x1 = (s.cx + s.bw * 0.5) * w;
        double x2 = x1 + s.skew * w;
        double y0 = s.top_y * h;
        double drop = s.skew * h * 0.65;

        // Lit left/front face
        img.fill_polygon({{x0, y0 + drop}, {x1, y0}, {x1, static_cast<double>(h)}, {x0, static_cast<double>(h)}}, s.front);
        // Shadowed right return face
        img.fill_polygon({{x1, y0}, {x2, y0 + drop * 1.3}, {x2, static_cast<double>(h)}, {x1, static_cast<double>(h)}}, s.side);
        // Crisp specular arris edge along x1
        img.draw_line(x1, y0, x1, h, static_cast<uint8_t>(std::min(255, static_cast<int>(s.front) + 52)), 1.3);
    }

    // Ground mist fade
    detail::parallel_rows(h, [&](int y) {
        if (y < h * 0.58) return;
        double m = std::pow((y - h * 0.58) / (h * 0.42), 2.1);
        uint8_t* row = img.px.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        for (int x = 0; x < w; ++x) {
            row[x] = clamp_u8(row[x] * (1.0 - 0.65 * m) + 9.0 * m);
        }
    });
    detail::apply_vignette(img, 0.4);
    detail::add_grain(img, 1.3, 909);
    return img;
}

// ── 10 · Caustic Silk Ribbon (76-strand harmonic silver wave) ─────────────
inline GrayImage design_10_ribbon(int w, int h) {
    double k = std::min(w, h) / 1080.0;
    GrayImage img(w, h, 8);
    GrayImage accum(w, h, 0);

    constexpr int STRANDS = 76;
    int steps = std::max(280, w / 3);
    for (int s = 0; s < STRANDS; ++s) {
        double u = static_cast<double>(s) / (STRANDS - 1);  // 0..1
        double env = std::sin(u * std::numbers::pi);
        uint8_t lum = clamp_u8(18.0 + 185.0 * std::pow(env, 1.7));
        std::vector<std::pair<double, double>> pts;
        pts.reserve(static_cast<size_t>(steps + 1));
        for (int i = 0; i <= steps; ++i) {
            double t = static_cast<double>(i) / steps;
            double x = t * w;
            double wave1 = std::sin(t * 5.2 + u * 2.35) * h * 0.19;
            double wave2 = std::cos(t * 8.4 - u * 3.80) * h * 0.085;
            double twist = (u - 0.5) * h * (0.28 * std::cos(t * 3.6 + 0.6));
            double y = h * 0.52 + wave1 + wave2 + twist;
            pts.emplace_back(x, y);
        }
        GrayImage strand(w, h, 0);
        strand.draw_polyline(pts, static_cast<uint8_t>(lum / 5), std::max(1.0, 1.35 * k));
        accum.add_clamped(strand);
    }

    GrayImage bloom = accum.gaussian_blur(14.0 * k);
    for (uint8_t& p : bloom.px) p = static_cast<uint8_t>(p * 0.45);
    img.add_clamped(bloom);
    img.add_clamped(accum);
    detail::apply_vignette(img, 0.35);
    detail::add_grain(img, 1.0, 1010);
    return img;
}

// ── 11 · Gravitational Singularity (relativistic accretion & photon ring) ─
inline GrayImage design_11_singularity(int w, int h) {
    double k = std::min(w, h) / 1080.0;
    double cx = w * 0.5, cy = h * 0.5;
    double r0 = std::min(w, h) * 0.165;

    GrayImage img(w, h, 4);
    std::mt19937 rng(1111);
    for (int i = 0; i < 240; ++i) {
        int sx = static_cast<int>(rng() % static_cast<uint32_t>(w));
        int sy = static_cast<int>(rng() % static_cast<uint32_t>(h));
        img.set_px(sx, sy, static_cast<uint8_t>(28 + (rng() % 110)));
    }

    GrayImage halo(w, h, 0);
    for (int i = 28; i >= 1; --i) {
        double t = i / 28.0;
        double rad = r0 * (1.0 + t * 1.48);
        uint8_t lum = clamp_u8(210.0 * std::pow(1.0 - t, 2.35));
        halo.fill_ellipse(cx, cy, rad, rad * 0.93, lum);
    }
    halo = halo.gaussian_blur(18.0 * k);

    int diag = static_cast<int>(std::ceil(std::hypot(w, h)));
    if (diag % 2 != 0) ++diag;
    double dc = diag * 0.5;

    auto make_disc = [&](bool front) -> GrayImage {
        GrayImage layer(diag, diag, 0);
        for (int ring = 0; ring < 28; ++ring) {
            double f = ring / 27.0;
            double rx = r0 * (1.14 + f * 2.12);
            double ry = rx * 0.15;
            double base = 242.0 * std::exp(-f * 1.38) * (0.66 + 0.34 * std::pow(std::sin(f * 13.0), 2.0));
            double start_deg = front ? 0.0 : 180.0;
            for (int seg = 0; seg < 16; ++seg) {
                double a0 = start_deg + seg * 11.25;
                double a1 = a0 + 11.15;
                double mid = ((a0 + a1) * 0.5) * (std::numbers::pi / 180.0);
                double doppler = 0.94 - 0.26 * std::cos(mid);
                uint8_t lum = clamp_u8(std::clamp(base * doppler, 0.0, 252.0));
                layer.stroke_arc(dc, dc, rx, ry, a0, a1, lum, std::max(2.0, 2.6 * k));
            }
        }
        return layer.gaussian_blur(1.5 * k).rotate_deg(14.0);
    };

    int ox = (diag - w) / 2, oy = (diag - h) / 2;
    GrayImage bg = make_disc(false).crop(ox, oy, w, h);
    GrayImage fg = make_disc(true).crop(ox, oy, w, h);

    img.add_clamped(halo);
    img.add_clamped(bg);

    GrayImage mask(w, h, 0);
    mask.fill_ellipse(cx, cy, r0, r0, 255);
    mask = mask.gaussian_blur(1.3 * k);
    img = GrayImage::composite(GrayImage(w, h, 2), img, mask);

    img.stroke_ellipse(cx, cy, r0 * 1.012, r0 * 1.012, 236, std::max(1.2, 2.0 * k));
    img.add_clamped(fg);
    detail::add_grain(img, 0.9, 1112);
    return img;
}

// ── 12 · Liquid Mercury (Blinn-Phong specular ferrofluid chrome) ──────────
inline GrayImage design_12_mercury(int w, int h) {
    GrayImage img(w, h, 0);
    double inv = 1.0 / std::min(w, h);
    auto height_fn = [&](double nx, double ny) -> double {
        double wx = nx + 0.24 * std::sin(ny * 7.2) + 0.12 * std::cos((nx + ny) * 11.0);
        double wy = ny + 0.24 * std::cos(nx * 6.8) + 0.12 * std::sin((nx - ny) * 10.5);
        double r = std::hypot(wx - 0.52, wy - 0.48);
        return 0.55 * std::sin(r * 22.0 - 0.8) * std::exp(-r * 1.4) +
               0.45 * detail::fbm2d(wx * 5.2, wy * 5.2, 1212, 4);
    };

    double eps = 2.0 * inv;
    // Normalized light L = (-0.45, -0.55, 0.703) & half-vector H with V=(0,0,1)
    double lx = -0.45, ly = -0.55, lz = 0.703;
    double hx = lx, hy = ly, hz = lz + 1.0;
    double hlen = std::sqrt(hx * hx + hy * hy + hz * hz);
    hx /= hlen; hy /= hlen; hz /= hlen;

    detail::parallel_rows(h, [&](int y) {
        uint8_t* row = img.px.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        double ny = y * inv;
        for (int x = 0; x < w; ++x) {
            double nx = x * inv;
            double dhx = (height_fn(nx + eps, ny) - height_fn(nx - eps, ny)) / (2.0 * eps);
            double dhy = (height_fn(nx, ny + eps) - height_fn(nx, ny - eps)) / (2.0 * eps);
            double sx = -dhx * 0.42, sy = -dhy * 0.42, sz = 1.0;
            double slen = std::sqrt(sx * sx + sy * sy + sz * sz);
            sx /= slen; sy /= slen; sz /= slen;

            double ndotl = std::max(0.0, sx * lx + sy * ly + sz * lz);
            double ndoth = std::max(0.0, sx * hx + sy * hy + sz * hz);
            double fresnel = std::pow(1.0 - sz, 3.0);
            // Studio softbox reflection bands on reflected normal
            double env = 0.5 + 0.5 * std::sin((sx * 3.4 + sy * 2.2) * 3.14159);
            double lum = 8.0 + 38.0 * ndotl + 46.0 * env * (0.35 + 0.65 * fresnel) +
                         168.0 * std::pow(ndoth, 28.0) + detail::gauss_px(1213, x, y, 0.85);
            row[x] = clamp_u8(lum);
        }
    });
    detail::apply_vignette(img, 0.38);
    return img;
}

// ── 13 · Rain & Bokeh Noir (shallow-DOF optical bokeh & silver rain) ──────
inline GrayImage design_13_rain_bokeh(int w, int h) {
    double k = std::min(w, h) / 1080.0;
    GrayImage img(w, h, 7);

    // Subtle diagonal volumetric streetlamp shafts
    GrayImage shafts(w, h, 0);
    shafts.fill_polygon({{w * 0.18, 0.0}, {w * 0.34, 0.0}, {w * 0.62, static_cast<double>(h)}, {w * 0.36, static_cast<double>(h)}}, 18);
    shafts.fill_polygon({{w * 0.58, 0.0}, {w * 0.72, 0.0}, {w * 0.96, static_cast<double>(h)}, {w * 0.74, static_cast<double>(h)}}, 14);
    img.add_clamped(shafts.gaussian_blur(36.0 * k));

    // Out-of-focus optical bokeh discs with bright spherical-aberration rims
    GrayImage bokeh(w, h, 0);
    std::mt19937 rng(1313);
    for (int i = 0; i < 34; ++i) {
        double bx = (0.06 + 0.88 * (rng() / static_cast<double>(rng.max()))) * w;
        double by = (0.14 + 0.72 * (rng() / static_cast<double>(rng.max()))) * h;
        double br = (14.0 + 54.0 * std::pow(rng() / static_cast<double>(rng.max()), 1.6)) * k;
        uint8_t fill_lum = static_cast<uint8_t>(10 + (rng() % 28));
        GrayImage disc(w, h, 0);
        disc.fill_ellipse(bx, by, br, br, fill_lum);
        disc.stroke_ellipse(bx, by, br, br, static_cast<uint8_t>(fill_lum + 18), std::max(1.2, 2.0 * k));
        bokeh.add_clamped(disc);
    }
    img.add_clamped(bokeh.gaussian_blur(4.5 * k));

    // Crisp foreground silver rain streaks (-11 deg)
    GrayImage rain(w, h, 0);
    double dx_unit = std::sin(-11.0 * std::numbers::pi / 180.0);
    double dy_unit = std::cos(-11.0 * std::numbers::pi / 180.0);
    for (int i = 0; i < 190; ++i) {
        double rx = (rng() / static_cast<double>(rng.max())) * w;
        double ry = (rng() / static_cast<double>(rng.max())) * h;
        double len = (24.0 + (rng() % 85)) * k;
        uint8_t col = static_cast<uint8_t>(22 + (rng() % 68));
        rain.draw_line(rx, ry, rx + dx_unit * len, ry + dy_unit * len, col, std::max(1.0, 1.1 * k));
    }
    img.add_clamped(rain);
    detail::apply_vignette(img, 0.42);
    detail::add_grain(img, 1.2, 1314);
    return img;
}

// ── 14 · Bauhaus Compass (Swiss architectural drafting & golden ratio) ────
inline GrayImage design_14_bauhaus(int w, int h) {
    double k = std::min(w, h) / 1080.0;
    double cx = w * 0.5, cy = h * 0.5;
    double r = std::min(w, h) * 0.28;

    GrayImage img(w, h, 10);

    // Fine architectural drafting grid & crosshairs
    int step = std::max(24, static_cast<int>(std::round(64.0 * k)));
    for (int x = static_cast<int>(cx) % step; x < w; x += step) img.draw_line(x, 0, x, h - 1, 15, 1.0);
    for (int y = static_cast<int>(cy) % step; y < h; y += step) img.draw_line(0, y, w - 1, y, 15, 1.0);
    img.draw_line(w * 0.06, cy, w * 0.94, cy, 34, 1.0);
    img.draw_line(cx, h * 0.06, cx, h * 0.94, 34, 1.0);
    img.draw_line(cx - r * 1.45, cy - r * 1.45, cx + r * 1.45, cy + r * 1.45, 24, 1.0);

    // Golden-ratio framing rectangle & compass rings
    double phi = 1.61803398875;
    img.stroke_rect(static_cast<int>(cx - r * phi * 0.78), static_cast<int>(cy - r * 0.78),
                    static_cast<int>(cx + r * phi * 0.78), static_cast<int>(cy + r * 0.78), 32, 1);

    img.stroke_ellipse(cx, cy, r * 1.28, r * 1.28, 46, std::max(1.0, 1.3 * k));
    img.stroke_ellipse(cx, cy, r * 0.98, r * 0.98, 72, std::max(1.0, 1.5 * k));
    img.stroke_arc(cx - r * 0.38, cy, r * 1.15, r * 1.15, -62.0, 62.0, 56, std::max(1.0, 1.3 * k));

    // Degree vernier ticks around outer ring
    for (int deg = 0; deg < 360; deg += 5) {
        double rad = deg * (std::numbers::pi / 180.0);
        bool major = (deg % 30 == 0);
        double r_in = r * 1.28 - (major ? 8.0 * k : 3.5 * k);
        double r_out = r * 1.28 + (major ? 8.0 * k : 3.5 * k);
        img.draw_line(cx + std::cos(rad) * r_in, cy + std::sin(rad) * r_in,
                      cx + std::cos(rad) * r_out, cy + std::sin(rad) * r_out,
                      major ? 92 : 40, 1.0);
    }

    // Venetian-sliced monochrome sphere off-center left + solid obsidian counter-disc
    double scx = cx - r * 0.22, scy = cy - r * 0.04, sr = r * 0.56;
    int sri = static_cast<int>(std::ceil(sr));
    for (int dy = -sri; dy <= sri; ++dy) {
        double span = std::sqrt(std::max(0.0, sr * sr - dy * dy));
        double t = (dy + sr) / (2.0 * sr);
        uint8_t val = clamp_u8(16.0 + 212.0 * std::pow(1.0 - t, 1.35));
        if (dy > 0 && (static_cast<int>(dy / std::max(1.0, 4.5 * k)) % 2 == 0)) continue;
        img.draw_line(scx - span, scy + dy, scx + span, scy + dy, val, 1.0);
    }
    img.stroke_ellipse(scx, scy, sr, sr, 165, std::max(1.0, 1.4 * k));

    // Crisp Bauhaus geometric counter-accent node
    double ncx = cx + r * 0.62, ncy = cy - r * 0.38;
    img.fill_ellipse(ncx, ncy, 15.0 * k, 15.0 * k, 12);
    img.stroke_ellipse(ncx, ncy, 15.0 * k, 15.0 * k, 215, std::max(1.0, 1.6 * k));
    img.fill_ellipse(ncx, ncy, 4.2 * k, 4.2 * k, 240);

    detail::apply_vignette(img, 0.35);
    detail::add_grain(img, 1.05, 1414);
    return img;
}

struct DesignSpec {
    const char* stem;
    const char* title;
    const char* subtitle;
    GrayImage (*fn)(int, int);
};

inline constexpr std::array<DesignSpec, 14> DESIGNS = {{
    {"mono-01", "Noir Vignette",             "Radial obsidian falloff · film grain",          design_01_vignette},
    {"mono-02", "Architectural Grid",        "Sub-pixel drafting lines · vignette",           design_02_grid},
    {"mono-03", "Concentric Arcs",           "Off-axis Interference rings",                   design_03_arcs},
    {"mono-04", "Topographic Contours",      "Procedural elevation isolines",                 design_04_contours},
    {"mono-05", "Diagonal Strata",           "45° brushed graphite bands",                    design_05_diagonal},
    {"mono-06", "Horizon Mist",              "16K-ready atmospheric horizon",                 design_06_horizon},
    {"mono-07", "Solar Eclipse",             "4K/16K off-axis corona & rim light",            design_07_eclipse},
    {"mono-08", "Alpine Ridgeline",          "7-layer silhouette · silver mist",              design_08_ridgeline},
    {"mono-09", "Brutalist Monoliths",       "Side-lit architectural slabs",                  design_09_monoliths},
    {"mono-10", "Caustic Silk Ribbon",       "76-strand harmonic silver wave",                design_10_ribbon},
    {"mono-11", "Gravitational Singularity", "4K relativistic accretion halo & photon ring",  design_11_singularity},
    {"mono-12", "Liquid Mercury",            "Blinn-Phong specular ferrofluid chrome",        design_12_mercury},
    {"mono-13", "Rain & Bokeh Noir",         "Shallow-DOF optical bokeh & silver rain",       design_13_rain_bokeh},
    {"mono-14", "Bauhaus Compass",           "Swiss architectural drafting & golden ratio",   design_14_bauhaus},
}};

inline const DesignSpec* find_design(const std::string& query) {
    if (query.empty()) return nullptr;
    // Allow bare number ("3" -> "mono-03"), "06" -> "mono-06", or substring/exact stem
    bool all_digits = true;
    for (char c : query) {
        if (c < '0' || c > '9') { all_digits = false; break; }
    }
    if (all_digits) {
        int idx = std::stoi(query);
        if (idx >= 1 && idx <= static_cast<int>(DESIGNS.size())) {
            return &DESIGNS[static_cast<size_t>(idx - 1)];
        }
        return nullptr;
    }
    for (const auto& d : DESIGNS) {
        std::string stem(d.stem);
        if (stem == query || stem.find(query) != std::string::npos) return &d;
    }
    return nullptr;
}

}  // namespace mono_wall
