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

[ -d "$CACHE_DIR" ] || mkdir -p "$CACHE_DIR" 2>/dev/null || true

_mono_trim() {
    local s="$1"
    s="${s#"${s%%[![:space:]]*}"}"
    s="${s%"${s##*[![:space:]]}"}"
    REPLY="$s"
}

# ── Read a value from modules/user.lua (pure Bash, zero subprocess forks) ──
#   mono_var gaps_in 5                    →  5
#   mono_var wallpaper_dir                →  /home/you/Pictures/Wallpapers
#   mono_var terminal_float               →  kitty --class kitty-float
#
# modules/user.lua is the single source of truth for the shell scripts too, so
# you never have to set the same thing in two places. Quote-aware: a "--"
# inside a string is kept, a trailing "-- comment" is dropped.
mono_var() {
    local name="$1" fallback="${2:-}" line rest val=""

    if [ -r "$USER_LUA" ]; then
        while IFS= read -r line || [ -n "$line" ]; do
            rest="${line#"${line%%[![:space:]]*}"}"
            case "$rest" in
                "${name}"[[:space:]=]*)
                    rest="${rest#"$name"}"
                    rest="${rest#"${rest%%[![:space:]]*}"}"
                    case "$rest" in
                        =*) rest="${rest#=}" ;;
                        *)  continue ;;
                    esac
                    local i=0 len="${#rest}" ch nxt q="" out=""
                    while [ "$i" -lt "$len" ]; do
                        ch="${rest:$i:1}"
                        if [ -n "$q" ]; then
                            [ "$ch" = "$q" ] && q=""
                            out+="$ch"
                        elif [ "$ch" = '"' ] || [ "$ch" = "'" ]; then
                            q="$ch"
                            out+="$ch"
                        elif [ "$ch" = "-" ]; then
                            nxt="${rest:$((i + 1)):1}"
                            [ "$nxt" = "-" ] && break
                            out+="$ch"
                        else
                            out+="$ch"
                        fi
                        i=$((i + 1))
                    done
                    val="$out"
                    break
                    ;;
            esac
        done < "$USER_LUA"
    fi

    if [ -z "$val" ]; then
        printf '%s' "$fallback"
        return
    fi

    # trailing comma / whitespace
    _mono_trim "$val"; val="$REPLY"
    while [ "${val%,}" != "$val" ]; do
        val="${val%,}"
        _mono_trim "$val"; val="$REPLY"
    done

    # "home .. \"/Pictures\"" (a Lua concatenation) → "$HOME" + "/Pictures"
    local prefix=""
    case "$val" in
        home[[:space:]]*..*|HOME[[:space:]]*..*|\$HOME[[:space:]]*..*|home..*|HOME..*|\$HOME..*)
            prefix="$HOME"
            val="${val#*..}" ;;
    esac

    # trim, then drop surrounding quotes
    _mono_trim "$val"; val="$REPLY"
    while [ "${val#+}" != "$val" ]; do
        val="${val#+}"
        _mono_trim "$val"; val="$REPLY"
    done
    case "$val" in
        \"*\") val="${val#\"}"; val="${val%\"}" ;;
    esac
    case "$val" in
        \'*\') val="${val#\'}"; val="${val%\'}" ;;
    esac

    val="${prefix}${val}"
    val="${val//\$\{HOME\}/$HOME}"
    val="${val//\$HOME/$HOME}"
    _mono_trim "$val"; val="$REPLY"

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

# Native Pure C11 Hyprland IPC & JSON engine ----------------------------------

resolve_ipc_bin() {
    local cand
    for cand in \
        "$HYPR_DIR/bin/mono-ipc" \
        "$SCRIPTS_DIR/../../../bin/mono-ipc" \
        "$CACHE_DIR/mono-ipc"
    do
        if [ -x "$cand" ]; then
            printf '%s\n' "$cand"
            return 0
        fi
    done

    local src="$HYPR_DIR/src/mono-ipc.c"
    local cc_bin=""
    if [ -f "$src" ]; then
        if have cc; then cc_bin="cc"
        elif have gcc; then cc_bin="gcc"
        elif have clang; then cc_bin="clang"
        fi
        if [ -n "$cc_bin" ]; then
            mkdir -p "$CACHE_DIR"
            if "$cc_bin" -std=c11 -O2 "$src" -o "$CACHE_DIR/mono-ipc" -lm 2>/dev/null; then
                printf '%s\n' "$CACHE_DIR/mono-ipc"
                return 0
            fi
        fi
    fi
    return 1
}

mono_ipc() {
    local ipc_bin
    ipc_bin="$(resolve_ipc_bin)" || return 1
    "$ipc_bin" "$@"
}

# Active window helpers -------------------------------------------------------

win_class()              { mono_ipc win-class 2>/dev/null; }
monitor_of_active_win()  { mono_ipc win-monitor 2>/dev/null; }

# Geometry of a monitor (logical pixels, i.e. what dispatchers expect).
monitor_geometry() {  # monitor_geometry <id>  →  "x<TAB>y<TAB>width<TAB>height"
    mono_ipc monitor-geom "${1:-0}" 2>/dev/null | head -1
}

