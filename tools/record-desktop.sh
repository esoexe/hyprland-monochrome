#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════════
#  tools/record-desktop.sh — record a live 1920x1080 Wayland session of this
#  monochrome desktop showcasing 2.0 visuals, Command Palette, Focus/Zen,
#  RESIZE modal submap, pure-monochrome GTK3 Thunar, 10 wallpapers, live
#  Tesseract OCR, inner screen recording, and the 122-check test suite.
#
#      bash tools/record-desktop.sh [output.mp4]
# ═══════════════════════════════════════════════════════════════════════════

set -uo pipefail
REPO="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${1:-$REPO/docs/desktop-demo.mp4}"

for req in sway swaybg waybar rofi foot thunar dunst wf-recorder grim tesseract xdotool ffmpeg zsh starship fastfetch btop; do
    command -v "$req" >/dev/null 2>&1 || {
        printf 'missing required tool: %s\n' "$req" >&2
        exit 1
    }
done

export LANG=C.UTF-8
export LC_ALL=C.UTF-8

WORK="$(mktemp -d)"
cleanup() {
    if [ -f "$WORK/pids" ]; then
        while IFS= read -r p; do
            [ -n "$p" ] && kill "$p" 2>/dev/null || true
        done < "$WORK/pids"
    fi
    rm -rf "$WORK"
}
trap cleanup EXIT

RT="$WORK/rt"
DEMO_HOME="$WORK/home"
DEMO_BIN="$WORK/bin"
mkdir -p "$RT" "$DEMO_HOME" "$DEMO_BIN"
chmod 700 "$RT"

printf '==> Installing hyprland-monochrome into isolated HOME (%s)…\n' "$DEMO_HOME"
(
    cd "$REPO" &&
    HOME="$DEMO_HOME" \
    XDG_CONFIG_HOME="$DEMO_HOME/.config" \
    XDG_CACHE_HOME="$DEMO_HOME/.cache" \
    bash install.sh --yes --no-packages --no-zsh
) >"$WORK/install.log" 2>&1

# Export all 117 real keybinds (including [RESIZE] & [CAPTURE] submaps) from
# docs/KEYBINDS.md into `hyprctl binds -j` format for `menu.sh keybinds`.
python3 - "$REPO/docs/KEYBINDS.md" "$WORK/binds.json" <<'PY'
import json, pathlib, re, sys
md = pathlib.Path(sys.argv[1]).read_text()
out = []
for m in re.finditer(r'^\|\s*`([^`]+)`\s*\|\s*([^|]+?)\s*\|', md, flags=re.M):
    combo, desc = m.group(1).strip(), m.group(2).strip()
    submap = ""
    if combo.startswith("["):
        tag, _, combo = combo.partition("] ")
        submap = tag.lstrip("[")
    parts = [p.strip() for p in combo.split('+')]
    mask = 0
    key = parts[-1] if parts else ""
    for mod in parts[:-1]:
        u = mod.upper()
        if u == "SUPER": mask |= 64
        elif u == "CTRL": mask |= 4
        elif u == "ALT": mask |= 8
        elif u == "SHIFT": mask |= 1
    out.append({"key": key, "keycode": 0, "modmask": mask, "submap": submap, "description": desc})
pathlib.Path(sys.argv[2]).write_text(json.dumps(out))
PY

# Bridge Hyprland IPC calls (clients, binds, activewindow, batch gap keywords)
# to live Sway IPC so palette.sh, focus.sh, layout.sh and screenshot.sh act on
# real live Wayland windows and gaps.
cat > "$DEMO_BIN/hyprctl" <<EOF
#!/usr/bin/env bash
case "\${1:-}" in
    binds) cat "$WORK/binds.json" ;;
    clients)
        swaymsg -t get_tree 2>/dev/null | jq -c '
            [ .. | objects
              | select(.type? == "workspace") as \$ws
              | (\$ws.nodes[]?, \$ws.floating_nodes[]?)
              | recurse(.nodes[]?, .floating_nodes[]?)
              | select(.pid? != null and (.name // "") != "")
              | {address: ("0x" + (.id | tostring)), mapped: true,
                 title: .name, class: (.app_id // .window_properties.class // "app"),
                 workspace: {name: (\$ws.name // "1")}} ]' ;;
    monitors)
        printf '[{"id":0,"name":"HEADLESS-1","width":1920,"height":1080,"refreshRate":60.0,"x":0,"y":0,"scale":1.0,"transform":0,"focused":true}]\n' ;;
    activewindow)
        swaymsg -t get_tree 2>/dev/null | jq -c '
            recurse(.nodes[]?, .floating_nodes[]?) | select(.focused == true) |
            {class: (.app_id // .window_properties.class // "foot"),
             title: (.name // ""), monitor: 0,
             at: [.rect.x, .rect.y], size: [.rect.width, .rect.height]}' | head -1 ;;
    --batch)
        batch="\${2:-}"
        if printf '%s' "\$batch" | grep -q 'togglefloating'; then
             dims="\$(printf '%s' "\$batch" | sed -n 's/.*resizeactive exact \([0-9]*\) \([0-9]*\).*/\1 \2/p')"
            swaymsg "floating enable" >/dev/null 2>&1 || true
            if [ -n "\$dims" ]; then
                swaymsg "resize set \${dims% *} px \${dims#* } px" >/dev/null 2>&1 || true
            fi
        fi
        gi="\$(printf '%s' "\$batch" | sed -n 's/.*gaps_in \([0-9]*\).*/\1/p')"
        go="\$(printf '%s' "\$batch" | sed -n 's/.*gaps_out \([0-9]*\).*/\1/p')"
        if [ -n "\$go" ]; then
            old_go="\$(cat "$WORK/cur_go" 2>/dev/null || echo 10)"
            old_gi="\$(cat "$WORK/cur_gi" 2>/dev/null || echo 5)"
            new_gi="\${gi:-\$old_gi}"
            for step in 1 2 3 4 5 6 7 8 9 10 11 12; do
                cur_o="\$(awk -v a="\$old_go" -v b="\$go" -v s="\$step" 'BEGIN { t=s/12.0; e=1-(1-t)*(1-t)*(1-t); printf "%d", a+(b-a)*e+0.5 }')"
                cur_i="\$(awk -v a="\$old_gi" -v b="\$new_gi" -v s="\$step" 'BEGIN { t=s/12.0; e=1-(1-t)*(1-t)*(1-t); printf "%d", a+(b-a)*e+0.5 }')"
                swaymsg "gaps inner all set \$cur_i; gaps outer all set \$cur_o" >/dev/null 2>&1 || true
                sleep 0.025
            done
            printf '%s\n' "\$go" > "$WORK/cur_go"
            printf '%s\n' "\$new_gi" > "$WORK/cur_gi"
        elif [ -n "\$gi" ]; then
            swaymsg "gaps inner all set \$gi" >/dev/null 2>&1 || true
        fi
        printf 'ok\n' ;;
    dispatch)
        if [ "\${2:-}" = "focuswindow" ]; then
            wid="\${3#address:0x}"
            swaymsg "[con_id=\$wid] focus" >/dev/null 2>&1 || true
        elif [ "\${2:-}" = "movewindowpixel" ]; then
            spec="\${3:-}"
            tx="\$(printf '%s' "\$spec" | awk '{print \$2}')"
            ty="\$(printf '%s' "\$spec" | awk '{sub(/,.*/, "", \$3); print \$3}')"
            read -r sx sy < <(swaymsg -t get_tree 2>/dev/null | jq -r 'recurse(.nodes[]?, .floating_nodes[]?) | select(.focused == true) | "\(.rect.x) \(.rect.y)"' | head -1)
            sx="\${sx:-600}"; sy="\${sy:-240}"
            for step in $(seq 1 16); do
                read -r cx cy < <(awk -v x0="\$sx" -v y0="\$sy" -v x1="\$tx" -v y1="\$ty" -v s="\$step" 'BEGIN {
                    t = s / 16.0;
                    e = 1.0 - exp(-5.2 * t) * cos(3.6 * t);
                    if (s == 16) e = 1.0;
                    printf "%d %d\n", x0 + (x1 - x0) * e + 0.5, y0 + (y1 - y0) * e + 0.5
                }')
                swaymsg "move position \$cx px \$cy px" >/dev/null 2>&1 || true
                sleep 0.024
            done
        fi
        printf 'ok\n' ;;
    version) printf '{"tag":"v0.55.2"}\n' ;;
    configerrors) printf '\n' ;;
    *) printf '{}\n' ;;
esac
EOF

cat > "$DEMO_BIN/awww-daemon" <<'EOF'
#!/usr/bin/env bash
exec sleep 3600
EOF

cat > "$DEMO_BIN/awww" <<EOF
#!/usr/bin/env bash
if [ "\${1:-}" = "img" ]; then
    img=""
    for arg in "\${@:2}"; do
        if [ -f "\$arg" ]; then img="\$arg"; break; fi
    done
    [ -n "\$img" ] || exit 0
    case "\$img" in
        *.gif)
            scaled="$WORK/bg-\$(basename "\$img" .gif).png"
            [ -f "\$scaled" ] || magick "\${img}[0]" -resize 1920x1080 "\$scaled" 2>/dev/null || cp "\$img" "\$scaled"
            img="\$scaled" ;;
        *16k*|*4k*)
            scaled="$WORK/bg-\$(basename "\$img")"
            [ -f "\$scaled" ] || magick "\$img" -resize 1920x1080 -quality 92 "\$scaled" 2>/dev/null || cp "\$img" "\$scaled"
            img="\$scaled" ;;
    esac
    old_pid="\$(cat "$WORK/swaybg.pid" 2>/dev/null || true)"
    swaybg -i "\$img" -m fill >/dev/null 2>&1 </dev/null &
    new_pid=\$!
    printf '%s' "\$new_pid" > "$WORK/swaybg.pid"
    printf '%s\n' "\$new_pid" >> "$WORK/pids"
    sleep 0.35
    [ -n "\$old_pid" ] && kill "\$old_pid" 2>/dev/null || true
    exit 0
fi
exit 0
EOF

printf '65 0\n' > "$WORK/vol"
cat > "$DEMO_BIN/wpctl" <<EOF
#!/usr/bin/env bash
read -r v m < "$WORK/vol"
case "\${1:-}" in
    get-volume)
        awk -v v="\$v" -v m="\$m" 'BEGIN {
            printf "Volume: %.2f%s\n", v / 100.0, (m == 1 ? " [MUTED]" : "")
        }' ;;
    set-volume)
        for a in "\$@"; do
            case "\$a" in
                *%+) v=\$(( v + 5 )); [ "\$v" -gt 100 ] && v=100 ;;
                *%-) v=\$(( v - 5 )); [ "\$v" -lt 0 ] && v=0 ;;
            esac
        done
        printf '%s %s\n' "\$v" "\$m" > "$WORK/vol" ;;
    set-mute)
        m=\$(( 1 - m ))
        printf '%s %s\n' "\$v" "\$m" > "$WORK/vol" ;;
    status) printf 'Sinks:\n │  * 42. Built-in Audio Stereo [vol: 0.%s]\n' "\$v" ;;
    inspect) printf 'node.description = "Built-in Audio Stereo"\n' ;;
esac
EOF
chmod +x "$DEMO_BIN/hyprctl" "$DEMO_BIN/awww" "$DEMO_BIN/awww-daemon" "$DEMO_BIN/wpctl"

# Adapt the throw-away copy of Waybar's config to Sway workspace/mode/window IPC
python3 - "$DEMO_HOME/.config/waybar/config.jsonc" "$DEMO_HOME/.config/waybar/style.css" <<'PY'
import pathlib, sys
cfg = pathlib.Path(sys.argv[1])
s = cfg.read_text()
s = s.replace('"hyprland/workspaces"', '"sway/workspaces"')
s = s.replace('"hyprland/submap"', '"sway/mode"')
s = s.replace('"hyprland/window"', '"sway/window"')
s = s.replace('"separate-outputs": true', '"max-length": 70')
for mod in ('"temperature",\n', '"bluetooth",\n', '"battery",\n', '"custom/updates",\n'):
    s = s.replace('        ' + mod, '')
cfg.write_text(s)

css = pathlib.Path(sys.argv[2])
text = css.read_text()
text = text.replace('#workspaces button.active {', '#workspaces button.active,\n#workspaces button.focused {')
text = text.replace('#submap {', '#submap,\n#mode {')
css.write_text(text)
PY

# Clean, curated .desktop entries for Rofi drun
mkdir -p "$DEMO_HOME/.local/share/applications"
for spec in \
    "palette|Command Palette (Super + Shift + P)|~/.config/hypr/scripts/palette.sh" \
    "focus|Deep-Work Focus & Zen (Super + Z)|~/.config/hypr/scripts/focus.sh menu" \
    "kitty|Kitty Terminal|kitty" \
    "thunar|Thunar File Manager|thunar" \
    "btop|btop++ System Monitor|foot -e btop" \
    "wallpapers|Wallpaper Studio (Super + W)|~/.config/hypr/scripts/wallpaper.sh" \
    "recorder|Screen Recorder (Super + Alt + Print)|~/.config/hypr/scripts/record.sh toggle" \
    "keybinds|Interactive Keybinds (Super + /)|~/.config/hypr/scripts/menu.sh keybinds" \
    "settings|Monochrome Settings (Super + I)|~/.config/hypr/scripts/settings.sh"
do
    IFS='|' read -r id name exec_cmd <<<"$spec"
    cat > "$DEMO_HOME/.local/share/applications/${id}.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=${name}
Exec=${exec_cmd}
Terminal=false
Categories=Utility;
EOF
done

# Pre-warm wallpaper thumbnails so opening the Rofi 3x2 gallery on camera is instant
mkdir -p "$DEMO_HOME/.cache/hyprland-monochrome/wallpaper-thumbs"
for img in "$DEMO_HOME/Pictures/Wallpapers"/*.jpg; do
    [ -f "$img" ] || continue
    h="$(printf '%s' "$img" | md5sum | cut -d' ' -f1)"
    magick "$img" -thumbnail '640x360^' -gravity center -extent 640x360 -quality 82 \
        "$DEMO_HOME/.cache/hyprland-monochrome/wallpaper-thumbs/${h}.jpg" 2>/dev/null || true
done

cat > "$WORK/sway.cfg" <<'EOF'
output HEADLESS-1 resolution 1920x1080 position 0,0
xwayland enable
default_border pixel 2
default_floating_border pixel 2
gaps inner 6
gaps outer 12
smart_gaps off
client.focused          #e6e6e6 #0d0d0d #e6e6e6 #ffffff   #e6e6e6
client.focused_inactive #2a2a2a #0a0a0a #8a8a8a #2a2a2a   #2a2a2a
client.unfocused        #2a2a2a #0a0a0a #6f6f6f #2a2a2a   #2a2a2a
for_window [app_id="thunar"] floating enable, resize set 1000 610, move position 860 220
mode "RESIZE" {
    bindsym Escape mode "default"
}
EOF

cat > "$WORK/foot.ini" <<'EOF'
font=JetBrainsMono Nerd Font:size=10.5
pad=16x14
[colors]
alpha=0.93
background=0a0a0a
foreground=e6e6e6
regular0=141414
regular1=9a9a9a
regular2=b8b8b8
regular3=c8c8c8
regular4=8a8a8a
regular5=a8a8a8
regular6=bdbdbd
regular7=d6d6d6
bright0=6e6e6e
bright1=c8c8c8
bright2=dcdcdc
bright3=e4e4e4
bright4=bcbcbc
bright5=cccccc
bright6=dadada
bright7=ffffff
EOF

cat > "$WORK/scene3.sh" <<'EOF'
#!/usr/bin/env bash
B=$'\033[1m'; D=$'\033[2m'; G=$'\033[32m'; R=$'\033[0m'
show_cmd() { printf '\n%smono%s %s$%s %s%s%s\n' "$D" "$R" "$G" "$R" "$B" "$1" "$R"; sleep 0.35; }

printf '%s▎ 3.0 Studio Edition · Doctor, Sessions, 14 Wallpapers, OCR & Zen%s\n' "$B" "$R"
sleep 0.5

show_cmd '~/.config/hypr/scripts/doctor.sh   # 7/7 monochrome toolkits audited'
~/.config/hypr/scripts/doctor.sh
# Wait while 3x2 Wallpaper Studio switches across Singularity & Liquid Mercury
while [ ! -f "$WORK/wall.done" ]; do sleep 0.2; done
sleep 0.4

show_cmd '~/.config/hypr/scripts/workspace-session.sh save studio && ~/.config/hypr/scripts/workspace-session.sh list'
~/.config/hypr/scripts/workspace-session.sh save studio
~/.config/hypr/scripts/workspace-session.sh list
sleep 0.9

show_cmd 'MONO_SLURP_GEOM="20,60 780x360" ~/.config/hypr/scripts/screenshot.sh ocr'
MONO_SLURP_GEOM="20,60 780x360" ~/.config/hypr/scripts/screenshot.sh ocr | head -4
sleep 1.0

show_cmd '~/.config/hypr/scripts/focus.sh zen   # hides bar & frames workspace (28px)'
~/.config/hypr/scripts/focus.sh zen
sleep 1.8
show_cmd '~/.config/hypr/scripts/focus.sh zen   # restores bar, gaps & Pomodoro pill'
~/.config/hypr/scripts/focus.sh zen
sleep 1.2

show_cmd '~/.config/hypr/scripts/record.sh start screen   # watch ● timer beside ◎ Focus!'
~/.config/hypr/scripts/record.sh start screen
sleep 1.5
show_cmd '~/.config/hypr/scripts/record.sh status'
~/.config/hypr/scripts/record.sh status | sed "s|$HOME|~|g"
sleep 1.6
show_cmd '~/.config/hypr/scripts/record.sh stop && ffprobe ~/Videos/Recordings/rec-*.mp4'
~/.config/hypr/scripts/record.sh stop
ffprobe -v error -show_entries stream=codec_name,width,height,nb_frames:format=duration "$HOME"/Videos/Recordings/rec-*.mp4
sleep 2.0
touch "$WORK/s3.done"
exec sleep 60
EOF

cat > "$WORK/scene4.sh" <<'EOF'
#!/usr/bin/env bash
B=$'\033[1m'; D=$'\033[2m'; G=$'\033[32m'; C=$'\033[36m'; R=$'\033[0m'
printf '%s▎ 1/2 · Installer, backups, --link & non-Arch sandbox (test/install-sandbox.sh)%s\n' "$B$C" "$R"
printf '%smono%s %s$%s %sbash test/install-sandbox.sh%s\n\n' "$D" "$R" "$G" "$R" "$B" "$R"
sleep 0.35
bash test/install-sandbox.sh | sed "s|/tmp/tmp\.[^/ ]*|/tmp/sandbox|g"
sleep 1.5

printf '\n%s▎ 2/2 · Full repository verification — 9 suites / 161 smoke checks (test/run-all.sh)%s\n' "$B$C" "$R"
printf '%smono%s %s$%s %sbash test/run-all.sh%s\n' "$D" "$R" "$G" "$R" "$B" "$R"
sleep 0.35
bash test/run-all.sh
touch "$WORK/s4.done"
exec sleep 60
EOF
chmod +x "$WORK/scene3.sh" "$WORK/scene4.sh"

RAW_MKV="$WORK/raw.mkv"

printf '==> Booting headless 1920x1080 Wayland session and recording 4.0 Kinetic walkthrough…\n'

# Note: GTK_THEME is intentionally NOT set so GTK3 reads ~/.config/gtk-3.0/settings.ini
# AND layers our monochrome ~/.config/gtk-3.0/gtk.css override cleanly on top!
HOME="$DEMO_HOME" \
XDG_CONFIG_HOME="$DEMO_HOME/.config" \
XDG_CACHE_HOME="$DEMO_HOME/.cache" \
XDG_DATA_HOME="$DEMO_HOME/.local/share" \
XDG_DATA_DIRS="$DEMO_HOME/.local/share:/usr/share" \
XDG_RUNTIME_DIR="$RT" \
GIO_USE_VFS=local \
PATH="$DEMO_BIN:$PATH" \
WLR_BACKENDS=headless \
WLR_RENDERER=pixman \
WLR_HEADLESS_OUTPUTS=1 \
REPO="$REPO" \
WORK="$WORK" \
RAW_MKV="$RAW_MKV" \
dbus-run-session -- bash -c '
    set -u
    export WORK
    sway -c "$WORK/sway.cfg" >"$WORK/sway.log" 2>&1 </dev/null &
    SWAY_PID=$!; printf "%s\n" "$SWAY_PID" >> "$WORK/pids"
    for _ in $(seq 1 40); do [ -S "$XDG_RUNTIME_DIR/wayland-1" ] && break; sleep 0.1; done
    export WAYLAND_DISPLAY=wayland-1
    SWAYSOCK="$(find "$XDG_RUNTIME_DIR" -maxdepth 1 -name "sway-ipc.*.sock" | head -1)"
    export SWAYSOCK
    export DISPLAY=:0

    # Start on the new 4K Gravitational Singularity design (mono-11-4k.jpg) + Waybar + Dunst
    ~/.config/hypr/scripts/wallpaper.sh set "$HOME/Pictures/Wallpapers/mono-11-4k.jpg" >/dev/null 2>&1
    waybar >"$WORK/waybar.log" 2>&1 </dev/null &
    printf "%s\n" "$!" >> "$WORK/pids"
    dunst -config ~/.config/dunst/dunstrc >"$WORK/dunst.log" 2>&1 </dev/null &
    printf "%s\n" "$!" >> "$WORK/pids"
    sleep 1.2

    # Start outer full-session recorder into Matroska (stream-safe, never loses moov)
    wf-recorder -y -D -r 30 -f "$RAW_MKV" >"$WORK/outer-rec.log" 2>&1 </dev/null &
    OUTER_PID=$!
    sleep 0.6

    wait_rofi() {
        for _ in $(seq 1 25); do
            xdotool search --onlyvisible --class "Rofi" >/dev/null 2>&1 && return 0
            sleep 0.1
        done
    }

    # ── Scene 1: Workspace 1 — Zsh + Starship + Monochrome Neovim + btop + Pomodoro ──
    ~/.config/hypr/scripts/focus.sh start 25 "Deep Work" >/dev/null 2>&1
    foot -c "$WORK/foot.ini" -D "$REPO" -e zsh >/dev/null 2>&1 </dev/null &
    W1_T1=$!; printf "%s\n" "$W1_T1" >> "$WORK/pids"
    sleep 1.4

    if command -v nvim >/dev/null 2>&1; then
        foot -c "$WORK/foot.ini" -D "$REPO" -e nvim -R .config/hypr/modules/smart.lua >/dev/null 2>&1 </dev/null &
        W1_NV=$!
        sleep 2.3
        kill "$W1_NV" 2>/dev/null || true
        sleep 0.3
    fi

    foot -c "$WORK/foot.ini" -e btop >/dev/null 2>&1 </dev/null &
    W1_T2=$!; printf "%s\n" "$W1_T2" >> "$WORK/pids"
    sleep 2.0

    # ── Scene 2: Command Palette (Super+Shift+P), Live Math, Keybinds & RESIZE ──
    ~/.config/hypr/scripts/palette.sh >/dev/null 2>&1 </dev/null &
    RF=$!
    wait_rofi; sleep 0.9
    xdotool key Down; sleep 0.35
    xdotool key Down; sleep 0.35
    xdotool type --delay 65 "= (15360*8640)/1e6"
    sleep 0.9
    xdotool key Return
    sleep 0.4
    wait_rofi; sleep 1.6
    xdotool key Escape
    wait "$RF" 2>/dev/null || kill "$RF" 2>/dev/null || true
    sleep 0.5

    # Motion Physics Menu (Super + M -> M) & GLSL Shader Studio (Super + M -> S)
    ~/.config/hypr/scripts/motion.sh menu >/dev/null 2>&1 </dev/null &
    RF=$!
    wait_rofi; sleep 0.85
    xdotool key Down; sleep 0.4
    xdotool key Down; sleep 0.65
    xdotool key Escape
    wait "$RF" 2>/dev/null || kill "$RF" 2>/dev/null || true
    sleep 0.35

    ~/.config/hypr/scripts/shader.sh menu >/dev/null 2>&1 </dev/null &
    RF=$!
    wait_rofi; sleep 0.85
    xdotool key Down; sleep 0.4
    xdotool key Down; sleep 0.65
    xdotool key Escape
    wait "$RF" 2>/dev/null || kill "$RF" 2>/dev/null || true
    sleep 0.35

    # Smooth 30fps modal RESIZE glide -> Waybar lights up [RESIZE] pill!
    swaymsg "mode RESIZE" >/dev/null
    sleep 0.35
    for _ in $(seq 1 12); do swaymsg "resize grow width 9 px" >/dev/null; sleep 0.03; done
    sleep 0.35
    for _ in $(seq 1 12); do swaymsg "resize shrink width 9 px" >/dev/null; sleep 0.03; done
    swaymsg "mode default" >/dev/null
    sleep 0.35

    # Kinetic PiP 4-Corner Damped-Spring Glide (Super + .) across corners!
    ~/.config/hypr/scripts/pip-corner.sh 1 >/dev/null 2>&1
    sleep 0.35
    ~/.config/hypr/scripts/pip-corner.sh 2 >/dev/null 2>&1
    sleep 0.35
    ~/.config/hypr/scripts/pip-corner.sh 3 >/dev/null 2>&1
    sleep 0.35
    swaymsg "floating disable" >/dev/null 2>&1 || true
    sleep 0.4

    # ── Scene 3: Workspace 2 — Monochrome GTK3 Thunar, 10 Wallpapers, OCR & Zen ──
    swaymsg "workspace number 2" >/dev/null
    sleep 0.35
    notify-send -a mono "Workspace 2" "Studio Wallpapers · Monochrome GTK · OCR · Zen Mode"

    MONO_NO_BANNER=1 foot -c "$WORK/foot.ini" -D "$REPO" -e "$WORK/scene3.sh" >/dev/null 2>&1 </dev/null &
    W2_T1=$!; printf "%s\n" "$W2_T1" >> "$WORK/pids"
    sleep 1.0

    DBUS_SYSTEM_BUS_ADDRESS=unix:path=/dev/null thunar "$HOME/Pictures/Wallpapers" >/dev/null 2>&1 </dev/null &
    W2_TH=$!; printf "%s\n" "$W2_TH" >> "$WORK/pids"
    for _ in $(seq 1 30); do
        swaymsg -t get_tree 2>/dev/null | grep -qi thunar && break
        sleep 0.2
    done
    # Wait briefly for Thunar tumbler thumbnails to finish rendering cleanly
    sleep 3.2

    # Open the 3x2 Wallpaper Studio, pan across mono-07..mono-10, and apply mono-10!
    ~/.config/hypr/scripts/wallpaper.sh pick >/dev/null 2>&1 </dev/null &
    RF=$!
    wait_rofi; sleep 0.9
    xdotool key Right; sleep 0.45
    xdotool key Right; sleep 0.45
    xdotool key Right; sleep 0.55
    xdotool key Right; sleep 0.55
    xdotool key Down;  sleep 0.65
    xdotool key Return
    wait "$RF" 2>/dev/null || true
    sleep 1.2

    # Close Thunar window so Liquid Mercury & Zen Mode framing shine unobstructed!
    swaymsg "[app_id=thunar] kill" >/dev/null 2>&1 || kill "$W2_TH" 2>/dev/null || true
    sleep 0.8
    ~/.config/hypr/scripts/wallpaper.sh set "$HOME/Pictures/Wallpapers/mono-12.jpg" >/dev/null 2>&1
    sleep 0.8
    touch "$WORK/wall.done"

    while [ ! -f "$WORK/s3.done" ]; do sleep 0.2; done

    # ── Scene 4: Workspace 3 — Full Automated 161-Check Verification Live ────
    swaymsg "workspace number 3" >/dev/null
    sleep 0.35
    ~/.config/hypr/scripts/wallpaper.sh set "$HOME/Pictures/Wallpapers/mono-14.jpg" >/dev/null 2>&1
    notify-send -a mono "Workspace 3 · Verification" "Running install-sandbox.sh & 161-check run-all.sh"

    MONO_NO_BANNER=1 foot -c "$WORK/foot.ini" -D "$REPO" -e "$WORK/scene4.sh" >/dev/null 2>&1 </dev/null &
    W3_T1=$!; printf "%s\n" "$W3_T1" >> "$WORK/pids"

    while [ ! -f "$WORK/s4.done" ]; do sleep 0.25; done
    sleep 2.0

    # Finale: Settings Menu (Super + I) showing new Command Palette, Focus & Layout entries
    ~/.config/hypr/scripts/settings.sh >/dev/null 2>&1 </dev/null &
    RF=$!
    wait_rofi; sleep 0.9
    xdotool key Down; sleep 0.4
    xdotool key Down; sleep 0.8
    xdotool key Escape
    wait "$RF" 2>/dev/null || kill "$RF" 2>/dev/null || true
    sleep 1.6

    # Stop outer wf-recorder cleanly via SIGINT and wait for full encoder flush
    kill -INT "$OUTER_PID" 2>/dev/null || true
    for _ in $(seq 1 120); do kill -0 "$OUTER_PID" 2>/dev/null || break; sleep 0.1; done

    kill "$W1_T1" "$W1_T2" "$W2_T1" "$W3_T1" "$SWAY_PID" 2>/dev/null || true
'

printf '==> Optimizing MP4 for universal browser playback (%s)…\n' "$OUT"
mkdir -p "$(dirname "$OUT")"
ffmpeg -y -loglevel error -i "$RAW_MKV" \
    -c:v libx264 -preset medium -crf 22 -pix_fmt yuv420p -movflags +faststart \
    "$OUT"

ls -lh "$OUT"
ffprobe -v error -show_entries format=duration,size:stream=codec_name,profile,pix_fmt,width,height,r_frame_rate "$OUT"
