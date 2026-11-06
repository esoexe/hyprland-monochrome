#!/usr/bin/env bash
# Screenshots, region OCR & annotation.
#
#   screenshot.sh region    (Super + PRINT)      — drag to select
#   screenshot.sh full      (Super + SHIFT PRINT) — whole screen
#   screenshot.sh window    (Super + CTRL PRINT)  — focused window
#   screenshot.sh ocr       (Super + ALT PRINT)   — OCR screen region -> clipboard
#   screenshot.sh edit      (CAPTURE -> e)        — region -> swappy / satty

set -u
. "$(dirname "$0")/lib.sh"

# Tolerate --flags as well as plain arguments (handy in keybinds).
case "${1:-}" in --*) set -- "${1#--}" "${@:2}" ;; esac

DIR="$(mono_var screenshot_dir "$HOME/Pictures/Screenshots")"
mkdir -p "$DIR" 2>/dev/null || true
FILE="$DIR/$(date +%Y-%m-%d_%H-%M-%S).png"

select_region() {
    if [ -n "${MONO_SLURP_GEOM:-}" ]; then
        printf '%s' "$MONO_SLURP_GEOM"
        return 0
    fi
    have slurp || { notify "Screenshot failed" "slurp is not installed" "dialog-error"; return 1; }
    local g
    g="$(slurp 2>/dev/null)" || return 1
    [ -n "$g" ] || return 1
    printf '%s' "$g"
}

notify_shot() {
    [ "$(mono_var screenshot_notify true)" = "false" ] && return 0
    notify-send -a "mono" -i "$FILE" "Screenshot saved" "$(basename "$FILE")" 2>/dev/null \
        || notify "Screenshot saved" "$FILE"
}

run_ocr_on_file() {
    local img="$1"
    if ! have tesseract; then
        notify "OCR unavailable" "Install tesseract + tesseract-data-eng to extract text" "dialog-error"
        exit 1
    fi
    local txt preview
    txt="$(tesseract "$img" stdout --psm 6 -l "${MONO_OCR_LANG:-eng}" 2>/dev/null | sed -e 's/[[:space:]]*$//' -e '/./,$!d')"
    if [ -z "$txt" ]; then
        notify "OCR" "No readable text detected in selection"
        exit 0
    fi
    if have wl-copy; then
        printf '%s' "$txt" | wl-copy 2>/dev/null || true
    fi
    preview="$(printf '%s' "$txt" | tr '\n' ' ' | cut -c1-110)"
    notify "OCR copied to clipboard" "$preview"
    printf '%s\n' "$txt"
}

mode="${1:-region}"

if [ "$mode" = "ocr-file" ]; then
    [ -f "${2:-}" ] || { echo "usage: screenshot.sh ocr-file <image.png>" >&2; exit 1; }
    run_ocr_on_file "$2"
    exit 0
fi

have grim || { notify "Screenshot failed" "grim is not installed" "dialog-error"; exit 1; }

case "$mode" in
    region)
        geom="$(select_region)" || exit 0
        sleep 0.12
        grim -g "$geom" "$FILE" || exit 1 ;;
    full)
        grim "$FILE" || exit 1 ;;
    window)
        win="$(hyprctl activewindow -j 2>/dev/null)" || exit 1
        read -r x y w h <<<"$(printf '%s' "$win" | jq -r '"\(.at[0]) \(.at[1]) \(.size[0]) \(.size[1])"' 2>/dev/null)"
        [ -z "${w:-}" ] && { notify "Screenshot failed" "No focused window" "dialog-error"; exit 1; }
        grim -g "${w}x${h}+${x}+${y}" "$FILE" || exit 1 ;;
    ocr)
        geom="$(select_region)" || exit 0
        sleep 0.10
        ocr_img="$CACHE_DIR/ocr-capture.png"
        grim -g "$geom" "$ocr_img" || exit 1
        run_ocr_on_file "$ocr_img"
        rm -f "$ocr_img"
        exit 0 ;;
    edit)
        geom="$(select_region)" || exit 0
        sleep 0.12
        grim -g "$geom" "$FILE" || exit 1
        if have satty; then
            satty --filename "$FILE" --output-filename "$FILE"
            exit 0
        elif have swappy; then
            swappy -f "$FILE" -o "$FILE"
            exit 0
        fi
        notify "Screenshot saved" "$(basename "$FILE") (install satty or swappy to annotate)"
        exit 0 ;;
    *)
        echo "usage: screenshot.sh {region|full|window|ocr|ocr-file <img>|edit}" >&2; exit 1 ;;
esac

if have wl-copy; then
    wl-copy --type image/png < "$FILE" 2>/dev/null || true
fi

notify_shot
