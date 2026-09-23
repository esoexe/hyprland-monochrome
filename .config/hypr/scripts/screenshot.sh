#!/usr/bin/env bash
# Screenshots: saves a PNG, copies it to the clipboard and shows a preview.
#
#   screenshot.sh region    (Super + PRINT)      — drag to select
#   screenshot.sh full      (Super + SHIFT PRINT) — whole screen
#   screenshot.sh window    (Super + CTRL PRINT)  — focused window

set -u
. "$(dirname "$0")/lib.sh"

# Tolerate --flags as well as plain arguments (handy in keybinds).
case "${1:-}" in --*) set -- "${1#--}" "${@:2}" ;; esac

DIR="$(mono_var screenshot_dir "$HOME/Pictures/Screenshots")"
mkdir -p "$DIR" 2>/dev/null || true
FILE="$DIR/$(date +%Y-%m-%d_%H-%M-%S).png"

have grim || { notify "Screenshot failed" "grim is not installed" "dialog-error"; exit 1; }

notify_shot() {
    [ "$(mono_var screenshot_notify true)" = "false" ] && return 0
    notify-send -a "mono" -i "$FILE" "Screenshot saved" "$(basename "$FILE")" 2>/dev/null \
        || notify "Screenshot saved" "$FILE"
}

case "${1:-region}" in
    region)
        have slurp || { notify "Screenshot failed" "slurp is not installed" "dialog-error"; exit 1; }
        geom="$(slurp 2>/dev/null)" || exit 0            # user pressed Escape
        [ -z "$geom" ] && exit 0
        sleep 0.15                                       # let the selection UI vanish
        grim -g "$geom" "$FILE" || exit 1 ;;
    full)
        grim "$FILE" || exit 1 ;;
    window)
        win="$(hyprctl activewindow -j 2>/dev/null)" || exit 1
        read -r x y w h <<<"$(printf '%s' "$win" | jq -r '"\(.at[0]) \(.at[1]) \(.size[0]) \(.size[1])"' 2>/dev/null)"
        [ -z "${w:-}" ] && { notify "Screenshot failed" "No focused window" "dialog-error"; exit 1; }
        grim -g "${w}x${h}+${x}+${y}" "$FILE" || exit 1 ;;
    *)
        echo "usage: screenshot.sh {region|full|window}" >&2; exit 1 ;;
esac

if have wl-copy; then
    wl-copy --type image/png < "$FILE" 2>/dev/null || true
fi

notify_shot
