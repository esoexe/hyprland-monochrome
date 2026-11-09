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

case "${1:-launcher}" in
    launcher)
        [ -x "$SCRIPTS_DIR/open-app.sh" ] && "$SCRIPTS_DIR/open-app.sh" sync-appimages >/dev/null 2>&1 || true
        rofi_menu launcher -show drun -modi "drun,run,filebrowser" -i \
            -display-columns 1 -display-column-separator "  " \
            -drun-display-format "{name}" \
            -display-drun "apps:" -display-run "cmd:" -display-filebrowser "files:" \
            -p "apps:" ;;
    windows)
        rofi_menu launcher -show window -i -p "windows:" ;;
    run)
        rofi_menu launcher -show run -i -p "run:" ;;
    keybinds)
        raw="$(mono_ipc binds-rofi 2>/dev/null || true)"
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
