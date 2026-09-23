#!/usr/bin/env bash
# Toggle the top bar on and off. Bound to Super + SHIFT + W.

set -u
. "$(dirname "$0")/lib.sh"

if pgrep -x waybar >/dev/null 2>&1; then
    pkill -x waybar
    notify "Top bar" "Hidden"
else
    setsid waybar >/dev/null 2>&1 &
    sleep 0.2
    notify "Top bar" "Shown"
fi
