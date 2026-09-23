#!/usr/bin/env bash
# Rofi menus for the monochrome setup.
#
#   menu.sh launcher   (Super + D)        applications
#   menu.sh windows    (Super + SHIFT+D)  switch to any open window
#   menu.sh run        (menu entry)       run a command
#   menu.sh keybinds   (Super + /)        cheat sheet built from your live binds

set -u
. "$(dirname "$0")/lib.sh"

# Tolerate --flags as well as plain arguments (handy in keybinds).
case "${1:-}" in --*) set -- "${1#--}" "${@:2}" ;; esac

MODS_JQ='
  def mods:
    [ (if ((.modmask / 64) % 2) >= 1 then "Super" else empty end),
      (if ((.modmask / 4)  % 2) >= 1 then "Ctrl"  else empty end),
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
        list="$(hyprctl binds -j 2>/dev/null | jq -r "$MODS_JQ"'
            .[]
            | select(.description != null and .description != "" and .description != "unknown")
            | (if (.key // "") == "" then "code " + (.keycode | tostring)
               else (.key | ascii_upcase) end) as $k
            | (mods) as $m
            | (if $m == "" then $k else "\($m) + \($k)" end) as $combo
            | "\($combo)\t\(.description)"
        ' 2>/dev/null | awk -F'\t' '!seen[$1]++ { printf "%-26s  %s\n", $1, $2 }' | sort -k1,1)"
        [ -z "$list" ] && { notify "Keybinds" "hyprctl binds returned nothing" "dialog-error"; exit 1; }
        printf '%s\n' "$list" | rofi_menu keybinds -dmenu -i -p "shortcuts:" ;;
    *)
        echo "usage: menu.sh {launcher|windows|run|keybinds}" >&2; exit 1 ;;
esac
