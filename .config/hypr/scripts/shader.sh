#!/usr/bin/env bash
# Real-Time GPU Screen-Shader Switcher (GLSL ES 3.00 via decoration:screen_shader).
#
#   shader.sh [menu|cycle|set <off|noir|cinema-grain|crt-studio|paper-eink|oled-pitch>|status]

set -u
. "$(dirname "$0")/lib.sh"

case "${1:-}" in --*) set -- "${1#--}" "${@:2}" ;; esac

SHADER_DIR="$HYPR_DIR/shaders"
SHADER_STATE="$CACHE_DIR/shader.state"

current_shader() {
    local s
    s="$(cat "$SHADER_STATE" 2>/dev/null || echo off)"
    case "$s" in noir|cinema-grain|crt-studio|paper-eink|oled-pitch) printf '%s' "$s" ;; *) printf 'off' ;; esac
}

apply_shader() {
    local name="${1:-off}"
    if [ "$name" = "off" ] || [ "$name" = "none" ]; then
        printf 'off\n' > "$SHADER_STATE"
        hyprctl keyword decoration:screen_shader "" >/dev/null 2>&1 || true
        notify "GPU Shader · Off" "Direct unshaded framebuffer"
        return 0
    fi

    local frag="$SHADER_DIR/${name}.frag"
    if [ ! -f "$frag" ]; then
        echo "shader not found: $frag" >&2
        return 1
    fi

    printf '%s\n' "$name" > "$SHADER_STATE"
    hyprctl keyword decoration:screen_shader "$frag" >/dev/null 2>&1 || true
    case "$name" in
        noir)         notify "GPU Shader · Noir" "Full-screen Rec.709 Leica monochrome filter" ;;
        cinema-grain) notify "GPU Shader · 35mm Film" "Silver-halide grain & optical corner vignette" ;;
        crt-studio)   notify "GPU Shader · P4 Studio CRT" "Subtle white-phosphor scanlines & curvature" ;;
        paper-eink)   notify "GPU Shader · Matte E-Ink" "Low-glare matte paper contrast for long reading" ;;
        oled-pitch)   notify "GPU Shader · OLED Pitch" "True-black pixel-off saver & silver micro-contrast" ;;
    esac
}

cmd_cycle() {
    case "$(current_shader)" in
        off)          apply_shader noir ;;
        noir)         apply_shader cinema-grain ;;
        cinema-grain) apply_shader crt-studio ;;
        crt-studio)   apply_shader paper-eink ;;
        paper-eink)   apply_shader oled-pitch ;;
        *)            apply_shader off ;;
    esac
}

cmd_menu() {
    local cur pick
    cur="$(current_shader)"
    pick="$(printf '%s\n' \
        "○  Off — Direct Unshaded Framebuffer (Default)" \
        "◐  Noir — Hardware Rec.709 Monochrome Enforcer" \
        "◈  35mm Film — Silver-Halide Grain & Optical Vignette" \
        "▤  P4 Studio CRT — Subtle Scanlines & Curvature" \
        "▫  Matte E-Ink — Low-Glare Paper Contrast" \
        "●  OLED Pitch — True-Black Pixel-Off & Micro-Contrast" \
        | rofi_menu settings -dmenu -i -p "shader:" -mesg "Active GLSL ES 3.00 screen shader: ${cur}")" || exit 0

    case "$pick" in
        *"Off"*)        apply_shader off ;;
        *"Noir"*)       apply_shader noir ;;
        *"35mm Film"*)  apply_shader cinema-grain ;;
        *"Studio CRT"*) apply_shader crt-studio ;;
        *"Matte E-Ink"*)apply_shader paper-eink ;;
        *"OLED Pitch"*) apply_shader oled-pitch ;;
    esac
}

case "${1:-menu}" in
    menu)   cmd_menu ;;
    cycle)  cmd_cycle ;;
    set)    apply_shader "${2:-off}" ;;
    status) printf '%s\n' "$(current_shader)" ;;
    *)
        echo "usage: shader.sh {menu|cycle|set <off|noir|cinema-grain|crt-studio|paper-eink|oled-pitch>|status}" >&2
        exit 1 ;;
esac
