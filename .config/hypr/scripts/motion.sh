#!/usr/bin/env bash
# Kinetic Motion Profile & Liquid-Border Switcher (Super + Shift + M).
#
# Hot-swaps compositor animation physics in 0ms via atomic `hyprctl --batch`:
#   fluid      Apple-style damped springs + 18% slidefade parallax (default)
#   snappy     140ms ultra-crisp studio precision
#   cinematic  deep 26% showcase parallax + orbiting liquid-silver border
#   minimal    zero animations & blur (laptop battery / competitive gaming)
#
#   motion.sh [menu|cycle|set <profile>|border-loop [on|off|toggle]|status]

set -u
. "$(dirname "$0")/lib.sh"

case "${1:-}" in --*) set -- "${1#--}" "${@:2}" ;; esac

MOTION_STATE="$CACHE_DIR/motion.state"
LOOP_STATE="$CACHE_DIR/border-loop.state"

current_profile() {
    local p
    p="$(cat "$MOTION_STATE" 2>/dev/null || true)"
    [ -z "$p" ] && p="$(mono_var motion_profile fluid)"
    case "$p" in fluid|snappy|cinematic|minimal) printf '%s' "$p" ;; *) printf 'fluid' ;; esac
}

current_loop() {
    local l
    l="$(cat "$LOOP_STATE" 2>/dev/null || true)"
    if [ -z "$l" ]; then
        l="$(mono_var border_loop false)"
    fi
    case "$l" in true|1|on) printf 'on' ;; *) printf 'off' ;; esac
}

apply_profile() {
    local p="$1"
    local blur_def round_def
    blur_def="$(mono_var blur_enabled true)"
    round_def="$(mono_var rounding 10)"

    case "$p" in
        fluid)
            printf 'fluid\n' > "$MOTION_STATE"
            hyprctl --batch "\
keyword animations:enabled true ; \
keyword decoration:blur:enabled $blur_def ; \
keyword decoration:shadow:enabled true ; \
keyword decoration:rounding $round_def ; \
keyword bezier expoOut,0.16,1.0,0.3,1.0 ; \
keyword bezier velvetPop,0.12,1.04,0.28,1.0 ; \
keyword bezier monoSnap,0.15,0.0,0.1,1.0 ; \
keyword animation windowsIn,1,4.6,velvetPop,popin 86% ; \
keyword animation windowsOut,1,3.4,monoSnap,popin 91% ; \
keyword animation windowsMove,1,4.8,expoOut ; \
keyword animation workspaces,1,5.2,expoOut,slidefade 18% ; \
keyword animation specialWorkspace,1,4.6,velvetPop,slidefadevert ; \
keyword animation layersIn,1,3.8,velvetPop,popin 92%" >/dev/null 2>&1 || true
            notify "Motion · Fluid" "Damped silk curves + 18% workspace parallax" ;;
        snappy)
            printf 'snappy\n' > "$MOTION_STATE"
            hyprctl --batch "\
keyword animations:enabled true ; \
keyword decoration:blur:enabled $blur_def ; \
keyword decoration:shadow:enabled true ; \
keyword decoration:rounding $round_def ; \
keyword bezier expoOut,0.16,1.0,0.3,1.0 ; \
keyword bezier monoSnap,0.15,0.0,0.1,1.0 ; \
keyword animation windowsIn,1,2.4,expoOut,popin 94% ; \
keyword animation windowsOut,1,1.9,monoSnap,popin 95% ; \
keyword animation windowsMove,1,2.5,expoOut ; \
keyword animation workspaces,1,2.8,expoOut,slidefade 10% ; \
keyword animation specialWorkspace,1,2.5,expoOut,slidefadevert ; \
keyword animation layersIn,1,2.0,expoOut,popin 95%" >/dev/null 2>&1 || true
            notify "Motion · Snappy" "140ms studio precision · crisp zero-overshoot" ;;
        cinematic)
            printf 'cinematic\n' > "$MOTION_STATE"
            printf 'on\n' > "$LOOP_STATE"
            hyprctl --batch "\
keyword animations:enabled true ; \
keyword decoration:blur:enabled true ; \
keyword decoration:shadow:enabled true ; \
keyword decoration:rounding $round_def ; \
keyword bezier expoOut,0.16,1.0,0.3,1.0 ; \
keyword bezier velvetPop,0.10,1.06,0.26,1.0 ; \
keyword bezier monoLinear,0.0,0.0,1.0,1.0 ; \
keyword animation windowsIn,1,6.5,velvetPop,popin 78% ; \
keyword animation windowsOut,1,4.8,expoOut,popin 86% ; \
keyword animation windowsMove,1,6.4,expoOut ; \
keyword animation workspaces,1,7.2,expoOut,slidefade 26% ; \
keyword animation specialWorkspace,1,6.2,velvetPop,slidefadevert ; \
keyword animation borderangle,1,55,monoLinear,loop" >/dev/null 2>&1 || true
            notify "Motion · Cinematic" "Deep 26% parallax + orbiting liquid-silver border" ;;
        minimal)
            printf 'minimal\n' > "$MOTION_STATE"
            printf 'off\n' > "$LOOP_STATE"
            hyprctl --batch "\
keyword animations:enabled false ; \
keyword decoration:blur:enabled false ; \
keyword decoration:shadow:enabled false" >/dev/null 2>&1 || true
            notify "Motion · Minimal" "Zero animations & blur (battery / low-latency mode)" ;;
        *)
            echo "unknown motion profile: $p (expected fluid|snappy|cinematic|minimal)" >&2
            return 1 ;;
    esac
}

cmd_border_loop() {
    local mode="${1:-toggle}"
    if [ "$mode" = "toggle" ]; then
        [ "$(current_loop)" = "on" ] && mode="off" || mode="on"
    fi
    if [ "$mode" = "on" ]; then
        printf 'on\n' > "$LOOP_STATE"
        hyprctl --batch "\
keyword bezier monoLinear,0.0,0.0,1.0,1.0 ; \
keyword animation borderangle,1,55,monoLinear,loop" >/dev/null 2>&1 || true
        notify "Liquid Silver Border · ON" "Active window border gradient orbits smoothly"
    else
        printf 'off\n' > "$LOOP_STATE"
        hyprctl --batch "\
keyword bezier expoOut,0.16,1.0,0.3,1.0 ; \
keyword animation borderangle,1,18,expoOut,once" >/dev/null 2>&1 || true
        notify "Liquid Silver Border · OFF" "Static hairline gradient (saves laptop battery)"
    fi
}

cmd_cycle() {
    case "$(current_profile)" in
        fluid)     apply_profile snappy ;;
        snappy)    apply_profile cinematic ;;
        cinematic) apply_profile minimal ;;
        *)         apply_profile fluid ;;
    esac
}

cmd_menu() {
    local cur loop pick
    cur="$(current_profile)"
    loop="$(current_loop)"
    pick="$(printf '%s\n' \
        "∿  Fluid — Damped Springs & 18% Parallax (Default)" \
        "⚡  Snappy — 140ms Crisp Studio Precision" \
        "✦  Cinematic — Deep 26% Parallax + Orbiting Silver Border" \
        "◌  Minimal — Zero Motion & Blur (Battery / Esports)" \
        "⟳  Toggle Orbiting Liquid-Silver Active Border (${loop})" \
        | rofi_menu settings -dmenu -i -p "motion:" -mesg "Active physics: ${cur} · liquid border: ${loop}")" || exit 0

    case "$pick" in
        *"Fluid"*)     apply_profile fluid ;;
        *"Snappy"*)    apply_profile snappy ;;
        *"Cinematic"*) apply_profile cinematic ;;
        *"Minimal"*)   apply_profile minimal ;;
        *"Orbiting"*)  cmd_border_loop toggle ;;
    esac
}

case "${1:-menu}" in
    menu)        cmd_menu ;;
    cycle)       cmd_cycle ;;
    set)         apply_profile "${2:-fluid}" ;;
    border-loop) cmd_border_loop "${2:-toggle}" ;;
    status)      printf '%s (border-loop: %s)\n' "$(current_profile)" "$(current_loop)" ;;
    *)
        echo "usage: motion.sh {menu|cycle|set <fluid|snappy|cinematic|minimal>|border-loop [on|off|toggle]|status}" >&2
        exit 1 ;;
esac
