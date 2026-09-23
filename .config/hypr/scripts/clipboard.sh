#!/usr/bin/env bash
# Clipboard history powered by cliphist + rofi.
#
#   clipboard.sh          pick an entry and copy it back to the clipboard
#   clipboard.sh delete    pick an entry to delete
#   clipboard.sh wipe      clear the whole history (asks first)
#
# Keys in the menu:  Enter = copy · Delete key = remove that entry · Super+DEL = wipe

set -u
. "$(dirname "$0")/lib.sh"

# Tolerate --flags as well as plain arguments (handy in keybinds).
case "${1:-}" in --*) set -- "${1#--}" "${@:2}" ;; esac

have cliphist || { notify "cliphist missing" "sudo pacman -S cliphist" "dialog-error"; exit 1; }

THUMBS="$CACHE_DIR/clipboard-thumbs"
mkdir -p "$THUMBS" 2>/dev/null || true

# Turn an image entry into a small thumbnail rofi can show as an icon.
thumb_for() {
    local id="$1"
    local thumb="$THUMBS/$id.png"
    if [ ! -f "$thumb" ] && have magick; then
        cliphist decode "$id" 2>/dev/null \
            | magick - -thumbnail '240x240>' -background '#101010' -gravity center \
                    -extent 240x240 "$thumb" 2>/dev/null || true
    fi
    [ -f "$thumb" ] && printf '%s' "$thumb"
}

build_list() {
    cliphist list | while IFS=$'\t' read -r id preview; do
        if [ -z "${id:-}" ]; then continue; fi
        case "$preview" in
            *"binary data"*)
                icon="$(thumb_for "$id")"
                if [ -n "$icon" ]; then
                    printf '%s\t%s\0icon\x1f%s\n' "$id" "image" "$icon"
                else
                    printf '%s\t%s\n' "$id" "image"
                fi ;;
            *)
                printf '%s\t%s\n' "$id" "$preview" ;;
        esac
    done
}

raw_line() { cliphist list | awk -F'\t' -v id="$1" '$1 == id { print; exit }'; }

confirm() {
    local answer
    answer="$(printf 'yes\nno\n' | rofi_menu powermenu -dmenu -i -p "$1" )" || exit 0
    [ "$answer" = "yes" ]
}

case "${1:-pick}" in
    wipe)
        confirm "clear clipboard history?" && { cliphist wipe; notify "Clipboard" "History cleared"; }
        exit 0 ;;
    delete)
        choice="$(build_list | rofi_menu clipboard -dmenu -i -show-icons \
                    -display-columns 2 -p "delete:" )" || exit 0
        [ -z "$choice" ] && exit 0
        id="${choice%%$'\t'*}"
        raw="$(raw_line "$id")"
        [ -n "$raw" ] && printf '%s\n' "$raw" | cliphist delete
        notify "Clipboard" "Entry removed"
        exit 0 ;;
esac

choice="$(build_list | rofi_menu clipboard -dmenu -i -show-icons \
            -display-columns 2 -p "clipboard:" \
            -kb-custom-1 "Delete" -kb-custom-2 "Super+Delete" )"
status=$?

case "$status" in
    0)
        [ -z "$choice" ] && exit 0
        id="${choice%%$'\t'*}"
        cliphist decode "$id" 2>/dev/null | wl-copy
        notify "Clipboard" "Copied to clipboard"
        ;;
    10)  # Delete key — delete the highlighted entry
        id="${choice%%$'\t'*}"
        raw="$(raw_line "$id")"
        [ -n "$raw" ] && printf '%s\n' "$raw" | cliphist delete
        notify "Clipboard" "Entry removed"
        ;;
    11)  # custom-2 — wipe everything
        confirm "clear clipboard history?" && { cliphist wipe; notify "Clipboard" "History cleared"; }
        ;;
esac
