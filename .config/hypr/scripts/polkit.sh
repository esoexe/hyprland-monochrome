#!/usr/bin/env bash
# Start a polkit authentication agent, unless one is already running.
# Without it you get no GUI password prompt when an app needs root.

set -u
. "$(dirname "$0")/lib.sh"

running() {
    pgrep -x hyprpolkitagent          >/dev/null 2>&1 && return 0
    pgrep -f polkit-gnome-authentication-agent-1 >/dev/null 2>&1 && return 0
    pgrep -f polkit-kde-authentication-agent-1   >/dev/null 2>&1 && return 0
    pgrep -x lxqt-policykit-agent     >/dev/null 2>&1 && return 0
    pgrep -f polkit-mate-authentication-agent-1  >/dev/null 2>&1 && return 0
    return 1
}

running && exit 0

# 1) systemd user unit (Arch's hyprpolkitagent package ships one)
if have systemctl && systemctl --user start hyprpolkitagent.service 2>/dev/null; then
    exit 0
fi

# 2) direct binary paths across Arch, Debian/Ubuntu, Fedora, openSUSE & Nix
for agent in \
    hyprpolkitagent \
    /usr/lib/hyprpolkitagent/hyprpolkitagent \
    /usr/libexec/hyprpolkitagent \
    /usr/lib/polkit-gnome/polkit-gnome-authentication-agent-1 \
    /usr/lib/policykit-1-gnome/polkit-gnome-authentication-agent-1 \
    /usr/libexec/polkit-gnome-authentication-agent-1 \
    /usr/lib/polkit-kde-authentication-agent-1 \
    /usr/libexec/kf6/polkit-kde-authentication-agent-1 \
    lxqt-policykit-agent \
    /usr/lib/mate-polkit/polkit-mate-authentication-agent-1
do
    if have "$agent" || [ -x "$agent" ]; then
        setsid "$agent" >/dev/null 2>&1 &
        exit 0
    fi
done

notify "No polkit agent found" "Install hyprpolkitagent (recommended) or polkit-gnome" "dialog-warning"
