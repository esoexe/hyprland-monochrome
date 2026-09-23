#!/usr/bin/env bash
# Volume / microphone handling with a monochrome on-screen display.
# Used by the media keys, the Waybar audio module and the settings menu.
#
#   volume.sh up|down|mute|mic|status

set -u
. "$(dirname "$0")/lib.sh"

# Tolerate --flags as well as plain arguments (handy in keybinds).
case "${1:-}" in --*) set -- "${1#--}" "${@:2}" ;; esac

SINK="@DEFAULT_AUDIO_SINK@"
SRC="@DEFAULT_AUDIO_SOURCE@"
ID=1337   # notification replace-id keeps the OSD from stacking up

bar() {  # bar <percent 0..100> [width]
    awk -v p="$1" -v w="${2:-20}" 'BEGIN {
        filled = int((p / 100) * w + 0.5);
        s = "";
        for (i = 0; i < filled; i++) s = s "█";
        for (i = filled; i < w; i++) s = s "─";
        print s
    }'
}

state() {  # state <@DEFAULT_...>  →  "percent muted"
    local line
    line="$(wpctl get-volume "$1" 2>/dev/null)" || { echo "0 0"; return; }
    local pct muted
    pct="$(printf '%s' "$line" | awk '{ printf "%d", $2 * 100 }')"
    case "$line" in *MUTED*) muted=1 ;; *) muted=0 ;; esac
    echo "$pct $muted"
}

osd() {  # osd <title> <body>
    if have notify-send; then
        notify-send -r "$ID" -a "mono" -i audio-volume-high \
            -h string:x-canonical-private-synchronous:mono-volume \
            -h int:value:"$1" "$2" "$3" 2>/dev/null || notify "$2" "$3"
    fi
}

case "${1:-status}" in
    up)
        wpctl set-volume -l 1.5 "$SINK" 5%+ >/dev/null 2>&1 ;;
    down)
        wpctl set-volume "$SINK" 5%- >/dev/null 2>&1 ;;
    mute)
        wpctl set-mute "$SINK" toggle >/dev/null 2>&1 ;;
    mic)
        wpctl set-mute "$SRC" toggle >/dev/null 2>&1
        read -r p m <<<"$(state "$SRC")"
        [ "$m" = "1" ] && osd 100 "Microphone" "Muted" || osd 100 "Microphone" "Live · ${p}%"
        exit 0 ;;
    status)
        read -r p m <<<"$(state "$SINK")"
        [ "$m" = "1" ] && echo "muted" || echo "${p}%"
        exit 0 ;;
    *)
        echo "usage: volume.sh {up|down|mute|mic|status}" >&2; exit 1 ;;
esac

read -r p m <<<"$(state "$SINK")"
if [ "$m" = "1" ]; then
    osd 100 "Volume" "Muted"
else
    osd "$p" "Volume · ${p}%" "$(bar "$p")"
fi
