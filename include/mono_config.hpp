#pragma once
// ═══════════════════════════════════════════════════════════════════════════
//  include/mono_config.hpp — Zero-dependency C++20 config, JSONC & TOML engine
//
//  Shared across `tools/keybind-table.cpp`, `tools/build-windows-kit.cpp`,
//  `tools/build-preview.cpp`, and `tools/mono-util.cpp`.
// ═══════════════════════════════════════════════════════════════════════════

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace mono_cfg {

namespace fs = std::filesystem;

inline std::string trim(const std::string& s) {
    auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

inline std::string to_upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

inline std::string to_lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

inline std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= text.size()) {
        size_t nl = text.find('\n', start);
        if (nl == std::string::npos) {
            if (start < text.size()) {
                std::string tail = text.substr(start);
                if (!tail.empty() && tail.back() == '\r') tail.pop_back();
                out.push_back(std::move(tail));
            }
            break;
        }
        std::string line = text.substr(start, nl - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        out.push_back(std::move(line));
        start = nl + 1;
    }
    return out;
}

inline std::string read_text(const fs::path& p) {
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs) throw std::runtime_error("cannot open file: " + p.string());
    return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

inline void write_text(const fs::path& p, const std::string& s) {
    if (p.has_parent_path()) fs::create_directories(p.parent_path());
    std::ofstream ofs(p, std::ios::binary);
    if (!ofs) throw std::runtime_error("cannot write file: " + p.string());
    ofs.write(s.data(), static_cast<std::streamsize>(s.size()));
}

inline fs::path find_repo_root(const char* argv0) {
    fs::path p = fs::weakly_canonical(fs::path(argv0)).parent_path();
    for (int i = 0; i < 4 && !p.empty(); ++i) {
        if (fs::exists(p / ".config/hypr/hyprland.lua")) return p;
        p = p.parent_path();
    }
    fs::path cwd = fs::current_path();
    for (int i = 0; i < 4 && !cwd.empty(); ++i) {
        if (fs::exists(cwd / ".config/hypr/hyprland.lua")) return cwd;
        cwd = cwd.parent_path();
    }
    throw std::runtime_error("cannot locate hyprland-monochrome repository root");
}

inline std::string html_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '&':  out += "&amp;"; break;
            case '<':  out += "&lt;"; break;
            case '>':  out += "&gt;"; break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#x27;"; break;
            default:   out.push_back(c); break;
        }
    }
    return out;
}

// ── String-aware JSONC comment stripper + RFC-8259 JSON DOM ───────────────

inline std::string strip_jsonc_comments(const std::string& src) {
    std::string out;
    out.reserve(src.size());
    bool in_str = false;
    for (size_t i = 0; i < src.size(); ++i) {
        char c = src[i];
        if (in_str) {
            out.push_back(c);
            if (c == '\\' && i + 1 < src.size()) {
                out.push_back(src[++i]);
            } else if (c == '"') {
                in_str = false;
            }
        } else {
            if (c == '"') {
                in_str = true;
                out.push_back(c);
            } else if (c == '/' && i + 1 < src.size() && src[i + 1] == '/') {
                i += 2;
                while (i < src.size() && src[i] != '\n') ++i;
                if (i < src.size()) out.push_back('\n');
            } else if (c == '/' && i + 1 < src.size() && src[i + 1] == '*') {
                i += 2;
                while (i + 1 < src.size() && !(src[i] == '*' && src[i + 1] == '/')) ++i;
                ++i;
            } else {
                out.push_back(c);
            }
        }
    }
    return out;
}

struct JsonVal {
    using Array = std::vector<JsonVal>;
    using Object = std::vector<std::pair<std::string, JsonVal>>;
    std::variant<std::nullptr_t, bool, int64_t, double, std::string, Array, Object> v = nullptr;

    JsonVal() = default;
    JsonVal(std::nullptr_t) : v(nullptr) {}
    JsonVal(bool b) : v(b) {}
    JsonVal(int i) : v(static_cast<int64_t>(i)) {}
    JsonVal(int64_t i) : v(i) {}
    JsonVal(double d) : v(d) {}
    JsonVal(const char* s) : v(std::string(s)) {}
    JsonVal(std::string s) : v(std::move(s)) {}
    JsonVal(Array a) : v(std::move(a)) {}
    JsonVal(Object o) : v(std::move(o)) {}

    bool is_array() const { return std::holds_alternative<Array>(v); }
    bool is_object() const { return std::holds_alternative<Object>(v); }
    bool is_string() const { return std::holds_alternative<std::string>(v); }

    const Array& as_array() const { return std::get<Array>(v); }
    const Object& as_object() const { return std::get<Object>(v); }
    const std::string& as_string() const { return std::get<std::string>(v); }

    const JsonVal* get(const std::string& key) const {
        if (!is_object()) return nullptr;
        for (const auto& kv : as_object()) {
            if (kv.first == key) return &kv.second;
        }
        return nullptr;
    }

    std::vector<std::string> get_string_array(const std::string& key) const {
        std::vector<std::string> res;
        const JsonVal* arr = get(key);
        if (!arr || !arr->is_array()) return res;
        for (const auto& item : arr->as_array()) {
            if (item.is_string()) res.push_back(item.as_string());
        }
        return res;
    }
};

namespace detail {

inline void append_utf8(std::string& out, uint32_t cp) {
    if (cp <= 0x7Fu) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FFu) {
        out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0xFFFFu) {
        out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else {
        out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
}

class JsonParser {
    std::string s_;
    size_t pos_ = 0;

    void skip_ws() {
        while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) ++pos_;
    }

    char peek() {
        skip_ws();
        return pos_ < s_.size() ? s_[pos_] : '\0';
    }

    uint16_t parse_hex4() {
        if (pos_ + 4 > s_.size()) throw std::runtime_error("JSON: truncated \\u escape");
        uint16_t val = 0;
        for (int i = 0; i < 4; ++i) {
            char ch = s_[pos_++];
            val <<= 4;
            if (ch >= '0' && ch <= '9') val |= static_cast<uint16_t>(ch - '0');
            else if (ch >= 'a' && ch <= 'f') val |= static_cast<uint16_t>(10 + ch - 'a');
            else if (ch >= 'A' && ch <= 'F') val |= static_cast<uint16_t>(10 + ch - 'A');
            else throw std::runtime_error("JSON: invalid hex in \\u escape");
        }
        return val;
    }

public:
    explicit JsonParser(std::string src) : s_(std::move(src)) {}

    JsonVal parse_doc() {
        JsonVal v = parse_val();
        skip_ws();
        if (pos_ != s_.size()) throw std::runtime_error("JSON: trailing characters");
        return v;
    }

private:
    JsonVal parse_val() {
        char c = peek();
        if (c == '"') return parse_str();
        if (c == '{') return parse_obj();
        if (c == '[') return parse_arr();
        if (c == 't') { expect("true"); return JsonVal(true); }
        if (c == 'f') { expect("false"); return JsonVal(false); }
        if (c == 'n') { expect("null"); return JsonVal(nullptr); }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_num();
        throw std::runtime_error("JSON: unexpected character");
    }

    void expect(const char* lit) {
        skip_ws();
        for (size_t i = 0; lit[i] != '\0'; ++i) {
            if (pos_ >= s_.size() || s_[pos_++] != lit[i]) {
                throw std::runtime_error(std::string("JSON: expected ") + lit);
            }
        }
    }

    std::string parse_str() {
        skip_ws();
        if (pos_ >= s_.size() || s_[pos_++] != '"') throw std::runtime_error("JSON: expected '\"'");
        std::string out;
        while (pos_ < s_.size()) {
            char c = s_[pos_++];
            if (c == '"') return out;
            if (static_cast<unsigned char>(c) < 0x20u) throw std::runtime_error("JSON: control char in string");
            if (c == '\\') {
                if (pos_ >= s_.size()) throw std::runtime_error("JSON: unfinished escape");
                char e = s_[pos_++];
                switch (e) {
                    case '"':  out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/':  out.push_back('/'); break;
                    case 'b':  out.push_back('\b'); break;
                    case 'f':  out.push_back('\f'); break;
                    case 'n':  out.push_back('\n'); break;
                    case 'r':  out.push_back('\r'); break;
                    case 't':  out.push_back('\t'); break;
                    case 'u': {
                        uint32_t cp = parse_hex4();
                        if (cp >= 0xD800u && cp <= 0xDBFFu && pos_ + 1 < s_.size() &&
                            s_[pos_] == '\\' && s_[pos_ + 1] == 'u') {
                            pos_ += 2;
                            uint32_t low = parse_hex4();
                            cp = 0x10000u + (((cp - 0xD800u) << 10) | (low - 0xDC00u));
                        }
                        append_utf8(out, cp);
                        break;
                    }
                    default: throw std::runtime_error("JSON: invalid escape");
                }
            } else {
                out.push_back(c);
            }
        }
        throw std::runtime_error("JSON: unterminated string");
    }

    JsonVal parse_num() {
        skip_ws();
        size_t start = pos_;
        if (s_[pos_] == '-') ++pos_;
        if (pos_ >= s_.size() || !std::isdigit(static_cast<unsigned char>(s_[pos_]))) {
            throw std::runtime_error("JSON: invalid number");
        }
        if (s_[pos_] == '0') {
            ++pos_;
        } else {
            while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) ++pos_;
        }
        bool is_float = false;
        if (pos_ < s_.size() && s_[pos_] == '.') {
            is_float = true;
            ++pos_;
            if (pos_ >= s_.size() || !std::isdigit(static_cast<unsigned char>(s_[pos_]))) {
                throw std::runtime_error("JSON: invalid fraction");
            }
            while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) ++pos_;
        }
        if (pos_ < s_.size() && (s_[pos_] == 'e' || s_[pos_] == 'E')) {
            is_float = true;
            ++pos_;
            if (pos_ < s_.size() && (s_[pos_] == '+' || s_[pos_] == '-')) ++pos_;
            if (pos_ >= s_.size() || !std::isdigit(static_cast<unsigned char>(s_[pos_]))) {
                throw std::runtime_error("JSON: invalid exponent");
            }
            while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) ++pos_;
        }
        std::string sub = s_.substr(start, pos_ - start);
        if (!is_float) {
            try { return JsonVal(static_cast<int64_t>(std::stoll(sub))); } catch (...) {}
        }
        return JsonVal(std::stod(sub));
    }

    JsonVal parse_arr() {
        ++pos_;  // '['
        JsonVal::Array arr;
        if (peek() == ']') { ++pos_; return JsonVal(std::move(arr)); }
        for (;;) {
            arr.push_back(parse_val());
            char c = peek();
            if (c == ',') { ++pos_; continue; }
            if (c == ']') { ++pos_; break; }
            throw std::runtime_error("JSON: expected ',' or ']'");
        }
        return JsonVal(std::move(arr));
    }

    JsonVal parse_obj() {
        ++pos_;  // '{'
        JsonVal::Object obj;
        if (peek() == '}') { ++pos_; return JsonVal(std::move(obj)); }
        for (;;) {
            if (peek() != '"') throw std::runtime_error("JSON: expected string key");
            std::string k = parse_str();
            if (peek() != ':') throw std::runtime_error("JSON: expected ':'");
            ++pos_;
            obj.emplace_back(std::move(k), parse_val());
            char c = peek();
            if (c == ',') { ++pos_; continue; }
            if (c == '}') { ++pos_; break; }
            throw std::runtime_error("JSON: expected ',' or '}'");
        }
        return JsonVal(std::move(obj));
    }
};

}  // namespace detail

inline JsonVal parse_jsonc(const std::string& text) {
    return detail::JsonParser(strip_jsonc_comments(text)).parse_doc();
}

inline JsonVal parse_json_strict(const std::string& text) {
    return detail::JsonParser(text).parse_doc();
}

inline std::string json_escape_str(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20u) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    out.push_back('"');
    return out;
}

inline std::string json_dump(const JsonVal& val, int indent = 4, int depth = 0) {
    std::string pad(indent > 0 ? static_cast<size_t>(depth * indent) : 0, ' ');
    std::string pad_in(indent > 0 ? static_cast<size_t>((depth + 1) * indent) : 0, ' ');
    std::string sep_kv = (indent > 0) ? ": " : ":";

    if (std::holds_alternative<std::nullptr_t>(val.v)) return "null";
    if (const auto* b = std::get_if<bool>(&val.v)) return *b ? "true" : "false";
    if (const auto* i = std::get_if<int64_t>(&val.v)) return std::to_string(*i);
    if (const auto* d = std::get_if<double>(&val.v)) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.8g", *d);
        return buf;
    }
    if (const auto* s = std::get_if<std::string>(&val.v)) return json_escape_str(*s);
    if (const auto* arr = std::get_if<JsonVal::Array>(&val.v)) {
        if (arr->empty()) return "[]";
        std::string out = "[";
        for (size_t i = 0; i < arr->size(); ++i) {
            if (indent > 0) out += "\n" + pad_in;
            out += json_dump((*arr)[i], indent, depth + 1);
            if (i + 1 < arr->size()) out += ",";
        }
        if (indent > 0) out += "\n" + pad;
        out += "]";
        return out;
    }
    const auto& obj = std::get<JsonVal::Object>(val.v);
    if (obj.empty()) return "{}";
    std::string out = "{";
    for (size_t i = 0; i < obj.size(); ++i) {
        if (indent > 0) out += "\n" + pad_in;
        out += json_escape_str(obj[i].first) + sep_kv + json_dump(obj[i].second, indent, depth + 1);
        if (i + 1 < obj.size()) out += ",";
    }
    if (indent > 0) out += "\n" + pad;
    out += "}";
    return out;
}

// ── Clean TOML Validator & Section String Reader ──────────────────────────

inline std::map<std::string, std::map<std::string, std::string>> parse_and_validate_toml(const std::string& src) {
    std::map<std::string, std::map<std::string, std::string>> sections;
    std::string cur_sec;
    size_t i = 0;
    const size_t n = src.size();

    auto skip_hspace = [&]() {
        while (i < n && (src[i] == ' ' || src[i] == '\t')) ++i;
    };
    auto skip_comment_and_nl = [&]() {
        skip_hspace();
        if (i < n && src[i] == '#') {
            while (i < n && src[i] != '\n') ++i;
        }
        if (i < n && src[i] == '\r') ++i;
        if (i < n && src[i] == '\n') ++i;
    };

    auto parse_quoted = [&](char q, bool multiline) -> std::string {
        std::string out;
        while (i < n) {
            if (multiline && i + 2 < n && src[i] == q && src[i + 1] == q && src[i + 2] == q) {
                i += 3;
                return out;
            }
            if (!multiline && src[i] == q) {
                ++i;
                return out;
            }
            if (!multiline && src[i] == '\n') throw std::runtime_error("TOML: newline in single-line string");
            if (q == '"' && src[i] == '\\') {
                ++i;
                if (i >= n) throw std::runtime_error("TOML: unfinished escape");
                if (src[i] == '\r' || src[i] == '\n' || src[i] == ' ' || src[i] == '\t') {
                    while (i < n && (src[i] == ' ' || src[i] == '\t' || src[i] == '\r' || src[i] == '\n')) ++i;
                    continue;
                }
                char e = src[i++];
                if (e == 'n') out.push_back('\n');
                else if (e == 't') out.push_back('\t');
                else out.push_back(e);
            } else {
                out.push_back(src[i++]);
            }
        }
        throw std::runtime_error("TOML: unterminated string");
    };

    while (i < n) {
        skip_hspace();
        if (i >= n) break;
        if (src[i] == '#' || src[i] == '\r' || src[i] == '\n') {
            skip_comment_and_nl();
            continue;
        }
        if (src[i] == '[') {
            bool arr_tbl = (i + 1 < n && src[i + 1] == '[');
            i += arr_tbl ? 2 : 1;
            size_t start = i;
            while (i < n && src[i] != ']' && src[i] != '\n') ++i;
            if (i >= n || src[i] != ']') throw std::runtime_error("TOML: unclosed section header");
            cur_sec = trim(src.substr(start, i - start));
            if (cur_sec.empty()) throw std::runtime_error("TOML: empty section header");
            ++i;
            if (arr_tbl) {
                if (i >= n || src[i] != ']') throw std::runtime_error("TOML: expected ']]'");
                ++i;
            }
            skip_hspace();
            if (i < n && src[i] != '#' && src[i] != '\r' && src[i] != '\n') {
                throw std::runtime_error("TOML: trailing garbage after section header");
            }
            skip_comment_and_nl();
            continue;
        }

        // Key
        std::string key;
        if (src[i] == '"' || src[i] == '\'') {
            char q = src[i++];
            key = parse_quoted(q, false);
        } else {
            size_t ks = i;
            while (i < n && (std::isalnum(static_cast<unsigned char>(src[i])) ||
                             src[i] == '_' || src[i] == '-' || src[i] == '.')) {
                ++i;
            }
            key = src.substr(ks, i - ks);
        }
        if (key.empty()) throw std::runtime_error("TOML: expected key");
        skip_hspace();
        if (i >= n || src[i] != '=') throw std::runtime_error("TOML: expected '=' after " + key);
        ++i;
        skip_hspace();
        if (i >= n) throw std::runtime_error("TOML: missing value for " + key);

        // Value
        if ((src[i] == '"' || src[i] == '\'') && i + 2 < n && src[i + 1] == src[i] && src[i + 2] == src[i]) {
            char q = src[i];
            i += 3;
            std::string val = parse_quoted(q, true);
            sections[cur_sec][key] = val;
        } else if (src[i] == '"' || src[i] == '\'') {
            char q = src[i++];
            std::string val = parse_quoted(q, false);
            sections[cur_sec][key] = val;
        } else if (src[i] == '[') {
            int depth = 0;
            while (i < n) {
                if (src[i] == '"' || src[i] == '\'') {
                    char q = src[i++];
                    parse_quoted(q, false);
                    continue;
                }
                if (src[i] == '#') {
                    while (i < n && src[i] != '\n') ++i;
                    continue;
                }
                if (src[i] == '[') ++depth;
                else if (src[i] == ']') {
                    --depth;
                    ++i;
                    if (depth == 0) break;
                    continue;
                }
                ++i;
            }
            if (depth != 0) throw std::runtime_error("TOML: unclosed array");
        } else {
            size_t vs = i;
            while (i < n && src[i] != '#' && src[i] != '\r' && src[i] != '\n') ++i;
            std::string raw = trim(src.substr(vs, i - vs));
            if (raw.empty()) throw std::runtime_error("TOML: empty bare value");
            if (raw != "true" && raw != "false") {
                size_t consumed = 0;
                std::string clean;
                for (char ch : raw) if (ch != '_') clean.push_back(ch);
                double d = std::stod(clean, &consumed);
                (void)d;
                if (consumed != clean.size()) throw std::runtime_error("TOML: invalid scalar: " + raw);
            }
            sections[cur_sec][key] = raw;
        }
        skip_hspace();
        if (i < n && src[i] != '#' && src[i] != '\r' && src[i] != '\n') {
            throw std::runtime_error("TOML: unexpected token at end of line");
        }
        skip_comment_and_nl();
    }
    return sections;
}

// ── Shared Dotfile Readers (user.lua, kitty, rofi, starship, waybar, pkgs) ─

inline std::string lua_value(const std::string& user_lua_text, const std::string& key, const std::string& fallback) {
    std::regex re("\\b" + key + R"RE(\s*=\s*(?:"([^"]*)"|([0-9.]+|true|false)))RE");
    std::smatch m;
    if (std::regex_search(user_lua_text, m, re)) {
        return m[1].matched ? m[1].str() : m[2].str();
    }
    return fallback;
}

inline std::map<std::string, std::string> parse_kitty_palette(const std::string& text) {
    std::map<std::string, std::string> out;
    std::regex re(R"(^([a-z_0-9]+)\s+(#[0-9a-fA-F]{6})\b)");
    for (const auto& raw : split_lines(text)) {
        std::string line = trim(raw);
        if (line.empty() || line[0] == '#') continue;
        std::smatch m;
        if (std::regex_search(line, m, re)) {
            out[m[1].str()] = to_lower(m[2].str());
        }
    }
    return out;
}

inline std::vector<std::pair<std::string, std::string>> parse_rofi_palette(const std::string& text) {
    std::vector<std::pair<std::string, std::string>> out;
    size_t star = text.find('*');
    if (star == std::string::npos) return out;
    size_t lbrace = text.find('{', star);
    size_t rbrace = text.find('}', lbrace);
    if (lbrace == std::string::npos || rbrace == std::string::npos) return out;
    std::string block = text.substr(lbrace + 1, rbrace - lbrace - 1);
    std::regex re(R"(([\w-]+)\s*:\s*([^;]+);)");
    auto begin = std::sregex_iterator(block.begin(), block.end(), re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        std::string name = (*it)[1].str();
        std::string val = trim((*it)[2].str());
        if (!val.empty() && (val[0] == '#' || val.rfind("rgba", 0) == 0)) {
            out.emplace_back(name, val);
        }
    }
    return out;
}

inline std::vector<std::string> parse_waybar_colours(const std::string& css) {
    std::vector<std::string> seen;
    std::regex re(R"(#[0-9a-fA-F]{6}\b)");
    auto begin = std::sregex_iterator(css.begin(), css.end(), re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        std::string c = to_lower((*it)[0].str());
        if (std::find(seen.begin(), seen.end(), c) == seen.end()) {
            seen.push_back(c);
        }
    }
    return seen;
}

inline std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::string>>>>
parse_packages_txt(const std::string& text) {
    std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::string>>>> groups;
    for (const auto& raw : split_lines(text)) {
        std::string line = trim(raw);
        if (line.empty()) continue;
        // Banner: `# ── Title ──────`
        if (line[0] == '#') {
            std::string rest = trim(line.substr(1));
            // Strip leading and trailing UTF-8 `─` (\xe2\x94\x80) or ASCII `-`
            auto strip_rules = [](std::string s) {
                while (!s.empty()) {
                    if (s[0] == '-' || s[0] == ' ') s.erase(0, 1);
                    else if (s.size() >= 3 && s.compare(0, 3, "\xe2\x94\x80") == 0) s.erase(0, 3);
                    else break;
                }
                while (!s.empty()) {
                    if (s.back() == '-' || s.back() == ' ') s.pop_back();
                    else if (s.size() >= 3 && s.compare(s.size() - 3, 3, "\xe2\x94\x80") == 0) s.erase(s.size() - 3);
                    else break;
                }
                return s;
            };
            // Check if rest begins with >=3 rule dashes (ASCII `-` or UTF-8 `─`)
            int lead_rules = 0;
            size_t p = 0;
            while (p < rest.size()) {
                if (rest[p] == '-') { ++lead_rules; ++p; }
                else if (p + 3 <= rest.size() && rest.compare(p, 3, "\xe2\x94\x80") == 0) { ++lead_rules; p += 3; }
                else break;
            }
            if (lead_rules >= 3) {
                std::string title = strip_rules(rest);
                if (!title.empty()) groups.push_back({title, {}});
            }
            continue;
        }
        size_t hash = line.find('#');
        std::string name = trim(hash == std::string::npos ? line : line.substr(0, hash));
        std::string comment = (hash == std::string::npos) ? "" : trim(line.substr(hash + 1));
        if (name.empty()) continue;
        if (groups.empty()) groups.push_back({"Packages", {}});
        groups.back().second.emplace_back(name, comment);
    }
    std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::string>>>> non_empty;
    for (auto& g : groups) {
        if (!g.second.empty()) non_empty.push_back(std::move(g));
    }
    return non_empty;
}

inline std::vector<std::pair<std::string, std::string>> parse_aur_packages_txt(const std::string& text) {
    std::vector<std::pair<std::string, std::string>> out;
    for (const auto& raw : split_lines(text)) {
        std::string line = trim(raw);
        if (line.empty() || line[0] == '#') continue;
        size_t hash = line.find('#');
        std::string name = trim(hash == std::string::npos ? line : line.substr(0, hash));
        std::string comment = (hash == std::string::npos) ? "" : trim(line.substr(hash + 1));
        if (!name.empty()) out.emplace_back(name, comment);
    }
    return out;
}

}  // namespace mono_cfg
