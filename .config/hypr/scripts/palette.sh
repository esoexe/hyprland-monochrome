#!/usr/bin/env bash
# Monochrome Command Palette (Super + K) — Raycast / Linear style omnibar.
#
# Aggregates open windows across workspaces, focus/Zen sprints, tiling & gap
# presets, screen capture + OCR, wallpaper studio, config quick-edit, and a
# safe sub-millisecond Pure C11 math / hex / bitwise calculator (prefix with '='
# or type an arithmetic expression and press Enter).
#
#   palette.sh              interactive Rofi command palette
#   palette.sh calc "expr"  evaluate a math/hex expression safely
#   palette.sh list         emit the current palette entries to stdout

set -u
. "$(dirname "$0")/lib.sh"

case "${1:-}" in --*) set -- "${1#--}" "${@:2}" ;; esac

resolve_calc_bin() {
    local cand
    for cand in \
        "$SCRIPTS_DIR/../bin/mono-calc" \
        "$SCRIPTS_DIR/../../../bin/mono-calc" \
        "$CACHE_DIR/mono-calc"
    do
        if [ -x "$cand" ]; then
            printf '%s\n' "$cand"
            return 0
        fi
    done

    local src="$SCRIPTS_DIR/../src/mono-calc.c"
    local cc_bin=""
    if [ -f "$src" ]; then
        if have cc; then cc_bin="cc"
        elif have gcc; then cc_bin="gcc"
        elif have clang; then cc_bin="clang"
        fi
        if [ -n "$cc_bin" ]; then
            mkdir -p "$CACHE_DIR"
            if "$cc_bin" -std=c11 -O2 "$src" -o "$CACHE_DIR/mono-calc" -lm 2>/dev/null; then
                printf '%s\n' "$CACHE_DIR/mono-calc"
                return 0
            fi
        fi
    fi
    return 1
}

eval_math() {
    local raw="${1:-}" calc_bin
    calc_bin="$(resolve_calc_bin)" || return 1
    "$calc_bin" "$raw"
}

# Build TSV: "<visible label>\t<action token>"
build_entries() {
    # 1. Contextual active states first
    local rstat
    rstat="$("$SCRIPTS_DIR/record.sh" status 2>/dev/null || echo idle)"
    case "$rstat" in
        recording*|paused*|replay*)
            printf '■  Stop Screen Recording (%s)\trecord:stop\n' "${rstat%% ->*}"
            printf '⏸  Pause / Resume Screen Recording\trecord:pause\n' ;;
    esac

    local fstat
    fstat="$("$SCRIPTS_DIR/focus.sh" status 2>/dev/null || echo off)"
    case "$fstat" in
        active*|paused*)
            printf '⏯  Pause / Resume Focus (%s)\tfocus:pause\n' "$fstat"
            printf '⏹  Stop Focus Timer\tfocus:stop\n' ;;
        zen)
            printf '◌  Exit Zen Mode (restore bar & notifications)\tfocus:zen\n' ;;
    esac

    # 2. Live open windows across all workspaces (Pure C11 mono-ipc)
    if have hyprctl || [ -n "${HYPRLAND_INSTANCE_SIGNATURE:-}" ]; then
        mono_ipc clients-palette 2>/dev/null || true
    fi

    # 3. Core productivity, capture, layout & configuration actions
    cat <<'EOF'
◎  Focus · Start 25m Deep Work (Pomodoro)	focus:start25
◎  Focus · Start 50m Extended Session	focus:start50
☕  Focus · Start 5m Short Break	focus:break5
◌  Focus · Toggle Distraction-Free Zen Mode (Super + Z)	focus:zen
🔕  Focus · Toggle Do Not Disturb	focus:dnd
◒  Drawer · Toggle Dropdown Terminal (Super + Shift + T)	drawer:term
▣  Drawer · Toggle Floating btop++ Monitor (Super + Shift + B)	drawer:btop
▣  Control Deck · Launch Pure C11 Terminal Studio (mono-tui · Super + Ctrl + M)	tui:open
✎  Drawer · Toggle Scratch Markdown Notes (Super + ')	drawer:notes
◰  Drawer · Toggle Quick File Drawer	drawer:files
󰀻  Open · Universal App / File / AppImage / Flatpak Opener (Super + Shift + O)	open:picker
⌨  Keyboard · Open Keyboard Type & Layout Studio (Super + Shift + X)	kb:menu
⌨  Keyboard · Switch to Next Keyboard Layout (Super + X)	kb:next
⌨  Keyboard · Set 60% / HHKB Mode (CapsLock -> Escape)	kb:60
⌨  Keyboard · Set Vim Mode (Swap CapsLock & Escape)	kb:vim
⌨  Keyboard · Set Standard ANSI 104-Key Mode	kb:ansi
↖  Cursor · Open Cursor Design Studio (Super + Ctrl + C)	cursor:menu
↖  Cursor · Cycle Monochrome Cursor Design (Super + Alt + C)	cursor:cycle
↖  Cursor · Set Monochrome-Obsidian Pointer	cursor:obsidian
↖  Cursor · Set Monochrome-Ivory Pointer	cursor:ivory
↖  Cursor · Set Monochrome-Precision Reticle Pointer	cursor:precision
↖  Cursor · Set Monochrome-Dot Halo Pointer	cursor:dot
󰖟  Browser · Open OpenCode Startpage (Super + Alt + B)	browser:startpage
󰏘  Browser · Apply OpenCode Theme to All Browser Profiles	browser:apply
󰓅  Hardware · PC GPU, Chassis & Power Profile Menu	hw:menu
⚡  Hardware · Cycle Power Profile (Performance / Balanced / Saver)	hw:power
💾  Session · Save Current Workspace Snapshot ('work')	sess:save
↺  Session · Restore Workspace Snapshot ('work')	sess:restore
🗂  Session · Open Workspace Session Manager	sess:menu
∿  Motion · Fluid Damped Springs & 18% Parallax (Default)	motion:fluid
⚡  Motion · Snappy 140ms Studio Precision	motion:snappy
✦  Motion · Cinematic Parallax + Orbiting Silver Border	motion:cinematic
◌  Motion · Minimal Zero-Motion (Battery / Esports)	motion:minimal
⟳  Motion · Toggle Orbiting Liquid-Silver Active Border	motion:loop
⚡  Velocity · Toggle Low-Latency Turbo / Game Mode (Super + F12)	perf:turbo
◈  Glass · Kawase 2-Pass Frosted Obsidian	perf:kawase
⚡  Glass · Fast X-Ray 1-Pass (~2.5x Lower GPU Load)	perf:xray
✦  Glass · Liquid Acrylic Lens Refraction (0.0 Aberration)	perf:acrylic
○  Glass · Disable Blur (Maximum GPU Headroom)	perf:glassoff
⏱  Velocity · Run Sub-Millisecond Stack Benchmark	perf:bench
◐  Shader · GLSL Noir Hardware Monochrome Enforcer	shader:noir
◈  Shader · GLSL 35mm Silver-Halide Film Grain	shader:grain
▤  Shader · GLSL P4 Studio CRT Scanlines	shader:crt
▫  Shader · GLSL Matte E-Ink Reading Tone	shader:eink
●  Shader · GLSL OLED Pitch True-Black Pixel-Off Saver	shader:oled
○  Shader · Disable GLSL Screen Shader (Off)	shader:off
✦  Wallpaper · Cycle Animated Live Monochrome Wallpaper	wall:live
◹  Window · Spring Floating / PiP Window to Next Corner (Super + .)	pip:corner
⊞  Layout · Cycle Tiling Engine (Dwindle / Master / Scrolling)	layout:cycle
⊞  Layout · Switch to Master-Stack	layout:master
⊞  Layout · Switch to Scrolling Strip	layout:scrolling
⊞  Layout · Switch to Dwindle BSP	layout:dwindle
⊟  Gaps · Airy Gallery Framing (10 / 22px)	gaps:airy
⊟  Gaps · Compact Laptop Density (2 / 4px)	gaps:compact
⊟  Gaps · Zero Edge-to-Edge Monocle (0px)	gaps:zero
⊟  Gaps · Restore Default (from user.lua)	gaps:default
✂  Capture · Screenshot Region to Clipboard	shot:region
✂  Capture · Screenshot Full Screen	shot:full
✂  Capture · Screenshot Focused Window	shot:window
⌕  Capture · OCR Screen Region to Text (Tesseract)	shot:ocr
✎  Capture · Region Screenshot + Annotate	shot:edit
●  Capture · Screen Recording Menu / Toggle	record:menu
↺  Capture · Save Last 60s Instant Replay	record:replay
◐  Desktop · Wallpaper Studio (Visual Gallery)	wall:select
⟳  Desktop · Next Wallpaper	wall:next
🎲  Desktop · Random Wallpaper	wall:random
☾  Desktop · Toggle Night Shift (Warm Filter)	night:toggle
◫  Desktop · Toggle Top Bar (Waybar)	bar:toggle
⌨  Help · Searchable Keybind Cheat Sheet (Super + /)	menu:keybinds
⚙  System · Hardware & Desktop Settings (Super + I)	menu:settings
📋  Clipboard · History Picker (Super + V)	menu:clip
✎  Config · Edit modules/user.lua	edit:user
✎  Config · Edit modules/binds.lua	edit:binds
✎  Config · Edit waybar/config.jsonc	edit:waybar
↻  System · Reload Hyprland & Restart Waybar	sys:reload
✓  System · Run Health & Monochrome Theme Doctor	sys:doctor
⏻  System · Lock Screen	sys:lock
EOF
}

open_in_editor() {
    local file="$1"
    local term editor
    local -a term_cmd
    term="$(mono_var terminal kitty)"
    read -r -a term_cmd <<<"$term"
    editor="${EDITOR:-nvim}"
    have "$editor" || editor="nano"
    nohup "${term_cmd[@]}" -e "$editor" "$file" </dev/null >/dev/null 2>&1 &
}

run_token() {
    local tok="$1"
    case "$tok" in
        win:*)          hyprctl dispatch focuswindow "address:${tok#win:}" >/dev/null 2>&1 || true ;;
        focus:start25)  "$SCRIPTS_DIR/focus.sh" start 25 "Deep Work" ;;
        focus:start50)  "$SCRIPTS_DIR/focus.sh" start 50 "Extended Focus" ;;
        focus:break5)   "$SCRIPTS_DIR/focus.sh" start 5 "Short Break" ;;
        focus:pause)    "$SCRIPTS_DIR/focus.sh" pause ;;
        focus:stop)     "$SCRIPTS_DIR/focus.sh" stop ;;
        focus:zen)      "$SCRIPTS_DIR/focus.sh" zen ;;
        focus:dnd)      "$SCRIPTS_DIR/focus.sh" dnd ;;
        drawer:term)    "$SCRIPTS_DIR/scratchpad.sh" term ;;
        drawer:btop)    "$SCRIPTS_DIR/scratchpad.sh" btop ;;
        drawer:notes)   "$SCRIPTS_DIR/scratchpad.sh" notes ;;
        drawer:files)   "$SCRIPTS_DIR/scratchpad.sh" files ;;
        open:picker)    "$SCRIPTS_DIR/open-app.sh" picker ;;
        browser:startpage) "$SCRIPTS_DIR/browser-theme.sh" open-startpage ;;
        browser:apply)  "$SCRIPTS_DIR/browser-theme.sh" apply ;;
        hw:menu)        "$SCRIPTS_DIR/hardware.sh" menu ;;
        hw:power)       "$SCRIPTS_DIR/hardware.sh" power cycle ;;
        sess:save)      "$SCRIPTS_DIR/workspace-session.sh" save work ;;
        sess:restore)   "$SCRIPTS_DIR/workspace-session.sh" restore work ;;
        sess:menu)      "$SCRIPTS_DIR/workspace-session.sh" menu ;;
        motion:fluid)   "$SCRIPTS_DIR/motion.sh" set fluid ;;
        motion:snappy)  "$SCRIPTS_DIR/motion.sh" set snappy ;;
        motion:cinematic) "$SCRIPTS_DIR/motion.sh" set cinematic ;;
        motion:minimal) "$SCRIPTS_DIR/motion.sh" set minimal ;;
        motion:loop)    "$SCRIPTS_DIR/motion.sh" border-loop toggle ;;
        perf:turbo)     "$SCRIPTS_DIR/perf.sh" turbo toggle ;;
        perf:kawase)    "$SCRIPTS_DIR/perf.sh" glass kawase ;;
        perf:xray)      "$SCRIPTS_DIR/perf.sh" glass xray ;;
        perf:acrylic)   "$SCRIPTS_DIR/perf.sh" glass acrylic ;;
        perf:glassoff)  "$SCRIPTS_DIR/perf.sh" glass off ;;
        perf:bench)     "$SCRIPTS_DIR/perf.sh" bench --rofi ;;
        shader:noir)    "$SCRIPTS_DIR/shader.sh" set noir ;;
        shader:grain)   "$SCRIPTS_DIR/shader.sh" set cinema-grain ;;
        shader:crt)     "$SCRIPTS_DIR/shader.sh" set crt-studio ;;
        shader:eink)    "$SCRIPTS_DIR/shader.sh" set paper-eink ;;
        shader:oled)    "$SCRIPTS_DIR/shader.sh" set oled-pitch ;;
        shader:off)     "$SCRIPTS_DIR/shader.sh" set off ;;
        wall:live)      "$SCRIPTS_DIR/wallpaper.sh" live ;;
        pip:corner)     "$SCRIPTS_DIR/pip-corner.sh" ;;
        layout:cycle)   "$SCRIPTS_DIR/layout.sh" cycle ;;
        layout:master)  "$SCRIPTS_DIR/layout.sh" set master ;;
        layout:scrolling) "$SCRIPTS_DIR/layout.sh" set scrolling ;;
        layout:dwindle) "$SCRIPTS_DIR/layout.sh" set dwindle ;;
        gaps:airy)      "$SCRIPTS_DIR/layout.sh" gaps airy ;;
        gaps:compact)   "$SCRIPTS_DIR/layout.sh" gaps compact ;;
        gaps:zero)      "$SCRIPTS_DIR/layout.sh" gaps zero ;;
        gaps:default)   "$SCRIPTS_DIR/layout.sh" gaps default ;;
        shot:region)    "$SCRIPTS_DIR/screenshot.sh" region ;;
        shot:full)      "$SCRIPTS_DIR/screenshot.sh" full ;;
        shot:window)    "$SCRIPTS_DIR/screenshot.sh" window ;;
        shot:ocr)       "$SCRIPTS_DIR/screenshot.sh" ocr ;;
        shot:edit)      "$SCRIPTS_DIR/screenshot.sh" edit ;;
        record:menu)    "$SCRIPTS_DIR/record.sh" menu ;;
        record:stop)    "$SCRIPTS_DIR/record.sh" stop ;;
        record:pause)   "$SCRIPTS_DIR/record.sh" pause ;;
        record:replay)  "$SCRIPTS_DIR/record.sh" replay ;;
        wall:select)    "$SCRIPTS_DIR/wallpaper.sh" select ;;
        wall:next)      "$SCRIPTS_DIR/wallpaper.sh" next ;;
        wall:random)    "$SCRIPTS_DIR/wallpaper.sh" random ;;
        night:toggle)   "$SCRIPTS_DIR/nightshift.sh" toggle ;;
        bar:toggle)
            if pgrep -x waybar >/dev/null 2>&1; then
                pkill -x waybar 2>/dev/null || true
            else
                ( waybar </dev/null >/dev/null 2>&1 & )
            fi ;;
        menu:keybinds)  "$SCRIPTS_DIR/menu.sh" keybinds ;;
        menu:settings)  "$SCRIPTS_DIR/settings.sh" ;;
        menu:clip)      "$SCRIPTS_DIR/clipboard.sh" ;;
        tui:open)       "$SCRIPTS_DIR/tui.sh" ;;
        kb:menu)        "$SCRIPTS_DIR/keyboard.sh" menu ;;
        kb:next)        "$SCRIPTS_DIR/keyboard.sh" next ;;
        kb:60)          "$SCRIPTS_DIR/keyboard.sh" type compact60 ;;
        kb:vim)         "$SCRIPTS_DIR/keyboard.sh" type vim ;;
        kb:ansi)        "$SCRIPTS_DIR/keyboard.sh" type ansi ;;
        cursor:menu)    "$SCRIPTS_DIR/cursor.sh" menu ;;
        cursor:cycle)   "$SCRIPTS_DIR/cursor.sh" cycle ;;
        cursor:obsidian)  "$SCRIPTS_DIR/cursor.sh" theme obsidian ;;
        cursor:ivory)     "$SCRIPTS_DIR/cursor.sh" theme ivory ;;
        cursor:precision) "$SCRIPTS_DIR/cursor.sh" theme precision ;;
        cursor:dot)       "$SCRIPTS_DIR/cursor.sh" theme dot ;;
        edit:user)      open_in_editor "$HYPR_DIR/modules/user.lua" ;;
        edit:binds)     open_in_editor "$HYPR_DIR/modules/binds.lua" ;;
        edit:waybar)    open_in_editor "$CONFIG_DIR/waybar/config.jsonc" ;;
        sys:reload)
            hyprctl reload >/dev/null 2>&1 || true
            pkill -SIGUSR2 -x waybar 2>/dev/null || true
            notify "Reloaded" "Hyprland config & Waybar refreshed" ;;
        sys:doctor)     "$SCRIPTS_DIR/doctor.sh" --rofi ;;
        sys:lock)       hyprlock >/dev/null 2>&1 & ;;
    esac
}

status_banner() {
    local lstat fstat
    lstat="$("$SCRIPTS_DIR/layout.sh" status 2>/dev/null || echo 'dwindle')"
    fstat="$("$SCRIPTS_DIR/focus.sh" status 2>/dev/null || echo 'off')"
    printf 'layout: %s  ·  focus: %s  ·  tip: type "= 1920*1080" to calculate' "$lstat" "$fstat"
}

cmd_interactive() {
    local banner="${1:-}"
    [ -z "$banner" ] && banner="$(status_banner)"

    local table labels choice tok ans
    table="$(build_entries)"
    labels="$(printf '%s\n' "$table" | cut -f1)"

    choice="$(printf '%s\n' "$labels" | rofi_menu palette -dmenu -i -p "⌘" -mesg "$banner")" || exit 0
    [ -z "$choice" ] && exit 0

    tok="$(awk -F'\t' -v pick="$choice" '$1 == pick { print $2; exit }' <<<"$table")"
    if [ -n "$tok" ]; then
        run_token "$tok"
        exit 0
    fi

    # Not a menu row — try evaluating as a math / hex expression!
    if ans="$(eval_math "$choice" 2>/dev/null)"; then
        if have wl-copy; then
            printf '%s' "$ans" | wl-copy 2>/dev/null || true
        fi
        cmd_interactive "= ${choice#=}  →  ${ans}   (copied to clipboard)"
        exit 0
    fi

    # Otherwise try opening as URL, file path, AppImage, Flatpak ID, or CLI command!
    if [ -x "$SCRIPTS_DIR/open-app.sh" ] && "$SCRIPTS_DIR/open-app.sh" open "$choice" >/dev/null 2>&1; then
        exit 0
    fi

    notify "Command Palette" "No matching command or valid expression: $choice" "dialog-information"
}

case "${1:-open}" in
    open|menu) cmd_interactive ;;
    calc)
        shift
        ans="$(eval_math "$*")" || { echo "invalid expression: $*" >&2; exit 1; }
        printf '%s\n' "$ans" ;;
    list)
        build_entries | cut -f1 ;;
    *)
        echo "usage: palette.sh [open|calc <expr>|list]" >&2
        exit 1 ;;
esac
