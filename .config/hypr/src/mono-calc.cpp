// ═══════════════════════════════════════════════════════════════════════════
//  .config/hypr/src/mono-calc.cpp — Safe sub-millisecond C++20 expression
//  evaluator for the Monochrome Command Palette (Super + K / Super + ;).
//
//  Supports arithmetic (+, -, *, /, //, %, **), bitwise (|, ^, &, <<, >>, ~),
//  hex/binary/octal literals (0xff, 0b1010, 0o755), constants (pi, e, tau),
//  and math/radix functions (sqrt, sin, cos, tan, log, log2, log10, ceil,
//  floor, round, abs, hex, bin, oct, int, float, min, max).
//  Zero shell/OS access — rejects any unknown identifier or syntax with rc=1.
// ═══════════════════════════════════════════════════════════════════════════

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace {

using Value = std::variant<double, std::string>;

double as_num(const Value& v) {
    if (const auto* d = std::get_if<double>(&v)) return *d;
    throw std::runtime_error("numeric operand required");
}

int64_t as_i64(const Value& v) {
    double d = as_num(v);
    if (!std::isfinite(d) || std::floor(d) != d || std::abs(d) > 9.0e15) {
        throw std::runtime_error("integer operand required");
    }
    return static_cast<int64_t>(d);
}

std::string format_radix(int64_t v, int base, const char* prefix) {
    bool neg = v < 0;
    uint64_t u = neg ? static_cast<uint64_t>(-(v + 1)) + 1ULL : static_cast<uint64_t>(v);
    if (u == 0) return std::string(prefix) + "0";
    std::string digits;
    static constexpr char HEX[] = "0123456789abcdef";
    while (u > 0) {
        digits.push_back(HEX[u % static_cast<uint64_t>(base)]);
        u /= static_cast<uint64_t>(base);
    }
    std::reverse(digits.begin(), digits.end());
    return (neg ? "-" : "") + std::string(prefix) + digits;
}

class Parser {
    std::string s_;
    size_t pos_ = 0;

    void skip_ws() {
        while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) ++pos_;
    }

    bool match_str(const char* lit) {
        skip_ws();
        size_t i = 0;
        while (lit[i] != '\0') {
            if (pos_ + i >= s_.size() || s_[pos_ + i] != lit[i]) return false;
            ++i;
        }
        pos_ += i;
        return true;
    }

    char peek() {
        skip_ws();
        return pos_ < s_.size() ? s_[pos_] : '\0';
    }

public:
    explicit Parser(std::string expr) : s_(std::move(expr)) {}

    Value evaluate() {
        Value v = parse_bitor();
        skip_ws();
        if (pos_ != s_.size()) throw std::runtime_error("unexpected trailing input");
        return v;
    }

private:
    Value parse_bitor() {
        Value lhs = parse_bitxor();
        while (peek() == '|' && (pos_ + 1 >= s_.size() || s_[pos_ + 1] != '|')) {
            ++pos_;
            lhs = static_cast<double>(as_i64(lhs) | as_i64(parse_bitxor()));
        }
        return lhs;
    }

    Value parse_bitxor() {
        Value lhs = parse_bitand();
        while (peek() == '^') {
            ++pos_;
            lhs = static_cast<double>(as_i64(lhs) ^ as_i64(parse_bitand()));
        }
        return lhs;
    }

    Value parse_bitand() {
        Value lhs = parse_shift();
        while (peek() == '&' && (pos_ + 1 >= s_.size() || s_[pos_ + 1] != '&')) {
            ++pos_;
            lhs = static_cast<double>(as_i64(lhs) & as_i64(parse_shift()));
        }
        return lhs;
    }

    Value parse_shift() {
        Value lhs = parse_add();
        for (;;) {
            skip_ws();
            if (match_str("<<")) {
                int64_t sh = as_i64(parse_add());
                if (sh < 0 || sh > 62) throw std::runtime_error("shift range");
                lhs = static_cast<double>(as_i64(lhs) << sh);
            } else if (match_str(">>")) {
                int64_t sh = as_i64(parse_add());
                if (sh < 0 || sh > 62) throw std::runtime_error("shift range");
                lhs = static_cast<double>(as_i64(lhs) >> sh);
            } else {
                break;
            }
        }
        return lhs;
    }

    Value parse_add() {
        Value lhs = parse_mul();
        for (;;) {
            char c = peek();
            if (c == '+') {
                ++pos_;
                lhs = as_num(lhs) + as_num(parse_mul());
            } else if (c == '-') {
                ++pos_;
                lhs = as_num(lhs) - as_num(parse_mul());
            } else {
                break;
            }
        }
        return lhs;
    }

    Value parse_mul() {
        Value lhs = parse_unary();
        for (;;) {
            skip_ws();
            if (match_str("//")) {
                double r = as_num(parse_unary());
                if (r == 0.0) throw std::runtime_error("div by zero");
                lhs = std::floor(as_num(lhs) / r);
            } else if (peek() == '*' && (pos_ + 1 >= s_.size() || s_[pos_ + 1] != '*')) {
                ++pos_;
                lhs = as_num(lhs) * as_num(parse_unary());
            } else if (peek() == '/') {
                ++pos_;
                double r = as_num(parse_unary());
                if (r == 0.0) throw std::runtime_error("div by zero");
                lhs = as_num(lhs) / r;
            } else if (peek() == '%') {
                ++pos_;
                double r = as_num(parse_unary());
                if (r == 0.0) throw std::runtime_error("mod by zero");
                double l = as_num(lhs);
                lhs = l - std::floor(l / r) * r;
            } else {
                break;
            }
        }
        return lhs;
    }

    Value parse_unary() {
        char c = peek();
        if (c == '+') { ++pos_; return +as_num(parse_unary()); }
        if (c == '-') { ++pos_; return -as_num(parse_unary()); }
        if (c == '~') { ++pos_; return static_cast<double>(~as_i64(parse_unary())); }
        return parse_pow();
    }

    Value parse_pow() {
        Value base = parse_primary();
        skip_ws();
        if (match_str("**")) {
            double exp = as_num(parse_unary());  // right-associative, unary allowed in exponent
            if (std::abs(exp) > 1024.0) throw std::runtime_error("exponent too large");
            double res = std::pow(as_num(base), exp);
            if (!std::isfinite(res)) throw std::runtime_error("math domain");
            return res;
        }
        return base;
    }

    Value parse_primary() {
        char c = peek();
        if (c == '(') {
            ++pos_;
            Value v = parse_bitor();
            if (peek() != ')') throw std::runtime_error("missing ')'");
            ++pos_;
            return v;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            return parse_number();
        }
        if (std::isalpha(static_cast<unsigned char>(c))) {
            return parse_ident_or_call();
        }
        throw std::runtime_error("unexpected token");
    }

    Value parse_number() {
        skip_ws();
        if (s_[pos_] == '0' && pos_ + 1 < s_.size()) {
            char p = static_cast<char>(std::tolower(static_cast<unsigned char>(s_[pos_ + 1])));
            int base = (p == 'x') ? 16 : (p == 'b' ? 2 : (p == 'o' ? 8 : 0));
            if (base != 0) {
                pos_ += 2;
                uint64_t val = 0;
                int digits = 0;
                while (pos_ < s_.size()) {
                    char ch = s_[pos_];
                    if (ch == '_') { ++pos_; continue; }
                    int d = -1;
                    if (ch >= '0' && ch <= '9') d = ch - '0';
                    else if (ch >= 'a' && ch <= 'f') d = 10 + (ch - 'a');
                    else if (ch >= 'A' && ch <= 'F') d = 10 + (ch - 'A');
                    if (d < 0 || d >= base) break;
                    val = val * static_cast<uint64_t>(base) + static_cast<uint64_t>(d);
                    ++digits;
                    ++pos_;
                }
                if (digits == 0) throw std::runtime_error("invalid radix literal");
                return static_cast<double>(val);
            }
        }
        std::string clean;
        while (pos_ < s_.size()) {
            char ch = s_[pos_];
            if (ch == '_') { ++pos_; continue; }
            if (std::isdigit(static_cast<unsigned char>(ch)) || ch == '.') {
                clean.push_back(ch);
                ++pos_;
            } else if ((ch == 'e' || ch == 'E') && !clean.empty()) {
                clean.push_back(ch);
                ++pos_;
                if (pos_ < s_.size() && (s_[pos_] == '+' || s_[pos_] == '-')) {
                    clean.push_back(s_[pos_++]);
                }
            } else {
                break;
            }
        }
        size_t consumed = 0;
        double d = std::stod(clean, &consumed);
        if (consumed != clean.size() || !std::isfinite(d)) throw std::runtime_error("bad float");
        return d;
    }

    Value parse_ident_or_call() {
        skip_ws();
        std::string id;
        while (pos_ < s_.size() &&
               (std::isalnum(static_cast<unsigned char>(s_[pos_])) || s_[pos_] == '_')) {
            id.push_back(s_[pos_++]);
        }
        if (peek() != '(') {
            if (id == "pi") return std::numbers::pi;
            if (id == "e") return std::numbers::e;
            if (id == "tau") return 2.0 * std::numbers::pi;
            throw std::runtime_error("unknown constant: " + id);
        }
        ++pos_;  // '('
        std::vector<Value> args;
        if (peek() != ')') {
            for (;;) {
                args.push_back(parse_bitor());
                if (peek() == ',') {
                    ++pos_;
                } else {
                    break;
                }
            }
        }
        if (peek() != ')') throw std::runtime_error("expected ')'");
        ++pos_;

        auto req1 = [&]() -> double {
            if (args.size() != 1) throw std::runtime_error("expected 1 arg");
            return as_num(args[0]);
        };

        auto checked = [](double v) -> Value {
            if (!std::isfinite(v)) throw std::runtime_error("math domain");
            return v;
        };

        if (id == "sqrt") return checked(std::sqrt(req1()));
        if (id == "sin") return checked(std::sin(req1()));
        if (id == "cos") return checked(std::cos(req1()));
        if (id == "tan") return checked(std::tan(req1()));
        if (id == "log") {
            if (args.size() == 1) return checked(std::log(as_num(args[0])));
            if (args.size() == 2) return checked(std::log(as_num(args[0])) / std::log(as_num(args[1])));
            throw std::runtime_error("log takes 1 or 2 args");
        }
        if (id == "log2") return checked(std::log2(req1()));
        if (id == "log10") return checked(std::log10(req1()));
        if (id == "ceil") return checked(std::ceil(req1()));
        if (id == "floor") return checked(std::floor(req1()));
        if (id == "round") return checked(std::round(req1()));
        if (id == "abs") return checked(std::abs(req1()));
        if (id == "int") return checked(std::trunc(req1()));
        if (id == "float") return checked(req1());
        if (id == "hex") {
            if (args.size() != 1) throw std::runtime_error("hex takes 1 arg");
            return format_radix(as_i64(args[0]), 16, "0x");
        }
        if (id == "bin") {
            if (args.size() != 1) throw std::runtime_error("bin takes 1 arg");
            return format_radix(as_i64(args[0]), 2, "0b");
        }
        if (id == "oct") {
            if (args.size() != 1) throw std::runtime_error("oct takes 1 arg");
            return format_radix(as_i64(args[0]), 8, "0o");
        }
        if (id == "min" || id == "max") {
            if (args.empty()) throw std::runtime_error("min/max requires >=1 arg");
            double best = as_num(args[0]);
            for (size_t i = 1; i < args.size(); ++i) {
                double v = as_num(args[i]);
                best = (id == "min") ? std::min(best, v) : std::max(best, v);
            }
            return best;
        }
        throw std::runtime_error("unknown function: " + id);
    }
};

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;
    std::string expr = argv[1];
    auto l = expr.find_first_not_of(" \t\r\n");
    if (l == std::string::npos) return 1;
    auto r = expr.find_last_not_of(" \t\r\n");
    expr = expr.substr(l, r - l + 1);
    if (!expr.empty() && expr[0] == '=') {
        expr.erase(0, 1);
        auto l2 = expr.find_first_not_of(" \t\r\n");
        if (l2 == std::string::npos) return 1;
        expr.erase(0, l2);
    }
    if (expr.empty() || expr.size() > 256) return 1;

    try {
        Parser p(expr);
        Value val = p.evaluate();
        if (const auto* s = std::get_if<std::string>(&val)) {
            std::cout << *s << "\n";
        } else {
            double d = std::get<double>(val);
            if (!std::isfinite(d)) return 1;
            if (std::floor(d) == d && std::abs(d) < 1.0e15) {
                std::cout << static_cast<int64_t>(d) << "\n";
            } else {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%.8g", d);
                std::cout << buf << "\n";
            }
        }
        return 0;
    } catch (...) {
        return 1;
    }
}
