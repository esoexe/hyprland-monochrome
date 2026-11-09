#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════════
#  hardware.sh — Universal PC Hardware Probe, Power Profiles & Override Tool
#
#  Usage:
#    hardware.sh status [--json]            print GPU, chassis, battery & profile
#    hardware.sh power [profile|cycle|menu] performance | balanced | power-saver
#    hardware.sh write-override             scaffold modules/hardware-user.lua
#    hardware.sh menu                       interactive Rofi hardware & power menu
# ═══════════════════════════════════════════════════════════════════════════
set -euo pipefail
# shellcheck source=lib.sh
. "$(dirname "$0")/lib.sh"

detect_hardware() {
    HW_GPUS=""
    HW_HAS_NVIDIA=0
    HW_HAS_AMD=0
    HW_HAS_INTEL=0
    HW_IS_VM=0
    HW_CHASSIS="desktop"
    HW_BATTERY="none"
    HW_BACKLIGHT="none"
    HW_POWER_PROFILE="balanced"

    local card vendor
    for card in /sys/class/drm/card[0-9]/device/vendor; do
        [ -r "$card" ] || continue
        IFS= read -r vendor < "$card" || vendor=""
        case "${vendor,,}" in
            *10de*) HW_HAS_NVIDIA=1; HW_GPUS="${HW_GPUS:+$HW_GPUS + }NVIDIA" ;;
            *1002*) HW_HAS_AMD=1;    HW_GPUS="${HW_GPUS:+$HW_GPUS + }AMD" ;;
            *8086*) HW_HAS_INTEL=1;  HW_GPUS="${HW_GPUS:+$HW_GPUS + }Intel" ;;
            *1af4*|*15ad*|*80ee*|*1b36*) HW_IS_VM=1; HW_GPUS="${HW_GPUS:+$HW_GPUS + }VirtIO/VM" ;;
        esac
    done
    [ -r /proc/driver/nvidia/version ] && HW_HAS_NVIDIA=1
    [ -n "$HW_GPUS" ] || HW_GPUS="Generic / Software DRM"

    if have systemd-detect-virt && systemd-detect-virt --vm >/dev/null 2>&1; then
        HW_IS_VM=1
        HW_CHASSIS="vm ($(systemd-detect-virt --vm 2>/dev/null || printf 'virtual'))"
    fi

    local bat cap status
    for bat in /sys/class/power_supply/BAT*; do
        if [ -r "$bat/capacity" ]; then
            IFS= read -r cap < "$bat/capacity" || cap="?"
            IFS= read -r status < "$bat/status" 2>/dev/null || status="Unknown"
            HW_BATTERY="${cap}% (${status})"
            [ "$HW_IS_VM" -eq 0 ] && HW_CHASSIS="laptop"
            break
        fi
    done

    local bl
    for bl in /sys/class/backlight/*; do
        if [ -e "$bl" ]; then
            HW_BACKLIGHT="${bl##*/}"
            [ "$HW_IS_VM" -eq 0 ] && HW_CHASSIS="laptop"
            break
        fi
    done

    if have powerprofilesctl; then
        HW_POWER_PROFILE="$(powerprofilesctl get 2>/dev/null || printf 'balanced')"
    elif [ -r /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]; then
        IFS= read -r HW_POWER_PROFILE < /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor || HW_POWER_PROFILE="balanced"
    elif [ -r "$CACHE_DIR/power-profile" ]; then
        IFS= read -r HW_POWER_PROFILE < "$CACHE_DIR/power-profile" || HW_POWER_PROFILE="balanced"
    fi
}

set_power_profile() {
    local target="$1"
    if have powerprofilesctl; then
        powerprofilesctl set "$target" 2>/dev/null || true
    fi
    printf '%s\n' "$target" > "$CACHE_DIR/power-profile"

    # Also align compositor animation/perf profile cleanly on any PC (even without PPD)
    case "$target" in
        power-saver)
            [ -x "$SCRIPTS_DIR/motion.sh" ] && "$SCRIPTS_DIR/motion.sh" apply minimal >/dev/null 2>&1 || true
            ;;
        performance)
            [ -x "$SCRIPTS_DIR/motion.sh" ] && "$SCRIPTS_DIR/motion.sh" apply snappy >/dev/null 2>&1 || true
            ;;
        balanced)
            [ -x "$SCRIPTS_DIR/motion.sh" ] && "$SCRIPTS_DIR/motion.sh" apply fluid >/dev/null 2>&1 || true
            ;;
    esac
    notify "Power profile" "Active profile: $target"
}

cycle_power_profile() {
    detect_hardware
    case "$HW_POWER_PROFILE" in
        balanced*)    set_power_profile "performance" ;;
        performance*) set_power_profile "power-saver" ;;
        *)            set_power_profile "balanced" ;;
    esac
}

write_override() {
    detect_hardware
    local out="$HYPR_DIR/modules/hardware-user.lua"
    if [ -f "$out" ]; then
        notify "Hardware override exists" "$out"
        return 0
    fi
    cat > "$out" <<EOF
-- Auto-generated per-PC hardware override ($(date +%Y-%m-%d))
-- Detected: chassis=$HW_CHASSIS | gpu=$HW_GPUS | nvidia=$HW_HAS_NVIDIA | amd=$HW_HAS_AMD | intel=$HW_HAS_INTEL | vm=$HW_IS_VM
-- Safe to customize; loaded automatically at the end of modules/hardware.lua.

hl.config({
    misc = {
        vfr = true,
    },
})
EOF
    notify "Saved hardware override" "$out"
}

cmd_status() {
    detect_hardware
    if [ "${1:-}" = "--json" ]; then
        printf '{"chassis":"%s","gpus":"%s","battery":"%s","backlight":"%s","power_profile":"%s","nvidia":%d,"vm":%d}\n' \
            "$HW_CHASSIS" "$HW_GPUS" "$HW_BATTERY" "$HW_BACKLIGHT" "$HW_POWER_PROFILE" "$HW_HAS_NVIDIA" "$HW_IS_VM"
    elif [ "${1:-}" = "--summary" ]; then
        printf '%s · %s · profile=%s\n' "$HW_CHASSIS" "$HW_GPUS" "$HW_POWER_PROFILE"
    else
        printf 'Chassis       : %s\n' "$HW_CHASSIS"
        printf 'GPU(s)        : %s\n' "$HW_GPUS"
        printf 'Battery       : %s\n' "$HW_BATTERY"
        printf 'Backlight     : %s\n' "$HW_BACKLIGHT"
        printf 'Power profile : %s\n' "$HW_POWER_PROFILE"
    fi
}

cmd_menu() {
    detect_hardware
    local choice
    choice="$(printf '%s\n' \
        "󰓅  Power: Performance" \
        "󰾅  Power: Balanced          (current: $HW_POWER_PROFILE)" \
        "󰌪  Power: Power Saver" \
        "󰍹  Hardware Summary         ($HW_CHASSIS · $HW_GPUS)" \
        "󰒓  Write hardware-user.lua  (save per-PC overrides)" \
        "󰑐  Re-probe Monitors        (hyprctl reload)" \
        | rofi_menu list -dmenu -i -p "hardware")" || exit 0

    case "$choice" in
        *"Power: Performance"*) set_power_profile "performance" ;;
        *"Power: Balanced"*)    set_power_profile "balanced" ;;
        *"Power: Power Saver"*) set_power_profile "power-saver" ;;
        *"Hardware Summary"*)
            notify "PC Hardware" "Chassis: $HW_CHASSIS | GPU: $HW_GPUS | Battery: $HW_BATTERY | Profile: $HW_POWER_PROFILE"
            ;;
        *"Write hardware-user.lua"*)
            write_override
            if [ -f "$HYPR_DIR/modules/hardware-user.lua" ]; then
                "$SCRIPTS_DIR/open-app.sh" open "$HYPR_DIR/modules/hardware-user.lua" || true
            fi
            ;;
        *"Re-probe Monitors"*)
            have hyprctl && hyprctl reload >/dev/null 2>&1 || true
            notify "Monitors & Hardware" "Compositor configuration reloaded"
            ;;
    esac
}

case "${1:-status}" in
    status)         shift || true; cmd_status "${1:-}" ;;
    power)
        case "${2:-cycle}" in
            performance|balanced|power-saver) set_power_profile "$2" ;;
            menu)  cmd_menu ;;
            *)     cycle_power_profile ;;
        esac
        ;;
    write-override) write_override ;;
    menu)           cmd_menu ;;
    *)
        printf 'Usage: %s {status [--json]|power [performance|balanced|power-saver|cycle|menu]|write-override|menu}\n' "${0##*/}" >&2
        exit 2
        ;;
esac
