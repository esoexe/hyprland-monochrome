#!/usr/bin/env bash
# Cycle the opacity of the focused window. Bound to Super + O.
# Steps: 100% → 95% → 90% → 85% → 75% → back to 100%

set -u
. "$(dirname "$0")/lib.sh"

STATE="$CACHE_DIR/opacity"
STEPS=(1.0 0.95 0.9 0.85 0.75)

current="$(cat "$STATE" 2>/dev/null || echo 0)"
[[ "$current" =~ ^[0-9]+$ ]] || current=0
next=$(( (current + 1) % ${#STEPS[@]} ))
value="${STEPS[$next]}"
echo "$next" > "$STATE"

if [ "$(mono_ipc win-class --or-none 2>/dev/null || echo none)" = "none" ]; then
    notify "Window opacity" "No window focused"
    exit 0
fi

hyprctl dispatch "hl.dsp.window.set_prop({ prop = \"opacity\", value = \"${value} override\" })" >/dev/null

# 1.0 needs no override for long: drop it so the window follows the theme again
if [ "$value" = "1.0" ]; then
    hyprctl dispatch 'hl.dsp.window.set_prop({ prop = "opacity", value = "1.0" })' >/dev/null
fi

notify "Window opacity" "$(awk -v v="$value" 'BEGIN { printf "%d%%", v * 100 }')"
