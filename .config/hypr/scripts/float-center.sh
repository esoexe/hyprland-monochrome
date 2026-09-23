#!/usr/bin/env bash
# Toggle floating for the focused window. When a window *becomes* floating it is
# centred and sized to ~70% of its monitor, which is the size you want 99% of
# the time. Bound to Super + SPACE.

set -u
. "$(dirname "$0")/lib.sh"

hyprctl dispatch 'hl.dsp.window.float({ action = "toggle" })' >/dev/null 2>&1

win="$(hyprctl activewindow -j 2>/dev/null)"
[ -z "$win" ] && exit 0

floating="$(printf '%s' "$win" | jq -r '.floating // false')"
if [ "$floating" != "true" ]; then
    notify "Window" "Tiled"
    exit 0
fi

mon_id="$(printf '%s' "$win" | jq -r '.monitor // 0')"
read -r mx my mw mh <<<"$(monitor_geometry "$mon_id")"
[ -z "${mw:-}" ] && exit 0

read -r tw th tx ty <<<"$(awk -v mx="$mx" -v my="$my" -v mw="$mw" -v mh="$mh" 'BEGIN {
    w = int(mw * 0.70); h = int(mh * 0.70);
    if (w < 600) w = (mw > 600 ? 600 : mw);
    if (h < 400) h = (mh > 400 ? 400 : mh);
    x = mx + int((mw - w) / 2);
    y = my + int((mh - h) / 2);
    printf "%d %d %d %d", w, h, x, y
}')"

hyprctl dispatch "hl.dsp.window.resize({ x = ${tw}, y = ${th}, relative = false })" >/dev/null 2>&1
hyprctl dispatch "hl.dsp.window.move({ x = ${tx}, y = ${ty}, relative = false })"   >/dev/null 2>&1

notify "Window" "Floating · ${tw}×${th} · centred"
