#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════════
#  browser-theme.sh — OpenCode Browser Theme Installer & Manager
#
#  Applies the OpenCode design system (`#131010` / `#201d1d` / `#fdfcfc`,
#  100% monospace chrome, 4px utilitarian geometry, 1px structural borders)
#  across:
#    • Firefox, LibreWolf, Zen Browser, Floorp, and Waterfox profiles
#      (`chrome/userChrome.css`, `chrome/userContent.css`, `user.js`)
#    • Chromium, Brave, Google Chrome, Vivaldi, and Edge
#      (Wayland flags + Manifest V3 OpenCode theme & New-Tab extensions)
#    • Local offline OpenCode Startpage (`~/.config/browser/opencode/startpage.html`)
# ═══════════════════════════════════════════════════════════════════════════
set -euo pipefail
# shellcheck source=lib.sh
. "$(dirname "$0")/lib.sh"

OC_DIR="$CONFIG_DIR/browser/opencode"
FF_SRC="$OC_DIR/firefox"
STARTPAGE="$OC_DIR/startpage.html"

apply_firefox_profile() {
    local prof="$1"
    [ -d "$prof" ] || return 0
    [ -d "$FF_SRC" ] || return 0

    mkdir -p "$prof/chrome"
    cp -f "$FF_SRC/userChrome.css" "$prof/chrome/userChrome.css"
    cp -f "$FF_SRC/userContent.css" "$prof/chrome/userContent.css"

    # Merge OpenCode prefs safely without clobbering existing unrelated user.js lines
    local marker_begin="// >>> hyprland-monochrome opencode-theme >>>"
    local marker_end="// <<< hyprland-monochrome opencode-theme <<<"
    local tmp="$prof/user.js.mono.tmp"

    if [ -f "$prof/user.js" ]; then
        awk -v b="$marker_begin" -v e="$marker_end" '
            $0 == b { skip = 1; next }
            $0 == e { skip = 0; next }
            !skip { print }
        ' "$prof/user.js" > "$tmp"
    else
        : > "$tmp"
    fi

    {
        printf '\n%s\n' "$marker_begin"
        cat "$FF_SRC/user.js"
        if [ -f "$STARTPAGE" ]; then
            printf 'user_pref("browser.startup.homepage", "file://%s");\n' "$STARTPAGE"
        fi
        printf '%s\n' "$marker_end"
    } >> "$tmp"

    mv -f "$tmp" "$prof/user.js"
}

apply_all_firefox_family() {
    local count=0 root prof
    for root in \
        "$HOME/.mozilla/firefox" \
        "$HOME/.librewolf" \
        "$HOME/.zen" \
        "$HOME/.floorp" \
        "$HOME/.waterfox" \
        "$HOME/.var/app/org.mozilla.firefox/.mozilla/firefox" \
        "$HOME/.var/app/io.gitlab.librewolf-community/.librewolf" \
        "$HOME/.var/app/app.zen_browser.zen/.zen"
    do
        [ -d "$root" ] || continue
        for prof in "$root"/*.default* "$root"/*.dev-edition* "$root"/*release*; do
            [ -d "$prof" ] || continue
            apply_firefox_profile "$prof"
            count=$((count + 1))
        done
    done
    REPLY_FF_COUNT="$count"
}

apply_chromium_flags() {
    local f
    for f in \
        "$CONFIG_DIR/chromium-flags.conf" \
        "$CONFIG_DIR/brave-flags.conf" \
        "$CONFIG_DIR/chrome-flags.conf" \
        "$CONFIG_DIR/vivaldi-stable.conf"
    do
        [ -f "$f" ] && continue
        cat > "$f" <<'EOF'
--ozone-platform-hint=auto
--enable-features=WaylandWindowDecorations,WebUIDarkMode
--force-dark-mode
EOF
    done
}

cmd_apply() {
    local quiet="${1:-}"
    REPLY_FF_COUNT=0
    apply_all_firefox_family
    apply_chromium_flags
    if [ "$quiet" != "--quiet" ]; then
        notify "OpenCode Browser Theme" \
            "Applied to ${REPLY_FF_COUNT} Firefox-family profile(s); Chromium MV3 theme ready in ~/.config/browser/opencode"
        printf 'OpenCode browser theme applied (%d Firefox-family profile(s) updated).\n' "$REPLY_FF_COUNT"
        printf 'Startpage : file://%s\n' "$STARTPAGE"
        printf 'Chromium  : load unpacked %s/chromium-theme (and optional chromium-newtab)\n' "$OC_DIR"
    fi
}

cmd_status() {
    printf 'Startpage       : %s (%s)\n' "$STARTPAGE" "$([ -f "$STARTPAGE" ] && printf 'ready' || printf 'missing')"
    printf 'Firefox CSS     : %s (%s)\n' "$FF_SRC/userChrome.css" "$([ -f "$FF_SRC/userChrome.css" ] && printf 'ready' || printf 'missing')"
    printf 'Chromium MV3    : %s (%s)\n' "$OC_DIR/chromium-theme/manifest.json" "$([ -f "$OC_DIR/chromium-theme/manifest.json" ] && printf 'ready' || printf 'missing')"
    printf 'OpenCode TUI    : %s (%s)\n' "$CONFIG_DIR/opencode/themes/monochrome.json" "$([ -f "$CONFIG_DIR/opencode/themes/monochrome.json" ] && printf 'ready' || printf 'missing')"
}

cmd_menu() {
    local choice
    choice="$(printf '%s\n' \
        "󰖟  Open OpenCode Startpage" \
        "󰏘  Apply OpenCode Theme to All Browser Profiles" \
        "󰊯  Copy Chromium Unpacked Theme Path" \
        "󰈙  Edit Startpage (~/.config/browser/opencode/startpage.html)" \
        | rofi_menu list -dmenu -i -p "browser · opencode")" || exit 0

    case "$choice" in
        *"Open OpenCode Startpage"*)
            "$SCRIPTS_DIR/open-app.sh" browser "$STARTPAGE"
            ;;
        *"Apply OpenCode Theme"*)
            cmd_apply
            ;;
        *"Copy Chromium Unpacked Theme Path"*)
            if have wl-copy; then
                printf '%s' "$OC_DIR/chromium-theme" | wl-copy
                notify "Copied path" "$OC_DIR/chromium-theme (open chrome://extensions -> Load unpacked)"
            else
                notify "Chromium Theme Path" "$OC_DIR/chromium-theme"
            fi
            ;;
        *"Edit Startpage"*)
            "$SCRIPTS_DIR/open-app.sh" open "$STARTPAGE"
            ;;
    esac
}

case "${1:-status}" in
    apply)          shift || true; cmd_apply "${1:-}" ;;
    status)         cmd_status ;;
    open-startpage) "$SCRIPTS_DIR/open-app.sh" browser "$STARTPAGE" ;;
    launch)         shift || true; MONO_BROWSER_STARTPAGE=1 "$SCRIPTS_DIR/open-app.sh" browser "$@" ;;
    menu)           cmd_menu ;;
    *)
        printf 'Usage: %s {apply [--quiet]|status|open-startpage|launch [url]|menu}\n' "${0##*/}" >&2
        exit 2
        ;;
esac
