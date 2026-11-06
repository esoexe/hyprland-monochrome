#!/usr/bin/env bash
# Monochrome Command Palette (Super + K) — Raycast / Linear style omnibar.
#
# Aggregates open windows across workspaces, focus/Zen sprints, tiling & gap
# presets, screen capture + OCR, wallpaper studio, config quick-edit, and a
# safe Python-AST math / hex / unit calculator (prefix with '=' or type an
# arithmetic expression and press Enter).
#
#   palette.sh              interactive Rofi command palette
#   palette.sh calc "expr"  evaluate a math/hex expression safely
#   palette.sh list         emit the current palette entries to stdout

set -u
. "$(dirname "$0")/lib.sh"

case "${1:-}" in --*) set -- "${1#--}" "${@:2}" ;; esac

eval_math() {
    local raw="${1:-}"
    have python3 || return 1
    python3 - "$raw" <<'PY'
import ast, math, operator, sys

expr = sys.argv[1].strip()
if expr.startswith("="):
    expr = expr[1:].strip()
if not expr or len(expr) > 256:
    sys.exit(1)

OPS = {
    ast.Add: operator.add,
    ast.Sub: operator.sub,
    ast.Mult: operator.mul,
    ast.Div: operator.truediv,
    ast.FloorDiv: operator.floordiv,
    ast.Mod: operator.mod,
    ast.BitOr: operator.or_,
    ast.BitAnd: operator.and_,
    ast.BitXor: operator.xor,
    ast.LShift: operator.lshift,
    ast.RShift: operator.rshift,
}
UNARY = {
    ast.UAdd: operator.pos,
    ast.USub: operator.neg,
    ast.Invert: operator.inv,
}
FUNCS = {
    "sqrt": math.sqrt, "sin": math.sin, "cos": math.cos, "tan": math.tan,
    "log": math.log, "log2": math.log2, "log10": math.log10,
    "ceil": math.ceil, "floor": math.floor, "round": round, "abs": abs,
    "hex": hex, "bin": bin, "oct": oct, "int": int, "float": float,
    "min": min, "max": max,
}
CONSTS = {"pi": math.pi, "e": math.e, "tau": math.tau}

def safe_eval(node):
    if isinstance(node, ast.Expression):
        return safe_eval(node.body)
    if isinstance(node, ast.Constant) and isinstance(node.value, (int, float)):
        return node.value
    if isinstance(node, ast.Name) and node.id in CONSTS:
        return CONSTS[node.id]
    if isinstance(node, ast.UnaryOp) and type(node.op) in UNARY:
        return UNARY[type(node.op)](safe_eval(node.operand))
    if isinstance(node, ast.BinOp):
        if isinstance(node.op, ast.Pow):
            base, exp = safe_eval(node.left), safe_eval(node.right)
            if abs(exp) > 1024:
                raise ValueError("exponent too large")
            return base ** exp
        if type(node.op) in OPS:
            return OPS[type(node.op)](safe_eval(node.left), safe_eval(node.right))
    if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id in FUNCS:
        if node.keywords:
            raise ValueError("keywords not supported")
        args = [safe_eval(a) for a in node.args]
        return FUNCS[node.func.id](*args)
    raise ValueError("unsupported expression")

try:
    tree = ast.parse(expr, mode="eval")
    val = safe_eval(tree)
except Exception:
    sys.exit(1)

if isinstance(val, float):
    if val.is_integer() and abs(val) < 10**15:
        print(int(val))
    else:
        print(f"{val:.8g}")
else:
    print(val)
PY
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

    # 2. Live open windows across all workspaces
    if have hyprctl && have jq; then
        hyprctl clients -j 2>/dev/null | jq -r '
            .[]
            | select(.mapped == true and (.title // "") != "")
            | "◧  Window · \(.title | gsub("[\t\r\n]"; " ") | .[0:52])  [\(.class // "app") · ws \(.workspace.name // "?")]\twin:\(.address)"
        ' 2>/dev/null || true
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
✎  Drawer · Toggle Scratch Markdown Notes (Super + ')	drawer:notes
◰  Drawer · Toggle Quick File Drawer	drawer:files
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
