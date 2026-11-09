// ═══════════════════════════════════════════════════════════════════════════
//  tools/mono-util.c — Native Pure C11 verification & build helpers
//
//  Replaces every inline Python snippet across test suites & desktop tooling:
//    mono-util check-configs <repo>            validate JSON/JSONC + TOML
//    mono-util check-greyscale <repo>          verify R==G==B across toolkits
//    mono-util image-size <file.jpg>           print WxH of a JPEG file
//    mono-util gif-frames <file.gif>           print frame count of a GIF file
//    mono-util keybinds-json <in.md> <out.json> export KEYBINDS.md for hyprctl
//    mono-util sway-waybar <cfg.jsonc> <css>   adapt Waybar copy for Sway IPC
// ═══════════════════════════════════════════════════════════════════════════

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "../include/mono_config.h"
#include "../include/mono_image.h"

static void replace_inplace(char **sp, const char *from, const char *to) {
    char *next = mono_replace_all_dup(*sp, from, to);
    free(*sp);
    *sp = next;
}

static int cmd_check_configs(const char *repo) {
    static const char *jsonc_files[9] = {
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
    char path[PATH_MAX];
    for (size_t i = 0; i < 9; ++i) {
        mono_path_join(path, sizeof(path), repo, jsonc_files[i]);
        char *txt = mono_read_file(path);
        bool ok = false;
        JsonVal *v = mono_parse_jsonc(txt, &ok);
        json_free(v);
        free(txt);
        if (!ok) {
            fprintf(stderr, "invalid JSON/JSONC: %s\n", jsonc_files[i]);
            return 1;
        }
    }
    mono_path_join(path, sizeof(path), repo, ".config/starship.toml");
    char *toml_txt = mono_read_file(path);
    bool tok = false;
    TomlDoc td = mono_parse_and_validate_toml(toml_txt, &tok);
    toml_free(&td);
    free(toml_txt);
    if (!tok) {
        fprintf(stderr, "invalid TOML: .config/starship.toml\n");
        return 1;
    }
    return 0;
}

static int cmd_check_greyscale(const char *repo) {
    static const char *files[3] = {
        ".config/qt5ct/colors/Monochrome.conf",
        ".config/qt6ct/colors/Monochrome.conf",
        ".config/nvim/colors/monochrome.lua",
    };
    MonoStrList bad;
    mslist_init(&bad);
    char path[PATH_MAX];

    for (size_t f = 0; f < 3; ++f) {
        mono_path_join(path, sizeof(path), repo, files[f]);
        char *txt = mono_read_file(path);
        for (size_t i = 0; txt[i]; ++i) {
            if (txt[i] == '#') {
                size_t hlen = 0;
                while (isxdigit((unsigned char)txt[i + 1 + hlen])) ++hlen;
                bool boundary = !isalnum((unsigned char)txt[i + 1 + hlen]) && txt[i + 1 + hlen] != '_';
                if (boundary && (hlen == 6 || hlen == 8)) {
                    char raw[12];
                    memcpy(raw, txt + i + 1, hlen);
                    raw[hlen] = '\0';
                    char *low = mono_to_lower_dup(raw);
                    const char *rgb = low + (hlen - 6);
                    bool eq = (rgb[0] == rgb[2] && rgb[1] == rgb[3] &&
                               rgb[2] == rgb[4] && rgb[3] == rgb[5]);
                    if (!eq) {
                        MonoStr item;
                        mstr_init(&item);
                        mstr_printf(&item, "%s:#%s", files[f], raw);
                        mslist_push_owned(&bad, item.s);
                    }
                    free(low);
                }
                i += hlen;
            }
        }
        free(txt);
    }

    if (bad.len == 0) {
        printf("pure\n");
    } else {
        for (size_t i = 0; i < bad.len; ++i) {
            if (i > 0) printf(",");
            printf("%s", bad.items[i]);
        }
        printf("\n");
    }
    mslist_free(&bad);
    return 0;
}

static int cmd_keybinds_json(const char *md_path, const char *json_out) {
    char *md = mono_read_file(md_path);
    MonoStrList lines = mono_split_lines(md);
    free(md);

    JsonVal *out = json_new_arr();
    char *current_cat = mono_strdup("General");

    for (size_t i = 0; i < lines.len; ++i) {
        const char *line = lines.items[i];
        if (strncmp(line, "## ", 3) == 0 || strncmp(line, "##\t", 3) == 0) {
            const char *p = line + 2;
            while (*p == ' ' || *p == '\t') ++p;
            char *heading = mono_trim_dup(p);
            if (heading[0] != '\0' && strcmp(heading, "Contents") != 0) {
                replace_inplace(&heading, "\xE2\x80\x94", ":");
                replace_inplace(&heading, "\xE2\x80\x93", ":");
                free(current_cat);
                current_cat = heading;
            } else {
                free(heading);
            }
            continue;
        }

        if (line[0] != '|') continue;
        const char *p = line + 1;
        while (*p == ' ' || *p == '\t') ++p;
        if (*p != '`') continue;
        ++p;
        const char *bt2 = strchr(p, '`');
        if (!bt2) continue;
        char *raw_tok = mono_strndup(p, (size_t)(bt2 - p));
        char *raw_combo = mono_trim_dup(raw_tok);
        free(raw_tok);

        const char *q = bt2 + 1;
        while (*q == ' ' || *q == '\t') ++q;
        if (*q != '|') { free(raw_combo); continue; }
        ++q;
        while (*q == ' ' || *q == '\t') ++q;
        const char *pipe2 = strchr(q, '|');
        if (!pipe2) { free(raw_combo); continue; }
        char *raw_desc = mono_strndup(q, (size_t)(pipe2 - q));
        char *desc = mono_trim_dup(raw_desc);
        free(raw_desc);

        replace_inplace(&desc, "OpenCode", "Monochrome");
        replace_inplace(&desc, "opencode", "monochrome");
        replace_inplace(&desc, "\xE2\x80\x94", ":");
        replace_inplace(&desc, "\xE2\x80\x93", ":");

        char *combo = mono_strdup(raw_combo);
        char *submap = mono_strdup("");
        if (combo[0] == '[') {
            char *close = strstr(combo, "] ");
            if (close) {
                free(submap);
                submap = mono_strndup(combo + 1, (size_t)(close - (combo + 1)));
                char *rest = mono_strdup(close + 2);
                free(combo);
                combo = rest;
            }
        }

        MonoStrList parts;
        mslist_init(&parts);
        size_t clen = strlen(combo);
        size_t start = 0;
        while (start <= clen) {
            const char *plus = strchr(combo + start, '+');
            char *sub = plus ? mono_strndup(combo + start, (size_t)(plus - (combo + start)))
                             : mono_strdup(combo + start);
            mslist_push_owned(&parts, mono_trim_dup(sub));
            free(sub);
            if (!plus) break;
            start = (size_t)(plus - combo) + 1;
        }

        int mask = 0;
        const char *key = (parts.len > 0) ? parts.items[parts.len - 1] : "";
        for (size_t k = 0; k + 1 < parts.len; ++k) {
            char *u = mono_to_upper_dup(parts.items[k]);
            if (strcmp(u, "SUPER") == 0) mask |= 64;
            else if (strcmp(u, "CTRL") == 0) mask |= 4;
            else if (strcmp(u, "ALT") == 0) mask |= 8;
            else if (strcmp(u, "SHIFT") == 0) mask |= 1;
            free(u);
        }

        JsonVal *row = json_new_obj();
        json_obj_put(row, "shortcut", json_new_str(raw_combo));
        json_obj_put(row, "category", json_new_str(current_cat));
        json_obj_put(row, "key", json_new_str(key));
        json_obj_put(row, "keycode", json_new_int(0));
        json_obj_put(row, "modmask", json_new_int(mask));
        json_obj_put(row, "submap", json_new_str(submap));
        json_obj_put(row, "description", json_new_str(desc));
        json_arr_push(out, row);

        mslist_free(&parts);
        free(submap);
        free(combo);
        free(desc);
        free(raw_combo);
    }

    char *dumped = mono_json_dump(out, 0);
    mono_write_file(json_out, dumped);
    free(dumped);
    json_free(out);
    free(current_cat);
    mslist_free(&lines);
    return 0;
}

static int cmd_sway_waybar(const char *cfg_path, const char *css_path) {
    char *s = mono_read_file(cfg_path);
    replace_inplace(&s, "\"hyprland/workspaces\"", "\"sway/workspaces\"");
    replace_inplace(&s, "\"hyprland/submap\"", "\"sway/mode\"");
    replace_inplace(&s, "\"hyprland/window\"", "\"sway/window\"");
    replace_inplace(&s, "\"separate-outputs\": true", "\"max-length\": 70");
    replace_inplace(&s, "        \"temperature\",\n", "");
    replace_inplace(&s, "        \"bluetooth\",\n", "");
    replace_inplace(&s, "        \"battery\",\n", "");
    replace_inplace(&s, "        \"custom/updates\",\n", "");
    mono_write_file(cfg_path, s);
    free(s);

    char *css = mono_read_file(css_path);
    replace_inplace(&css, "#workspaces button.active {", "#workspaces button.active,\n#workspaces button.focused {");
    replace_inplace(&css, "#submap {", "#submap,\n#mode {");
    mono_write_file(css_path, css);
    free(css);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: mono-util <check-configs|check-greyscale|image-size|gif-frames|keybinds-json|sway-waybar> ...\n");
        return 1;
    }
    const char *cmd = argv[1];
    char repo[PATH_MAX];

    if (strcmp(cmd, "check-configs") == 0) {
        if (argc >= 3) snprintf(repo, sizeof(repo), "%s", argv[2]);
        else mono_find_repo_root(argv[0], repo, sizeof(repo));
        return cmd_check_configs(repo);
    }
    if (strcmp(cmd, "check-greyscale") == 0) {
        if (argc >= 3) snprintf(repo, sizeof(repo), "%s", argv[2]);
        else mono_find_repo_root(argv[0], repo, sizeof(repo));
        return cmd_check_greyscale(repo);
    }
    if (strcmp(cmd, "image-size") == 0 && argc >= 3) {
        int w = 0, h = 0;
        if (!mono_read_jpeg_size(argv[2], &w, &h) || w <= 0 || h <= 0) return 1;
        printf("%dx%d\n", w, h);
        return 0;
    }
    if (strcmp(cmd, "gif-frames") == 0 && argc >= 3) {
        int n = mono_count_gif_frames(argv[2]);
        printf("%d\n", n);
        return (n > 0) ? 0 : 1;
    }
    if (strcmp(cmd, "keybinds-json") == 0 && argc >= 4) {
        return cmd_keybinds_json(argv[2], argv[3]);
    }
    if (strcmp(cmd, "sway-waybar") == 0 && argc >= 4) {
        return cmd_sway_waybar(argv[2], argv[3]);
    }
    fprintf(stderr, "Unknown or incomplete command: %s\n", cmd);
    return 1;
}
