#!/usr/bin/env bash
# Kinetic Floating / PiP 4-Corner Spring Snap (Super + Alt + P).
#
# Cycles the focused window smoothly around the 4 screen corners using
# `movewindowpixel exact X Y` — paired with `silkSpring` on `windowsMove`,
# the window physically springs from corner to corner like macOS FaceTime PiP.
# If the focused window is tiled, floats and resizes it to a 34% PiP card first!

set -u
. "$(dirname "$0")/lib.sh"

case "${1:-}" in --*) set -- "${1#--}" "${@:2}" ;; esac

CORNER_STATE="$CACHE_DIR/pip-corner.state"

win_json="$(hyprctl activewindow -j 2>/dev/null || echo '{}')"
is_float="$(printf '%s' "$win_json" | jq -r '.floating // false')"
mon_id="$(printf '%s' "$win_json" | jq -r '.monitor // 0')"

IFS=$'\t' read -r mx my mw mh < <(monitor_geometry "$mon_id")
[ -z "${mw:-}" ] && { mx=0; my=0; mw=1920; mh=1080; }

if [ "$is_float" != "true" ]; then
    pw=$(( mw * 34 / 100 ))
    ph=$(( mh * 34 / 100 ))
    hyprctl --batch "dispatch togglefloating ; dispatch resizeactive exact $pw $ph" >/dev/null 2>&1 || true
    ww="$pw"; wh="$ph"
else
    ww="$(printf '%s' "$win_json" | jq -r '.size[0] // 640')"
    wh="$(printf '%s' "$win_json" | jq -r '.size[1] // 360')"
fi

pad_x=20
pad_top=54   # clears the 38px Waybar + breathing room
pad_bot=20

prev="$(cat "$CORNER_STATE" 2>/dev/null || echo 0)"
next="${1:-}"
if [ -z "$next" ]; then
    next=$(( (prev % 4) + 1 ))
fi

case "$next" in
    1|top-right)
        next=1; tx=$(( mx + mw - ww - pad_x )); ty=$(( my + pad_top )) ;;
    2|bottom-right)
        next=2; tx=$(( mx + mw - ww - pad_x )); ty=$(( my + mh - wh - pad_bot )) ;;
    3|bottom-left)
        next=3; tx=$(( mx + pad_x ));           ty=$(( my + mh - wh - pad_bot )) ;;
    4|top-left|*)
        next=4; tx=$(( mx + pad_x ));           ty=$(( my + pad_top )) ;;
esac

printf '%s\n' "$next" > "$CORNER_STATE"
hyprctl dispatch movewindowpixel "exact $tx $ty,activewindow" >/dev/null 2>&1 \
    || hyprctl dispatch moveactive "exact $tx $ty" >/dev/null 2>&1 || true
