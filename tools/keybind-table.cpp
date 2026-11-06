// ═══════════════════════════════════════════════════════════════════════════
//  tools/keybind-table.cpp — Generate docs/KEYBINDS.md from binds.lua (C++20)
//
//    ./bin/keybind-table            # rewrite docs/KEYBINDS.md
//    ./bin/keybind-table --check    # exit 1 if out of date
// ═══════════════════════════════════════════════════════════════════════════

#include "../include/keybind_table.hpp"

#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    bool check_only = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--check") check_only = true;
    }

    fs::path repo = mono_cfg::find_repo_root(argv[0]);
    fs::path binds_path = repo / ".config/hypr/modules/binds.lua";
    fs::path out_path = repo / "docs/KEYBINDS.md";

    auto [sections, main_mod] = mono_kb::parse(mono_cfg::read_text(binds_path), repo);
    std::string rendered = mono_kb::render_markdown(sections, main_mod);

    if (check_only) {
        if (fs::exists(out_path) && mono_cfg::read_text(out_path) == rendered) {
            std::cout << "docs/KEYBINDS.md is up to date.\n";
            return 0;
        }
        std::cerr << "docs/KEYBINDS.md is OUT OF DATE — run make docs (./bin/keybind-table)\n";
        return 1;
    }

    mono_cfg::write_text(out_path, rendered);
    size_t total = 0;
    for (const auto& [_, rows] : sections) total += rows.size();
    std::cout << "wrote docs/KEYBINDS.md — " << total << " rows in "
              << sections.size() << " sections\n";
    return 0;
}
