#!/usr/bin/env bash
# Desktop zoom (cursor zoom). Bound to Super + '-' and Super + '='.
# Reset with:  ~/.config/hypr/scripts/zoom.sh reset

set -u
. "$(dirname "$0")/lib.sh"

# Tolerate --flags as well as plain arguments (handy in keybinds).
case "${1:-}" in --*) set -- "${1#--}" "${@:2}" ;; esac

STATE="$CACHE_DIR/zoom"

# Prefer the real value from Hyprland, fall back to our own state file.
current="$(hyprctl getoption cursor.zoom_factor 2>/dev/null | awk '{print $NF}' | head -1)"
[[ "$current" =~ ^[0-9]+(\.[0-9]+)?$ ]] || current="$(cat "$STATE" 2>/dev/null || echo 1.00)"
[[ "$current" =~ ^[0-9.]+$ ]] || current=1.00

case "${1:-in}" in
    in)    value=$(awk -v c="$current" 'BEGIN { v = c + 0.25; if (v > 3.0) v = 3.0; printf "%.2f", v }') ;;
    out)   value=$(awk -v c="$current" 'BEGIN { v = c - 0.25; if (v < 1.0) v = 1.0; printf "%.2f", v }') ;;
    reset) value=1.00 ;;
    *)     echo "usage: zoom.sh {in|out|reset}" >&2; exit 1 ;;
esac

echo "$value" > "$STATE"
hyprctl eval "hl.config({ cursor = { zoom_factor = ${value} } })" >/dev/null 2>&1 \
    || notify "Zoom failed" "Run: hyprctl eval 'hl.config({cursor = {zoom_factor = 1.5}})'" "dialog-error"

[ "$value" = "1.00" ] && notify "Zoom" "Reset" || notify "Zoom" "${value}x"
