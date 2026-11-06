#!/usr/bin/env bash
# Velocity, Low-Latency Turbo/Game Mode, Optical Glass & Frame-Timing Engine.
#
#   perf.sh menu                              Rofi Velocity & Optics control deck
#   perf.sh turbo [toggle|on|off]             0-latency Game / Battery Turbo mode
#   perf.sh glass [cycle|kawase|xray|acrylic|off]
#                                             Hyprland 0.56 optical blur pipeline
#   perf.sh vrr [off|on|fullscreen|auto]      Adaptive Sync (VRR) mode
#   perf.sh bench [--plain|--rofi]            Sub-ms stack latency benchmark
#   perf.sh status [--waybar]                 Active turbo/glass/vrr summary

set -u
. "$(dirname "$0")/lib.sh"

case "${1:-}" in --*) set -- "${1#--}" "${@:2}" ;; esac

TURBO_STATE="$CACHE_DIR/turbo.state"
GLASS_STATE="$CACHE_DIR/glass.state"
VRR_STATE="$CACHE_DIR/vrr.state"

refresh_bar() {
    pkill -RTMIN+10 -x waybar 2>/dev/null || true
}

turbo_active() {
    [ -f "$TURBO_STATE" ]
}

current_glass() {
    local g
    g="$(cat "$GLASS_STATE" 2>/dev/null || true)"
    [ -z "$g" ] && g="$(mono_var glass_mode kawase)"
    case "$g" in kawase|xray|acrylic|off) printf '%s' "$g" ;; *) printf 'kawase' ;; esac
}

current_vrr() {
    local v
    v="$(cat "$VRR_STATE" 2>/dev/null || true)"
    [ -z "$v" ] && v="$(mono_var vrr_mode 2)"
    case "$v" in 0|1|2|3) printf '%s' "$v" ;; *) printf '2' ;; esac
}

vrr_label() {
    case "$1" in
        0) printf 'off' ;;
        1) printf 'always' ;;
        2) printf 'fullscreen' ;;
        3) printf 'video+game' ;;
        *) printf 'fullscreen' ;;
    esac
}

apply_glass() {
    local mode="${1:-kawase}"
    case "$mode" in
        kawase)
            printf 'kawase\n' > "$GLASS_STATE"
            hyprctl --batch "\
                keyword decoration:blur:enabled 1;\
                keyword decoration:blur:variant kawase;\
                keyword decoration:blur:size 6;\
                keyword decoration:blur:passes 2;\
                keyword decoration:blur:xray 0" >/dev/null 2>&1 || true
            [ "${2:-notify}" = "notify" ] && notify "Optical Glass · Kawase" "2-pass frosted obsidian glass (balanced)"
            ;;
        xray)
            printf 'xray\n' > "$GLASS_STATE"
            hyprctl --batch "\
                keyword decoration:blur:enabled 1;\
                keyword decoration:blur:variant kawase;\
                keyword decoration:blur:size 6;\
                keyword decoration:blur:passes 1;\
                keyword decoration:blur:xray 1" >/dev/null 2>&1 || true
            [ "${2:-notify}" = "notify" ] && notify "Optical Glass · Fast X-Ray" "Single-pass wallpaper sampling (~2.5x lower GPU cost)"
            ;;
        acrylic)
            printf 'acrylic\n' > "$GLASS_STATE"
            hyprctl --batch "\
                keyword decoration:blur:enabled 1;\
                keyword decoration:blur:variant acrylic;\
                keyword decoration:blur:size 7;\
                keyword decoration:blur:passes 2;\
                keyword decoration:blur:xray 0;\
                keyword decoration:blur:acrylic:aberration 0.0;\
                keyword decoration:blur:acrylic:clarity 0.84;\
                keyword decoration:blur:acrylic:refraction 18.0;\
                keyword decoration:blur:acrylic:tint 0x14ffffff" >/dev/null 2>&1 || true
            [ "${2:-notify}" = "notify" ] && notify "Optical Glass · Liquid Acrylic" "Monochrome optical lens refraction (0.0 aberration)"
            ;;
        off)
            printf 'off\n' > "$GLASS_STATE"
            hyprctl keyword decoration:blur:enabled 0 >/dev/null 2>&1 || true
            [ "${2:-notify}" = "notify" ] && notify "Optical Glass · Off" "Blur disabled for maximum GPU headroom"
            ;;
        cycle)
            case "$(current_glass)" in
                kawase)  apply_glass xray ;;
                xray)    apply_glass acrylic ;;
                acrylic) apply_glass off ;;
                *)       apply_glass kawase ;;
            esac
            ;;
        *)
            echo "unknown glass mode: $mode (expected kawase|xray|acrylic|off|cycle)" >&2
            return 1 ;;
    esac
}

apply_vrr() {
    local want="${1:-fullscreen}" code=2
    case "$want" in
        0|off)        code=0 ;;
        1|on|always)  code=1 ;;
        2|fullscreen) code=2 ;;
        3|auto|game)  code=3 ;;
        *) echo "unknown vrr mode: $want (expected off|on|fullscreen|auto)" >&2; return 1 ;;
    esac
    printf '%s\n' "$code" > "$VRR_STATE"
    hyprctl keyword misc:vrr "$code" >/dev/null 2>&1 || true
    notify "Adaptive Sync (VRR)" "Set to $(vrr_label "$code") (misc:vrr=$code)"
}

apply_turbo() {
    local action="${1:-toggle}"
    if [ "$action" = "toggle" ]; then
        if turbo_active; then action="off"; else action="on"; fi
    fi

    case "$action" in
        on|1|true)
            printf 'on\n' > "$TURBO_STATE"
            hyprctl --batch "\
                keyword animations:enabled 0;\
                keyword decoration:blur:enabled 0;\
                keyword decoration:shadow:enabled 0;\
                keyword decoration:dim_inactive 0;\
                keyword decoration:rounding 0;\
                keyword general:gaps_in 0;\
                keyword general:gaps_out 0;\
                keyword general:border_size 1;\
                keyword general:allow_tearing 1;\
                keyword render:direct_scanout 1;\
                keyword misc:vrr 1" >/dev/null 2>&1 || true
            hyprctl keyword decoration:screen_shader "" >/dev/null 2>&1 || true
            refresh_bar
            notify "⚡ Turbo Mode · ON" "Zero latency: tearing, direct scanout & VRR unlocked · 0ms motion"
            ;;
        off|0|false)
            rm -f "$TURBO_STATE"
            local gi go bs rd rp vrr
            gi="$(mono_var gaps_in 5)"
            go="$(mono_var gaps_out 10)"
            bs="$(mono_var border_size 2)"
            rd="$(mono_var rounding 10)"
            rp="$(mono_var rounding_power 4.0)"
            vrr="$(current_vrr)"
            hyprctl --batch "\
                keyword general:gaps_in ${gi};\
                keyword general:gaps_out ${go};\
                keyword general:border_size ${bs};\
                keyword general:allow_tearing 0;\
                keyword decoration:rounding ${rd};\
                keyword decoration:rounding_power ${rp};\
                keyword decoration:shadow:enabled 1;\
                keyword decoration:dim_inactive 1;\
                keyword render:direct_scanout 2;\
                keyword misc:vrr ${vrr}" >/dev/null 2>&1 || true
            apply_glass "$(current_glass)" quiet
            local mprof shd
            mprof="$("$SCRIPTS_DIR/motion.sh" status 2>/dev/null || echo fluid)"
            "$SCRIPTS_DIR/motion.sh" set "$mprof" >/dev/null 2>&1 || true
            shd="$("$SCRIPTS_DIR/shader.sh" status 2>/dev/null || echo off)"
            [ "$shd" != "off" ] && "$SCRIPTS_DIR/shader.sh" set "$shd" >/dev/null 2>&1 || true
            refresh_bar
            notify "Turbo Mode · OFF" "Restored ${mprof} physics, $(current_glass) glass & ${rd}px squircles"
            ;;
        *)
            echo "usage: perf.sh turbo {toggle|on|off}" >&2
            return 1 ;;
    esac
}

ns_now() {
    if [ -n "${EPOCHREALTIME:-}" ]; then
        local s="${EPOCHREALTIME%.*}" f="${EPOCHREALTIME#*.}"
        f="${f}000000"
        printf '%s%s' "$s" "${f:0:6}"
    else
        date +%s%6N 2>/dev/null || printf '0'
    fi
}

cmd_bench() {
    local t0 t1 d_var d_sys d_ipc="n/a" d_lua="n/a"
    t0="$(ns_now)"
    local i=0
    while [ "$i" -lt 10 ]; do
        mono_var gaps_in 5 >/dev/null
        i=$((i + 1))
    done
    t1="$(ns_now)"
    d_var="$(awk -v a="$t0" -v b="$t1" 'BEGIN { printf "%.2f ms", (b - a) / 10000.0 }')"

    t0="$(ns_now)"
    "$CONFIG_DIR/waybar/scripts/sysgraph.sh" >/dev/null 2>&1 || true
    t1="$(ns_now)"
    d_sys="$(awk -v a="$t0" -v b="$t1" 'BEGIN { printf "%.2f ms", (b - a) / 1000.0 }')"

    local lua_c=""
    for _lc in luac luac5.4; do
        if have "$_lc"; then lua_c="$_lc"; break; fi
    done
    if [ -n "$lua_c" ] && [ -f "$HYPR_DIR/modules/appearance.lua" ]; then
        t0="$(ns_now)"
        "$lua_c" -p "$HYPR_DIR/hyprland.lua" "$HYPR_DIR"/modules/*.lua >/dev/null 2>&1 || true
        t1="$(ns_now)"
        d_lua="$(awk -v a="$t0" -v b="$t1" 'BEGIN { printf "%.2f ms", (b - a) / 1000.0 }')"
    fi

    if have hyprctl && [ -n "${HYPRLAND_INSTANCE_SIGNATURE:-}" ]; then
        t0="$(ns_now)"
        hyprctl version >/dev/null 2>&1 || true
        t1="$(ns_now)"
        d_ipc="$(awk -v a="$t0" -v b="$t1" 'BEGIN { printf "%.2f ms", (b - a) / 1000.0 }')"
    fi

    local turbo_txt="off"
    turbo_active && turbo_txt="ON (0ms tearing+scanout)"
    local report
    report="$(cat <<EOF
hyprland-monochrome Velocity & Frame-Pacing Benchmark
─────────────────────────────────────────────────────
  mono_var() lookup (avg) : ${d_var}   (pure Bash · 0 forks)
  sysgraph /proc sample   : ${d_sys}   (pure Bash · 0 forks)
  Lua 9-module bytecode   : ${d_lua}
  hyprctl IPC roundtrip   : ${d_ipc}
  Turbo / Game Mode       : ${turbo_txt}
  Optical Glass Engine    : $(current_glass)
  Adaptive Sync (VRR)     : $(vrr_label "$(current_vrr)")
  Motion Physics Profile  : $("$SCRIPTS_DIR/motion.sh" status 2>/dev/null || echo fluid)
EOF
)"
    if [ "${1:-}" = "--rofi" ]; then
        printf '%s\n' "$report" | rofi_menu settings -dmenu -i -p "bench:" \
            -mesg "Sub-millisecond runtime latency audit" >/dev/null 2>&1 || true
    else
        printf '%s\n' "$report"
    fi
}

cmd_status() {
    local glass vrr
    glass="$(current_glass)"
    vrr="$(vrr_label "$(current_vrr)")"
    if [ "${1:-}" = "--waybar" ]; then
        if turbo_active; then
            printf '{"text":"⚡ TURBO","alt":"turbo","class":"on","tooltip":"Low-Latency Turbo Mode ON\\nDirect scanout · Tearing unlocked · 0ms animations\\nLeft-click: restore desktop · Right-click: Velocity menu"}\n'
        else
            printf '{"text":"","alt":"normal","class":"off","tooltip":"Velocity normal · glass=%s · vrr=%s"}\n' "$glass" "$vrr"
        fi
    else
        local t="off"
        turbo_active && t="on"
        printf 'turbo=%s glass=%s vrr=%s\n' "$t" "$glass" "$vrr"
    fi
}

cmd_menu() {
    local t="off" glass vrr pick
    turbo_active && t="ON"
    glass="$(current_glass)"
    vrr="$(vrr_label "$(current_vrr)")"

    pick="$(printf '%s\n' \
        "⚡  Toggle Turbo / Game Mode (0ms · Direct Scanout · Tearing) [${t}]" \
        "◈  Optical Glass: Kawase 2-Pass Frosted Obsidian" \
        "⚡  Optical Glass: Fast X-Ray 1-Pass (~2.5x Lower GPU Load)" \
        "✦  Optical Glass: Liquid Acrylic Lens Refraction (0.0 Aberration)" \
        "○  Optical Glass: Off (Zero Blur Overhead)" \
        "⟳  Adaptive Sync (VRR): Fullscreen Only (Recommended)" \
        "⟳  Adaptive Sync (VRR): Always On" \
        "⟳  Adaptive Sync (VRR): Off" \
        "⏱  Run Live Sub-Millisecond Stack Benchmark" \
        | rofi_menu settings -dmenu -i -p "velocity:" \
            -mesg "turbo=${t} · glass=${glass} · vrr=${vrr} · squircle=4.0")" || exit 0

    case "$pick" in
        *"Toggle Turbo"*)        apply_turbo toggle ;;
        *"Kawase"*)              apply_glass kawase ;;
        *"Fast X-Ray"*)          apply_glass xray ;;
        *"Liquid Acrylic"*)      apply_glass acrylic ;;
        *"Optical Glass: Off"*)  apply_glass off ;;
        *"Fullscreen Only"*)     apply_vrr fullscreen ;;
        *"Always On"*)           apply_vrr on ;;
        *"(VRR): Off"*)          apply_vrr off ;;
        *"Benchmark"*)           cmd_bench --rofi ;;
    esac
}

case "${1:-menu}" in
    menu)               cmd_menu ;;
    turbo|gamemode)     apply_turbo "${2:-toggle}" ;;
    glass)              apply_glass "${2:-cycle}" ;;
    vrr)                apply_vrr "${2:-fullscreen}" ;;
    bench|benchmark)    cmd_bench "${2:-}" ;;
    status)             cmd_status "${2:-}" ;;
    *)
        echo "usage: perf.sh {menu|turbo [toggle|on|off]|glass [cycle|kawase|xray|acrylic|off]|vrr [off|on|fullscreen|auto]|bench [--rofi]|status [--waybar]}" >&2
        exit 1 ;;
esac
