// ═══════════════════════════════════════════════════════════════════════════
//  wallpapers/animate.cpp — Seamless Looping Live Wallpapers for `awww` (C++20)
//
//  Renders 256-level zero-drift greyscale looping GIFs across CPU threads:
//    • orbital   -> mono-live-01-orbital.gif   (Bauhaus celestial orrery)
//    • pulse     -> mono-live-02-pulse.gif     (Relativistic singularity)
//    • lissajous -> mono-live-03-lissajous.gif (Tektronix P4 oscilloscope)
//
//    ./bin/wallpaper-anim                            # 1280x720, 36f @ 24fps
//    ./bin/wallpaper-anim 1920 1080 --frames 48
//    ./bin/wallpaper-anim 160 90 --frames 6 --fps 12 --only orbital
// ═══════════════════════════════════════════════════════════════════════════

#include "../include/mono_image.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using mono_img::GrayImage;
using mono_img::clamp_u8;

namespace {

constexpr double TAU = 2.0 * std::numbers::pi;

GrayImage render_orbital_frame(int w, int h, double phase) {
    double k = std::min(w, h) / 1080.0;
    double cx = w * 0.5, cy = h * 0.5;
    double tau = TAU * phase;

    GrayImage img(w, h, 8);
    img.draw_line(w * 0.08, cy, w * 0.92, cy, 18, 1.0);
    img.draw_line(cx, h * 0.08, cx, h * 0.92, 18, 1.0);

    double r_core = std::min(w, h) * 0.17;

    GrayImage halo(w, h, 0);
    double hr = r_core * (1.26 + 0.06 * std::sin(tau));
    halo.fill_ellipse(cx, cy, hr, hr, 38);
    img.add_clamped(halo.gaussian_blur(26.0 * k));

    std::array<double, 3> orbits = {r_core * 1.48, r_core * 1.98, r_core * 2.52};
    std::array<uint8_t, 3> ring_cols = {42, 34, 26};
    for (size_t i = 0; i < 3; ++i) {
        img.stroke_ellipse(cx, cy, orbits[i], orbits[i], ring_cols[i], std::max(1.0, 1.3 * k));
    }

    double r_tick = orbits[1];
    for (int idx = 0; idx < 36; ++idx) {
        double ang = tau / 36.0 + idx * (TAU / 36.0);
        double r_in = r_tick - ((idx % 3 == 0) ? 5.0 * k : 2.5 * k);
        double r_out = r_tick + ((idx % 3 == 0) ? 5.0 * k : 2.5 * k);
        img.draw_line(cx + std::cos(ang) * r_in, cy + std::sin(ang) * r_in,
                      cx + std::cos(ang) * r_out, cy + std::sin(ang) * r_out,
                      (idx % 3 == 0) ? 58 : 32, 1.0);
    }

    GrayImage sphere(w, h, 0);
    GrayImage smask(w, h, 0);
    int ri = static_cast<int>(r_core);
    double light_shift = 0.08 * std::sin(tau);
    for (int dy = -ri; dy <= ri; ++dy) {
        double span = std::sqrt(std::max(0.0, r_core * r_core - dy * dy));
        double t = (dy + r_core) / (2.0 * r_core);
        uint8_t val = clamp_u8(std::clamp(224.0 * std::pow(std::max(0.0, 1.0 - t + light_shift), 1.35) + 14.0, 14.0, 232.0));
        if (dy > 0 && (static_cast<int>(dy / std::max(1.0, 4.2 * k)) % 2 == 0)) {
            val = 10;
        }
        sphere.draw_line(cx - span, cy + dy, cx + span, cy + dy, val, 1.0);
        smask.draw_line(cx - span, cy + dy, cx + span, cy + dy, 255, 1.0);
    }
    img = GrayImage::composite(sphere, img, smask);
    img.stroke_ellipse(cx, cy, r_core, r_core, 150, std::max(1.0, 1.4 * k));

    struct Moon { double rad, ang, mr; uint8_t col; };
    std::array<Moon, 3> moons = {{
        {orbits[0],  1.0 * tau - 0.6, 11.0 * k, 242},
        {orbits[1], -1.0 * tau + 1.8,  7.5 * k, 196},
        {orbits[2],  2.0 * tau + 0.4,  5.0 * k, 155},
    }};
    GrayImage glow(w, h, 0);
    for (const auto& m : moons) {
        double mx = cx + std::cos(m.ang) * m.rad;
        double my = cy + std::sin(m.ang) * m.rad;
        glow.fill_ellipse(mx, my, m.mr * 2.4, m.mr * 2.4, static_cast<uint8_t>(m.col * 0.38));
        img.fill_ellipse(mx, my, m.mr, m.mr, m.col);
    }
    img.add_clamped(glow.gaussian_blur(7.0 * k));
    return img;
}

GrayImage render_pulse_frame(int w, int h, double phase) {
    double k = std::min(w, h) / 1080.0;
    double cx = w * 0.5, cy = h * 0.5;
    double r0 = std::min(w, h) * 0.165;
    double tau = TAU * phase;

    GrayImage img(w, h, 4);
    std::mt19937 rng(1101);
    for (int i = 0; i < 180; ++i) {
        int sx = static_cast<int>(rng() % static_cast<uint32_t>(w));
        int sy = static_cast<int>(rng() % static_cast<uint32_t>(h));
        double rphase = (rng() / static_cast<double>(rng.max())) * TAU;
        int base_lum = 35 + static_cast<int>(rng() % 111);
        double twinkle = 0.72 + 0.28 * std::sin(tau + rphase);
        img.set_px(sx, sy, static_cast<uint8_t>(std::max(10, static_cast<int>(base_lum * twinkle))));
    }

    GrayImage halo(w, h, 0);
    double breath = 1.0 + 0.055 * std::sin(tau);
    for (int i = 28; i >= 1; --i) {
        double t = i / 28.0;
        double rad = r0 * (1.0 + t * 1.45 * breath);
        uint8_t lum = clamp_u8(205.0 * std::pow(1.0 - t, 2.3) * (0.92 + 0.08 * std::sin(tau)));
        halo.fill_ellipse(cx, cy, rad, rad * 0.93, lum);
    }
    halo = halo.gaussian_blur(18.0 * k);

    int diag = static_cast<int>(std::ceil(std::hypot(w, h)));
    if (diag % 2 != 0) ++diag;
    double dc = diag * 0.5;

    auto make_disc = [&](bool front) -> GrayImage {
        GrayImage layer(diag, diag, 0);
        for (int ring = 0; ring < 26; ++ring) {
            double f = ring / 25.0;
            double rx = r0 * (1.14 + f * 2.10);
            double ry = rx * 0.15;
            double s_wave = std::sin(f * 12.0 - tau);
            double base = 238.0 * std::exp(-f * 1.4) * (0.62 + 0.38 * s_wave * s_wave);
            double start_deg = front ? 0.0 : 180.0;
            for (int seg = 0; seg < 16; ++seg) {
                double a0 = start_deg + seg * 11.25;
                double a1 = a0 + 11.15;
                double mid = ((a0 + a1) * 0.5) * (std::numbers::pi / 180.0);
                double doppler = (0.92 - 0.24 * std::cos(mid)) * (0.86 + 0.14 * std::cos(2.0 * mid - tau));
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
    mask = mask.gaussian_blur(1.2 * k);
    img = GrayImage::composite(GrayImage(w, h, 2), img, mask);

    uint8_t ring_lum = clamp_u8(218.0 + 24.0 * std::sin(tau));
    img.stroke_ellipse(cx, cy, r0 * 1.012, r0 * 1.012, ring_lum, std::max(1.0, 2.0 * k));
    img.add_clamped(fg);
    return img;
}

GrayImage render_lissajous_frame(int w, int h, double phase) {
    double k = std::min(w, h) / 1080.0;
    double cx = w * 0.5, cy = h * 0.5;
    double tau = TAU * phase;

    GrayImage img(w, h, 6);
    int gx0 = static_cast<int>(w * 0.14), gx1 = static_cast<int>(w * 0.86);
    int gy0 = static_cast<int>(h * 0.12), gy1 = static_cast<int>(h * 0.88);
    img.stroke_rect(gx0, gy0, gx1, gy1, 22, 1);
    for (int div = 1; div < 8; ++div) {
        int x = gx0 + (gx1 - gx0) * div / 8;
        img.draw_line(x, gy0, x, gy1, (div == 4) ? 24 : 14, 1.0);
    }
    for (int div = 1; div < 6; ++div) {
        int y = gy0 + (gy1 - gy0) * div / 6;
        img.draw_line(gx0, y, gx1, y, (div == 3) ? 24 : 14, 1.0);
    }
    for (int t = -15; t <= 15; ++t) {
        if (t == 0) continue;
        int tx = static_cast<int>(cx + t * (gx1 - gx0) / 34.0);
        int ty = static_cast<int>(cy + t * (gy1 - gy0) / 34.0);
        img.draw_line(tx, cy - 3.0 * k, tx, cy + 3.0 * k, 34, 1.0);
        img.draw_line(cx - 3.0 * k, ty, cx + 3.0 * k, ty, 34, 1.0);
    }

    double rx = std::min(w, h) * 0.31, ry = std::min(w, h) * 0.31;
    GrayImage glow(w, h, 0);
    GrayImage core(w, h, 0);

    constexpr int STEPS = 360;
    std::vector<std::pair<double, double>> ghost_pts;
    std::vector<std::pair<double, double>> pts;
    ghost_pts.reserve(STEPS + 1);
    pts.reserve(STEPS + 1);
    for (int i = 0; i <= STEPS; ++i) {
        double s = TAU * (static_cast<double>(i) / STEPS);
        ghost_pts.emplace_back(cx + rx * 0.74 * std::sin(2.0 * s - tau),
                               cy + ry * 0.74 * std::cos(3.0 * s));
        pts.emplace_back(cx + rx * std::sin(3.0 * s + tau),
                         cy + ry * std::sin(2.0 * s));
    }
    core.draw_polyline(ghost_pts, 54, std::max(1.0, 1.2 * k));
    glow.draw_polyline(pts, 145, std::max(3.0, 6.0 * k));
    core.draw_polyline(pts, 236, std::max(1.0, 2.0 * k));

    double head_x = cx + rx * std::sin(tau);
    double head_y = cy;
    double hr = 5.5 * k;
    glow.fill_ellipse(head_x, head_y, hr * 3.0, hr * 3.0, 210);
    core.fill_ellipse(head_x, head_y, hr, hr, 255);

    img.add_clamped(glow.gaussian_blur(8.5 * k));
    img.add_clamped(core);
    return img;
}

struct AnimSpec {
    const char* key;
    const char* filename;
    const char* desc;
    GrayImage (*fn)(int, int, double);
};

constexpr std::array<AnimSpec, 3> ANIMATIONS = {{
    {"orbital",   "mono-live-01-orbital.gif",   "Bauhaus celestial orrery loop",                     render_orbital_frame},
    {"pulse",     "mono-live-02-pulse.gif",     "breathing relativistic singularity loop",           render_pulse_frame},
    {"lissajous", "mono-live-03-lissajous.gif", "P4 white-phosphor harmonic Lissajous oscilloscope", render_lissajous_frame},
}};

fs::path resolve_default_outdir(const char* argv0) {
    fs::path exe = fs::weakly_canonical(fs::path(argv0));
    fs::path parent = exe.parent_path();
    if (parent.filename() == "bin" && fs::is_directory(parent.parent_path() / "wallpapers")) {
        return parent.parent_path() / "wallpapers";
    }
    return parent.empty() ? fs::current_path() : parent;
}

}  // namespace

int main(int argc, char* argv[]) {
    int width = 1280;
    int height = 720;
    int nframes = 36;
    int fps = 24;
    std::string only;
    fs::path out_dir = resolve_default_outdir(argv[0]);

    std::vector<int> pos_dims;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--frames" && i + 1 < argc) {
            nframes = std::max(1, std::stoi(argv[++i]));
        } else if (arg == "--fps" && i + 1 < argc) {
            fps = std::max(1, std::stoi(argv[++i]));
        } else if (arg == "--only" && i + 1 < argc) {
            only = argv[++i];
        } else if (arg == "--out-dir" && i + 1 < argc) {
            out_dir = fs::path(argv[++i]);
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0]
                      << " [width] [height] [--frames N] [--fps F] [--only orbital|pulse|lissajous] [--out-dir DIR]\n";
            return 0;
        } else if (!arg.empty() && arg[0] != '-') {
            pos_dims.push_back(std::stoi(arg));
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        }
    }
    if (pos_dims.size() >= 1) width = pos_dims[0];
    if (pos_dims.size() >= 2) height = pos_dims[1];

    std::vector<const AnimSpec*> selected;
    if (!only.empty()) {
        for (const auto& a : ANIMATIONS) {
            if (only == a.key) selected.push_back(&a);
        }
        if (selected.empty()) {
            std::cerr << "unknown animation '" << only << "' (choose from: orbital, pulse, lissajous)\n";
            return 1;
        }
    } else {
        for (const auto& a : ANIMATIONS) selected.push_back(&a);
    }

    fs::create_directories(out_dir);
    unsigned hw = std::max(1u, std::thread::hardware_concurrency());

    for (const auto* spec : selected) {
        fs::path out = out_dir / spec->filename;
        std::cout << "Rendering " << spec->filename << " (" << width << "x" << height
                  << ", " << nframes << " frames @ " << fps << "fps — " << spec->desc << ")...\n";

        std::vector<GrayImage> frames(static_cast<size_t>(nframes));
        int nthreads = std::min(nframes, static_cast<int>(std::min(hw, 16u)));
        std::vector<std::thread> pool;
        pool.reserve(static_cast<size_t>(nthreads));
        for (int t = 0; t < nthreads; ++t) {
            pool.emplace_back([&, t]() {
                for (int idx = t; idx < nframes; idx += nthreads) {
                    frames[static_cast<size_t>(idx)] =
                        spec->fn(width, height, static_cast<double>(idx) / nframes);
                }
            });
        }
        for (auto& th : pool) th.join();

        mono_img::save_gif_loop(frames, out, fps);
        std::cout << "  wrote " << out.filename().string() << " ("
                  << (fs::file_size(out) / 1024) << " KB)\n";
    }
    return 0;
}
