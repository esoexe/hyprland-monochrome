// ═══════════════════════════════════════════════════════════════════════════
//  tools/build-windows-kit.c — Build Windows 11 companion kit in Pure C11
//
//  Generates `windows/glazewm-config.yaml`, `windows-terminal-scheme.json`,
//  and `windows-terminal-settings.json` directly from `.config/kitty/theme.conf`
//  and `.config/hypr/modules/user.lua`.
//
//    ./bin/build-windows-kit            # write windows/*
//    ./bin/build-windows-kit --check    # exit 1 if out of date
// ═══════════════════════════════════════════════════════════════════════════

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "../include/mono_config.h"

static double lua_num(const char *user_lua, const char *key, double fallback) {
    char fb_buf[64];
    snprintf(fb_buf, sizeof(fb_buf), "%f", fallback);
    char *raw = mono_lua_value(user_lua, key, fb_buf);
    char *ep = NULL;
    double v = strtod(raw, &ep);
    if (!ep || *ep != '\0') v = fallback;
    free(raw);
    return v;
}

static JsonVal *build_wt_scheme(const MonoKVList *kitty) {
    JsonVal *o = json_new_obj();
    json_obj_put(o, "name", json_new_str("hyprland-monochrome"));
    json_obj_put(o, "background", json_new_str(mkv_get(kitty, "background", "#0a0a0a")));
    json_obj_put(o, "foreground", json_new_str(mkv_get(kitty, "foreground", "#e6e6e6")));
    json_obj_put(o, "cursorColor", json_new_str(mkv_get(kitty, "cursor", "#e6e6e6")));
    json_obj_put(o, "selectionBackground", json_new_str(mkv_get(kitty, "selection_background", "#e6e6e6")));
    json_obj_put(o, "black", json_new_str(mkv_get(kitty, "color0", "#141414")));
    json_obj_put(o, "red", json_new_str(mkv_get(kitty, "color1", "#141414")));
    json_obj_put(o, "green", json_new_str(mkv_get(kitty, "color2", "#141414")));
    json_obj_put(o, "yellow", json_new_str(mkv_get(kitty, "color3", "#141414")));
    json_obj_put(o, "blue", json_new_str(mkv_get(kitty, "color4", "#141414")));
    json_obj_put(o, "purple", json_new_str(mkv_get(kitty, "color5", "#141414")));
    json_obj_put(o, "cyan", json_new_str(mkv_get(kitty, "color6", "#141414")));
    json_obj_put(o, "white", json_new_str(mkv_get(kitty, "color7", "#141414")));
    json_obj_put(o, "brightBlack", json_new_str(mkv_get(kitty, "color8", "#ffffff")));
    json_obj_put(o, "brightRed", json_new_str(mkv_get(kitty, "color9", "#ffffff")));
    json_obj_put(o, "brightGreen", json_new_str(mkv_get(kitty, "color10", "#ffffff")));
    json_obj_put(o, "brightYellow", json_new_str(mkv_get(kitty, "color11", "#ffffff")));
    json_obj_put(o, "brightBlue", json_new_str(mkv_get(kitty, "color12", "#ffffff")));
    json_obj_put(o, "brightPurple", json_new_str(mkv_get(kitty, "color13", "#ffffff")));
    json_obj_put(o, "brightCyan", json_new_str(mkv_get(kitty, "color14", "#ffffff")));
    json_obj_put(o, "brightWhite", json_new_str(mkv_get(kitty, "color15", "#ffffff")));
    return o;
}

static JsonVal *build_wt_settings(const char *user_lua, const MonoKVList *kitty) {
    int opacity = (int)(lua_num(user_lua, "terminal_opacity", 0.92) * 100.0);

    JsonVal *font = json_new_obj();
    json_obj_put(font, "face", json_new_str("JetBrainsMono Nerd Font"));
    json_obj_put(font, "size", json_new_int(11));

    JsonVal *defs = json_new_obj();
    json_obj_put(defs, "colorScheme", json_new_str("hyprland-monochrome"));
    json_obj_put(defs, "font", font);
    json_obj_put(defs, "opacity", json_new_int(opacity));
    json_obj_put(defs, "padding", json_new_str("14"));
    json_obj_put(defs, "useAcrylic", json_new_bool(true));
    json_obj_put(defs, "cursorShape", json_new_str("bar"));
    json_obj_put(defs, "antialiasingMode", json_new_str("cleartype"));

    JsonVal *profiles = json_new_obj();
    json_obj_put(profiles, "defaults", defs);

    JsonVal *schemes = json_new_arr();
    json_arr_push(schemes, build_wt_scheme(kitty));

    JsonVal *root = json_new_obj();
    json_obj_put(root, "profiles", profiles);
    json_obj_put(root, "schemes", schemes);
    json_obj_put(root, "_comment", json_new_str(
        "Merge this into %LOCALAPPDATA%\\Packages\\Microsoft.WindowsTerminal_*\\LocalState\\settings.json "
        "\xe2\x80\x94 or open Settings with Ctrl+, \xe2\x86\x92 Open JSON file and paste. The scheme comes "
        "from .config/kitty/theme.conf, so both terminals are the same greys. 'opacity' and 'padding' come from modules/user.lua."));
    return root;
}

static char *build_glazewm_yaml(const char *user_lua, const MonoKVList *kitty) {
    int inner = (int)lua_num(user_lua, "gaps_in", 5.0);
    int outer = (int)lua_num(user_lua, "gaps_out", 10.0);
    int rounding = (int)lua_num(user_lua, "rounding", 10.0);
    double inactive = lua_num(user_lua, "inactive_opacity", 0.94);
    int count = (int)lua_num(user_lua, "workspace_count", 10.0);

    const char *text = mkv_get(kitty, "foreground", "#e6e6e6");
    const char *hairline = mkv_get(kitty, "inactive_border_color", "#2a2a2a");
    const char *corner = (rounding >= 8) ? "rounded" : ((rounding >= 4) ? "small_rounded" : "square");
    int op_pct = (int)round(inactive * 100.0);

    MonoStr o;
    mstr_init(&o);
    mstr_printf(&o,
        "# ═══════════════════════════════════════════════════════════════════════════\n"
        "#  glazewm-config.yaml — the Windows half of hyprland-monochrome\n"
        "#\n"
        "#  GENERATED by tools/build-windows-kit.c from modules/user.lua and\n"
        "#  kitty/theme.conf. Edit those and re-run `make docs`; edits here get overwritten.\n"
        "#\n"
        "#  Install:  winget install GlazeWM      (Zebar is offered during setup)\n"
        "#  Place at: %%userprofile%%\\.glzr\\glazewm\\config.yaml\n"
        "#\n"
        "#  Hyprland itself cannot run on Windows (no Wayland, no DRM), so this is the\n"
        "#  closest thing: GlazeWM is an i3-style tiling WM, and Zebar is a bar in the\n"
        "#  same spirit as Waybar.\n"
        "#\n"
        "#  Note on the modifier: this setup uses SUPER on Linux. Windows reserves some\n"
        "#  Windows-key combinations (lwin+l, lwin+d …), so GlazeWM's own docs advise\n"
        "#  against it. Everything below therefore uses ALT. If you want to match Linux\n"
        "#  exactly, swap the alt+ prefixes for lwin+ and expect a few clashes.\n"
        "# ═══════════════════════════════════════════════════════════════════════════\n\n"
        "general:\n"
        "  # Left empty on purpose: these commands run on every launch, and if Zebar is\n"
        "  # not installed GlazeWM will pop up an error each time. Uncomment the pair\n"
        "  # below once you have Zebar running.\n"
        "  startup_commands: []\n"
        "  shutdown_commands: []\n"
        "  # startup_commands: ['shell-exec zebar']\n"
        "  # shutdown_commands: ['shell-exec taskkill /IM zebar.exe /F']\n"
        "  focus_follows_cursor: false\n"
        "  cursor_jump:\n"
        "    enabled: true\n"
        "    trigger: 'monitor_focus'\n"
        "  hide_method: 'cloak'\n"
        "  show_all_in_taskbar: false\n\n"
        "# gaps_in / gaps_out from modules/user.lua. The top gap leaves room for Zebar.\n"
        "gaps:\n"
        "  scale_with_dpi: true\n"
        "  inner_gap: '%dpx'\n"
        "  outer_gap:\n"
        "    top: '%dpx'\n"
        "    right: '%dpx'\n"
        "    bottom: '%dpx'\n"
        "    left: '%dpx'\n\n"
        "# The monochrome look: a white border on the focused window, near-black on the\n"
        "# rest, no title bars (Hyprland draws none either).\n"
        "window_effects:\n"
        "  focused_window:\n"
        "    border:\n"
        "      enabled: true\n"
        "      color: '%s'\n"
        "    hide_title_bar:\n"
        "      enabled: true\n"
        "    corner_style:\n"
        "      enabled: true\n"
        "      style: '%s'\n"
        "    transparency:\n"
        "      enabled: false\n"
        "      opacity: '100%%'\n"
        "  other_windows:\n"
        "    border:\n"
        "      enabled: true\n"
        "      color: '%s'\n"
        "    hide_title_bar:\n"
        "      enabled: true\n"
        "    corner_style:\n"
        "      enabled: true\n"
        "      style: '%s'\n"
        "    transparency:\n"
        "      enabled: true\n"
        "      opacity: '%d%%'        # inactive_opacity from user.lua\n\n"
        "window_behavior:\n"
        "  initial_state: 'tiling'\n"
        "  state_defaults:\n"
        "    floating:\n"
        "      centered: true\n"
        "      shown_on_top: false\n"
        "    fullscreen:\n"
        "      maximized: false\n"
        "      shown_on_top: true\n\n"
        "workspaces:\n",
        inner, outer + 34, outer, outer, outer, text, corner, hairline, corner, op_pct);

    for (int i = 1; i <= count; ++i) {
        mstr_printf(&o, "  - name: '%d'\n", i);
    }

    mstr_puts(&o,
        "\nwindow_rules:\n"
        "  - commands: ['ignore']\n"
        "    match:\n"
        "      # Zebar is the bar — never tile it (our layer rules do the same for Waybar).\n"
        "      - window_process: { equals: 'zebar' }\n"
        "      - window_process: { equals: 'Zebar' }\n"
        "      # Picture-in-picture stays floating, like the rule in Hyprland.\n"
        "      - window_title: { regex: '[Pp]icture.in.[Pp]icture' }\n"
        "        window_class: { regex: 'Chrome_WidgetWin_1|MozillaDialogClass' }\n"
        "      # PowerToys widgets are overlays, not windows.\n"
        "      - window_process: { equals: 'PowerToys' }\n"
        "        window_class: { regex: 'HwndWrapper\\[PowerToys\\.PowerAccent.*?\\]' }\n"
        "      - window_title: { equals: 'Command Palette' }\n"
        "        window_class: { equals: 'WinUIDesktopWin32WindowClass' }\n\n"
        "binding_modes:\n"
        "  # alt+r, then HJKL / arrows — same idea as the resize keybinds in binds.lua.\n"
        "  - name: 'resize'\n"
        "    keybindings:\n"
        "      - commands: ['resize --width -2%']\n"
        "        bindings: ['h', 'left']\n"
        "      - commands: ['resize --width +2%']\n"
        "        bindings: ['l', 'right']\n"
        "      - commands: ['resize --height +2%']\n"
        "        bindings: ['k', 'up']\n"
        "      - commands: ['resize --height -2%']\n"
        "        bindings: ['j', 'down']\n"
        "      - commands: ['wm-disable-binding-mode --name resize']\n"
        "        bindings: ['escape', 'enter', 'alt+r']\n\n"
        "keybindings:\n"
        "  # ── Launching (the shell-exec equivalents of our Rofi bindings) ──────────\n"
        "  - commands: ['shell-exec wt']                      # alt+enter / alt+t → terminal\n"
        "    bindings: ['alt+enter']\n"
        "  - commands: ['shell-exec explorer']                # alt+e → file manager\n"
        "    bindings: ['alt+e']\n"
        "  - commands: ['shell-exec cmd /c start \"\" https://']  # alt+b → browser\n"
        "    bindings: ['alt+b']\n"
        "  - commands: ['shell-exec ms-screenclip:']          # alt+print → screenshot\n"
        "    bindings: ['alt+print_screen']\n"
        "  - commands: ['shell-exec wt -p \"Command Prompt\"']  # only if you want a second shell\n"
        "    bindings: ['alt+shift+t']\n\n"
        "  # ── Focus ────────────────────────────────────────────────────────────────\n"
        "  - commands: ['focus --direction left']\n"
        "    bindings: ['alt+h', 'alt+left']\n"
        "  - commands: ['focus --direction right']\n"
        "    bindings: ['alt+l', 'alt+right']\n"
        "  - commands: ['focus --direction up']\n"
        "    bindings: ['alt+k', 'alt+up']\n"
        "  - commands: ['focus --direction down']\n"
        "    bindings: ['alt+j', 'alt+down']\n\n"
        "  # ── Move the focused window ──────────────────────────────────────────────\n"
        "  - commands: ['move --direction left']\n"
        "    bindings: ['alt+shift+h', 'alt+shift+left']\n"
        "  - commands: ['move --direction right']\n"
        "    bindings: ['alt+shift+l', 'alt+shift+right']\n"
        "  - commands: ['move --direction up']\n"
        "    bindings: ['alt+shift+k', 'alt+shift+up']\n"
        "  - commands: ['move --direction down']\n"
        "    bindings: ['alt+shift+j', 'alt+shift+down']\n\n"
        "  # ── Resize ───────────────────────────────────────────────────────────────\n"
        "  - commands: ['resize --width -2%']\n"
        "    bindings: ['alt+u']\n"
        "  - commands: ['resize --width +2%']\n"
        "    bindings: ['alt+p']\n"
        "  - commands: ['resize --height +2%']\n"
        "    bindings: ['alt+o']\n"
        "  - commands: ['resize --height -2%']\n"
        "    bindings: ['alt+i']\n"
        "  - commands: ['wm-enable-binding-mode --name resize']\n"
        "    bindings: ['alt+r']\n\n"
        "  # ── Window state (Super+Space float, Super+F fullscreen on Linux) ────────\n"
        "  - commands: ['toggle-floating --centered']\n"
        "    bindings: ['alt+space']\n"
        "  - commands: ['toggle-tiling']\n"
        "    bindings: ['alt+shift+space']\n"
        "  - commands: ['toggle-fullscreen']\n"
        "    bindings: ['alt+f']\n"
        "  - commands: ['toggle-minimized']\n"
        "    bindings: ['alt+m']\n"
        "  - commands: ['toggle-tiling-direction']\n"
        "    bindings: ['alt+y']\n"
        "  - commands: ['wm-cycle-focus']\n"
        "    bindings: ['alt+tab']\n\n"
        "  # ── Windows ──────────────────────────────────────────────────────────────\n"
        "  - commands: ['close']\n"
        "    bindings: ['alt+q']\n"
        "  - commands: ['wm-reload-config']\n"
        "    bindings: ['alt+shift+r']\n"
        "  - commands: ['wm-redraw']\n"
        "    bindings: ['alt+shift+w']\n"
        "  - commands: ['wm-toggle-pause']\n"
        "    bindings: ['alt+shift+p']\n"
        "  - commands: ['wm-exit']\n"
        "    bindings: ['alt+shift+e']\n\n"
        "  # ── Workspaces (alt+0 is the tenth) ──────────────────────────────────────\n");

    for (int i = 1; i <= count; ++i) {
        char kbuf[16];
        snprintf(kbuf, sizeof(kbuf), "%d", (i == 10) ? 0 : i);
        mstr_printf(&o, "  - commands: ['focus --workspace %d']\n    bindings: ['alt+%s']\n", i, kbuf);
    }
    mstr_putc(&o, '\n');
    for (int i = 1; i <= count; ++i) {
        char kbuf[16];
        snprintf(kbuf, sizeof(kbuf), "%d", (i == 10) ? 0 : i);
        mstr_printf(&o, "  - commands: ['move --workspace %d', 'focus --workspace %d']\n    bindings: ['alt+shift+%s']\n",
                    i, i, kbuf);
    }
    mstr_puts(&o,
        "\n  # ── Workspaces on other monitors ─────────────────────────────────────────\n"
        "  - commands: ['move-workspace --direction left']\n"
        "    bindings: ['alt+shift+a']\n"
        "  - commands: ['move-workspace --direction right']\n"
        "    bindings: ['alt+shift+d']\n");
    return o.s;
}

int main(int argc, char *argv[]) {
    bool check_only = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--check") == 0) check_only = true;
    }

    char repo[PATH_MAX], p_kitty[PATH_MAX], p_lua[PATH_MAX], win_dir[PATH_MAX];
    mono_find_repo_root(argv[0], repo, sizeof(repo));
    mono_path_join(p_kitty, sizeof(p_kitty), repo, ".config/kitty/theme.conf");
    mono_path_join(p_lua, sizeof(p_lua), repo, ".config/hypr/modules/user.lua");
    mono_path_join(win_dir, sizeof(win_dir), repo, "windows");

    char *kitty_txt = mono_read_file(p_kitty);
    MonoKVList kitty = mono_parse_kitty_palette(kitty_txt);
    free(kitty_txt);

    char *user_lua = mono_read_file(p_lua);

    char *yaml = build_glazewm_yaml(user_lua, &kitty);

    JsonVal *j_scheme = build_wt_scheme(&kitty);
    char *s_scheme_raw = mono_json_dump(j_scheme, 2);
    MonoStr ms_scheme; mstr_init(&ms_scheme); mstr_printf(&ms_scheme, "%s\n", s_scheme_raw);
    free(s_scheme_raw); json_free(j_scheme);

    JsonVal *j_settings = build_wt_settings(user_lua, &kitty);
    char *s_settings_raw = mono_json_dump(j_settings, 2);
    MonoStr ms_settings; mstr_init(&ms_settings); mstr_printf(&ms_settings, "%s\n", s_settings_raw);
    free(s_settings_raw); json_free(j_settings);

    const char *rel_names[3] = {
        "windows/glazewm-config.yaml",
        "windows/windows-terminal-scheme.json",
        "windows/windows-terminal-settings.json",
    };
    const char *contents[3] = { yaml, ms_scheme.s, ms_settings.s };

    int rc = 0;
    if (check_only) {
        for (int i = 0; i < 3; ++i) {
            char full[PATH_MAX];
            mono_path_join(full, sizeof(full), repo, rel_names[i]);
            bool match = false;
            if (mono_file_exists(full)) {
                char *cur = mono_read_file(full);
                match = (strcmp(cur, contents[i]) == 0);
                free(cur);
            }
            if (!match) {
                fprintf(stderr, "stale: %s\nrun: make docs (./bin/build-windows-kit)\n", rel_names[i]);
                rc = 1;
                break;
            }
        }
        if (rc == 0) printf("windows kit is up to date.\n");
    } else {
        mono_mkdir_p(win_dir);
        for (int i = 0; i < 3; ++i) {
            char full[PATH_MAX];
            mono_path_join(full, sizeof(full), repo, rel_names[i]);
            mono_write_file(full, contents[i]);
        }
        printf("wrote 3 files in windows/\n");
    }

    free(yaml);
    mstr_free(&ms_scheme);
    mstr_free(&ms_settings);
    free(user_lua);
    mkv_free(&kitty);
    return rc;
}
