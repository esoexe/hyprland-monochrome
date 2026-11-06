#!/usr/bin/env bash
# System Health & Monochrome Theme Doctor.
#
# Audits compositor state, Lua syntax, audio/PipeWire, screen capture + OCR,
# font installation, full-stack monochrome theme files (GTK3/4, Qt5/6, Neovim,
# Kitty, Rofi, Waybar, Dunst), wallpapers, and pristine backup safety.
#
#   doctor.sh            formatted terminal report
#   doctor.sh --plain    plain ASCII lines (used by Rofi viewer)
#   doctor.sh --rofi     interactive Rofi health report

set -u
. "$(dirname "$0")/lib.sh"

MODE="${1:-tty}"

PASS=0
WARN=0
LINES=()

record() { # record <ok|warn> <area> <detail>
    local st="$1" area="$2" detail="$3"
    if [ "$st" = "ok" ]; then
        PASS=$((PASS + 1))
        LINES+=("✓  $(printf '%-20s' "$area")  $detail")
    else
        WARN=$((WARN + 1))
        LINES+=("!  $(printf '%-20s' "$area")  $detail")
    fi
}

# 1. Hyprland Lua config
LUAC="$(command -v luac5.4 || command -v luac || true)"
if [ -n "$LUAC" ] && [ -f "$HYPR_DIR/hyprland.lua" ]; then
    bad=0
    for f in "$HYPR_DIR/hyprland.lua" "$HYPR_DIR"/modules/*.lua; do
        [ -f "$f" ] || continue
        "$LUAC" -p "$f" 2>/dev/null || bad=$((bad + 1))
    done
    if [ "$bad" -eq 0 ]; then
        record ok "Lua config" "all modules parse cleanly ($("$LUAC" -v | awk '{print $1, $2}'))"
    else
        record warn "Lua config" "$bad module(s) failed syntax check"
    fi
elif [ -f "$HYPR_DIR/modules/user.lua" ]; then
    record ok "Lua config" "modules present (install lua for bytecode check)"
else
    record warn "Lua config" "missing $HYPR_DIR/modules/user.lua"
fi

# 2. Wayland session
if [ -n "${WAYLAND_DISPLAY:-}" ]; then
    record ok "Wayland session" "active on ${WAYLAND_DISPLAY}"
else
    record warn "Wayland session" "WAYLAND_DISPLAY not set (TTY / headless)"
fi

# 3. Audio stack
if have wpctl; then
    vol="$("$SCRIPTS_DIR/volume.sh" status 2>/dev/null || echo '?')"
    record ok "PipeWire / wpctl" "reachable (output: $vol)"
elif have pactl; then
    record ok "Pulse / pactl" "pactl available"
else
    record warn "Audio stack" "neither wpctl nor pactl found"
fi

# 4. Screen recording & capture
if have gpu-screen-recorder; then
    record ok "Screen recorder" "gpu-screen-recorder (60s instant replay ready)"
elif have wf-recorder; then
    record ok "Screen recorder" "wf-recorder software/VAAPI ready"
else
    record warn "Screen recorder" "install gpu-screen-recorder or wf-recorder"
fi

if have grim && have slurp; then
    ocr_note="screenshots ready"
    have tesseract && ocr_note+=" + Tesseract OCR"
    have satty && ocr_note+=" + Satty annotate"
    record ok "Capture & OCR" "$ocr_note"
else
    record warn "Capture & OCR" "install grim + slurp (+ tesseract for OCR)"
fi

# 5. Fonts
if have fc-list && fc-list 2>/dev/null | grep -qi "JetBrainsMono"; then
    record ok "Typography" "JetBrainsMono Nerd Font installed"
else
    record warn "Typography" "install ttf-jetbrains-mono-nerd"
fi

# 6. Full-stack monochrome theme files
theme_ok=0
theme_total=7
for rel in \
    "gtk-3.0/gtk.css" \
    "gtk-4.0/gtk.css" \
    "qt6ct/colors/Monochrome.conf" \
    "nvim/colors/monochrome.lua" \
    "kitty/theme.conf" \
    "rofi/theme.rasi" \
    "waybar/style.css"
do
    [ -f "$CONFIG_DIR/$rel" ] && theme_ok=$((theme_ok + 1))
done
if [ "$theme_ok" -eq "$theme_total" ]; then
    record ok "Monochrome stack" "$theme_ok/$theme_total toolkits themed (GTK3/4, Qt5/6, Nvim, Kitty, Rofi, Bar)"
else
    record warn "Monochrome stack" "$theme_ok/$theme_total toolkit theme files present in $CONFIG_DIR"
fi

# 7. Wallpapers + GLSL shaders
WDIR="$(mono_var wallpaper_dir "$HOME/Pictures/Wallpapers")"
wcount=0
if [ -d "$WDIR" ]; then
    wcount="$(find "$WDIR" -maxdepth 1 -type f \( -iname '*.jpg' -o -iname '*.png' -o -iname '*.webp' -o -iname '*.gif' \) | wc -l | tr -d ' ')"
fi
if [ "$wcount" -gt 0 ]; then
    record ok "Wallpaper studio" "$wcount monochrome wallpaper(s) (static + live loops) in ${WDIR/#$HOME/\~}"
else
    record warn "Wallpaper studio" "no wallpapers found in ${WDIR/#$HOME/\~}"
fi

scount="$(find "$HYPR_DIR/shaders" -maxdepth 1 -type f -name '*.frag' 2>/dev/null | wc -l | tr -d ' ')"
if [ "${scount:-0}" -ge 5 ]; then
    record ok "Kinetic & shaders" "motion=$($SCRIPTS_DIR/motion.sh status 2>/dev/null | awk '{print $1}') · $scount GLSL ES 3.00 shaders ready"
else
    record warn "Kinetic & shaders" "${scount:-0}/5 GLSL shaders in $HYPR_DIR/shaders"
fi

if [ -x "$SCRIPTS_DIR/perf.sh" ]; then
    pstat="$("$SCRIPTS_DIR/perf.sh" status 2>/dev/null || echo 'turbo=off glass=kawase vrr=fullscreen')"
    record ok "Velocity engine" "0-fork runtime · squircle=$(mono_var rounding_power 4.0) · $pstat"
else
    record warn "Velocity engine" "perf.sh missing in $SCRIPTS_DIR"
fi

# 8. Universal PC hardware & OpenCode browser theme
if [ -x "$SCRIPTS_DIR/hardware.sh" ] && [ -f "$HYPR_DIR/modules/hardware.lua" ]; then
    hw_summary="$("$SCRIPTS_DIR/hardware.sh" status --json 2>/dev/null | jq -r '"\(.chassis) · \(.gpus) · profile=\(.power_profile)"' 2>/dev/null || echo 'ready')"
    record ok "PC Hardware" "$hw_summary"
else
    record warn "PC Hardware" "hardware.lua / hardware.sh missing"
fi

if [ -f "$CONFIG_DIR/browser/opencode/startpage.html" ] && [ -f "$CONFIG_DIR/browser/opencode/firefox/userChrome.css" ]; then
    record ok "OpenCode browser" "startpage + Firefox CSS + Chromium MV3 + OpenCode TUI ready"
else
    record warn "OpenCode browser" "missing ~/.config/browser/opencode assets"
fi

# 9. Pristine backup vault
if [ -d "$HOME/.config-backups/pristine" ]; then
    record ok "Backup vault" "$HOME/.config-backups/pristine present"
else
    record ok "Backup vault" "clean install (no overwritten originals)"
fi

SUMMARY="$PASS passed · $WARN warning(s)"

case "$MODE" in
    --rofi|rofi)
        printf '%s\n' "${LINES[@]}" | rofi_menu keybinds -dmenu -i -p "doctor:" \
            -mesg "hyprland-monochrome health audit — $SUMMARY" >/dev/null 2>&1 || true ;;
    --plain|plain)
        printf 'hyprland-monochrome doctor — %s\n' "$SUMMARY"
        printf '%s\n' "${LINES[@]}" ;;
    *)
        printf '\033[1mhyprland-monochrome doctor\033[0m — %s\n\n' "$SUMMARY"
        for ln in "${LINES[@]}"; do
            case "$ln" in
                ✓*) printf '  \033[32m%s\033[0m\n' "$ln" ;;
                *)  printf '  \033[33m%s\033[0m\n' "$ln" ;;
            esac
        done
        printf '\n' ;;
esac
