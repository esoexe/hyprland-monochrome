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
DEFAULT_WALL="$(mono_var default_wallpaper "$WALL_DIR/mono-11.gif")"
THUMBS="$CACHE_DIR/wallpaper-thumbs"
mkdir -p "$WALL_DIR" "$THUMBS" 2>/dev/null || true

have awww || { notify "awww missing" "sudo pacman -S awww" "dialog-error"; exit 1; }

ensure_daemon() {
    pgrep -x awww-daemon >/dev/null 2>&1 || { setsid awww-daemon >/dev/null 2>&1 & sleep 0.6; }
}

apply() {  # apply <path> [transition_override]
    local img="$1"
    local tmode="${2:-}"
    [ -f "$img" ] || { notify "Wallpaper" "File not found: $img" "dialog-error"; return 1; }
    ensure_daemon

    if [ -z "$tmode" ]; then
        tmode="$(cat "$CACHE_DIR/wallpaper.transition" 2>/dev/null || true)"
        [ -z "$tmode" ] && tmode="$(mono_var wallpaper_transition choreo)"
    fi

    local -a tflags
    case "$tmode" in
        choreo)
            local step_idx
            step_idx="$(cat "$CACHE_DIR/wallpaper.tstep" 2>/dev/null || echo 0)"
            step_idx=$(( (step_idx + 1) % 4 ))
            printf '%s\n' "$step_idx" > "$CACHE_DIR/wallpaper.tstep"
            case "$step_idx" in
                0) tflags=(--transition-type grow --transition-pos "0.5,0.5") ;;
                1) tflags=(--transition-type wave --transition-angle 28 --transition-wave "32,16") ;;
                2) tflags=(--transition-type outer --transition-pos "0.5,0.5") ;;
                *) tflags=(--transition-type wipe --transition-angle 35) ;;
            esac ;;
        wave)  tflags=(--transition-type wave --transition-angle 28 --transition-wave "32,16") ;;
        wipe)  tflags=(--transition-type wipe --transition-angle 35) ;;
        grow|center) tflags=(--transition-type grow --transition-pos "0.5,0.5") ;;
        outer) tflags=(--transition-type outer --transition-pos "0.5,0.5") ;;
        fade|simple|any|none) tflags=(--transition-type "$tmode") ;;
        *)     tflags=(--transition-type any) ;;
    esac

    if awww img "$img" \
        "${tflags[@]}" \
        --transition-bezier 0.16,1.0,0.3,1.0 \
        --transition-duration 1.1 \
        --transition-fps 120 \
        --transition-step 90 >/dev/null 2>&1
    then
        printf '%s' "$img" > "$STATE"
        printf '%s' "$img" > "$CACHE_DIR/current_wallpaper"
        return 0
    fi
    notify "Wallpaper failed" "awww could not set $img" "dialog-error"
    return 1
}

thumb_for() {  # thumb_for <path>  →  prints thumbnail path
    local img="$1" name thumb src_spec
    name="$(printf '%s' "$img" | md5sum | cut -d' ' -f1)"
    thumb="$THUMBS/$name.jpg"
    if [ ! -f "$thumb" ]; then
        if have magick; then
            src_spec="$img"
            case "${img,,}" in *.gif) src_spec="${img}[0]" ;; esac
            magick "$src_spec" -thumbnail '640x360^' -gravity center -extent 640x360 \
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

    live)
        mapfile -t gifs < <(find "$WALL_DIR" -maxdepth 2 -type f -iname '*.gif' 2>/dev/null | sort)
        if [ "${#gifs[@]}" -eq 0 ]; then
            notify "Live Wallpaper" "No animated .gif wallpapers in $WALL_DIR" "dialog-warning"
            exit 1
        fi
        cur="$(cat "$STATE" 2>/dev/null || true)"
        lidx=0
        for i in "${!gifs[@]}"; do [ "${gifs[$i]}" = "$cur" ] && lidx=$(( (i + 1) % ${#gifs[@]} )); done
        apply "${gifs[$lidx]}" grow
        notify "Live Kinetic Wallpaper" "$(basename "${gifs[$lidx]}")"
        exit 0 ;;

    transition)
        tset="${2:-choreo}"
        printf '%s\n' "$tset" > "$CACHE_DIR/wallpaper.transition"
        notify "Wallpaper Transition" "120fps Bezier style: $tset"
        exit 0 ;;
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
