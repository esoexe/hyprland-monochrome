/* ═══════════════════════════════════════════════════════════════════════════
 *  .config/hypr/src/mono-calc.c — Safe sub-millisecond Pure C11 expression
 *  evaluator for the Monochrome Command Palette (Super + K / Super + ;).
 *
 *  Supports arithmetic (+, -, *, /, //, %, **), bitwise (|, ^, &, <<, >>, ~),
 *  hex/binary/octal literals (0xff, 0b1010, 0o755), constants (pi, e, tau),
 *  and math/radix functions (sqrt, sin, cos, tan, log, log2, log10, ceil,
 *  floor, round, abs, hex, bin, oct, int, float, min, max).
 *  Zero shell/OS access — rejects any unknown identifier or syntax with rc=1.
 * ═══════════════════════════════════════════════════════════════════════════ */

#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MONO_PI 3.14159265358979323846264338327950288
#define MONO_E  2.71828182845904523536028747135266250

typedef struct {
    bool is_str;
    double num;
    char str[128];
} CalcVal;

typedef struct {
    const char *s;
    size_t len;
    size_t pos;
    jmp_buf err;
} Parser;

static CalcVal make_num(double d) {
    CalcVal v;
    v.is_str = false;
    v.num = d;
    v.str[0] = '\0';
    return v;
}

static double req_num(Parser *p, CalcVal v) {
    if (v.is_str) longjmp(p->err, 1);
    return v.num;
}

static int64_t req_i64(Parser *p, CalcVal v) {
    double d = req_num(p, v);
    if (!isfinite(d) || floor(d) != d || fabs(d) > 9.0e15) {
        longjmp(p->err, 1);
    }
    return (int64_t)d;
}

static CalcVal format_radix(int64_t v, int base, const char *prefix) {
    CalcVal out;
    out.is_str = true;
    out.num = 0.0;

    bool neg = (v < 0);
    uint64_t u = neg ? ((uint64_t)(-(v + 1)) + 1ULL) : (uint64_t)v;
    if (u == 0) {
        snprintf(out.str, sizeof(out.str), "%s0", prefix);
        return out;
    }

    static const char HEX[] = "0123456789abcdef";
    char rev[80];
    size_t n = 0;
    while (u > 0 && n + 1 < sizeof(rev)) {
        rev[n++] = HEX[u % (uint64_t)base];
        u /= (uint64_t)base;
    }

    size_t wp = 0;
    if (neg) out.str[wp++] = '-';
    for (size_t i = 0; prefix[i] != '\0' && wp + 2 < sizeof(out.str); ++i) {
        out.str[wp++] = prefix[i];
    }
    while (n > 0 && wp + 1 < sizeof(out.str)) {
        out.str[wp++] = rev[--n];
    }
    out.str[wp] = '\0';
    return out;
}

static void skip_ws(Parser *p) {
    while (p->pos < p->len && isspace((unsigned char)p->s[p->pos])) {
        p->pos++;
    }
}

static char peek_ch(Parser *p) {
    skip_ws(p);
    return (p->pos < p->len) ? p->s[p->pos] : '\0';
}

static bool match_str(Parser *p, const char *lit) {
    skip_ws(p);
    size_t i = 0;
    while (lit[i] != '\0') {
        if (p->pos + i >= p->len || p->s[p->pos + i] != lit[i]) return false;
        ++i;
    }
    p->pos += i;
    return true;
}

static CalcVal parse_bitor(Parser *p);

static CalcVal parse_number(Parser *p) {
    skip_ws(p);
    if (p->s[p->pos] == '0' && p->pos + 1 < p->len) {
        char pr = (char)tolower((unsigned char)p->s[p->pos + 1]);
        int base = (pr == 'x') ? 16 : ((pr == 'b') ? 2 : ((pr == 'o') ? 8 : 0));
        if (base != 0) {
            p->pos += 2;
            uint64_t val = 0;
            int digits = 0;
            while (p->pos < p->len) {
                char ch = p->s[p->pos];
                if (ch == '_') { p->pos++; continue; }
                int d = -1;
                if (ch >= '0' && ch <= '9') d = ch - '0';
                else if (ch >= 'a' && ch <= 'f') d = 10 + (ch - 'a');
                else if (ch >= 'A' && ch <= 'F') d = 10 + (ch - 'A');
                if (d < 0 || d >= base) break;
                val = val * (uint64_t)base + (uint64_t)d;
                digits++;
                p->pos++;
            }
            if (digits == 0) longjmp(p->err, 1);
            return make_num((double)val);
        }
    }

    char clean[128];
    size_t clen = 0;
    while (p->pos < p->len && clen + 2 < sizeof(clean)) {
        char ch = p->s[p->pos];
        if (ch == '_') { p->pos++; continue; }
        if (isdigit((unsigned char)ch) || ch == '.') {
            clean[clen++] = ch;
            p->pos++;
        } else if ((ch == 'e' || ch == 'E') && clen > 0) {
            clean[clen++] = ch;
            p->pos++;
            if (p->pos < p->len && (p->s[p->pos] == '+' || p->s[p->pos] == '-') && clen + 1 < sizeof(clean)) {
                clean[clen++] = p->s[p->pos++];
            }
        } else {
            break;
        }
    }
    clean[clen] = '\0';
    if (clen == 0) longjmp(p->err, 1);

    char *endp = NULL;
    double d = strtod(clean, &endp);
    if (!endp || *endp != '\0' || !isfinite(d)) longjmp(p->err, 1);
    return make_num(d);
}

static CalcVal checked_num(Parser *p, double v) {
    if (!isfinite(v)) longjmp(p->err, 1);
    return make_num(v);
}

static CalcVal parse_ident_or_call(Parser *p) {
    skip_ws(p);
    char id[64];
    size_t ilen = 0;
    while (p->pos < p->len && (isalnum((unsigned char)p->s[p->pos]) || p->s[p->pos] == '_')) {
        if (ilen + 1 >= sizeof(id)) longjmp(p->err, 1);
        id[ilen++] = p->s[p->pos++];
    }
    id[ilen] = '\0';

    if (peek_ch(p) != '(') {
        if (strcmp(id, "pi") == 0) return make_num(MONO_PI);
        if (strcmp(id, "e") == 0) return make_num(MONO_E);
        if (strcmp(id, "tau") == 0) return make_num(2.0 * MONO_PI);
        longjmp(p->err, 1);
    }

    p->pos++; /* '(' */
    CalcVal args[32];
    size_t nargs = 0;
    if (peek_ch(p) != ')') {
        for (;;) {
            if (nargs >= 32) longjmp(p->err, 1);
            args[nargs++] = parse_bitor(p);
            if (peek_ch(p) == ',') {
                p->pos++;
            } else {
                break;
            }
        }
    }
    if (peek_ch(p) != ')') longjmp(p->err, 1);
    p->pos++;

    if (strcmp(id, "sqrt") == 0)  { if (nargs != 1) longjmp(p->err, 1); return checked_num(p, sqrt(req_num(p, args[0]))); }
    if (strcmp(id, "sin") == 0)   { if (nargs != 1) longjmp(p->err, 1); return checked_num(p, sin(req_num(p, args[0]))); }
    if (strcmp(id, "cos") == 0)   { if (nargs != 1) longjmp(p->err, 1); return checked_num(p, cos(req_num(p, args[0]))); }
    if (strcmp(id, "tan") == 0)   { if (nargs != 1) longjmp(p->err, 1); return checked_num(p, tan(req_num(p, args[0]))); }
    if (strcmp(id, "log") == 0) {
        if (nargs == 1) return checked_num(p, log(req_num(p, args[0])));
        if (nargs == 2) return checked_num(p, log(req_num(p, args[0])) / log(req_num(p, args[1])));
        longjmp(p->err, 1);
    }
    if (strcmp(id, "log2") == 0)  { if (nargs != 1) longjmp(p->err, 1); return checked_num(p, log2(req_num(p, args[0]))); }
    if (strcmp(id, "log10") == 0) { if (nargs != 1) longjmp(p->err, 1); return checked_num(p, log10(req_num(p, args[0]))); }
    if (strcmp(id, "ceil") == 0)  { if (nargs != 1) longjmp(p->err, 1); return checked_num(p, ceil(req_num(p, args[0]))); }
    if (strcmp(id, "floor") == 0) { if (nargs != 1) longjmp(p->err, 1); return checked_num(p, floor(req_num(p, args[0]))); }
    if (strcmp(id, "round") == 0) { if (nargs != 1) longjmp(p->err, 1); return checked_num(p, round(req_num(p, args[0]))); }
    if (strcmp(id, "abs") == 0)   { if (nargs != 1) longjmp(p->err, 1); return checked_num(p, fabs(req_num(p, args[0]))); }
    if (strcmp(id, "int") == 0)   { if (nargs != 1) longjmp(p->err, 1); return checked_num(p, trunc(req_num(p, args[0]))); }
    if (strcmp(id, "float") == 0) { if (nargs != 1) longjmp(p->err, 1); return checked_num(p, req_num(p, args[0])); }
    if (strcmp(id, "hex") == 0)   { if (nargs != 1) longjmp(p->err, 1); return format_radix(req_i64(p, args[0]), 16, "0x"); }
    if (strcmp(id, "bin") == 0)   { if (nargs != 1) longjmp(p->err, 1); return format_radix(req_i64(p, args[0]), 2, "0b"); }
    if (strcmp(id, "oct") == 0)   { if (nargs != 1) longjmp(p->err, 1); return format_radix(req_i64(p, args[0]), 8, "0o"); }
    if (strcmp(id, "min") == 0 || strcmp(id, "max") == 0) {
        if (nargs == 0) longjmp(p->err, 1);
        bool is_min = (strcmp(id, "min") == 0);
        double best = req_num(p, args[0]);
        for (size_t i = 1; i < nargs; ++i) {
            double v = req_num(p, args[i]);
            best = is_min ? (v < best ? v : best) : (v > best ? v : best);
        }
        return make_num(best);
    }
    longjmp(p->err, 1);
}

static CalcVal parse_unary(Parser *p);

static CalcVal parse_primary(Parser *p) {
    char c = peek_ch(p);
    if (c == '(') {
        p->pos++;
        CalcVal v = parse_bitor(p);
        if (peek_ch(p) != ')') longjmp(p->err, 1);
        p->pos++;
        return v;
    }
    if (isdigit((unsigned char)c) || c == '.') {
        return parse_number(p);
    }
    if (isalpha((unsigned char)c)) {
        return parse_ident_or_call(p);
    }
    longjmp(p->err, 1);
}

static CalcVal parse_pow(Parser *p) {
    CalcVal base = parse_primary(p);
    skip_ws(p);
    if (match_str(p, "**")) {
        double exp_v = req_num(p, parse_unary(p));
        if (fabs(exp_v) > 1024.0) longjmp(p->err, 1);
        double res = pow(req_num(p, base), exp_v);
        if (!isfinite(res)) longjmp(p->err, 1);
        return make_num(res);
    }
    return base;
}

static CalcVal parse_unary(Parser *p) {
    char c = peek_ch(p);
    if (c == '+') { p->pos++; return make_num(+req_num(p, parse_unary(p))); }
    if (c == '-') { p->pos++; return make_num(-req_num(p, parse_unary(p))); }
    if (c == '~') { p->pos++; return make_num((double)(~req_i64(p, parse_unary(p)))); }
    return parse_pow(p);
}

static CalcVal parse_mul(Parser *p) {
    CalcVal lhs = parse_unary(p);
    for (;;) {
        skip_ws(p);
        if (match_str(p, "//")) {
            double r = req_num(p, parse_unary(p));
            if (r == 0.0) longjmp(p->err, 1);
            lhs = make_num(floor(req_num(p, lhs) / r));
        } else if (peek_ch(p) == '*' && (p->pos + 1 >= p->len || p->s[p->pos + 1] != '*')) {
            p->pos++;
            lhs = make_num(req_num(p, lhs) * req_num(p, parse_unary(p)));
        } else if (peek_ch(p) == '/') {
            p->pos++;
            double r = req_num(p, parse_unary(p));
            if (r == 0.0) longjmp(p->err, 1);
            lhs = make_num(req_num(p, lhs) / r);
        } else if (peek_ch(p) == '%') {
            p->pos++;
            double r = req_num(p, parse_unary(p));
            if (r == 0.0) longjmp(p->err, 1);
            double l = req_num(p, lhs);
            lhs = make_num(l - floor(l / r) * r);
        } else {
            break;
        }
    }
    return lhs;
}

static CalcVal parse_add(Parser *p) {
    CalcVal lhs = parse_mul(p);
    for (;;) {
        char c = peek_ch(p);
        if (c == '+') {
            p->pos++;
            lhs = make_num(req_num(p, lhs) + req_num(p, parse_mul(p)));
        } else if (c == '-') {
            p->pos++;
            lhs = make_num(req_num(p, lhs) - req_num(p, parse_mul(p)));
        } else {
            break;
        }
    }
    return lhs;
}

static CalcVal parse_shift(Parser *p) {
    CalcVal lhs = parse_add(p);
    for (;;) {
        skip_ws(p);
        if (match_str(p, "<<")) {
            int64_t sh = req_i64(p, parse_add(p));
            if (sh < 0 || sh > 62) longjmp(p->err, 1);
            lhs = make_num((double)(int64_t)((uint64_t)req_i64(p, lhs) << sh));
        } else if (match_str(p, ">>")) {
            int64_t sh = req_i64(p, parse_add(p));
            if (sh < 0 || sh > 62) longjmp(p->err, 1);
            lhs = make_num((double)(req_i64(p, lhs) >> sh));
        } else {
            break;
        }
    }
    return lhs;
}

static CalcVal parse_bitand(Parser *p) {
    CalcVal lhs = parse_shift(p);
    while (peek_ch(p) == '&' && (p->pos + 1 >= p->len || p->s[p->pos + 1] != '&')) {
        p->pos++;
        lhs = make_num((double)(req_i64(p, lhs) & req_i64(p, parse_shift(p))));
    }
    return lhs;
}

static CalcVal parse_bitxor(Parser *p) {
    CalcVal lhs = parse_bitand(p);
    while (peek_ch(p) == '^') {
        p->pos++;
        lhs = make_num((double)(req_i64(p, lhs) ^ req_i64(p, parse_bitand(p))));
    }
    return lhs;
}

static CalcVal parse_bitor(Parser *p) {
    CalcVal lhs = parse_bitxor(p);
    while (peek_ch(p) == '|' && (p->pos + 1 >= p->len || p->s[p->pos + 1] != '|')) {
        p->pos++;
        lhs = make_num((double)(req_i64(p, lhs) | req_i64(p, parse_bitxor(p))));
    }
    return lhs;
}

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;
    const char *raw = argv[1];
    while (*raw && isspace((unsigned char)*raw)) raw++;
    if (*raw == '=') {
        raw++;
        while (*raw && isspace((unsigned char)*raw)) raw++;
    }
    size_t len = strlen(raw);
    while (len > 0 && isspace((unsigned char)raw[len - 1])) len--;
    if (len == 0 || len > 256) return 1;

    Parser p;
    p.s = raw;
    p.len = len;
    p.pos = 0;

    if (setjmp(p.err) != 0) {
        return 1;
    }

    CalcVal val = parse_bitor(&p);
    skip_ws(&p);
    if (p.pos != p.len) return 1;

    if (val.is_str) {
        printf("%s\n", val.str);
    } else {
        double d = val.num;
        if (!isfinite(d)) return 1;
        if (floor(d) == d && fabs(d) < 1.0e15) {
            printf("%" PRId64 "\n", (int64_t)d);
        } else {
            printf("%.8g\n", d);
        }
    }
    return 0;
}
