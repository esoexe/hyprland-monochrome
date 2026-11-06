#!/usr/bin/env bash
# Rofi menus for the monochrome setup.
#
#   menu.sh launcher   (Super + D)        applications
#   menu.sh windows    (Super + SHIFT+D)  switch to any open window
#   menu.sh run        (menu entry)       run a command
#   menu.sh keybinds   (Super + /)        interactive searchable shortcut sheet

set -u
. "$(dirname "$0")/lib.sh"

# Tolerate --flags as well as plain arguments (handy in keybinds).
case "${1:-}" in --*) set -- "${1#--}" "${@:2}" ;; esac

MODS_JQ='
  def mods:
    [ (if ((.modmask / 64) % 2) >= 1 then "Super" else empty end),
      (if ((.modmask / 4)  % 2) >= 2 or ((.modmask / 4) % 2) >= 1 then "Ctrl" else empty end),
      (if ((.modmask / 8)  % 2) >= 1 then "Alt"   else empty end),
      (if ((.modmask / 1)  % 2) >= 1 then "Shift" else empty end) ] | join(" + ");
'

case "${1:-launcher}" in
    launcher)
        rofi_menu launcher -show drun -i \
            -display-columns 1 -display-column-separator "  " \
            -drun-display-format "{name}" \
            -p "apps:" ;;
    windows)
        rofi_menu launcher -show window -i -p "windows:" ;;
    run)
        rofi_menu launcher -show run -i -p "run:" ;;
    keybinds)
        raw="$(hyprctl binds -j 2>/dev/null | jq -r "$MODS_JQ"'
            .[]
            | select(.description != null and .description != "" and .description != "unknown")
            | (if (.key // "") == "" then "code " + (.keycode | tostring)
               else (.key | ascii_upcase) end) as $k
            | (mods) as $m
            | (if $m == "" then $k else "\($m) + \($k)" end) as $combo
            | (if (.submap // "") != "" then "[\(.submap)] \($combo)" else $combo end) as $label
            | "\($label)\t\(.description)\t\(.dispatcher // "")\t\(.arg // "")"
        ' 2>/dev/null | awk -F'\t' '!seen[$1]++ { printf "%-28s  %s\t%s\t%s\n", $1, $2, $3, $4 }' | sort -k1,1)"
        [ -z "$raw" ] && { notify "Keybinds" "hyprctl binds returned nothing" "dialog-error"; exit 1; }

        labels="$(printf '%s\n' "$raw" | cut -f1)"
        pick="$(printf '%s\n' "$labels" | rofi_menu keybinds -dmenu -i -p "shortcuts:" \
            -mesg "↵ press Enter on any shortcut to trigger it immediately  ·  Esc to close")" || exit 0
        [ -z "$pick" ] && exit 0

        disp="$(awk -F'\t' -v p="$pick" '$1 == p { print $2; exit }' <<<"$raw")"
        arg="$(awk -F'\t' -v p="$pick" '$1 == p { print $3; exit }' <<<"$raw")"
        if [ -n "$disp" ] && [ "$disp" != "submap" ]; then
            if [ -n "$arg" ]; then
                hyprctl dispatch "$disp" "$arg" >/dev/null 2>&1 || true
            else
                hyprctl dispatch "$disp" >/dev/null 2>&1 || true
            fi
        fi ;;
    *)
        echo "usage: menu.sh {launcher|windows|run|keybinds}" >&2; exit 1 ;;
esac
