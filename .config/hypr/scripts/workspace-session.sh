#!/usr/bin/env bash
# Workspace Session Snapshot Engine — save & restore multi-workspace setups.
#
# Captures open windows (mapped safely from window class -> command), target
# workspace IDs, active tiling layout, gap preset, and active wallpaper into
# ~/.config/hypr/sessions/<name>.json so you can restore your whole desktop
# environment in one command after rebooting.
#
#   workspace-session.sh save [name]       save current session (default: work)
#   workspace-session.sh restore [name]    restore saved session
#   workspace-session.sh list              list saved session profiles
#   workspace-session.sh show [name]       print snapshot summary
#   workspace-session.sh delete <name>     remove a saved snapshot
#   workspace-session.sh menu              interactive Rofi session manager

set -u
. "$(dirname "$0")/lib.sh"

case "${1:-}" in --*) set -- "${1#--}" "${@:2}" ;; esac

SESS_DIR="$HYPR_DIR/sessions"
mkdir -p "$SESS_DIR" 2>/dev/null || true

sanitize_name() {
    local raw="${1:-work}"
    raw="$(printf '%s' "$raw" | tr '[:upper:] ' '[:lower:]-' | tr -cd 'a-z0-9_-')"
    [ -z "$raw" ] && raw="work"
    printf '%s' "$raw"
}

class_to_cmd() {
    local cls="${1,,}"
    case "$cls" in
        kitty|kitty-float) printf '%s' "$(mono_var terminal kitty)" ;;
        foot)              printf 'foot' ;;
        alacritty)         printf 'alacritty' ;;
        wezterm*)          printf 'wezterm' ;;
        thunar)            printf '%s' "$(mono_var file_manager thunar)" ;;
        firefox*)          printf 'firefox' ;;
        brave*)            printf 'brave' ;;
        chromium*)         printf 'chromium' ;;
        google-chrome*)    printf 'google-chrome-stable' ;;
        zen*)              printf 'zen-browser' ;;
        librewolf*)        printf 'librewolf' ;;
        code|code-oss)     printf 'code' ;;
        vscodium|codium)   printf 'codium' ;;
        dev.zed.zed|zed)   printf 'zeditor' ;;
        obsidian)          printf 'obsidian' ;;
        spotify)           printf 'spotify' ;;
        mpv)               printf 'mpv' ;;
        pavucontrol)       printf 'pavucontrol' ;;
        *)
            # Accept simple binary-safe class names if installed in PATH
            if [[ "$cls" =~ ^[a-z0-9._-]+$ ]] && have "$cls"; then
                printf '%s' "$cls"
            fi ;;
    esac
}

cmd_save() {
    local name file layout gaps wall count
    name="$(sanitize_name "${1:-work}")"
    file="$SESS_DIR/${name}.json"

    layout="$("$SCRIPTS_DIR/layout.sh" status 2>/dev/null | awk '{print $1}')"
    gaps="$("$SCRIPTS_DIR/layout.sh" status 2>/dev/null | sed -n 's/.*(\(.*\)).*/\1/p')"
    wall="$(cat "$CACHE_DIR/current_wallpaper" 2>/dev/null || true)"

    # Filter client rows (ignore scratchpads & unmapped helper surfaces) and serialize via Pure C11 mono-ipc
    count="$(
        while IFS=$'\t' read -r ws cls floating title; do
            [ -n "$cls" ] || continue
            case "$ws" in special:*|-*) continue ;; esac
            case "$cls" in dropdown-term|mono-btop|mono-notes|mono-files|Rofi|rofi) continue ;; esac
            lcmd="$(class_to_cmd "$cls")"
            [ -n "$lcmd" ] || continue
            printf '%s\t%s\t%s\t%s\t%s\n' "$ws" "$cls" "$lcmd" "${floating:-false}" "$title"
        done < <(mono_ipc session-clients 2>/dev/null) \
        | mono_ipc session-write "$file" "$name" "${layout:-dwindle}" "${gaps:-default}" "$wall"
    )"
    [ -z "$count" ] && count=0

    notify "Session saved · $name" "$count window(s), layout=${layout:-dwindle}, gaps=${gaps:-default}"
    printf 'saved %s (%s windows) -> %s\n' "$name" "$count" "$file"
}

cmd_restore() {
    local name file
    name="$(sanitize_name "${1:-work}")"
    file="$SESS_DIR/${name}.json"
    if [ ! -f "$file" ]; then
        notify "Session restore" "No saved snapshot '$name'" "dialog-warning"
        echo "session not found: $file" >&2
        exit 1
    fi

    local layout gaps wall count
    IFS=$'\t' read -r layout gaps wall count < <(mono_ipc session-read "$file" meta 2>/dev/null)
    [ -z "${layout:-}" ] && layout="dwindle"
    [ -z "${gaps:-}" ] && gaps="default"
    [ -z "${count:-}" ] && count="0"

    [ -n "${wall:-}" ] && [ -f "$wall" ] && "$SCRIPTS_DIR/wallpaper.sh" set "$wall" >/dev/null 2>&1 || true
    [ -n "$layout" ] && "$SCRIPTS_DIR/layout.sh" set "$layout" >/dev/null 2>&1 || true
    [ -n "$gaps" ]   && "$SCRIPTS_DIR/layout.sh" gaps "$gaps" >/dev/null 2>&1 || true

    while IFS=$'\t' read -r ws fl lcmd; do
        [ -n "$lcmd" ] || continue
        local rule="[workspace ${ws} silent]"
        [ "$fl" = "true" ] && rule="[workspace ${ws} silent; float]"
        hyprctl dispatch exec "$rule $lcmd" >/dev/null 2>&1 || true
    done < <(mono_ipc session-read "$file" windows 2>/dev/null)

    notify "Session restored · $name" "Launched $count window(s) across workspaces"
    printf 'restored %s (%s windows)\n' "$name" "$count"
}

cmd_list() {
    local f base lay _g _w cnt
    for f in "$SESS_DIR"/*.json; do
        [ -f "$f" ] || continue
        base="$(basename "$f" .json)"
        IFS=$'\t' read -r lay _g _w cnt < <(mono_ipc session-read "$f" meta 2>/dev/null)
        printf '%s\t%s windows · %s\n' "$base" "${cnt:-0}" "${lay:-dwindle}"
    done
}

cmd_show() {
    local name file
    name="$(sanitize_name "${1:-work}")"
    file="$SESS_DIR/${name}.json"
    [ -f "$file" ] || { echo "session not found: $name" >&2; exit 1; }
    mono_ipc session-read "$file" pretty
}

cmd_delete() {
    local name file
    name="$(sanitize_name "${1:-}")"
    file="$SESS_DIR/${name}.json"
    if [ -f "$file" ]; then
        rm -f "$file"
        notify "Session deleted" "Removed snapshot '$name'"
    fi
}

cmd_menu() {
    local entries="💾  Save Current Workspace Session as 'work'\n💾  Save Current Session with Custom Name…"
    local line
    while IFS=$'\t' read -r sname meta; do
        [ -n "$sname" ] || continue
        entries+="\n↺  Restore '${sname}'  (${meta})"
        entries+="\n🗑  Delete '${sname}'"
    done < <(cmd_list)

    local pick
    pick="$(printf '%b\n' "$entries" | rofi_menu settings -dmenu -i -p "sessions:" \
        -mesg "Save open apps + workspaces + layout + wallpaper & restore anytime")" || exit 0

    case "$pick" in
        *"Save Current Workspace Session as 'work'"*)
            cmd_save work ;;
        *"Custom Name"*)
            local custom
            custom="$(printf '' | rofi_menu settings -dmenu -p "session name:" -mesg "Type a profile name (e.g. coding, writing, stream)")" || exit 0
            [ -n "$custom" ] && cmd_save "$custom" ;;
        "↺  Restore '"*)
            line="${pick#"↺  Restore '"}"
            cmd_restore "${line%%\'*}" ;;
        "🗑  Delete '"*)
            line="${pick#"🗑  Delete '"}"
            cmd_delete "${line%%\'*}" ;;
    esac
}

case "${1:-menu}" in
    save)    cmd_save "${2:-work}" ;;
    restore) cmd_restore "${2:-work}" ;;
    list)    cmd_list ;;
    show)    cmd_show "${2:-work}" ;;
    delete)  cmd_delete "${2:-work}" ;;
    menu)    cmd_menu ;;
    *)
        echo "usage: workspace-session.sh {menu|save [name]|restore [name]|list|show [name]|delete <name>}" >&2
        exit 1 ;;
esac
