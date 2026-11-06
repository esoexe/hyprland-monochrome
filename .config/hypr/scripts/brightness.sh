#!/usr/bin/env bash
# Backlight control with a monochrome Dunst progress-bar OSD.
#
#   brightness.sh up|down|set <pct>|status

set -u
. "$(dirname "$0")/lib.sh"

case "${1:-}" in --*) set -- "${1#--}" "${@:2}" ;; esac

ID=1338

bar() {
    awk -v p="$1" -v w="${2:-20}" 'BEGIN {
        filled = int((p / 100) * w + 0.5);
        s = "";
        for (i = 0; i < filled; i++) s = s "█";
        for (i = filled; i < w; i++) s = s "─";
        print s
    }'
}

pct_now() {
    have brightnessctl || { echo 0; return 1; }
    brightnessctl -m 2>/dev/null | awk -F',' 'NR==1 { gsub(/%/, "", $4); print int($4 + 0) }'
}

osd() {
    local p="$1"
    if have notify-send; then
        notify-send -r "$ID" -a "mono" -i display-brightness-symbolic \
            -h string:x-canonical-private-synchronous:mono-brightness \
            -h int:value:"$p" "Brightness · ${p}%" "$(bar "$p")" 2>/dev/null \
            || notify "Brightness · ${p}%" "$(bar "$p")"
    fi
}

if ! have brightnessctl; then
    [ "${1:-status}" = "status" ] && { echo "unavailable"; exit 0; }
    notify "Brightness" "brightnessctl is not installed" "dialog-warning"
    exit 1
fi

case "${1:-status}" in
    up)     brightnessctl -e4 -n2 set 5%+ >/dev/null 2>&1 || true ;;
    down)   brightnessctl -e4 -n2 set 5%- >/dev/null 2>&1 || true ;;
    set)    brightnessctl set "${2:-50}%" >/dev/null 2>&1 || true ;;
    status) printf '%s%%\n' "$(pct_now)"; exit 0 ;;
    *)      echo "usage: brightness.sh {up|down|set <pct>|status}" >&2; exit 1 ;;
esac

p="$(pct_now)"
osd "${p:-50}"
