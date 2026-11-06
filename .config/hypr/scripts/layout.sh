#!/usr/bin/env bash
# Dynamic Tiling Layout & Gap-Preset Switcher.
#
#   layout.sh                 Rofi layout & gap-preset picker
#   layout.sh cycle           cycle dwindle -> master -> scrolling
#   layout.sh set <name>      set layout: dwindle | master | scrolling
#   layout.sh gaps <preset>   apply preset: compact | default | airy | zero | cycle
#   layout.sh status          print "<layout> (<gaps>)"

set -u
. "$(dirname "$0")/lib.sh"

case "${1:-}" in --*) set -- "${1#--}" "${@:2}" ;; esac

LAYOUT_STATE="$CACHE_DIR/layout.state"
GAPS_STATE="$CACHE_DIR/gaps.state"

current_layout() {
    local cur=""
    if [ -f "$LAYOUT_STATE" ]; then
        cur="$(cat "$LAYOUT_STATE" 2>/dev/null)"
    fi
    if [ -z "$cur" ] && have hyprctl; then
        cur="$(hyprctl getoption general:layout -j 2>/dev/null | jq -r '.str // empty' 2>/dev/null)"
    fi
    [ -z "$cur" ] && cur="$(mono_var layout dwindle)"
    printf '%s' "$cur"
}

current_gaps() {
    local cur="default"
    [ -f "$GAPS_STATE" ] && cur="$(cat "$GAPS_STATE" 2>/dev/null)"
    [ -z "$cur" ] && cur="default"
    printf '%s' "$cur"
}

set_layout() {
    local target="$1"
    case "$target" in
        dwindle|master|scrolling) ;;
        *)
            echo "unknown layout '$target' (expected dwindle|master|scrolling)" >&2
            exit 1 ;;
    esac
    if have hyprctl; then
        hyprctl keyword general:layout "$target" >/dev/null 2>&1 || true
    fi
    printf '%s\n' "$target" > "$LAYOUT_STATE"
    notify "Tiling layout" "Switched to ${target^}"
}

cycle_layout() {
    local cur next
    cur="$(current_layout)"
    case "$cur" in
        dwindle)   next="master" ;;
        master)    next="scrolling" ;;
        *)         next="dwindle" ;;
    esac
    set_layout "$next"
}

apply_gaps() {
    local preset="$1"
    if [ "$preset" = "cycle" ]; then
        case "$(current_gaps)" in
            default) preset="compact" ;;
            compact) preset="airy" ;;
            airy)    preset="zero" ;;
            *)       preset="default" ;;
        esac
    fi

    local gi go bs rd desc
    case "$preset" in
        compact)
            gi=2; go=4; bs=1; rd=6
            desc="Compact · 2/4px gaps (laptop density)" ;;
        default)
            gi="$(mono_var gaps_in 5)"
            go="$(mono_var gaps_out 10)"
            bs="$(mono_var border_size 1)"
            rd="$(mono_var rounding 10)"
            desc="Default · ${gi}/${go}px gaps (user.lua)" ;;
        airy)
            gi=10; go=22; bs=1; rd=14
            desc="Airy · 10/22px gaps (gallery framing)" ;;
        zero)
            gi=0; go=0; bs=1; rd=0
            desc="Zero · 0px edge-to-edge monocle" ;;
        *)
            echo "unknown gap preset '$preset' (expected compact|default|airy|zero|cycle)" >&2
            exit 1 ;;
    esac

    if have hyprctl; then
        hyprctl --batch "keyword general:gaps_in $gi; keyword general:gaps_out $go; keyword general:border_size $bs; keyword decoration:rounding $rd" >/dev/null 2>&1 || true
    fi
    printf '%s\n' "$preset" > "$GAPS_STATE"
    notify "Gap preset" "$desc"
}

cmd_menu() {
    local l g pick
    l="$(current_layout)"
    g="$(current_gaps)"

    pick="$(cat <<EOF | rofi_menu settings -dmenu -i -p "layout:" -mesg "Current: layout=${l} · gaps=${g}"
⊞  Layout: Dwindle (spiral BSP)
⊞  Layout: Master (main + stack)
⊞  Layout: Scrolling (infinite strip)
⊟  Gaps: Default (user.lua)
⊟  Gaps: Compact (2/4px laptop)
⊟  Gaps: Airy (10/22px gallery)
⊟  Gaps: Zero (0px edge-to-edge)
EOF
    )" || exit 0

    case "$pick" in
        *"Layout: Dwindle"*)   set_layout dwindle ;;
        *"Layout: Master"*)    set_layout master ;;
        *"Layout: Scrolling"*) set_layout scrolling ;;
        *"Gaps: Default"*)     apply_gaps default ;;
        *"Gaps: Compact"*)     apply_gaps compact ;;
        *"Gaps: Airy"*)        apply_gaps airy ;;
        *"Gaps: Zero"*)        apply_gaps zero ;;
    esac
}

case "${1:-menu}" in
    menu)   cmd_menu ;;
    cycle)  cycle_layout ;;
    set)    set_layout "${2:-dwindle}" ;;
    gaps)   apply_gaps "${2:-cycle}" ;;
    status) printf '%s (%s)\n' "$(current_layout)" "$(current_gaps)" ;;
    *)
        echo "usage: layout.sh {menu|cycle|set <dwindle|master|scrolling>|gaps <compact|default|airy|zero|cycle>|status}" >&2
        exit 1 ;;
esac
