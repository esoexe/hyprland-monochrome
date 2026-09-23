#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════════
#  lib.sh — shared helpers for every hyprland-monochrome script.
#  Source it, don't run it:      . "$(dirname "$0")/lib.sh"
# ═══════════════════════════════════════════════════════════════════════════

CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}"
HYPR_DIR="$CONFIG_DIR/hypr"
# Used by the scripts that source this file, so not "unused" here.
# shellcheck disable=SC2034
SCRIPTS_DIR="$HYPR_DIR/scripts"
ROFI_DIR="$CONFIG_DIR/rofi"
CACHE_DIR="${XDG_CACHE_HOME:-$HOME/.cache}/hyprland-monochrome"
USER_LUA="$HYPR_DIR/modules/user.lua"

mkdir -p "$CACHE_DIR" 2>/dev/null || true

# ── Read a value from modules/user.lua ─────────────────────────────────────
#   mono_var gaps_in 5                    →  5
#   mono_var wallpaper_dir                →  /home/you/Pictures/Wallpapers
#   mono_var terminal_float               →  kitty --class kitty-float
#
# modules/user.lua is the single source of truth for the shell scripts too, so
# you never have to set the same thing in two places. Quote-aware: a "--"
# inside a string is kept, a trailing "-- comment" is dropped.
mono_var() {
    local name="$1" fallback="${2:-}" val

    val="$(awk -v key="$name" '
        function strip_comment(s,   i, c, q, out, n) {
            SQ = sprintf("%c", 39)            # single quote
            q = ""; out = ""; n = length(s)
            for (i = 1; i <= n; i++) {
                c = substr(s, i, 1)
                if (q != "") { if (c == q) q = ""; out = out c }
                else if (c == "\"" || c == SQ) { q = c; out = out c }
                else if (c == "-" && substr(s, i + 1, 1) == "-") break
                else out = out c
            }
            return out
        }
        BEGIN { pat = "^[[:space:]]*" key "[[:space:]]*=" }
        $0 ~ pat {
            line = $0
            sub(pat, "", line)
            print strip_comment(line)
            exit
        }
    ' "$USER_LUA" 2>/dev/null)"

    if [ -z "$val" ]; then
        printf '%s' "$fallback"
        return
    fi

    # trailing comma / whitespace
    val="$(printf '%s' "$val" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//' -e 's/,*$//' -e 's/[[:space:]]*$//')"

    # "home .. \"/Pictures\"" (a Lua concatenation) → "$HOME" + "/Pictures"
    local prefix=""
    case "$val" in
        home[[:space:]]*..*|HOME[[:space:]]*..*|\$HOME[[:space:]]*..*)
            prefix="$HOME"
            val="${val#*..}" ;;
    esac

    # trim, then drop surrounding quotes
    val="$(printf '%s' "$val" | sed -e 's/^[[:space:]]*//' -e 's/^+[[:space:]]*//' -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')"
    case "$val" in
        \"*\") val="${val#\"}"; val="${val%\"}" ;;
    esac
    case "$val" in
        \'*\') val="${val#\'}"; val="${val%\'}" ;;
    esac

    val="${prefix}${val}"
    val="${val//\$\{HOME\}/$HOME}"
    val="${val//\$HOME/$HOME}"
    val="$(printf '%s' "$val" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')"

    if [ -z "$val" ]; then val="$fallback"; fi
    printf '%s' "$val"
}

# ── Small helpers ──────────────────────────────────────────────────────────

have() { command -v "$1" >/dev/null 2>&1; }

notify() {  # notify <summary> [body] [icon]
    have notify-send || return 0
    notify-send -a "mono" -i "${3:-utilities-terminal}" "$1" "${2:-}" 2>/dev/null || true
}

# Rofi with our monochrome theme. $1 = menu name (file in ~/.config/rofi,
# each of which @imports theme.rasi so a single -theme flag merges both).
rofi_menu() {
    local menu="$1"; shift
    rofi -no-config -theme "$ROFI_DIR/${menu}.rasi" "$@"
}

# Active window helpers -------------------------------------------------------

win_class()  { hyprctl activewindow -j 2>/dev/null | jq -r '.class // ""' 2>/dev/null; }
monitor_of_active_win() { hyprctl activewindow -j 2>/dev/null | jq -r '.monitor // 0' 2>/dev/null; }

# Geometry of a monitor (logical pixels, i.e. what dispatchers expect).
monitor_geometry() {  # monitor_geometry <id>  →  "x y width height"
    hyprctl monitors -j 2>/dev/null | jq -r --argjson id "${1:-0}" '
        .[] | select(.id == $id) |
        [ .x, .y, ((.width / (.scale // 1)) | floor), ((.height / (.scale // 1)) | floor) ]
        | @tsv' 2>/dev/null | head -1
}

