#ifndef MONO_KEYBIND_TABLE_H
#define MONO_KEYBIND_TABLE_H
// ═══════════════════════════════════════════════════════════════════════════
//  include/keybind_table.h — Pure C11 Hyprland Lua keybind parser & renderer
//
//  Shared between `tools/keybind-table.c` and `tools/build-preview.c`
//  so `docs/KEYBINDS.md` and `docs/preview.html` read the exact same AST.
// ═══════════════════════════════════════════════════════════════════════════

#include "mono_config.h"

typedef struct {
    char *title;
    MonoKVList rows;
} KbSection;

typedef struct {
    KbSection *secs;
    size_t len;
    size_t cap;
    char *main_mod;
} KbDoc;

static inline void kb_doc_free(KbDoc *doc) {
    for (size_t i = 0; i < doc->len; ++i) {
        free(doc->secs[i].title);
        mkv_free(&doc->secs[i].rows);
    }
    free(doc->secs);
    free(doc->main_mod);
    doc->secs = NULL;
    doc->main_mod = NULL;
    doc->len = 0;
    doc->cap = 0;
}

static inline KbSection *kb_add_section(KbDoc *doc, const char *title) {
    if (doc->len + 1 > doc->cap) {
        size_t ncap = doc->cap ? doc->cap * 2 : 8;
        KbSection *ns = (KbSection *)realloc(doc->secs, ncap * sizeof(KbSection));
        if (!ns) abort();
        doc->secs = ns;
        doc->cap = ncap;
    }
    doc->secs[doc->len].title = mono_strdup(title);
    mkv_init(&doc->secs[doc->len].rows);
    return &doc->secs[doc->len++];
}

static inline char *kb_mod_label(const char *mod) {
    char *u = mono_to_upper_dup(mod);
    char *res = NULL;
    if (strcmp(u, "SUPER") == 0) res = mono_strdup("Super");
    else if (strcmp(u, "ALT") == 0) res = mono_strdup("Alt");
    else if (strcmp(u, "CTRL") == 0) res = mono_strdup("Ctrl");
    else if (strcmp(u, "SHIFT") == 0) res = mono_strdup("Shift");
    else res = mono_strdup(mod);
    free(u);
    return res;
}

static inline const char *kb_special_key_label(const char *upper) {
    static const struct { const char *k; const char *v; } TBL[] = {
        {"RETURN", "Enter"}, {"SPACE", "Space"}, {"TAB", "Tab"}, {"ESCAPE", "Escape"},
        {"GRAVE", "`"}, {"SLASH", "/"}, {"BACKSLASH", "\\"}, {"SEMICOLON", ";"},
        {"APOSTROPHE", "'"}, {"COMMA", ","}, {"PERIOD", "."}, {"EQUAL", "="},
        {"MINUS", "-"}, {"PRINT", "Print"}, {"DELETE", "Delete"}, {"BACKSPACE", "Backspace"},
        {"LEFT", "Left"}, {"RIGHT", "Right"}, {"UP", "Up"}, {"DOWN", "Down"},
        {"MOUSE_DOWN", "scroll down"}, {"MOUSE_UP", "scroll up"},
        {"MOUSE_LEFT", "scroll left"}, {"MOUSE_RIGHT", "scroll right"},
        {NULL, NULL}
    };
    for (size_t i = 0; TBL[i].k != NULL; ++i) {
        if (strcmp(upper, TBL[i].k) == 0) return TBL[i].v;
    }
    return "";
}

static inline char *kb_read_main_mod(const char *repo_root) {
    char path[PATH_MAX];
    mono_path_join(path, sizeof(path), repo_root, ".config/hypr/modules/user.lua");
    char *txt = mono_read_file(path);
    char *val = mono_lua_value(txt, "mainMod", "SUPER");
    free(txt);
    return val;
}

static inline char *kb_prettify_keys(const char *raw) {
    char *trimmed = mono_trim_dup(raw);
    MonoStrList parts;
    mslist_init(&parts);

    size_t tlen = strlen(trimmed);
    size_t start = 0;
    while (start <= tlen) {
        const char *plus = strchr(trimmed + start, '+');
        char *sub = plus ? mono_strndup(trimmed + start, (size_t)(plus - (trimmed + start)))
                         : mono_strdup(trimmed + start);
        char *part = mono_trim_dup(sub);
        free(sub);
        if (part[0] != '\0') {
            char *upper = mono_to_upper_dup(part);
            const char *sk = kb_special_key_label(upper);
            if (strcmp(upper, "SUPER") == 0 || strcmp(upper, "ALT") == 0 ||
                strcmp(upper, "CTRL") == 0 || strcmp(upper, "SHIFT") == 0) {
                mslist_push_owned(&parts, kb_mod_label(upper));
            } else if (sk[0] != '\0') {
                mslist_push(&parts, sk);
            } else {
                bool has_alpha = false, all_lower = true;
                for (size_t i = 0; part[i]; ++i) {
                    unsigned char ch = (unsigned char)part[i];
                    if (isalpha(ch)) {
                        has_alpha = true;
                        if (!islower(ch)) all_lower = false;
                    }
                }
                if (has_alpha && all_lower) {
                    char *cap = mono_strdup(part);
                    cap[0] = (char)toupper((unsigned char)cap[0]);
                    mslist_push_owned(&parts, cap);
                } else {
                    mslist_push(&parts, part);
                }
            }
            free(upper);
        }
        free(part);
        if (!plus) break;
        start = (size_t)(plus - trimmed) + 1;
    }
    free(trimmed);

    MonoStr joined;
    mstr_init(&joined);
    for (size_t i = 0; i < parts.len; ++i) {
        if (i > 0) mstr_puts(&joined, " + ");
        mstr_puts(&joined, parts.items[i]);
    }
    mslist_free(&parts);
    return joined.s;
}

static inline char *kb_title_case(const char *s) {
    char *out = mono_strdup(s);
    bool prev_cased = false;
    for (size_t i = 0; out[i]; ++i) {
        unsigned char ch = (unsigned char)out[i];
        if (isalpha(ch)) {
            out[i] = (char)(prev_cased ? tolower(ch) : toupper(ch));
            prev_cased = true;
        } else {
            prev_cased = false;
        }
    }
    return out;
}

static inline bool kb_line_has_bind_call(const char *line) {
    static const char *NAMES[] = {"sh", "act", "hl.bind", "media", "hold", NULL};
    for (size_t n = 0; NAMES[n] != NULL; ++n) {
        const char *kw = NAMES[n];
        size_t klen = strlen(kw);
        const char *p = line;
        while ((p = strstr(p, kw)) != NULL) {
            bool left_ok = (p == line) || (!isalnum((unsigned char)p[-1]) && p[-1] != '_' && p[-1] != '.');
            const char *q = p + klen;
            while (*q == ' ' || *q == '\t') ++q;
            if (left_ok && *q == '(') return true;
            p += klen;
        }
    }
    return false;
}

typedef struct { int lineno; char *stmt; } KbStmt;

static inline char *kb_strip_helper_defs(const char *src) {
    MonoStrList lines = mono_split_lines(src);
    MonoStr out;
    mstr_init(&out);
    bool skipping = false;
    for (size_t i = 0; i < lines.len; ++i) {
        const char *line = lines.items[i];
        if (!skipping &&
            (mono_starts_with(line, "local function sh(") ||
             mono_starts_with(line, "local function act(") ||
             mono_starts_with(line, "local function media(") ||
             mono_starts_with(line, "local function hold("))) {
            skipping = true;
            continue;
        }
        if (skipping) {
            if (strcmp(line, "end") == 0) skipping = false;
            continue;
        }
        mstr_puts(&out, line);
        mstr_putc(&out, '\n');
    }
    mslist_free(&lines);
    return out.s;
}

static inline char *kb_parse_header_title(const char *line) {
    if (!mono_starts_with(line, "--")) return mono_strdup("");
    char *rest = mono_trim_dup(line + 2);
    size_t rlen = strlen(rest);
    int cnt = 0;
    size_t p = 0;
    while (p < rlen) {
        if (rest[p] == '=' || rest[p] == '-') { ++cnt; ++p; }
        else if (p + 3 <= rlen && (memcmp(rest + p, "\xe2\x95\x90", 3) == 0 || memcmp(rest + p, "\xe2\x94\x80", 3) == 0)) {
            ++cnt; p += 3;
        } else break;
    }
    if (cnt < 3) { free(rest); return mono_strdup(""); }
    char *body = mono_trim_dup(rest + p);
    free(rest);
    size_t blen = strlen(body);
    while (blen > 0) {
        if (body[blen - 1] == '=' || body[blen - 1] == '-' || body[blen - 1] == ' ') {
            body[--blen] = '\0';
        } else if (blen >= 3 && (memcmp(body + blen - 3, "\xe2\x95\x90", 3) == 0 || memcmp(body + blen - 3, "\xe2\x94\x80", 3) == 0)) {
            blen -= 3;
            body[blen] = '\0';
        } else break;
    }
    char *res = mono_trim_dup(body);
    free(body);
    return res;
}

static inline bool kb_match_submap_open(const char *line, char *out_name, size_t cap) {
    const char *p = line;
    while (*p == ' ' || *p == '\t') ++p;
    if (!mono_starts_with(p, "hl.define_submap")) return false;
    p += 16;
    while (*p == ' ' || *p == '\t') ++p;
    if (*p != '(') return false;
    ++p;
    while (*p == ' ' || *p == '\t') ++p;
    if (*p != '"') return false;
    ++p;
    const char *end = strchr(p, '"');
    if (!end) return false;
    size_t n = (size_t)(end - p);
    if (n + 1 >= cap) n = cap - 1;
    memcpy(out_name, p, n);
    out_name[n] = '\0';
    return true;
}

static inline bool kb_match_submap_close(const char *line) {
    const char *p = line;
    while (*p == ' ' || *p == '\t') ++p;
    if (!mono_starts_with(p, "end")) return false;
    p += 3;
    while (*p == ' ' || *p == '\t') ++p;
    if (*p != ')') return false;
    ++p;
    while (*p == ' ' || *p == '\t') ++p;
    return *p == '\0';
}

static inline bool kb_is_key_loop_call(const char *stmt) {
    const char *p = NULL;
    if (mono_starts_with(stmt, "sh")) p = stmt + 2;
    else if (mono_starts_with(stmt, "act")) p = stmt + 3;
    else return false;
    while (*p == ' ' || *p == '\t') ++p;
    if (*p != '(') return false;
    ++p;
    while (*p == ' ' || *p == '\t') ++p;
    return strncmp(p, "key", 3) == 0 && !mono_is_ident_char(p[3]);
}

static inline char *kb_extract_explicit_desc(const char *stmt) {
    const char *p = stmt;
    while ((p = strstr(p, "description")) != NULL) {
        bool left_ok = (p == stmt) || !mono_is_ident_char(p[-1]);
        const char *q = p + 11;
        if (left_ok && !mono_is_ident_char(*q)) {
            while (*q == ' ' || *q == '\t') ++q;
            if (*q == '=') {
                ++q;
                while (*q == ' ' || *q == '\t') ++q;
                if (*q == '"') {
                    ++q;
                    MonoStr s;
                    mstr_init(&s);
                    while (*q && *q != '"') {
                        if (*q == '\\' && q[1]) {
                            mstr_putc(&s, *q++);
                        }
                        mstr_putc(&s, *q++);
                    }
                    return s.s;
                }
            }
        }
        p += 11;
    }
    return NULL;
}

static inline KbDoc kb_parse(const char *raw_text, const char *repo_root) {
    KbDoc doc = {NULL, 0, 0, kb_read_main_mod(repo_root)};
    char *text = kb_strip_helper_defs(raw_text);
    MonoStrList lines = mono_split_lines(text);

    size_t nlines = lines.len;
    char **section_of = (char **)calloc(nlines + 2, sizeof(char *));
    char **submap_of = (char **)calloc(nlines + 2, sizeof(char *));
    char *current_title = mono_strdup("");
    char current_submap[128] = {0};

    for (size_t i = 0; i < nlines; ++i) {
        const char *line = lines.items[i];
        char *hdr = kb_parse_header_title(line);
        if (hdr[0] != '\0' && !mono_starts_with(hdr, "modules/binds")) {
            free(current_title);
            current_title = mono_strdup(hdr);
        }
        free(hdr);

        char sm_tmp[128];
        if (kb_match_submap_open(line, sm_tmp, sizeof(sm_tmp))) {
            snprintf(current_submap, sizeof(current_submap), "%s", sm_tmp);
        } else if (current_submap[0] != '\0' && kb_match_submap_close(line)) {
            current_submap[0] = '\0';
        }
        section_of[i + 1] = mono_strdup(current_title[0] ? current_title : "Other");
        submap_of[i + 1] = mono_strdup(current_submap);
    }
    free(current_title);

    // Strip comments line-by-line & extract balanced statements
    MonoStr buf;
    mstr_init(&buf);
    int depth = 0;
    size_t start_line = 0;

    for (size_t idx = 0; idx < nlines; ++idx) {
        const char *raw_l = lines.items[idx];
        MonoStr cl;
        mstr_init(&cl);
        bool in_str = false;
        char quote = '\0';
        for (size_t j = 0; raw_l[j]; ++j) {
            char ch = raw_l[j];
            if (in_str) {
                mstr_putc(&cl, ch);
                if (ch == quote && (j == 0 || raw_l[j - 1] != '\\')) in_str = false;
            } else if (ch == '"' || ch == '\'') {
                in_str = true;
                quote = ch;
                mstr_putc(&cl, ch);
            } else if (ch == '-' && raw_l[j + 1] == '-') {
                break;
            } else {
                mstr_putc(&cl, ch);
            }
        }

        if (buf.len == 0 && !kb_line_has_bind_call(cl.s)) {
            mstr_free(&cl);
            continue;
        }
        if (buf.len == 0) start_line = idx + 1;
        else mstr_putc(&buf, ' ');
        mstr_puts(&buf, cl.s);
        for (size_t j = 0; cl.s[j]; ++j) {
            if (cl.s[j] == '(') ++depth;
            else if (cl.s[j] == ')') --depth;
        }
        mstr_free(&cl);

        if (depth <= 0) {
            char *stmt = mono_trim_dup(buf.s);
            buf.len = 0;
            buf.s[0] = '\0';
            depth = 0;

            MonoStrList strings;
            mslist_init(&strings);
            for (size_t j = 0; stmt[j]; ++j) {
                if (stmt[j] == '"') {
                    ++j;
                    MonoStr lit;
                    mstr_init(&lit);
                    while (stmt[j] && stmt[j] != '"') {
                        if (stmt[j] == '\\' && stmt[j + 1]) {
                            mstr_putc(&lit, stmt[j++]);
                        }
                        mstr_putc(&lit, stmt[j++]);
                    }
                    mslist_push_owned(&strings, lit.s);
                    if (!stmt[j]) break;
                }
            }

            if (strings.len >= 2) {
                char *keys = mono_strdup(strings.items[0]);
                char *exp_d = kb_extract_explicit_desc(stmt);
                char *desc = exp_d ? exp_d : mono_strdup(strings.items[strings.len - 1]);
                char *keys_low = mono_to_lower_dup(keys);

                bool skip = (keys[0] == '\0' || desc[0] == '\0' || strcmp(keys, desc) == 0 ||
                             strstr(stmt, ".. key") != NULL || kb_is_key_loop_call(stmt) ||
                             strstr(keys_low, "mouse:") != NULL);

                if (!skip) {
                    int spaces = 0;
                    bool has_alpha = false, all_lower = true;
                    for (size_t k = 0; desc[k]; ++k) {
                        unsigned char ch = (unsigned char)desc[k];
                        if (ch == ' ') ++spaces;
                        else if (isalpha(ch)) {
                            has_alpha = true;
                            if (!islower(ch)) all_lower = false;
                        }
                    }
                    if (spaces > 4 && has_alpha && all_lower) skip = true;
                }

                if (!skip) {
                    const char *sm = submap_of[start_line];
                    char *final_keys = NULL;
                    if (sm && sm[0] != '\0') {
                        MonoStr fk;
                        mstr_init(&fk);
                        mstr_printf(&fk, "[%s] %s", sm, keys);
                        final_keys = fk.s;
                    } else {
                        final_keys = mono_strdup(keys);
                    }
                    const char *sec = section_of[start_line];
                    if (doc.len == 0 || strcmp(doc.secs[doc.len - 1].title, sec) != 0) {
                        kb_add_section(&doc, sec);
                    }
                    mkv_push(&doc.secs[doc.len - 1].rows, final_keys, desc);
                    free(final_keys);
                }

                free(keys_low);
                free(keys);
                free(desc);
            }
            mslist_free(&strings);
            free(stmt);
        }
    }
    mstr_free(&buf);

    for (size_t i = 1; i <= nlines; ++i) {
        free(section_of[i]);
        free(submap_of[i]);
    }
    free(section_of);
    free(submap_of);
    mslist_free(&lines);
    free(text);

    for (size_t i = 0; i < doc.len; ++i) {
        char *u = mono_to_upper_dup(doc.secs[i].title);
        if (mono_starts_with(u, "WORKSPACES")) {
            mkv_push(&doc.secs[i].rows, "1..0", "Switch workspace (10 is bound to the 0 key)");
            mkv_push(&doc.secs[i].rows, "SHIFT + 1..0", "Move the focused window to that workspace");
            mkv_push(&doc.secs[i].rows, "ALT + 1..0", "Move it there without following");
        }
        free(u);
    }

    return doc;
}

static inline char *kb_key_label(const char *keys, const char *main_mod) {
    char *label = kb_mod_label(main_mod);
    MonoStr out;
    mstr_init(&out);

    if (keys[0] == '[') {
        const char *pos = strstr(keys, "] ");
        if (pos) {
            size_t plen = (size_t)(pos - keys) + 2;
            mstr_append(&out, keys, plen);
            char *p = kb_prettify_keys(keys + plen);
            mstr_puts(&out, p);
            free(p);
            free(label);
            return out.s;
        }
    }
    if (strcmp(keys, "1..0") == 0 || strcmp(keys, "SHIFT + 1..0") == 0 || strcmp(keys, "ALT + 1..0") == 0) {
        char *p = kb_prettify_keys(keys);
        mstr_printf(&out, "%s + %s", label, p);
        free(p);
        free(label);
        return out.s;
    }
    if (mono_starts_with(keys, "XF86") || mono_starts_with(keys, "switch:")) {
        mstr_puts(&out, keys);
        free(label);
        return out.s;
    }
    char *u = mono_to_upper_dup(keys);
    if (mono_starts_with(u, "ALT") || mono_starts_with(u, "CTRL")) {
        free(u);
        free(label);
        mstr_free(&out);
        return kb_prettify_keys(keys);
    }
    free(u);
    char *p = kb_prettify_keys(keys);
    mstr_printf(&out, "%s + %s", label, p);
    free(p);
    free(label);
    return out.s;
}

static inline KbDoc kb_documented_rows(const KbDoc *src) {
    KbDoc out = {NULL, 0, 0, mono_strdup(src->main_mod)};
    for (size_t i = 0; i < src->len; ++i) {
        KbSection *sec = kb_add_section(&out, src->secs[i].title);
        for (size_t j = 0; j < src->secs[i].rows.len; ++j) {
            char *kl = kb_key_label(src->secs[i].rows.items[j].key, src->main_mod);
            mkv_push(&sec->rows, kl, src->secs[i].rows.items[j].val);
            free(kl);
        }
    }
    return out;
}

static inline void kb_append_code_span(MonoStr *out, const char *s) {
    if (strchr(s, '`') != NULL) mstr_printf(out, "`` %s ``", s);
    else mstr_printf(out, "`%s`", s);
}

static inline char *kb_render_markdown(const KbDoc *raw_doc) {
    char *label = kb_mod_label(raw_doc->main_mod);
    KbDoc doc = kb_documented_rows(raw_doc);
    MonoStr out;
    mstr_init(&out);

    mstr_printf(&out,
        "# Keybinds\n\n"
        "> This file is generated from `~/.config/hypr/modules/binds.lua`.\n"
        "> Edit the Lua file, then run `make docs` (`./bin/keybind-table`) to refresh\n"
        "> this page. Every bind also carries a description, so `hyprctl binds`\n"
        "> and the cheat sheet (`%s + /`) show the same words.\n\n"
        "The **main modifier is `%s`**. Change it in one place:\n"
        "`modules/user.lua` → `mainMod`.\n\n"
        "## Contents\n\n", label, label);

    for (size_t i = 0; i < raw_doc->len; ++i) {
        char *low = mono_to_lower_dup(raw_doc->secs[i].title);
        MonoStr anchor;
        mstr_init(&anchor);
        for (size_t k = 0; low[k]; ++k) {
            if (low[k] == ' ') mstr_putc(&anchor, '-');
            else if (low[k] == '/' || low[k] == '&') continue;
            else mstr_putc(&anchor, low[k]);
        }
        char *tc = kb_title_case(raw_doc->secs[i].title);
        mstr_printf(&out, "- [%s](#%s)\n", tc, anchor.s);
        free(tc);
        mstr_free(&anchor);
        free(low);
    }
    mstr_putc(&out, '\n');

    for (size_t i = 0; i < doc.len; ++i) {
        char *tc = kb_title_case(doc.secs[i].title);
        mstr_printf(&out, "## %s\n\n| Keys | Action |\n| --- | --- |\n", tc);
        free(tc);
        for (size_t j = 0; j < doc.secs[i].rows.len; ++j) {
            mstr_puts(&out, "| ");
            kb_append_code_span(&out, doc.secs[i].rows.items[j].key);
            mstr_printf(&out, " | %s |\n", doc.secs[i].rows.items[j].val);
        }
        mstr_putc(&out, '\n');
    }

    mstr_printf(&out,
        "## Mouse\n\n"
        "| Action | Result |\n"
        "| --- | --- |\n"
        "| `%s + drag left button` | Move a window |\n"
        "| `%s + drag right button` | Resize a window |\n"
        "| `%s + scroll` | Previous / next workspace |\n"
        "| `%s + Ctrl + scroll` | Drag the focused window to another workspace |\n"
        "| `%s + drag on a border/gap` | Resize (also works while tiling) |\n\n"
        "## Cheat sheet at runtime\n\n"
        "```bash\n"
        "%s + /                 # searchable list built from your live binds\n"
        "hyprctl binds            # the raw list\n"
        "```\n\n"
        "## Changing the modifier\n\n"
        "Open `~/.config/hypr/modules/user.lua` and set, for example:\n\n"
        "```lua\n"
        "mainMod = \"ALT\",          -- or \"CTRL\" / \"SUPER\"\n"
        "```\n\n"
        "Then reload: `hyprctl reload`.\n",
        label, label, label, label, label, label);

    kb_doc_free(&doc);
    free(label);
    return out.s;
}

#endif
