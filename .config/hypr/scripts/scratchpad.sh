#!/usr/bin/env bash
# Named Multi-Scratchpad Drawer System.
#
# Each drawer lives in its own Hyprland `special:<name>` workspace with custom
# floating geometry in modules/rules.lua. First summon launches the app; every
# subsequent summon toggles the drawer in 0ms.
#
#   scratchpad.sh term      (Super + Shift + T)   dropdown terminal
#   scratchpad.sh btop      (Super + Shift + B)   floating btop++ telemetry
#   scratchpad.sh notes     (Super + ')           floating ~/Documents/scratch.md
#   scratchpad.sh files     (SCRATCH -> F)        floating file manager
#   scratchpad.sh menu                            Rofi drawer launcher
#   scratchpad.sh status                          list active scratchpad drawers

set -u
. "$(dirname "$0")/lib.sh"

case "${1:-}" in --*) set -- "${1#--}" "${@:2}" ;; esac

has_class() {
    local cls="$1"
    have hyprctl || [ -n "${HYPRLAND_INSTANCE_SIGNATURE:-}" ] || return 1
    mono_ipc has-client "$cls" >/dev/null 2>&1
}

toggle_drawer() { # toggle_drawer <ws_name> <class> <launch_cmd...>
    local ws="$1" cls="$2"
    shift 2
    if ! has_class "$cls"; then
        nohup "$@" </dev/null >/dev/null 2>&1 &
        sleep 0.20
    fi
    hyprctl dispatch togglespecialworkspace "$ws" >/dev/null 2>&1 || true
}

ensure_notes_file() {
    local notes_dir="$HOME/Documents"
    local notes_file="$notes_dir/scratch.md"
    mkdir -p "$notes_dir" 2>/dev/null || true
    if [ ! -f "$notes_file" ]; then
        cat > "$notes_file" <<EOF
# Scratchpad Notes

Quick monochrome scratch buffer (\`Super + '\` to show/hide from any workspace).

- [ ] 
EOF
    fi
    printf '%s' "$notes_file"
}

term_bin() {
    local t
    t="$(mono_var terminal kitty)"
    printf '%s' "${t%% *}"
}

cmd_drawer() {
    local tbin ed
    tbin="$(term_bin)"
    case "${1:-term}" in
        term)
            local ws cls
            ws="$(mono_var dropdown_name dropdown)"
            cls="dropdown-term"
            toggle_drawer "$ws" "$cls" "$tbin" --class "$cls" ;;
        btop)
            if ! have btop; then
                notify "Scratchpad" "btop is not installed" "dialog-warning"
                exit 1
            fi
            toggle_drawer "btop" "mono-btop" "$tbin" --class "mono-btop" -e btop ;;
        notes)
            local nf
            nf="$(ensure_notes_file)"
            ed="nvim"
            have "$ed" || ed="${EDITOR:-nano}"
            toggle_drawer "notes" "mono-notes" "$tbin" --class "mono-notes" -e "$ed" "$nf" ;;
        files)
            local fm
            fm="$(mono_var file_manager thunar)"
            if [ "${fm%% *}" = "thunar" ]; then
                toggle_drawer "files" "mono-files" thunar --class=mono-files "$HOME"
            else
                toggle_drawer "files" "$fm" "$fm"
            fi ;;
    esac
}

cmd_status() {
    local out=()
    for pair in "dropdown-term:term" "mono-btop:btop" "mono-notes:notes" "mono-files:files"; do
        local cls="${pair%%:*}" lbl="${pair#*:}"
        if has_class "$cls"; then out+=("$lbl"); fi
    done
    if [ "${#out[@]}" -eq 0 ]; then
        printf 'idle\n'
    else
        printf 'warm: %s\n' "${out[*]}"
    fi
}

cmd_menu() {
    local st pick
    st="$(cmd_status)"
    pick="$(cat <<'EOF' | rofi_menu settings -dmenu -i -p "drawers:" -mesg "Named floating drawers ($st) · Esc to close"
◒  Dropdown Terminal (Super + Shift + T)
▣  System Monitor · btop++ (Super + Shift + B)
✎  Scratch Markdown Notes (Super + ')
◰  Quick File Drawer (Thunar)
EOF
    )" || exit 0

    case "$pick" in
        *"Dropdown Terminal"*) cmd_drawer term ;;
        *"System Monitor"*)    cmd_drawer btop ;;
        *"Scratch Markdown"*)  cmd_drawer notes ;;
        *"Quick File"*)        cmd_drawer files ;;
    esac
}

case "${1:-menu}" in
    term|btop|notes|files) cmd_drawer "$1" ;;
    menu)   cmd_menu ;;
    status) cmd_status ;;
    *)
        echo "usage: scratchpad.sh {term|btop|notes|files|menu|status}" >&2
        exit 1 ;;
esac
