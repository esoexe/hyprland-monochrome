// ═══════════════════════════════════════════════════════════════════════════
//  tools/mono-util.cpp — Native C++20 verification & build helpers
//
//  Replaces every inline Python snippet across test suites & desktop tooling:
//    mono-util check-configs <repo>            validate JSON/JSONC + TOML
//    mono-util check-greyscale <repo>          verify R==G==B across toolkits
//    mono-util image-size <file.jpg>           print WxH of a JPEG file
//    mono-util gif-frames <file.gif>           print frame count of a GIF file
//    mono-util keybinds-json <in.md> <out.json> export KEYBINDS.md for hyprctl
//    mono-util sway-waybar <cfg.jsonc> <css>   adapt Waybar copy for Sway IPC
// ═══════════════════════════════════════════════════════════════════════════

#include "../include/mono_config.hpp"
#include "../include/mono_image.hpp"

#include <array>
#include <filesystem>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using mono_cfg::JsonVal;

namespace {

void replace_all(std::string& s, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

int cmd_check_configs(const fs::path& repo) {
    const std::array<const char*, 9> jsonc_files = {
        ".config/waybar/config.jsonc",
        ".config/fastfetch/config.jsonc",
        ".config/browser/opencode/chromium-theme/manifest.json",
        ".config/browser/opencode/chromium-newtab/manifest.json",
        ".config/opencode/tui.json",
        ".config/opencode/themes/monochrome.json",
        "site/keybinds.json",
        "windows/windows-terminal-scheme.json",
        "windows/windows-terminal-settings.json",
    };
    for (const char* rel : jsonc_files) {
        mono_cfg::parse_jsonc(mono_cfg::read_text(repo / rel));
    }
    mono_cfg::parse_and_validate_toml(mono_cfg::read_text(repo / ".config/starship.toml"));
    return 0;
}

int cmd_check_greyscale(const fs::path& repo) {
    const std::array<const char*, 3> files = {
        ".config/qt5ct/colors/Monochrome.conf",
        ".config/qt6ct/colors/Monochrome.conf",
        ".config/nvim/colors/monochrome.lua",
    };
    std::regex hex_re(R"(#([0-9a-fA-F]{6,8})\b)");
    std::vector<std::string> bad;
    for (const char* rel : files) {
        std::string txt = mono_cfg::read_text(repo / rel);
        for (auto it = std::sregex_iterator(txt.begin(), txt.end(), hex_re);
             it != std::sregex_iterator(); ++it) {
            std::string hex = mono_cfg::to_lower((*it)[1].str());
            if (hex.size() != 6 && hex.size() != 8) continue;
            std::string rgb = hex.substr(hex.size() - 6);
            if (!(rgb.substr(0, 2) == rgb.substr(2, 2) && rgb.substr(2, 2) == rgb.substr(4, 2))) {
                bad.push_back(std::string(rel) + ":#" + (*it)[1].str());
            }
        }
    }
    if (bad.empty()) {
        std::cout << "pure\n";
    } else {
        for (size_t i = 0; i < bad.size(); ++i) {
            if (i > 0) std::cout << ",";
            std::cout << bad[i];
        }
        std::cout << "\n";
    }
    return 0;
}

int cmd_keybinds_json(const fs::path& md_path, const fs::path& json_out) {
    std::string md = mono_cfg::read_text(md_path);
    std::regex row_re(R"(^\|\s*`([^`]+)`\s*\|\s*([^|]+?)\s*\|)");
    JsonVal::Array out;

    for (const auto& line : mono_cfg::split_lines(md)) {
        std::smatch m;
        if (!std::regex_search(line, m, row_re)) continue;
        std::string combo = mono_cfg::trim(m[1].str());
        std::string desc = mono_cfg::trim(m[2].str());
        std::string submap;
        if (!combo.empty() && combo[0] == '[') {
            size_t close = combo.find("] ");
            if (close != std::string::npos) {
                submap = combo.substr(1, close - 1);
                combo = combo.substr(close + 2);
            }
        }
        std::vector<std::string> parts;
        size_t start = 0;
        while (start <= combo.size()) {
            size_t plus = combo.find('+', start);
            parts.push_back(mono_cfg::trim(plus == std::string::npos
                                           ? combo.substr(start)
                                           : combo.substr(start, plus - start)));
            if (plus == std::string::npos) break;
            start = plus + 1;
        }
        int mask = 0;
        std::string key = parts.empty() ? "" : parts.back();
        for (size_t i = 0; i + 1 < parts.size(); ++i) {
            std::string u = mono_cfg::to_upper(parts[i]);
            if (u == "SUPER") mask |= 64;
            else if (u == "CTRL") mask |= 4;
            else if (u == "ALT") mask |= 8;
            else if (u == "SHIFT") mask |= 1;
        }
        out.push_back(JsonVal(JsonVal::Object{
            {"key", key},
            {"keycode", 0},
            {"modmask", mask},
            {"submap", submap},
            {"description", desc},
        }));
    }
    mono_cfg::write_text(json_out, mono_cfg::json_dump(JsonVal(std::move(out)), 0));
    return 0;
}

int cmd_sway_waybar(const fs::path& cfg_path, const fs::path& css_path) {
    std::string s = mono_cfg::read_text(cfg_path);
    replace_all(s, "\"hyprland/workspaces\"", "\"sway/workspaces\"");
    replace_all(s, "\"hyprland/submap\"", "\"sway/mode\"");
    replace_all(s, "\"hyprland/window\"", "\"sway/window\"");
    replace_all(s, "\"separate-outputs\": true", "\"max-length\": 70");
    for (const char* mod : {"\"temperature\",\n", "\"bluetooth\",\n", "\"battery\",\n", "\"custom/updates\",\n"}) {
        replace_all(s, std::string("        ") + mod, "");
    }
    mono_cfg::write_text(cfg_path, s);

    std::string css = mono_cfg::read_text(css_path);
    replace_all(css, "#workspaces button.active {", "#workspaces button.active,\n#workspaces button.focused {");
    replace_all(css, "#submap {", "#submap,\n#mode {");
    mono_cfg::write_text(css_path, css);
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: mono-util <check-configs|check-greyscale|image-size|gif-frames|keybinds-json|sway-waybar> ...\n";
        return 1;
    }
    std::string cmd = argv[1];
    try {
        if (cmd == "check-configs") {
            fs::path repo = (argc >= 3) ? fs::path(argv[2]) : mono_cfg::find_repo_root(argv[0]);
            return cmd_check_configs(repo);
        }
        if (cmd == "check-greyscale") {
            fs::path repo = (argc >= 3) ? fs::path(argv[2]) : mono_cfg::find_repo_root(argv[0]);
            return cmd_check_greyscale(repo);
        }
        if (cmd == "image-size" && argc >= 3) {
            auto [w, h] = mono_img::read_jpeg_size(argv[2]);
            if (w <= 0 || h <= 0) return 1;
            std::cout << w << "x" << h << "\n";
            return 0;
        }
        if (cmd == "gif-frames" && argc >= 3) {
            int n = mono_img::count_gif_frames(argv[2]);
            std::cout << n << "\n";
            return (n > 0) ? 0 : 1;
        }
        if (cmd == "keybinds-json" && argc >= 4) {
            return cmd_keybinds_json(argv[2], argv[3]);
        }
        if (cmd == "sway-waybar" && argc >= 4) {
            return cmd_sway_waybar(argv[2], argv[3]);
        }
        std::cerr << "Unknown or incomplete command: " << cmd << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "mono-util (" << cmd << "): " << ex.what() << "\n";
        return 1;
    }
}
