// ═══════════════════════════════════════════════════════════════════════════
//  wallpapers/generate.cpp — Procedural Monochrome Wallpaper Studio (C++20)
//
//  Renders all 14 pure-greyscale procedural wallpapers at any resolution
//  from 1080p up to 16K (15360×8640), multi-threaded across CPU cores with
//  zero external libraries.
//
//    ./bin/wallpaper-gen                              # 1920x1080, all 14
//    ./bin/wallpaper-gen 3840 2160                    # 4K UHD
//    ./bin/wallpaper-gen 15360 8640 --only 11 --suffix -16k --quality 90
// ═══════════════════════════════════════════════════════════════════════════

#include "generate.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

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
    int width = 1920;
    int height = 1080;
    std::string only;
    std::string suffix;
    int quality = 92;
    fs::path out_dir = resolve_default_outdir(argv[0]);

    std::vector<int> pos_dims;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--only" && i + 1 < argc) {
            only = argv[++i];
        } else if (arg == "--suffix" && i + 1 < argc) {
            suffix = argv[++i];
        } else if (arg == "--quality" && i + 1 < argc) {
            quality = std::stoi(argv[++i]);
        } else if (arg == "--out-dir" && i + 1 < argc) {
            out_dir = fs::path(argv[++i]);
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0]
                      << " [width] [height] [--only N|stem] [--suffix S] [--quality Q] [--out-dir DIR]\n";
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
    if (width <= 0 || height <= 0) {
        std::cerr << "Invalid dimensions: " << width << "x" << height << "\n";
        return 1;
    }

    std::vector<const mono_wall::DesignSpec*> selected;
    if (!only.empty()) {
        const auto* spec = mono_wall::find_design(only);
        if (!spec) {
            std::cerr << "No wallpaper design matched --only '" << only << "'\n";
            return 1;
        }
        selected.push_back(spec);
    } else {
        for (const auto& d : mono_wall::DESIGNS) selected.push_back(&d);
    }

    fs::create_directories(out_dir);
    for (const auto* spec : selected) {
        fs::path out = out_dir / (std::string(spec->stem) + suffix + ".jpg");
        std::cout << "Rendering " << out.filename().string() << " (" << width << "x" << height
                  << " — " << spec->title << ")...\n";
        auto img = spec->fn(width, height);
        mono_img::save_jpeg(img, out, quality);
        std::cout << "  wrote " << out.filename().string() << " ("
                  << (fs::file_size(out) / 1024) << " KB)\n";
    }
    return 0;
}
