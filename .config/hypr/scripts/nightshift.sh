#!/usr/bin/env bash
# Blue-light filter. Uses hyprsunset (native, preferred) and falls back to
# gammastep. Bound to Super + SHIFT + N.
#
#   nightshift.sh toggle     # on/off
#   nightshift.sh on|off
#   nightshift.sh 3500       # set a specific temperature (Kelvin)

set -u
. "$(dirname "$0")/lib.sh"

# Tolerate --flags as well as plain arguments (handy in keybinds).
case "${1:-}" in --*) set -- "${1#--}" "${@:2}" ;; esac

STATE="$CACHE_DIR/nightshift"
TEMP="$(mono_var night_shift_temp 4000)"
MODE="${1:-toggle}"

# ── hyprsunset (preferred) ────────────────────────────────────────────────
hyprsunset_on()  { hyprctl hyprsunset temperature "$1" >/dev/null 2>&1; }
hyprsunset_off() { hyprctl hyprsunset identity      >/dev/null 2>&1; }

if have hyprsunset; then
    pgrep -x hyprsunset >/dev/null 2>&1 || { setsid hyprsunset >/dev/null 2>&1 & sleep 0.5; }

    case "$MODE" in
        toggle)
            if [ "$(cat "$STATE" 2>/dev/null || echo off)" = "on" ]; then MODE=off; else MODE=on; fi ;;
    esac

    case "$MODE" in
        on|*[0-9]*)
            [ "$MODE" = "on" ] && TEMP="$TEMP" || TEMP="$MODE"
            if hyprsunset_on "$TEMP"; then
                echo on > "$STATE"
                notify "Night shift" "On · ${TEMP}K" "weather-clear-night"
            else
                notify "Night shift" "hyprsunset is not answering" "dialog-error"
            fi ;;
        off)
            hyprsunset_off
            echo off > "$STATE"
            notify "Night shift" "Off" "weather-clear" ;;
    esac
    exit 0
fi

# ── gammastep fallback ────────────────────────────────────────────────────
if have gammastep; then
    case "$MODE" in
        toggle) [ "$(cat "$STATE" 2>/dev/null || echo off)" = "on" ] && MODE=off || MODE=on ;;
    esac
    if [ "$MODE" = "off" ]; then
        pkill -x gammastep >/dev/null 2>&1
        gammastep -x >/dev/null 2>&1
        echo off > "$STATE"
        notify "Night shift" "Off"
    else
        pkill -x gammastep >/dev/null 2>&1
        setsid gammastep -O "${MODE/on/$TEMP}" >/dev/null 2>&1 &
        echo on > "$STATE"
        notify "Night shift" "On · $([ "$MODE" = on ] && echo "$TEMP" || echo "$MODE")K"
    fi
    exit 0
fi

notify "Night shift unavailable" "Install hyprsunset or gammastep" "dialog-warning"
