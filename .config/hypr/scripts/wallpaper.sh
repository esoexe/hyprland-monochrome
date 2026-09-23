#!/usr/bin/env bash
# Wallpaper picker + setter, powered by awww (the maintained swww).
#
#   wallpaper.sh pick            grid of thumbnails (Super + W)
#   wallpaper.sh random          pick one at random
#   wallpaper.sh next | prev     cycle through the folder
#   wallpaper.sh restore         re-apply the last wallpaper (used on login)
#   wallpaper.sh solid [colour]  flat colour background, e.g. 0d0d0d
#   wallpaper.sh set <file>      apply a specific file

set -u
. "$(dirname "$0")/lib.sh"

# Accept both "restore" and "--restore" so the flag can be used from anywhere.
case "${1:-}" in --*) set -- "${1#--}" "${@:2}" ;; esac

WALL_DIR="$(mono_var wallpaper_dir "$HOME/Pictures/Wallpapers")"
STATE="$CACHE_DIR/wallpaper"
DEFAULT_WALL="$(mono_var default_wallpaper "$WALL_DIR/mono-01.jpg")"
THUMBS="$CACHE_DIR/wallpaper-thumbs"
mkdir -p "$WALL_DIR" "$THUMBS" 2>/dev/null || true

have awww || { notify "awww missing" "sudo pacman -S awww" "dialog-error"; exit 1; }

ensure_daemon() {
    pgrep -x awww-daemon >/dev/null 2>&1 || { setsid awww-daemon >/dev/null 2>&1 & sleep 0.6; }
}

apply() {  # apply <path>
    local img="$1"
    [ -f "$img" ] || { notify "Wallpaper" "File not found: $img" "dialog-error"; return 1; }
    ensure_daemon
    if awww img "$img" \
        --transition-type any --transition-duration 0.7 \
        --transition-fps 60 --transition-step 5 >/dev/null 2>&1
    then
        printf '%s' "$img" > "$STATE"
        return 0
    fi
    notify "Wallpaper failed" "awww could not set $img" "dialog-error"
    return 1
}

thumb_for() {  # thumb_for <path>  →  prints thumbnail path
    local img="$1" name thumb
    name="$(printf '%s' "$img" | md5sum | cut -d' ' -f1)"
    thumb="$THUMBS/$name.jpg"
    if [ ! -f "$thumb" ]; then
        if have magick; then
            magick "$img" -thumbnail '640x360^' -gravity center -extent 640x360 \
                   -quality 80 "$thumb" 2>/dev/null || thumb="$img"
        else
            thumb="$img"
        fi
    fi
    printf '%s' "$thumb"
}

list_walls() {
    find "$WALL_DIR" -maxdepth 2 -type f \
        \( -iname '*.jpg' -o -iname '*.jpeg' -o -iname '*.png' -o -iname '*.webp' \
           -o -iname '*.gif' -o -iname '*.bmp' \) 2>/dev/null | sort
}

case "${1:-pick}" in
    set)
        apply "${2:?usage: wallpaper.sh set <file>}"; exit $? ;;

    restore)
        last="$(cat "$STATE" 2>/dev/null || true)"
        if [ -n "$last" ] && [ -f "$last" ]; then apply "$last"
        elif [ -f "$DEFAULT_WALL" ]; then apply "$DEFAULT_WALL"
        else
            first="$(list_walls | head -1)"
            [ -n "$first" ] && apply "$first"
        fi
        exit 0 ;;

    random)
        pick="$(list_walls | shuf -n1)"
        [ -n "$pick" ] && apply "$pick" || notify "Wallpaper" "No images in $WALL_DIR" "dialog-error"
        exit 0 ;;

    next|prev)
        mapfile -t walls < <(list_walls)
        [ "${#walls[@]}" -eq 0 ] && { notify "Wallpaper" "No images in $WALL_DIR" "dialog-error"; exit 1; }
        cur="$(cat "$STATE" 2>/dev/null || true)"
        idx=0
        for i in "${!walls[@]}"; do [ "${walls[$i]}" = "$cur" ] && idx="$i"; done
        if [ "${1}" = "next" ]; then idx=$(( (idx + 1) % ${#walls[@]} ))
        else idx=$(( (idx - 1 + ${#walls[@]}) % ${#walls[@]} )); fi
        apply "${walls[$idx]}"; exit 0 ;;

    solid)
        colour="${2:-0d0d0d}"
        tmp="$CACHE_DIR/solid-${colour}.png"
        if [ ! -f "$tmp" ]; then
            have magick || { notify "Wallpaper" "imagemagick is needed for solid colours" "dialog-error"; exit 1; }
            magick -size 16x16 "xc:#${colour}" "$tmp"
        fi
        apply "$tmp"; exit 0 ;;
esac

# ── Interactive picker ────────────────────────────────────────────────────
mapfile -t walls < <(list_walls)
if [ "${#walls[@]}" -eq 0 ]; then
    notify "Wallpaper" "No images in $WALL_DIR — add some, then try again" "dialog-info"
    exit 1
fi

choice="$(
    for w in "${walls[@]}"; do
        printf '%s\0icon\x1f%s\n' "$(basename "$w")" "$(thumb_for "$w")"
    done | rofi_menu wallpaper -dmenu -i -show-icons -p "wallpaper:"
)" || exit 0
[ -z "$choice" ] && exit 0

apply "$WALL_DIR/$choice"
notify-send -a mono -i "$WALL_DIR/$choice" "Wallpaper" "$choice" 2>/dev/null || true
