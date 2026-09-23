#!/usr/bin/env bash
# Monochrome logout / power menu (Super + ESCAPE or Super + `).
# Keyboard: type to filter, Enter to run.  Every action except Lock asks first.

set -u
. "$(dirname "$0")/lib.sh"

menu_entries() {
    cat <<'EOF'
󰌾	lock	Lock screen
󰍃	logout	Log out
󰒲	suspend	Suspend
󰜉	reboot	Reboot
󰐥	shutdown	Shut down
EOF
}

confirm() {  # confirm <label>
    local answer
    answer="$(printf 'no\nyes\n' | rofi_menu powermenu -dmenu -i -p "$1 — are you sure?")" || return 1
    [ "$answer" = "yes" ]
}

choice="$(menu_entries | rofi_menu powermenu -dmenu -i -display-columns 3 -p "power:" )" || exit 0
[ -z "$choice" ] && exit 0

action="$(printf '%s' "$choice" | awk -F'\t' '{ print $2 }')"

case "$action" in
    lock)     exec "$SCRIPTS_DIR/session.sh" lock ;;
    logout)   confirm "Log out"   && exec "$SCRIPTS_DIR/session.sh" quit ;;
    suspend)  confirm "Suspend"   && exec "$SCRIPTS_DIR/session.sh" suspend ;;
    reboot)   confirm "Reboot"    && exec "$SCRIPTS_DIR/session.sh" reboot ;;
    shutdown) confirm "Shut down" && exec "$SCRIPTS_DIR/session.sh" shutdown ;;
esac
