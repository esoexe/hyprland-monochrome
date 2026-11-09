#ifndef MONO_CONFIG_H
#define MONO_CONFIG_H
// ═══════════════════════════════════════════════════════════════════════════
//  include/mono_config.h — Zero-dependency Pure C11 config, JSONC & TOML engine
//
//  Shared across `tools/keybind-table.c`, `tools/build-windows-kit.c`,
//  `tools/build-preview.c`, and `tools/mono-util.c`.
// ═══════════════════════════════════════════════════════════════════════════

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

// ── Dynamic String Builder & String Lists ──────────────────────────────────

typedef struct {
    char *s;
    size_t len;
    size_t cap;
} MonoStr;

static inline void mstr_init(MonoStr *ms) {
    ms->cap = 128;
    ms->len = 0;
    ms->s = (char *)malloc(ms->cap);
    if (!ms->s) abort();
    ms->s[0] = '\0';
}

static inline void mstr_free(MonoStr *ms) {
    free(ms->s);
    ms->s = NULL;
    ms->len = 0;
    ms->cap = 0;
}

static inline void mstr_reserve(MonoStr *ms, size_t need) {
    if (need + 1 <= ms->cap) return;
    size_t ncap = ms->cap ? ms->cap * 2 : 128;
    while (ncap < need + 1) ncap *= 2;
    char *ns = (char *)realloc(ms->s, ncap);
    if (!ns) abort();
    ms->s = ns;
    ms->cap = ncap;
}

static inline void mstr_putc(MonoStr *ms, char c) {
    mstr_reserve(ms, ms->len + 1);
    ms->s[ms->len++] = c;
    ms->s[ms->len] = '\0';
}

static inline void mstr_append(MonoStr *ms, const char *data, size_t n) {
    if (n == 0) return;
    mstr_reserve(ms, ms->len + n);
    memcpy(ms->s + ms->len, data, n);
    ms->len += n;
    ms->s[ms->len] = '\0';
}

static inline void mstr_puts(MonoStr *ms, const char *str) {
    if (!str) return;
    mstr_append(ms, str, strlen(str));
}

static inline void mstr_printf(MonoStr *ms, const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n > 0) {
        mstr_reserve(ms, ms->len + (size_t)n);
        vsnprintf(ms->s + ms->len, (size_t)n + 1, fmt, ap2);
        ms->len += (size_t)n;
    }
    va_end(ap2);
}

static inline char *mono_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *d = (char *)malloc(n + 1);
    if (!d) abort();
    memcpy(d, s, n + 1);
    return d;
}

static inline char *mono_strndup(const char *s, size_t n) {
    char *d = (char *)malloc(n + 1);
    if (!d) abort();
    memcpy(d, s, n);
    d[n] = '\0';
    return d;
}

static inline char *mono_trim_dup(const char *s) {
    if (!s) return mono_strdup("");
    while (*s && (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')) ++s;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r' || s[len - 1] == '\n')) {
        --len;
    }
    return mono_strndup(s, len);
}

static inline char *mono_to_upper_dup(const char *s) {
    char *d = mono_strdup(s);
    for (size_t i = 0; d[i]; ++i) d[i] = (char)toupper((unsigned char)d[i]);
    return d;
}

static inline char *mono_to_lower_dup(const char *s) {
    char *d = mono_strdup(s);
    for (size_t i = 0; d[i]; ++i) d[i] = (char)tolower((unsigned char)d[i]);
    return d;
}

static inline bool mono_starts_with(const char *s, const char *prefix) {
    size_t n = strlen(prefix);
    return strncmp(s, prefix, n) == 0;
}

typedef struct {
    char **items;
    size_t len;
    size_t cap;
} MonoStrList;

static inline void mslist_init(MonoStrList *l) { l->items = NULL; l->len = 0; l->cap = 0; }

static inline void mslist_push_owned(MonoStrList *l, char *s) {
    if (l->len + 1 > l->cap) {
        size_t ncap = l->cap ? l->cap * 2 : 16;
        char **ni = (char **)realloc(l->items, ncap * sizeof(char *));
        if (!ni) abort();
        l->items = ni;
        l->cap = ncap;
    }
    l->items[l->len++] = s;
}

static inline void mslist_push(MonoStrList *l, const char *s) {
    mslist_push_owned(l, mono_strdup(s));
}

static inline bool mslist_contains(const MonoStrList *l, const char *s) {
    for (size_t i = 0; i < l->len; ++i) {
        if (strcmp(l->items[i], s) == 0) return true;
    }
    return false;
}

static inline void mslist_free(MonoStrList *l) {
    for (size_t i = 0; i < l->len; ++i) free(l->items[i]);
    free(l->items);
    l->items = NULL;
    l->len = 0;
    l->cap = 0;
}

static inline MonoStrList mono_split_lines(const char *text) {
    MonoStrList out;
    mslist_init(&out);
    size_t total = strlen(text);
    size_t start = 0;
    while (start <= total) {
        const char *nl = strchr(text + start, '\n');
        if (!nl) {
            if (start < total) {
                size_t len = total - start;
                if (len > 0 && text[start + len - 1] == '\r') --len;
                mslist_push_owned(&out, mono_strndup(text + start, len));
            }
            break;
        }
        size_t len = (size_t)(nl - (text + start));
        if (len > 0 && text[start + len - 1] == '\r') --len;
        mslist_push_owned(&out, mono_strndup(text + start, len));
        start = (size_t)(nl - text) + 1;
    }
    return out;
}

typedef struct { char *key; char *val; } MonoKV;

typedef struct {
    MonoKV *items;
    size_t len;
    size_t cap;
} MonoKVList;

static inline void mkv_init(MonoKVList *l) { l->items = NULL; l->len = 0; l->cap = 0; }

static inline void mkv_set(MonoKVList *l, const char *k, const char *v) {
    for (size_t i = 0; i < l->len; ++i) {
        if (strcmp(l->items[i].key, k) == 0) {
            free(l->items[i].val);
            l->items[i].val = mono_strdup(v);
            return;
        }
    }
    if (l->len + 1 > l->cap) {
        size_t ncap = l->cap ? l->cap * 2 : 16;
        MonoKV *ni = (MonoKV *)realloc(l->items, ncap * sizeof(MonoKV));
        if (!ni) abort();
        l->items = ni;
        l->cap = ncap;
    }
    l->items[l->len].key = mono_strdup(k);
    l->items[l->len].val = mono_strdup(v);
    l->len++;
}

static inline void mkv_push(MonoKVList *l, const char *k, const char *v) {
    if (l->len + 1 > l->cap) {
        size_t ncap = l->cap ? l->cap * 2 : 16;
        MonoKV *ni = (MonoKV *)realloc(l->items, ncap * sizeof(MonoKV));
        if (!ni) abort();
        l->items = ni;
        l->cap = ncap;
    }
    l->items[l->len].key = mono_strdup(k);
    l->items[l->len].val = mono_strdup(v);
    l->len++;
}

static inline const char *mkv_get(const MonoKVList *l, const char *k, const char *fallback) {
    for (size_t i = 0; i < l->len; ++i) {
        if (strcmp(l->items[i].key, k) == 0) return l->items[i].val;
    }
    return fallback;
}

static inline void mkv_free(MonoKVList *l) {
    for (size_t i = 0; i < l->len; ++i) {
        free(l->items[i].key);
        free(l->items[i].val);
    }
    free(l->items);
    l->items = NULL;
    l->len = 0;
    l->cap = 0;
}

// ── Filesystem & Escaping Helpers ──────────────────────────────────────────

static inline void mono_path_join(char *dst, size_t cap, const char *a, const char *b) {
    int n = snprintf(dst, cap, "%s/%s", a, b);
    if (n < 0 || (size_t)n >= cap) {
        fprintf(stderr, "path overflow: %s/%s\n", a, b);
        exit(1);
    }
}

static inline bool mono_file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static inline bool mono_is_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static inline uint64_t mono_file_size(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0) ? (uint64_t)st.st_size : 0;
}

static inline void mono_mkdir_p(const char *dir) {
    char tmp[PATH_MAX];
    int n = snprintf(tmp, sizeof(tmp), "%s", dir);
    if (n <= 0 || (size_t)n >= sizeof(tmp)) return;
    if (tmp[n - 1] == '/' && n > 1) tmp[n - 1] = '\0';
    for (char *p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

static inline char *mono_read_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "cannot open file: %s\n", path);
        exit(1);
    }
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); exit(1); }
    long sz = ftell(fp);
    if (sz < 0) { fclose(fp); exit(1); }
    rewind(fp);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) abort();
    size_t got = (sz > 0) ? fread(buf, 1, (size_t)sz, fp) : 0;
    fclose(fp);
    buf[got] = '\0';
    return buf;
}

static inline void mono_write_file(const char *path, const char *content) {
    char parent[PATH_MAX];
    int n = snprintf(parent, sizeof(parent), "%s", path);
    if (n > 0 && (size_t)n < sizeof(parent)) {
        char *slash = strrchr(parent, '/');
        if (slash && slash != parent) {
            *slash = '\0';
            mono_mkdir_p(parent);
        }
    }
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "cannot write file: %s\n", path);
        exit(1);
    }
    size_t len = strlen(content);
    if (len > 0 && fwrite(content, 1, len, fp) != len) {
        fclose(fp);
        exit(1);
    }
    fclose(fp);
}

static inline void mono_parent_dir(char *path) {
    char *slash = strrchr(path, '/');
    if (!slash) {
        path[0] = '\0';
    } else if (slash == path) {
        path[1] = '\0';
    } else {
        *slash = '\0';
    }
}

static inline void mono_find_repo_root(const char *argv0, char *out, size_t cap) {
    char probe[PATH_MAX];
    char dir[PATH_MAX];

    if (argv0 && realpath(argv0, dir)) {
        mono_parent_dir(dir);
        for (int i = 0; i < 4 && dir[0] != '\0'; ++i) {
            int n = snprintf(probe, sizeof(probe), "%s/.config/hypr/hyprland.lua", dir);
            if (n > 0 && (size_t)n < sizeof(probe) && mono_file_exists(probe)) {
                snprintf(out, cap, "%s", dir);
                return;
            }
            mono_parent_dir(dir);
        }
    }

    if (getcwd(dir, sizeof(dir))) {
        for (int i = 0; i < 4 && dir[0] != '\0'; ++i) {
            int n = snprintf(probe, sizeof(probe), "%s/.config/hypr/hyprland.lua", dir);
            if (n > 0 && (size_t)n < sizeof(probe) && mono_file_exists(probe)) {
                snprintf(out, cap, "%s", dir);
                return;
            }
            mono_parent_dir(dir);
        }
    }

    fprintf(stderr, "cannot locate hyprland-monochrome repository root\n");
    exit(1);
}

static inline char *mono_html_escape(const char *s) {
    MonoStr out;
    mstr_init(&out);
    for (size_t i = 0; s[i]; ++i) {
        switch (s[i]) {
            case '&':  mstr_puts(&out, "&amp;"); break;
            case '<':  mstr_puts(&out, "&lt;"); break;
            case '>':  mstr_puts(&out, "&gt;"); break;
            case '"':  mstr_puts(&out, "&quot;"); break;
            case '\'': mstr_puts(&out, "&#x27;"); break;
            default:   mstr_putc(&out, s[i]); break;
        }
    }
    return out.s;
}

static inline char *mono_replace_all_dup(const char *src, const char *from, const char *to) {
    size_t flen = strlen(from);
    if (flen == 0) return mono_strdup(src);
    MonoStr out;
    mstr_init(&out);
    const char *p = src;
    const char *hit;
    while ((hit = strstr(p, from)) != NULL) {
        mstr_append(&out, p, (size_t)(hit - p));
        mstr_puts(&out, to);
        p = hit + flen;
    }
    mstr_puts(&out, p);
    return out.s;
}

// ── String-Aware JSONC Comment Stripper + RFC-8259 JSON DOM ────────────────

static inline char *mono_strip_jsonc_comments(const char *src) {
    size_t n = strlen(src);
    MonoStr out;
    mstr_init(&out);
    bool in_str = false;
    for (size_t i = 0; i < n; ++i) {
        char c = src[i];
        if (in_str) {
            mstr_putc(&out, c);
            if (c == '\\' && i + 1 < n) {
                mstr_putc(&out, src[++i]);
            } else if (c == '"') {
                in_str = false;
            }
        } else {
            if (c == '"') {
                in_str = true;
                mstr_putc(&out, c);
            } else if (c == '/' && i + 1 < n && src[i + 1] == '/') {
                i += 2;
                while (i < n && src[i] != '\n') ++i;
                if (i < n) mstr_putc(&out, '\n');
            } else if (c == '/' && i + 1 < n && src[i + 1] == '*') {
                i += 2;
                while (i + 1 < n && !(src[i] == '*' && src[i + 1] == '/')) ++i;
                ++i;
            } else {
                mstr_putc(&out, c);
            }
        }
    }
    return out.s;
}

typedef enum {
    JSON_NULL = 0,
    JSON_BOOL,
    JSON_INT,
    JSON_FLOAT,
    JSON_STR,
    JSON_ARR,
    JSON_OBJ
} JsonKind;

typedef struct JsonVal JsonVal;
typedef struct { char *key; JsonVal *val; } JsonProp;

struct JsonVal {
    JsonKind kind;
    bool b;
    int64_t i;
    double d;
    char *str;
    struct { JsonVal **items; size_t len; size_t cap; } arr;
    struct { JsonProp *items; size_t len; size_t cap; } obj;
};

static inline JsonVal *json_alloc(JsonKind k) {
    JsonVal *v = (JsonVal *)calloc(1, sizeof(JsonVal));
    if (!v) abort();
    v->kind = k;
    return v;
}

static inline JsonVal *json_new_null(void) { return json_alloc(JSON_NULL); }
static inline JsonVal *json_new_bool(bool b) { JsonVal *v = json_alloc(JSON_BOOL); v->b = b; return v; }
static inline JsonVal *json_new_int(int64_t i) { JsonVal *v = json_alloc(JSON_INT); v->i = i; return v; }
static inline JsonVal *json_new_float(double d) { JsonVal *v = json_alloc(JSON_FLOAT); v->d = d; return v; }
static inline JsonVal *json_new_str(const char *s) { JsonVal *v = json_alloc(JSON_STR); v->str = mono_strdup(s); return v; }
static inline JsonVal *json_new_arr(void) { return json_alloc(JSON_ARR); }
static inline JsonVal *json_new_obj(void) { return json_alloc(JSON_OBJ); }

static inline void json_arr_push(JsonVal *a, JsonVal *item) {
    if (a->arr.len + 1 > a->arr.cap) {
        size_t ncap = a->arr.cap ? a->arr.cap * 2 : 8;
        JsonVal **ni = (JsonVal **)realloc(a->arr.items, ncap * sizeof(JsonVal *));
        if (!ni) abort();
        a->arr.items = ni;
        a->arr.cap = ncap;
    }
    a->arr.items[a->arr.len++] = item;
}

static inline void json_obj_put(JsonVal *o, const char *key, JsonVal *val) {
    if (o->obj.len + 1 > o->obj.cap) {
        size_t ncap = o->obj.cap ? o->obj.cap * 2 : 8;
        JsonProp *ni = (JsonProp *)realloc(o->obj.items, ncap * sizeof(JsonProp));
        if (!ni) abort();
        o->obj.items = ni;
        o->obj.cap = ncap;
    }
    o->obj.items[o->obj.len].key = mono_strdup(key);
    o->obj.items[o->obj.len].val = val;
    o->obj.len++;
}

static inline const JsonVal *json_obj_get(const JsonVal *o, const char *key) {
    if (!o || o->kind != JSON_OBJ) return NULL;
    for (size_t i = 0; i < o->obj.len; ++i) {
        if (strcmp(o->obj.items[i].key, key) == 0) return o->obj.items[i].val;
    }
    return NULL;
}

static inline MonoStrList json_get_string_array(const JsonVal *o, const char *key) {
    MonoStrList out;
    mslist_init(&out);
    const JsonVal *arr = json_obj_get(o, key);
    if (!arr || arr->kind != JSON_ARR) return out;
    for (size_t i = 0; i < arr->arr.len; ++i) {
        if (arr->arr.items[i]->kind == JSON_STR) {
            mslist_push(&out, arr->arr.items[i]->str);
        }
    }
    return out;
}

static inline void json_free(JsonVal *v) {
    if (!v) return;
    if (v->kind == JSON_STR) {
        free(v->str);
    } else if (v->kind == JSON_ARR) {
        for (size_t i = 0; i < v->arr.len; ++i) json_free(v->arr.items[i]);
        free(v->arr.items);
    } else if (v->kind == JSON_OBJ) {
        for (size_t i = 0; i < v->obj.len; ++i) {
            free(v->obj.items[i].key);
            json_free(v->obj.items[i].val);
        }
        free(v->obj.items);
    }
    free(v);
}

typedef struct {
    const char *s;
    size_t len;
    size_t pos;
    bool err;
    char msg[128];
} JsonParser;

static inline void jp_fail(JsonParser *p, const char *m) {
    if (!p->err) {
        p->err = true;
        snprintf(p->msg, sizeof(p->msg), "%s", m);
    }
}

static inline void jp_skip_ws(JsonParser *p) {
    while (p->pos < p->len && isspace((unsigned char)p->s[p->pos])) p->pos++;
}

static inline char jp_peek(JsonParser *p) {
    jp_skip_ws(p);
    return (p->pos < p->len) ? p->s[p->pos] : '\0';
}

static inline void jp_append_utf8(MonoStr *out, uint32_t cp) {
    if (cp <= 0x7Fu) {
        mstr_putc(out, (char)cp);
    } else if (cp <= 0x7FFu) {
        mstr_putc(out, (char)(0xC0u | (cp >> 6)));
        mstr_putc(out, (char)(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0xFFFFu) {
        mstr_putc(out, (char)(0xE0u | (cp >> 12)));
        mstr_putc(out, (char)(0x80u | ((cp >> 6) & 0x3Fu)));
        mstr_putc(out, (char)(0x80u | (cp & 0x3Fu)));
    } else {
        mstr_putc(out, (char)(0xF0u | (cp >> 18)));
        mstr_putc(out, (char)(0x80u | ((cp >> 12) & 0x3Fu)));
        mstr_putc(out, (char)(0x80u | ((cp >> 6) & 0x3Fu)));
        mstr_putc(out, (char)(0x80u | (cp & 0x3Fu)));
    }
}

static inline uint16_t jp_parse_hex4(JsonParser *p) {
    if (p->pos + 4 > p->len) { jp_fail(p, "JSON: truncated \\u"); return 0; }
    uint16_t val = 0;
    for (int i = 0; i < 4; ++i) {
        char ch = p->s[p->pos++];
        val <<= 4;
        if (ch >= '0' && ch <= '9') val |= (uint16_t)(ch - '0');
        else if (ch >= 'a' && ch <= 'f') val |= (uint16_t)(10 + ch - 'a');
        else if (ch >= 'A' && ch <= 'F') val |= (uint16_t)(10 + ch - 'A');
        else { jp_fail(p, "JSON: invalid hex"); return 0; }
    }
    return val;
}

static inline char *jp_parse_raw_str(JsonParser *p) {
    jp_skip_ws(p);
    if (p->pos >= p->len || p->s[p->pos++] != '"') { jp_fail(p, "JSON: expected '\"'"); return mono_strdup(""); }
    MonoStr out;
    mstr_init(&out);
    while (p->pos < p->len) {
        char c = p->s[p->pos++];
        if (c == '"') return out.s;
        if ((unsigned char)c < 0x20u) { jp_fail(p, "JSON: control char"); mstr_free(&out); return mono_strdup(""); }
        if (c == '\\') {
            if (p->pos >= p->len) { jp_fail(p, "JSON: unfinished escape"); mstr_free(&out); return mono_strdup(""); }
            char e = p->s[p->pos++];
            switch (e) {
                case '"':  mstr_putc(&out, '"'); break;
                case '\\': mstr_putc(&out, '\\'); break;
                case '/':  mstr_putc(&out, '/'); break;
                case 'b':  mstr_putc(&out, '\b'); break;
                case 'f':  mstr_putc(&out, '\f'); break;
                case 'n':  mstr_putc(&out, '\n'); break;
                case 'r':  mstr_putc(&out, '\r'); break;
                case 't':  mstr_putc(&out, '\t'); break;
                case 'u': {
                    uint32_t cp = jp_parse_hex4(p);
                    if (cp >= 0xD800u && cp <= 0xDBFFu && p->pos + 1 < p->len &&
                        p->s[p->pos] == '\\' && p->s[p->pos + 1] == 'u') {
                        p->pos += 2;
                        uint32_t low = jp_parse_hex4(p);
                        cp = 0x10000u + (((cp - 0xD800u) << 10) | (low - 0xDC00u));
                    }
                    jp_append_utf8(&out, cp);
                    break;
                }
                default:
                    jp_fail(p, "JSON: invalid escape");
                    mstr_free(&out);
                    return mono_strdup("");
            }
        } else {
            mstr_putc(&out, c);
        }
    }
    jp_fail(p, "JSON: unterminated string");
    mstr_free(&out);
    return mono_strdup("");
}

static inline JsonVal *jp_parse_val(JsonParser *p);

static inline JsonVal *jp_parse_num(JsonParser *p) {
    jp_skip_ws(p);
    size_t start = p->pos;
    if (p->s[p->pos] == '-') p->pos++;
    if (p->pos >= p->len || !isdigit((unsigned char)p->s[p->pos])) { jp_fail(p, "JSON: bad num"); return json_new_null(); }
    if (p->s[p->pos] == '0') {
        p->pos++;
    } else {
        while (p->pos < p->len && isdigit((unsigned char)p->s[p->pos])) p->pos++;
    }
    bool is_float = false;
    if (p->pos < p->len && p->s[p->pos] == '.') {
        is_float = true;
        p->pos++;
        if (p->pos >= p->len || !isdigit((unsigned char)p->s[p->pos])) { jp_fail(p, "JSON: bad frac"); return json_new_null(); }
        while (p->pos < p->len && isdigit((unsigned char)p->s[p->pos])) p->pos++;
    }
    if (p->pos < p->len && (p->s[p->pos] == 'e' || p->s[p->pos] == 'E')) {
        is_float = true;
        p->pos++;
        if (p->pos < p->len && (p->s[p->pos] == '+' || p->s[p->pos] == '-')) p->pos++;
        if (p->pos >= p->len || !isdigit((unsigned char)p->s[p->pos])) { jp_fail(p, "JSON: bad exp"); return json_new_null(); }
        while (p->pos < p->len && isdigit((unsigned char)p->s[p->pos])) p->pos++;
    }
    char *sub = mono_strndup(p->s + start, p->pos - start);
    JsonVal *v;
    if (!is_float) {
        errno = 0;
        char *ep = NULL;
        long long iv = strtoll(sub, &ep, 10);
        if (errno == 0 && ep && *ep == '\0') {
            v = json_new_int((int64_t)iv);
            free(sub);
            return v;
        }
    }
    v = json_new_float(strtod(sub, NULL));
    free(sub);
    return v;
}

static inline void jp_expect(JsonParser *p, const char *lit) {
    jp_skip_ws(p);
    for (size_t i = 0; lit[i]; ++i) {
        if (p->pos >= p->len || p->s[p->pos++] != lit[i]) {
            jp_fail(p, "JSON: unexpected literal");
            return;
        }
    }
}

static inline JsonVal *jp_parse_val(JsonParser *p) {
    if (p->err) return json_new_null();
    char c = jp_peek(p);
    if (c == '"') {
        char *s = jp_parse_raw_str(p);
        JsonVal *v = json_alloc(JSON_STR);
        v->str = s;
        return v;
    }
    if (c == '[') {
        p->pos++;
        JsonVal *arr = json_new_arr();
        if (jp_peek(p) == ']') { p->pos++; return arr; }
        for (;;) {
            json_arr_push(arr, jp_parse_val(p));
            if (p->err) return arr;
            char sep = jp_peek(p);
            if (sep == ',') { p->pos++; continue; }
            if (sep == ']') { p->pos++; break; }
            jp_fail(p, "JSON: expected ',' or ']'");
            return arr;
        }
        return arr;
    }
    if (c == '{') {
        p->pos++;
        JsonVal *obj = json_new_obj();
        if (jp_peek(p) == '}') { p->pos++; return obj; }
        for (;;) {
            if (jp_peek(p) != '"') { jp_fail(p, "JSON: expected key"); return obj; }
            char *k = jp_parse_raw_str(p);
            if (jp_peek(p) != ':') { free(k); jp_fail(p, "JSON: expected ':'"); return obj; }
            p->pos++;
            JsonVal *val = jp_parse_val(p);
            json_obj_put(obj, k, val);
            free(k);
            if (p->err) return obj;
            char sep = jp_peek(p);
            if (sep == ',') { p->pos++; continue; }
            if (sep == '}') { p->pos++; break; }
            jp_fail(p, "JSON: expected ',' or '}'");
            return obj;
        }
        return obj;
    }
    if (c == 't') { jp_expect(p, "true"); return json_new_bool(true); }
    if (c == 'f') { jp_expect(p, "false"); return json_new_bool(false); }
    if (c == 'n') { jp_expect(p, "null"); return json_new_null(); }
    if (c == '-' || isdigit((unsigned char)c)) return jp_parse_num(p);
    jp_fail(p, "JSON: unexpected character");
    return json_new_null();
}

static inline JsonVal *mono_parse_json_strict(const char *text, bool *ok_out) {
    JsonParser p = { text, strlen(text), 0, false, {0} };
    JsonVal *v = jp_parse_val(&p);
    jp_skip_ws(&p);
    if (p.pos != p.len) jp_fail(&p, "JSON: trailing characters");
    if (ok_out) *ok_out = !p.err;
    if (p.err && !ok_out) {
        fprintf(stderr, "%s\n", p.msg);
        json_free(v);
        exit(1);
    }
    return v;
}

static inline JsonVal *mono_parse_jsonc(const char *text, bool *ok_out) {
    char *clean = mono_strip_jsonc_comments(text);
    JsonVal *v = mono_parse_json_strict(clean, ok_out);
    free(clean);
    return v;
}

static inline void mono_json_escape_into(MonoStr *out, const char *s) {
    mstr_putc(out, '"');
    for (size_t i = 0; s[i]; ++i) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '"':  mstr_puts(out, "\\\""); break;
            case '\\': mstr_puts(out, "\\\\"); break;
            case '\b': mstr_puts(out, "\\b"); break;
            case '\f': mstr_puts(out, "\\f"); break;
            case '\n': mstr_puts(out, "\\n"); break;
            case '\r': mstr_puts(out, "\\r"); break;
            case '\t': mstr_puts(out, "\\t"); break;
            default:
                if (c < 0x20u) mstr_printf(out, "\\u%04x", c);
                else mstr_putc(out, (char)c);
        }
    }
    mstr_putc(out, '"');
}

static inline void mono_json_dump_into(MonoStr *out, const JsonVal *val, int indent, int depth) {
    switch (val->kind) {
        case JSON_NULL:  mstr_puts(out, "null"); return;
        case JSON_BOOL:  mstr_puts(out, val->b ? "true" : "false"); return;
        case JSON_INT:   mstr_printf(out, "%" PRId64, val->i); return;
        case JSON_FLOAT: mstr_printf(out, "%.8g", val->d); return;
        case JSON_STR:   mono_json_escape_into(out, val->str); return;
        case JSON_ARR: {
            if (val->arr.len == 0) { mstr_puts(out, "[]"); return; }
            mstr_putc(out, '[');
            for (size_t i = 0; i < val->arr.len; ++i) {
                if (indent > 0) {
                    mstr_putc(out, '\n');
                    for (int sp = 0; sp < (depth + 1) * indent; ++sp) mstr_putc(out, ' ');
                }
                mono_json_dump_into(out, val->arr.items[i], indent, depth + 1);
                if (i + 1 < val->arr.len) mstr_putc(out, ',');
            }
            if (indent > 0) {
                mstr_putc(out, '\n');
                for (int sp = 0; sp < depth * indent; ++sp) mstr_putc(out, ' ');
            }
            mstr_putc(out, ']');
            return;
        }
        case JSON_OBJ: {
            if (val->obj.len == 0) { mstr_puts(out, "{}"); return; }
            mstr_putc(out, '{');
            for (size_t i = 0; i < val->obj.len; ++i) {
                if (indent > 0) {
                    mstr_putc(out, '\n');
                    for (int sp = 0; sp < (depth + 1) * indent; ++sp) mstr_putc(out, ' ');
                }
                mono_json_escape_into(out, val->obj.items[i].key);
                mstr_puts(out, (indent > 0) ? ": " : ":");
                mono_json_dump_into(out, val->obj.items[i].val, indent, depth + 1);
                if (i + 1 < val->obj.len) mstr_putc(out, ',');
            }
            if (indent > 0) {
                mstr_putc(out, '\n');
                for (int sp = 0; sp < depth * indent; ++sp) mstr_putc(out, ' ');
            }
            mstr_putc(out, '}');
            return;
        }
    }
}

static inline char *mono_json_dump(const JsonVal *val, int indent) {
    MonoStr out;
    mstr_init(&out);
    mono_json_dump_into(&out, val, indent, 0);
    return out.s;
}

// ── Clean Pure C11 TOML Validator & Section String Reader ──────────────────

typedef struct {
    char *name;
    MonoKVList kv;
} TomlSection;

typedef struct {
    TomlSection *secs;
    size_t len;
    size_t cap;
} TomlDoc;

static inline MonoKVList *toml_ensure_section(TomlDoc *doc, const char *sec) {
    for (size_t i = 0; i < doc->len; ++i) {
        if (strcmp(doc->secs[i].name, sec) == 0) return &doc->secs[i].kv;
    }
    if (doc->len + 1 > doc->cap) {
        size_t ncap = doc->cap ? doc->cap * 2 : 8;
        TomlSection *ns = (TomlSection *)realloc(doc->secs, ncap * sizeof(TomlSection));
        if (!ns) abort();
        doc->secs = ns;
        doc->cap = ncap;
    }
    doc->secs[doc->len].name = mono_strdup(sec);
    mkv_init(&doc->secs[doc->len].kv);
    return &doc->secs[doc->len++].kv;
}

static inline const MonoKVList *toml_get_section(const TomlDoc *doc, const char *sec) {
    for (size_t i = 0; i < doc->len; ++i) {
        if (strcmp(doc->secs[i].name, sec) == 0) return &doc->secs[i].kv;
    }
    return NULL;
}

static inline void toml_free(TomlDoc *doc) {
    for (size_t i = 0; i < doc->len; ++i) {
        free(doc->secs[i].name);
        mkv_free(&doc->secs[i].kv);
    }
    free(doc->secs);
    doc->secs = NULL;
    doc->len = 0;
    doc->cap = 0;
}

static inline char *toml_parse_quoted(const char *src, size_t n, size_t *ip, char q, bool multiline, bool *ok) {
    MonoStr out;
    mstr_init(&out);
    size_t i = *ip;
    while (i < n) {
        if (multiline && i + 2 < n && src[i] == q && src[i + 1] == q && src[i + 2] == q) {
            *ip = i + 3;
            return out.s;
        }
        if (!multiline && src[i] == q) {
            *ip = i + 1;
            return out.s;
        }
        if (!multiline && src[i] == '\n') { *ok = false; mstr_free(&out); return mono_strdup(""); }
        if (q == '"' && src[i] == '\\') {
            ++i;
            if (i >= n) { *ok = false; mstr_free(&out); return mono_strdup(""); }
            if (src[i] == '\r' || src[i] == '\n' || src[i] == ' ' || src[i] == '\t') {
                while (i < n && (src[i] == ' ' || src[i] == '\t' || src[i] == '\r' || src[i] == '\n')) ++i;
                continue;
            }
            char e = src[i++];
            if (e == 'n') mstr_putc(&out, '\n');
            else if (e == 't') mstr_putc(&out, '\t');
            else mstr_putc(&out, e);
        } else {
            mstr_putc(&out, src[i++]);
        }
    }
    *ok = false;
    mstr_free(&out);
    return mono_strdup("");
}

static inline TomlDoc mono_parse_and_validate_toml(const char *src, bool *ok_out) {
    TomlDoc doc = {NULL, 0, 0};
    bool ok = true;
    char *cur_sec = mono_strdup("");
    size_t i = 0, n = strlen(src);

    #define TOML_HSPACE() do { while (i < n && (src[i] == ' ' || src[i] == '\t')) ++i; } while (0)
    #define TOML_COMMENT_NL() do { \
        TOML_HSPACE(); \
        if (i < n && src[i] == '#') { while (i < n && src[i] != '\n') ++i; } \
        if (i < n && src[i] == '\r') ++i; \
        if (i < n && src[i] == '\n') ++i; \
    } while (0)

    while (i < n && ok) {
        TOML_HSPACE();
        if (i >= n) break;
        if (src[i] == '#' || src[i] == '\r' || src[i] == '\n') {
            TOML_COMMENT_NL();
            continue;
        }
        if (src[i] == '[') {
            bool arr_tbl = (i + 1 < n && src[i + 1] == '[');
            i += arr_tbl ? 2 : 1;
            size_t start = i;
            while (i < n && src[i] != ']' && src[i] != '\n') ++i;
            if (i >= n || src[i] != ']') { ok = false; break; }
            char *raw_sec = mono_strndup(src + start, i - start);
            free(cur_sec);
            cur_sec = mono_trim_dup(raw_sec);
            free(raw_sec);
            if (cur_sec[0] == '\0') { ok = false; break; }
            ++i;
            if (arr_tbl) {
                if (i >= n || src[i] != ']') { ok = false; break; }
                ++i;
            }
            TOML_HSPACE();
            if (i < n && src[i] != '#' && src[i] != '\r' && src[i] != '\n') { ok = false; break; }
            TOML_COMMENT_NL();
            continue;
        }

        char *key = NULL;
        if (src[i] == '"' || src[i] == '\'') {
            char q = src[i++];
            key = toml_parse_quoted(src, n, &i, q, false, &ok);
        } else {
            size_t ks = i;
            while (i < n && (isalnum((unsigned char)src[i]) || src[i] == '_' || src[i] == '-' || src[i] == '.')) ++i;
            key = mono_strndup(src + ks, i - ks);
        }
        if (!ok || key[0] == '\0') { free(key); ok = false; break; }
        TOML_HSPACE();
        if (i >= n || src[i] != '=') { free(key); ok = false; break; }
        ++i;
        TOML_HSPACE();
        if (i >= n) { free(key); ok = false; break; }

        if ((src[i] == '"' || src[i] == '\'') && i + 2 < n && src[i + 1] == src[i] && src[i + 2] == src[i]) {
            char q = src[i];
            i += 3;
            char *val = toml_parse_quoted(src, n, &i, q, true, &ok);
            if (ok) mkv_set(toml_ensure_section(&doc, cur_sec), key, val);
            free(val);
        } else if (src[i] == '"' || src[i] == '\'') {
            char q = src[i++];
            char *val = toml_parse_quoted(src, n, &i, q, false, &ok);
            if (ok) mkv_set(toml_ensure_section(&doc, cur_sec), key, val);
            free(val);
        } else if (src[i] == '[') {
            int depth = 0;
            while (i < n && ok) {
                if (src[i] == '"' || src[i] == '\'') {
                    char q = src[i++];
                    char *tmp = toml_parse_quoted(src, n, &i, q, false, &ok);
                    free(tmp);
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
            if (depth != 0) ok = false;
        } else {
            size_t vs = i;
            while (i < n && src[i] != '#' && src[i] != '\r' && src[i] != '\n') ++i;
            char *sub = mono_strndup(src + vs, i - vs);
            char *raw = mono_trim_dup(sub);
            free(sub);
            if (raw[0] == '\0') {
                ok = false;
            } else if (strcmp(raw, "true") != 0 && strcmp(raw, "false") != 0) {
                MonoStr clean;
                mstr_init(&clean);
                for (size_t k = 0; raw[k]; ++k) if (raw[k] != '_') mstr_putc(&clean, raw[k]);
                char *ep = NULL;
                double d = strtod(clean.s, &ep);
                (void)d;
                if (!ep || *ep != '\0' || clean.len == 0) ok = false;
                mstr_free(&clean);
            }
            if (ok) mkv_set(toml_ensure_section(&doc, cur_sec), key, raw);
            free(raw);
        }
        free(key);
        if (!ok) break;
        TOML_HSPACE();
        if (i < n && src[i] != '#' && src[i] != '\r' && src[i] != '\n') { ok = false; break; }
        TOML_COMMENT_NL();
    }

    #undef TOML_HSPACE
    #undef TOML_COMMENT_NL
    free(cur_sec);

    if (ok_out) *ok_out = ok;
    if (!ok && !ok_out) {
        fprintf(stderr, "TOML validation failed\n");
        toml_free(&doc);
        exit(1);
    }
    return doc;
}

// ── Shared Dotfile Readers (user.lua, kitty, rofi, waybar, packages) ───────

static inline bool mono_is_ident_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static inline char *mono_lua_value(const char *user_lua_text, const char *key, const char *fallback) {
    size_t klen = strlen(key);
    const char *p = user_lua_text;
    while ((p = strstr(p, key)) != NULL) {
        bool left_ok = (p == user_lua_text) || !mono_is_ident_char(p[-1]);
        bool right_ok = !mono_is_ident_char(p[klen]);
        if (left_ok && right_ok) {
            const char *q = p + klen;
            while (*q == ' ' || *q == '\t') ++q;
            if (*q == '=') {
                ++q;
                while (*q == ' ' || *q == '\t') ++q;
                if (*q == '"') {
                    ++q;
                    const char *end = strchr(q, '"');
                    if (end) return mono_strndup(q, (size_t)(end - q));
                } else if (strncmp(q, "true", 4) == 0 && !mono_is_ident_char(q[4])) {
                    return mono_strdup("true");
                } else if (strncmp(q, "false", 5) == 0 && !mono_is_ident_char(q[5])) {
                    return mono_strdup("false");
                } else if (isdigit((unsigned char)*q) || *q == '.') {
                    const char *s = q;
                    while (isdigit((unsigned char)*q) || *q == '.') ++q;
                    return mono_strndup(s, (size_t)(q - s));
                }
            }
        }
        p += klen;
    }
    return mono_strdup(fallback);
}

static inline bool mono_is_hex6(const char *s) {
    if (s[0] != '#') return false;
    for (int i = 1; i <= 6; ++i) {
        if (!isxdigit((unsigned char)s[i])) return false;
    }
    return !isalnum((unsigned char)s[7]);
}

static inline MonoKVList mono_parse_kitty_palette(const char *text) {
    MonoKVList out;
    mkv_init(&out);
    MonoStrList lines = mono_split_lines(text);
    for (size_t i = 0; i < lines.len; ++i) {
        char *line = mono_trim_dup(lines.items[i]);
        if (line[0] != '\0' && line[0] != '#') {
            size_t p = 0;
            while (line[p] && (islower((unsigned char)line[p]) || isdigit((unsigned char)line[p]) || line[p] == '_')) ++p;
            if (p > 0 && isspace((unsigned char)line[p])) {
                char *name = mono_strndup(line, p);
                while (line[p] && isspace((unsigned char)line[p])) ++p;
                if (mono_is_hex6(line + p)) {
                    char hex[8];
                    memcpy(hex, line + p, 7);
                    hex[7] = '\0';
                    char *low = mono_to_lower_dup(hex);
                    mkv_set(&out, name, low);
                    free(low);
                }
                free(name);
            }
        }
        free(line);
    }
    mslist_free(&lines);
    return out;
}

static inline MonoKVList mono_parse_rofi_palette(const char *text) {
    MonoKVList out;
    mkv_init(&out);
    const char *star = strchr(text, '*');
    if (!star) return out;
    const char *lb = strchr(star, '{');
    if (!lb) return out;
    const char *rb = strchr(lb, '}');
    if (!rb) return out;

    const char *p = lb + 1;
    while (p < rb) {
        while (p < rb && !isalnum((unsigned char)*p) && *p != '_' && *p != '-') ++p;
        if (p >= rb) break;
        const char *ks = p;
        while (p < rb && (isalnum((unsigned char)*p) || *p == '_' || *p == '-')) ++p;
        char *name = mono_strndup(ks, (size_t)(p - ks));
        while (p < rb && isspace((unsigned char)*p)) ++p;
        if (p < rb && *p == ':') {
            ++p;
            const char *semi = memchr(p, ';', (size_t)(rb - p));
            if (semi) {
                char *raw_v = mono_strndup(p, (size_t)(semi - p));
                char *val = mono_trim_dup(raw_v);
                free(raw_v);
                if (val[0] == '#' || mono_starts_with(val, "rgba")) {
                    mkv_push(&out, name, val);
                }
                free(val);
                p = semi + 1;
            }
        }
        free(name);
    }
    return out;
}

static inline MonoStrList mono_parse_waybar_colours(const char *css) {
    MonoStrList seen;
    mslist_init(&seen);
    for (size_t i = 0; css[i]; ++i) {
        if (css[i] == '#' && mono_is_hex6(css + i)) {
            char hex[8];
            memcpy(hex, css + i, 7);
            hex[7] = '\0';
            char *low = mono_to_lower_dup(hex);
            if (!mslist_contains(&seen, low)) {
                mslist_push_owned(&seen, low);
            } else {
                free(low);
            }
            i += 6;
        }
    }
    return seen;
}

typedef struct {
    char *title;
    MonoKVList entries;
} MonoPkgGroup;

typedef struct {
    MonoPkgGroup *groups;
    size_t len;
    size_t cap;
} MonoPkgList;

static inline void mpkg_free(MonoPkgList *l) {
    for (size_t i = 0; i < l->len; ++i) {
        free(l->groups[i].title);
        mkv_free(&l->groups[i].entries);
    }
    free(l->groups);
    l->groups = NULL;
    l->len = 0;
    l->cap = 0;
}

static inline MonoPkgGroup *mpkg_add_group(MonoPkgList *l, const char *title) {
    if (l->len + 1 > l->cap) {
        size_t ncap = l->cap ? l->cap * 2 : 8;
        MonoPkgGroup *ng = (MonoPkgGroup *)realloc(l->groups, ncap * sizeof(MonoPkgGroup));
        if (!ng) abort();
        l->groups = ng;
        l->cap = ncap;
    }
    l->groups[l->len].title = mono_strdup(title);
    mkv_init(&l->groups[l->len].entries);
    return &l->groups[l->len++];
}

static inline MonoPkgList mono_parse_packages_txt(const char *text) {
    MonoPkgList raw_groups = {NULL, 0, 0};
    MonoStrList lines = mono_split_lines(text);

    for (size_t i = 0; i < lines.len; ++i) {
        char *line = mono_trim_dup(lines.items[i]);
        if (line[0] == '\0') { free(line); continue; }
        if (line[0] == '#') {
            char *rest = mono_trim_dup(line + 1);
            int lead_rules = 0;
            size_t p = 0, rlen = strlen(rest);
            while (p < rlen) {
                if (rest[p] == '-') { ++lead_rules; ++p; }
                else if (p + 3 <= rlen && memcmp(rest + p, "\xe2\x94\x80", 3) == 0) { ++lead_rules; p += 3; }
                else break;
            }
            if (lead_rules >= 3) {
                size_t s = 0, e = rlen;
                while (s < e) {
                    if (rest[s] == '-' || rest[s] == ' ') ++s;
                    else if (s + 3 <= e && memcmp(rest + s, "\xe2\x94\x80", 3) == 0) s += 3;
                    else break;
                }
                while (e > s) {
                    if (rest[e - 1] == '-' || rest[e - 1] == ' ') --e;
                    else if (e >= s + 3 && memcmp(rest + e - 3, "\xe2\x94\x80", 3) == 0) e -= 3;
                    else break;
                }
                if (e > s) {
                    char *title = mono_strndup(rest + s, e - s);
                    mpkg_add_group(&raw_groups, title);
                    free(title);
                }
            }
            free(rest);
            free(line);
            continue;
        }
        char *hash = strchr(line, '#');
        char *name_raw = hash ? mono_strndup(line, (size_t)(hash - line)) : mono_strdup(line);
        char *name = mono_trim_dup(name_raw);
        char *comment = hash ? mono_trim_dup(hash + 1) : mono_strdup("");
        free(name_raw);
        if (name[0] != '\0') {
            if (raw_groups.len == 0) mpkg_add_group(&raw_groups, "Packages");
            mkv_push(&raw_groups.groups[raw_groups.len - 1].entries, name, comment);
        }
        free(name);
        free(comment);
        free(line);
    }
    mslist_free(&lines);

    MonoPkgList out = {NULL, 0, 0};
    for (size_t i = 0; i < raw_groups.len; ++i) {
        if (raw_groups.groups[i].entries.len > 0) {
            MonoPkgGroup *g = mpkg_add_group(&out, raw_groups.groups[i].title);
            for (size_t j = 0; j < raw_groups.groups[i].entries.len; ++j) {
                mkv_push(&g->entries, raw_groups.groups[i].entries.items[j].key, raw_groups.groups[i].entries.items[j].val);
            }
        }
    }
    mpkg_free(&raw_groups);
    return out;
}

static inline MonoKVList mono_parse_aur_packages_txt(const char *text) {
    MonoKVList out;
    mkv_init(&out);
    MonoStrList lines = mono_split_lines(text);
    for (size_t i = 0; i < lines.len; ++i) {
        char *line = mono_trim_dup(lines.items[i]);
        if (line[0] != '\0' && line[0] != '#') {
            char *hash = strchr(line, '#');
            char *name_raw = hash ? mono_strndup(line, (size_t)(hash - line)) : mono_strdup(line);
            char *name = mono_trim_dup(name_raw);
            char *comment = hash ? mono_trim_dup(hash + 1) : mono_strdup("");
            free(name_raw);
            if (name[0] != '\0') mkv_push(&out, name, comment);
            free(name);
            free(comment);
        }
        free(line);
    }
    mslist_free(&lines);
    return out;
}

#endif
