#!/usr/bin/env bash
# Quake-style dropdown terminal (Super + SHIFT + T).
# The window lives on a special workspace, so it never disturbs your layout.

set -u
. "$(dirname "$0")/lib.sh"

CLASS="$(mono_var dropdown_term 'kitty --class dropdown-term')"
NAME="$(mono_var dropdown_name dropdown)"

if mono_ipc has-client "dropdown-term" >/dev/null 2>&1; then
    hyprctl dispatch "hl.dsp.workspace.toggle_special(\"${NAME}\")" >/dev/null 2>&1
else
    setsid $CLASS >/dev/null 2>&1 &
    # give the window a moment to appear so the special workspace has content
    for _ in 1 2 3 4 5 6 7 8 9 10; do
        sleep 0.1
        mono_ipc has-client "dropdown-term" >/dev/null 2>&1 && break
    done
    hyprctl dispatch "hl.dsp.workspace.toggle_special(\"${NAME}\")" >/dev/null 2>&1
fi
