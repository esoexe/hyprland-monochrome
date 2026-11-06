#pragma once
// ═══════════════════════════════════════════════════════════════════════════
//  include/keybind_table.hpp — C++20 Hyprland Lua keybind parser & renderer
//
//  Shared between `tools/keybind-table.cpp` and `tools/build-preview.cpp`
//  so `docs/KEYBINDS.md` and `docs/preview.html` read the exact same AST.
// ═══════════════════════════════════════════════════════════════════════════

#include "mono_config.hpp"

#include <cctype>
#include <filesystem>
#include <map>
#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace mono_kb {

namespace fs = std::filesystem;
using Row = std::pair<std::string, std::string>;
using Section = std::pair<std::string, std::vector<Row>>;

inline std::string mod_label(const std::string& mod) {
    std::string u = mono_cfg::to_upper(mod);
    if (u == "SUPER") return "Super";
    if (u == "ALT") return "Alt";
    if (u == "CTRL") return "Ctrl";
    if (u == "SHIFT") return "Shift";
    return mod;
}

inline std::string special_key_label(const std::string& upper) {
    static const std::map<std::string, std::string> TBL = {
        {"RETURN", "Enter"},
        {"SPACE", "Space"},
        {"TAB", "Tab"},
        {"ESCAPE", "Escape"},
        {"GRAVE", "`"},
        {"SLASH", "/"},
        {"BACKSLASH", "\\"},
        {"SEMICOLON", ";"},
        {"APOSTROPHE", "'"},
        {"COMMA", ","},
        {"PERIOD", "."},
        {"EQUAL", "="},
        {"MINUS", "-"},
        {"PRINT", "Print"},
        {"DELETE", "Delete"},
        {"BACKSPACE", "Backspace"},
        {"LEFT", "Left"},
        {"RIGHT", "Right"},
        {"UP", "Up"},
        {"DOWN", "Down"},
        {"MOUSE_DOWN", "scroll down"},
        {"MOUSE_UP", "scroll up"},
        {"MOUSE_LEFT", "scroll left"},
        {"MOUSE_RIGHT", "scroll right"},
    };
    auto it = TBL.find(upper);
    return (it != TBL.end()) ? it->second : "";
}

inline std::string read_main_mod(const fs::path& repo_root) {
    std::string txt = mono_cfg::read_text(repo_root / ".config/hypr/modules/user.lua");
    return mono_cfg::lua_value(txt, "mainMod", "SUPER");
}

inline std::string prettify_keys(const std::string& raw) {
    std::string trimmed = mono_cfg::trim(raw);
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= trimmed.size()) {
        size_t plus = trimmed.find('+', start);
        std::string part = mono_cfg::trim(plus == std::string::npos
                                          ? trimmed.substr(start)
                                          : trimmed.substr(start, plus - start));
        if (!part.empty()) {
            std::string upper = mono_cfg::to_upper(part);
            if (upper == "SUPER" || upper == "ALT" || upper == "CTRL" || upper == "SHIFT") {
                out.push_back(mod_label(upper));
            } else if (std::string sk = special_key_label(upper); !sk.empty()) {
                out.push_back(sk);
            } else {
                bool has_alpha = false, all_lower = true;
                for (unsigned char ch : part) {
                    if (std::isalpha(ch)) {
                        has_alpha = true;
                        if (!std::islower(ch)) all_lower = false;
                    }
                }
                if (has_alpha && all_lower) {
                    std::string cap = part;
                    cap[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(cap[0])));
                    out.push_back(cap);
                } else {
                    out.push_back(part);
                }
            }
        }
        if (plus == std::string::npos) break;
        start = plus + 1;
    }
    std::string joined;
    for (size_t i = 0; i < out.size(); ++i) {
        if (i > 0) joined += " + ";
        joined += out[i];
    }
    return joined;
}

inline std::string title_case_py(const std::string& s) {
    std::string out = s;
    bool prev_cased = false;
    for (size_t i = 0; i < out.size(); ++i) {
        unsigned char ch = static_cast<unsigned char>(out[i]);
        if (std::isalpha(ch)) {
            out[i] = static_cast<char>(prev_cased ? std::tolower(ch) : std::toupper(ch));
            prev_cased = true;
        } else {
            prev_cased = false;
        }
    }
    return out;
}

inline std::vector<std::pair<int, std::string>> extract_statements(const std::string& text) {
    auto raw_lines = mono_cfg::split_lines(text);
    std::vector<std::string> cleaned;
    cleaned.reserve(raw_lines.size());

    for (const auto& line : raw_lines) {
        std::string out;
        bool in_str = false;
        char quote = '\0';
        for (size_t i = 0; i < line.size(); ++i) {
            char ch = line[i];
            if (in_str) {
                out.push_back(ch);
                if (ch == quote && (i == 0 || line[i - 1] != '\\')) {
                    in_str = false;
                }
            } else if (ch == '"' || ch == '\'') {
                in_str = true;
                quote = ch;
                out.push_back(ch);
            } else if (ch == '-' && i + 1 < line.size() && line[i + 1] == '-') {
                break;
            } else {
                out.push_back(ch);
            }
        }
        cleaned.push_back(std::move(out));
    }

    std::regex call_re(R"(\b(sh|act|hl\.bind|media|hold)\s*\()");
    std::vector<std::pair<int, std::string>> stmts;
    std::string buf;
    int depth = 0;
    int start_line = 0;

    for (size_t idx = 0; idx < cleaned.size(); ++idx) {
        const std::string& line = cleaned[idx];
        int lineno = static_cast<int>(idx) + 1;
        if (buf.empty() && !std::regex_search(line, call_re)) continue;
        if (buf.empty()) start_line = lineno;
        else buf.push_back(' ');
        buf += line;
        for (char c : line) {
            if (c == '(') ++depth;
            else if (c == ')') --depth;
        }
        if (depth <= 0) {
            stmts.emplace_back(start_line, mono_cfg::trim(buf));
            buf.clear();
            depth = 0;
        }
    }
    return stmts;
}

inline std::string strip_helper_defs(const std::string& src) {
    auto lines = mono_cfg::split_lines(src);
    std::string out;
    bool skipping = false;
    std::regex fn_start(R"(^local function (?:sh|act|media|hold)\()");
    for (const auto& line : lines) {
        if (!skipping && std::regex_search(line, fn_start)) {
            skipping = true;
            continue;
        }
        if (skipping) {
            if (line == "end") skipping = false;
            continue;
        }
        out += line + "\n";
    }
    return out;
}

inline std::string parse_header_title(const std::string& line) {
    if (line.rfind("--", 0) != 0) return "";
    std::string rest = mono_cfg::trim(line.substr(2));
    auto count_lead_bars = [](const std::string& s, size_t& consumed) {
        int cnt = 0;
        size_t p = 0;
        while (p < s.size()) {
            if (s[p] == '=' || s[p] == '-') { ++cnt; ++p; }
            else if (p + 3 <= s.size() && (s.compare(p, 3, "\xe2\x95\x90") == 0 ||
                                           s.compare(p, 3, "\xe2\x94\x80") == 0)) {
                ++cnt; p += 3;
            } else break;
        }
        consumed = p;
        return cnt;
    };
    size_t lead_bytes = 0;
    if (count_lead_bars(rest, lead_bytes) < 3) return "";
    std::string body = mono_cfg::trim(rest.substr(lead_bytes));
    // Strip trailing bars (`═`, `─`, `=`, `-`, spaces)
    while (!body.empty()) {
        if (body.back() == '=' || body.back() == '-' || body.back() == ' ') {
            body.pop_back();
        } else if (body.size() >= 3 && (body.compare(body.size() - 3, 3, "\xe2\x95\x90") == 0 ||
                                        body.compare(body.size() - 3, 3, "\xe2\x94\x80") == 0)) {
            body.erase(body.size() - 3);
        } else {
            break;
        }
    }
    return mono_cfg::trim(body);
}

inline std::pair<std::vector<Section>, std::string> parse(const std::string& raw_text, const fs::path& repo_root) {
    std::string text = strip_helper_defs(raw_text);
    auto lines = mono_cfg::split_lines(text);

    std::regex submap_open_re(R"RE(^\s*hl\.define_submap\(\s*"([^"]+)")RE");
    std::regex submap_close_re(R"RE(^\s*end\s*\)\s*$)RE");

    std::map<int, std::string> section_of;
    std::map<int, std::string> submap_of;
    std::string current_title;
    std::string current_submap;

    for (size_t i = 0; i < lines.size(); ++i) {
        int index = static_cast<int>(i) + 1;
        const std::string& line = lines[i];
        std::string hdr = parse_header_title(line);
        if (!hdr.empty() && hdr.rfind("modules/binds", 0) != 0) {
            current_title = hdr;
        }
        std::smatch sm;
        if (std::regex_search(line, sm, submap_open_re)) {
            current_submap = sm[1].str();
        } else if (!current_submap.empty() && std::regex_match(line, submap_close_re)) {
            current_submap.clear();
        }
        section_of[index] = current_title.empty() ? "Other" : current_title;
        submap_of[index] = current_submap;
    }

    std::regex str_re(R"(\"((?:[^\"\\]|\\.)*)\")");
    std::regex explicit_desc_re(R"(description\s*=\s*\"((?:[^\"\\]|\\.)*)\")");
    std::regex key_loop_re(R"(^(?:sh|act)\(\s*key\b)");

    struct Found { std::string sec, keys, desc; };
    std::vector<Found> found;

    for (const auto& [lineno, stmt] : extract_statements(text)) {
        std::vector<std::string> strings;
        for (auto it = std::sregex_iterator(stmt.begin(), stmt.end(), str_re);
             it != std::sregex_iterator(); ++it) {
            strings.push_back((*it)[1].str());
        }
        if (strings.size() < 2) continue;
        std::string keys = strings.front();
        std::string desc = strings.back();
        std::smatch em;
        if (std::regex_search(stmt, em, explicit_desc_re)) {
            desc = em[1].str();
        }
        if (keys.empty() || desc.empty() || keys == desc) continue;
        if (stmt.find(".. key") != std::string::npos || std::regex_search(stmt, key_loop_re)) continue;
        if (mono_cfg::to_lower(keys).find("mouse:") != std::string::npos) continue;

        int spaces = 0;
        bool has_alpha = false, all_lower = true;
        for (unsigned char c : desc) {
            if (c == ' ') ++spaces;
            else if (std::isalpha(c)) {
                has_alpha = true;
                if (!std::islower(c)) all_lower = false;
            }
        }
        if (spaces > 4 && has_alpha && all_lower) continue;

        std::string sm_name = submap_of[lineno];
        if (!sm_name.empty()) keys = "[" + sm_name + "] " + keys;
        found.push_back({section_of[lineno], keys, desc});
    }

    std::vector<Section> sections;
    for (const auto& f : found) {
        if (sections.empty() || sections.back().first != f.sec) {
            sections.push_back({f.sec, {}});
        }
        sections.back().second.emplace_back(f.keys, f.desc);
    }

    for (auto& [sec, entries] : sections) {
        if (mono_cfg::to_upper(sec).rfind("WORKSPACES", 0) == 0) {
            entries.emplace_back("1..0", "Switch workspace (10 is bound to the 0 key)");
            entries.emplace_back("SHIFT + 1..0", "Move the focused window to that workspace");
            entries.emplace_back("ALT + 1..0", "Move it there without following");
        }
    }

    return {sections, read_main_mod(repo_root)};
}

inline std::string code_span(const std::string& s) {
    if (s.find('`') != std::string::npos) return "`` " + s + " ``";
    return "`" + s + "`";
}

inline std::string key_label(const std::string& keys, const std::string& main_mod) {
    std::string label = mod_label(main_mod);
    if (!keys.empty() && keys[0] == '[') {
        size_t pos = keys.find("] ");
        if (pos != std::string::npos) {
            return keys.substr(0, pos + 2) + prettify_keys(keys.substr(pos + 2));
        }
    }
    if (keys == "1..0" || keys == "SHIFT + 1..0" || keys == "ALT + 1..0") {
        return label + " + " + prettify_keys(keys);
    }
    if (keys.rfind("XF86", 0) == 0 || keys.rfind("switch:", 0) == 0) {
        return keys;
    }
    std::string u = mono_cfg::to_upper(keys);
    if (u.rfind("ALT", 0) == 0 || u.rfind("CTRL", 0) == 0) {
        return prettify_keys(keys);
    }
    return label + " + " + prettify_keys(keys);
}

inline std::vector<Section> documented_rows(const std::vector<Section>& sections, const std::string& main_mod) {
    std::vector<Section> out;
    out.reserve(sections.size());
    for (const auto& [title, entries] : sections) {
        std::vector<Row> rows;
        rows.reserve(entries.size());
        for (const auto& [k, d] : entries) {
            rows.emplace_back(key_label(k, main_mod), d);
        }
        out.emplace_back(title, std::move(rows));
    }
    return out;
}

inline std::string render_markdown(const std::vector<Section>& sections, const std::string& main_mod) {
    std::string label = mod_label(main_mod);
    std::vector<std::string> lines = {
        "# Keybinds",
        "",
        "> This file is generated from `~/.config/hypr/modules/binds.lua`.",
        "> Edit the Lua file, then run `make docs` (`./bin/keybind-table`) to refresh",
        "> this page. Every bind also carries a description, so `hyprctl binds`",
        "> and the cheat sheet (`" + label + " + /`) show the same words.",
        "",
        "The **main modifier is `" + label + "`**. Change it in one place:",
        "`modules/user.lua` → `mainMod`.",
        "",
        "## Contents",
        "",
    };

    for (const auto& [title, _] : sections) {
        std::string anchor;
        for (char c : mono_cfg::to_lower(title)) {
            if (c == ' ') anchor.push_back('-');
            else if (c == '/' || c == '&') continue;
            else anchor.push_back(c);
        }
        lines.push_back("- [" + title_case_py(title) + "](#" + anchor + ")");
    }
    lines.push_back("");

    for (const auto& [title, entries] : documented_rows(sections, main_mod)) {
        lines.push_back("## " + title_case_py(title));
        lines.push_back("");
        lines.push_back("| Keys | Action |");
        lines.push_back("| --- | --- |");
        for (const auto& [keys, desc] : entries) {
            lines.push_back("| " + code_span(keys) + " | " + desc + " |");
        }
        lines.push_back("");
    }

    std::vector<std::string> tail = {
        "## Mouse",
        "",
        "| Action | Result |",
        "| --- | --- |",
        "| " + code_span(label + " + drag left button") + " | Move a window |",
        "| " + code_span(label + " + drag right button") + " | Resize a window |",
        "| " + code_span(label + " + scroll") + " | Previous / next workspace |",
        "| " + code_span(label + " + Ctrl + scroll") + " | Drag the focused window to another workspace |",
        "| " + code_span(label + " + drag on a border/gap") + " | Resize (also works while tiling) |",
        "",
        "## Cheat sheet at runtime",
        "",
        "```bash",
        label + " + /                 # searchable list built from your live binds",
        "hyprctl binds            # the raw list",
        "```",
        "",
        "## Changing the modifier",
        "",
        "Open `~/.config/hypr/modules/user.lua` and set, for example:",
        "",
        "```lua",
        "mainMod = \"ALT\",          -- or \"CTRL\" / \"SUPER\"",
        "```",
        "",
        "Then reload: `hyprctl reload`.",
        "",
    };
    lines.insert(lines.end(), tail.begin(), tail.end());

    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) out.push_back('\n');
        out += lines[i];
    }
    return out;
}

}  // namespace mono_kb
