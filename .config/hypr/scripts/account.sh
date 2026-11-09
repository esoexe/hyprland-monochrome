#!/usr/bin/env bash
# Account settings for the currently signed-in Linux user.
# Does not create users, use sudo, or read/store password material.
set -u
. "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"

TAB=$'\t'

account_info() {
    local row gecos login uid gid home shell groups
    row="$(getent passwd "$USER" 2>/dev/null || true)"
    if [ -z "$row" ]; then
        printf 'User record unavailable for %s\n' "$USER"
        return 1
    fi
    IFS=: read -r login _ uid gid gecos home shell <<< "$row"
    gecos="${gecos%%,*}"
    groups="$(id -nG 2>/dev/null || true)"
    printf 'Account      %s\n' "$login"
    printf 'Display name %s\n' "${gecos:-Not set}"
    printf 'User ID      %s\n' "$uid"
    printf 'Home         %s\n' "$home"
    printf 'Login shell  %s\n' "$shell"
    printf 'Groups       %s\n' "${groups:-Unavailable}"
}

terminal_run() {
    if have kitty; then
        setsid kitty --class mono-account -e "$@" >/dev/null 2>&1 &
    elif have foot; then
        setsid foot "$@" >/dev/null 2>&1 &
    elif have alacritty; then
        setsid alacritty -e "$@" >/dev/null 2>&1 &
    elif have xterm; then
        setsid xterm -e "$@" >/dev/null 2>&1 &
    else
        notify "Account settings" "Install Kitty, foot, Alacritty, or xterm to run interactive account tools" "dialog-warning"
        return 1
    fi
}

show_profile() {
    local info
    info="$(account_info)" || return 1
    printf '%s\n' "$info" | rofi_menu settings -dmenu -i -p "account profile:" -mesg "Current signed-in user. No password data is read or displayed."
}

change_shell() {
    local shell_file="${SHELLS_FILE:-/etc/shells}" selected
    if ! have chsh; then
        notify "Account settings" "chsh is not installed" "dialog-warning"
        return 1
    fi
    if [ ! -r "$shell_file" ]; then
        notify "Account settings" "Cannot read $shell_file" "dialog-error"
        return 1
    fi
    selected="$(grep -Ev '^[[:space:]]*(#|$)' "$shell_file" | sort -u | \
        rofi_menu settings -dmenu -i -p "select login shell:" -mesg "The selected shell must be listed in $shell_file.")" || return 0
    [ -n "$selected" ] || return 0
    grep -Fxq -- "$selected" "$shell_file" || {
        notify "Account settings" "That shell is not listed in $shell_file" "dialog-error"
        return 1
    }
    terminal_run chsh -s "$selected" "$USER"
}

show_menu() {
    local selected action
    while :; do
        selected="$(printf '%s\n' \
            "profile${TAB}View current account details" \
            "name${TAB}Change display name" \
            "shell${TAB}Change login shell" \
            "password${TAB}Change password" | \
            rofi_menu settings -dmenu -i -p "account settings:")" || return 0
        action="${selected%%$TAB*}"
        case "$action" in
            profile) show_profile ;;
            name)
                if have chfn; then terminal_run chfn "$USER"
                else notify "Account settings" "chfn is not installed" "dialog-warning"; fi ;;
            shell) change_shell ;;
            password)
                if have passwd; then terminal_run passwd
                else notify "Account settings" "passwd is not installed" "dialog-warning"; fi ;;
            *) return 0 ;;
        esac
    done
}

case "${1:-menu}" in
    info|--info) account_info ;;
    shell|--shell) change_shell ;;
    menu|--menu) have rofi || { echo "rofi is required for the account menu" >&2; exit 1; }; show_menu ;;
    --help|-h)
        printf 'Usage: %s [menu|info|shell]\n' "$0"
        printf 'Manages the current Linux account only; interactive changes use chfn, chsh, or passwd.\n'
        ;;
    *) echo "Unknown account action: $1" >&2; exit 2 ;;
esac
