#!/usr/bin/env bash
# Toggle floating for the focused window. When a window *becomes* floating it is
# centred and sized to ~70% of its monitor, which is the size you want 99% of
# the time. Bound to Super + SPACE.

set -u
. "$(dirname "$0")/lib.sh"

hyprctl dispatch 'hl.dsp.window.float({ action = "toggle" })' >/dev/null 2>&1

read -r floating tw th tx ty <<<"$(mono_ipc float-target 2>/dev/null)"
[ -z "${floating:-}" ] && exit 0

if [ "$floating" != "true" ]; then
    notify "Window" "Tiled"
    exit 0
fi

[ -z "${tw:-}" ] || [ "$tw" = "0" ] && exit 0

hyprctl dispatch "hl.dsp.window.resize({ x = ${tw}, y = ${th}, relative = false })" >/dev/null 2>&1
hyprctl dispatch "hl.dsp.window.move({ x = ${tx}, y = ${ty}, relative = false })"   >/dev/null 2>&1

notify "Window" "Floating · ${tw}×${th} · centred"
