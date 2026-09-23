#!/usr/bin/env bash
# Session actions. Works with plain Hyprland, uwsm-managed sessions and
# hyprshutdown (AUR) — whichever you have.
#
#   session.sh lock | suspend | hibernate | reboot | shutdown | quit

set -u
. "$(dirname "$0")/lib.sh"

# Tolerate --flags as well as plain arguments (handy in keybinds).
case "${1:-}" in --*) set -- "${1#--}" "${@:2}" ;; esac

lock_now() {
    pgrep -x hyprlock >/dev/null 2>&1 && return 0
    hyprlock >/dev/null 2>&1 &
    sleep 0.4
}

case "${1:-quit}" in
    lock)      lock_now ;;
    suspend)   lock_now; sleep 0.5; systemctl suspend ;;
    hibernate) lock_now; sleep 0.5; systemctl hibernate ;;
    reboot)    systemctl reboot ;;
    shutdown)  systemctl poweroff ;;
    quit)
        if [ -n "${UWSM_SESSION_ID:-}" ] && have uwsm; then
            uwsm stop
        elif have hyprshutdown; then
            hyprshutdown
        else
            hyprctl dispatch 'hl.dsp.exit()'
        fi ;;
    *)
        echo "usage: session.sh {lock|suspend|hibernate|reboot|shutdown|quit}" >&2
        exit 1 ;;
esac
