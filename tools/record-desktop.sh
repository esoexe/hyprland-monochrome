#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════════
#  tools/record-desktop.sh — record a live 1920x1080 Wayland session of this
#  monochrome desktop while using its menus, wallpapers, recorder, and tests.
#
#      bash tools/record-desktop.sh [output.mp4]
#
#  Why Sway headless here? Hyprland's Aquamarine backend requires a /dev/dri
#  GPU render node, which headless CI/containers lack. wlroots's headless +
#  pixman backend needs no GPU at all, so we boot a real 1920x1080 Wayland
#  session and run the repository's actual Waybar, Rofi (.rasi themes), Dunst,
#  Thunar (dark GTK3), Zsh + Starship + fastfetch + btop, wallpapers, grim
#  screenshots, inner wf-recorder via record.sh, and the full test suite.
# ═══════════════════════════════════════════════════════════════════════════

set -uo pipefail
REPO="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${1:-$REPO/docs/desktop-demo.mp4}"

for req in sway swaybg waybar rofi foot thunar dunst wf-recorder grim xdotool ffmpeg zsh starship fastfetch btop; do
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

# Export all 82 real keybinds from docs/KEYBINDS.md (generated from binds.lua)
# into `hyprctl binds -j` format so `menu.sh keybinds` shows the real table.
python3 - "$REPO/docs/KEYBINDS.md" "$WORK/binds.json" <<'PY'
import json, pathlib, re, sys
md = pathlib.Path(sys.argv[1]).read_text()
out = []
for m in re.finditer(r'^\|\s*`([^`]+)`\s*\|\s*([^|]+?)\s*\|', md, flags=re.M):
    combo, desc = m.group(1).strip(), m.group(2).strip()
    parts = [p.strip() for p in combo.split('+')]
    mask = 0
    key = parts[-1] if parts else ""
    for mod in parts[:-1]:
        u = mod.upper()
        if u == "SUPER": mask |= 64
        elif u == "CTRL": mask |= 4
        elif u == "ALT": mask |= 8
        elif u == "SHIFT": mask |= 1
    out.append({"key": key, "keycode": 0, "modmask": mask, "description": desc})
pathlib.Path(sys.argv[2]).write_text(json.dumps(out))
PY

# Compatibility bridges for compositor-specific IPC inside the headless session
cat > "$DEMO_BIN/hyprctl" <<EOF
#!/usr/bin/env bash
case "\${1:-}" in
    binds) cat "$WORK/binds.json" ;;
    monitors)
        printf '[{"id":0,"name":"HEADLESS-1","width":1920,"height":1080,"refreshRate":60.0,"x":0,"y":0,"scale":1.0,"transform":0,"focused":true}]\n' ;;
    activewindow)
        swaymsg -t get_tree 2>/dev/null | jq -c '
            recurse(.nodes[]?, .floating_nodes[]?) | select(.focused == true) |
            {class: (.app_id // .window_properties.class // "foot"),
             title: (.name // ""), monitor: 0,
             at: [.rect.x, .rect.y], size: [.rect.width, .rect.height]}' | head -1 ;;
    version) printf 'Hyprland 0.55.2 (headless Wayland harness)\n' ;;
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
if [ "\${1:-}" = "img" ] && [ -f "\${2:-}" ]; then
    img="\$2"
    old_pid="\$(cat "$WORK/swaybg.pid" 2>/dev/null || true)"
    swaybg -i "\$img" -m fill >/dev/null 2>&1 </dev/null &
    new_pid=\$!
    printf '%s' "\$new_pid" > "$WORK/swaybg.pid"
    printf '%s\n' "\$new_pid" >> "$WORK/pids"
    case "\$img" in *16k*) sleep 1.4 ;; *) sleep 0.35 ;; esac
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

# Adapt the throw-away copy of Waybar's config to Sway workspace/window IPC
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
css.write_text(css.read_text().replace(
    '#workspaces button.active {',
    '#workspaces button.active,\n#workspaces button.focused {'
))
PY

# Clean, curated .desktop entries so the Rofi app launcher lists desktop apps
mkdir -p "$DEMO_HOME/.local/share/applications"
for spec in \
    "kitty|Kitty Terminal|kitty" \
    "thunar|Thunar File Manager|thunar" \
    "btop|btop++ System Monitor|foot -e btop" \
    "wallpapers|Wallpaper Picker (Super + W)|~/.config/hypr/scripts/wallpaper.sh" \
    "recorder|Screen Recorder (Super + Alt + Print)|~/.config/hypr/scripts/record.sh toggle" \
    "keybinds|Keybind Cheat Sheet (Super + /)|~/.config/hypr/scripts/menu.sh keybinds" \
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

# Pre-warm wallpaper thumbnails so opening the Rofi 3x2 grid on camera is instant
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
for_window [app_id="thunar"] floating enable, resize set 1020 620, move position 840 220
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
show_cmd() { printf '\n%smono%s %s$%s %s%s%s\n' "$D" "$R" "$G" "$R" "$B" "$1" "$R"; sleep 0.4; }

printf '%s▎ Live desktop actions & screen-recording test%s\n' "$B" "$R"
sleep 0.9

show_cmd 'python3 -c "from PIL import Image; ... open(\"~/Pictures/Wallpapers/mono-06-16k.jpg\")"'
python3 -c '
import os; from PIL import Image; Image.MAX_IMAGE_PIXELS = None
im = Image.open(os.path.expanduser("~/Pictures/Wallpapers/mono-06-16k.jpg"))
print(f"mono-06-16k.jpg -> {im.size[0]}x{im.size[1]} ({im.size[0]*im.size[1]/1e6:.1f} MP native render)")
'
# Wait while Wallpaper Picker 3x2 grid opens and cycles wallpapers
while [ ! -f "$WORK/wall.done" ]; do sleep 0.2; done
sleep 0.4

show_cmd '~/.config/hypr/scripts/volume.sh up'
~/.config/hypr/scripts/volume.sh up
sleep 0.55
show_cmd '~/.config/hypr/scripts/volume.sh up'
~/.config/hypr/scripts/volume.sh up
sleep 0.7

show_cmd '~/.config/hypr/scripts/screenshot.sh full && ls -lh ~/Pictures/Screenshots/'
~/.config/hypr/scripts/screenshot.sh full
ls -lh "$HOME/Pictures/Screenshots/" | tail -1
sleep 0.9

show_cmd '~/.config/hypr/scripts/record.sh start screen   # watch Waybar top-right ● timer!'
~/.config/hypr/scripts/record.sh start screen
sleep 1.3
show_cmd '~/.config/hypr/scripts/record.sh status'
~/.config/hypr/scripts/record.sh status | sed "s|$HOME|~|g"
sleep 2.0
show_cmd '~/.config/hypr/scripts/record.sh stop'
~/.config/hypr/scripts/record.sh stop
sleep 0.6
show_cmd 'ffprobe ~/Videos/Recordings/rec-*.mp4'
ls -lh "$HOME/Videos/Recordings/" | tail -1
ffprobe -v error -show_entries stream=codec_name,width,height,nb_frames:format=duration "$HOME"/Videos/Recordings/rec-*.mp4
sleep 2.4
touch "$WORK/s3.done"
exec sleep 60
EOF

cat > "$WORK/scene4.sh" <<'EOF'
#!/usr/bin/env bash
B=$'\033[1m'; D=$'\033[2m'; G=$'\033[32m'; C=$'\033[36m'; R=$'\033[0m'
printf '%s▎ 1/2 · Installer, backups, --link & non-Arch sandbox (test/install-sandbox.sh)%s\n' "$B$C" "$R"
printf '%smono%s %s$%s %sbash test/install-sandbox.sh%s\n\n' "$D" "$R" "$G" "$R" "$B" "$R"
sleep 0.4
bash test/install-sandbox.sh | sed "s|/tmp/tmp\.[^/ ]*|/tmp/sandbox|g"
sleep 1.8

printf '\n%s▎ 2/2 · Full repository verification suite — 9 checks / 100 script checks (test/run-all.sh)%s\n' "$B$C" "$R"
printf '%smono%s %s$%s %sbash test/run-all.sh%s\n' "$D" "$R" "$G" "$R" "$B" "$R"
sleep 0.4
bash test/run-all.sh
touch "$WORK/s4.done"
exec sleep 60
EOF
chmod +x "$WORK/scene3.sh" "$WORK/scene4.sh"

RAW_MP4="$WORK/raw.mp4"

printf '==> Booting headless 1920x1080 Wayland session and recording walkthrough…\n'

HOME="$DEMO_HOME" \
XDG_CONFIG_HOME="$DEMO_HOME/.config" \
XDG_CACHE_HOME="$DEMO_HOME/.cache" \
XDG_DATA_HOME="$DEMO_HOME/.local/share" \
XDG_DATA_DIRS="$DEMO_HOME/.local/share:/usr/share" \
XDG_RUNTIME_DIR="$RT" \
GIO_USE_VFS=local \
GTK_THEME=Adwaita:dark \
PATH="$DEMO_BIN:$PATH" \
WLR_BACKENDS=headless \
WLR_RENDERER=pixman \
WLR_HEADLESS_OUTPUTS=1 \
REPO="$REPO" \
WORK="$WORK" \
RAW_MP4="$RAW_MP4" \
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

    # Wallpaper (start on the 16K layered waves) + Waybar + Dunst
    ~/.config/hypr/scripts/wallpaper.sh set "$HOME/Pictures/Wallpapers/mono-06-16k.jpg" >/dev/null 2>&1
    waybar >"$WORK/waybar.log" 2>&1 </dev/null &
    printf "%s\n" "$!" >> "$WORK/pids"
    dunst -config ~/.config/dunst/dunstrc >"$WORK/dunst.log" 2>&1 </dev/null &
    printf "%s\n" "$!" >> "$WORK/pids"
    sleep 1.2

    # Start outer full-session recorder (30 fps constant frame rate)
    wf-recorder -y -D -r 30 -f "$RAW_MP4" >"$WORK/outer-rec.log" 2>&1 </dev/null &
    OUTER_PID=$!
    sleep 0.6

    wait_rofi() {
        for _ in $(seq 1 25); do
            xdotool search --onlyvisible --class "Rofi" >/dev/null 2>&1 && return 0
            sleep 0.1
        done
    }

    # ── Scene 1: Workspace 1 — Zsh + Starship + fastfetch + btop ──────────
    notify-send -a mono "hyprland-monochrome" "1 · Shell (Zsh + Starship + fastfetch) & btop"
    foot -c "$WORK/foot.ini" -D "$REPO" -e zsh >/dev/null 2>&1 </dev/null &
    W1_T1=$!; printf "%s\n" "$W1_T1" >> "$WORK/pids"
    sleep 1.8

    foot -c "$WORK/foot.ini" -e btop >/dev/null 2>&1 </dev/null &
    W1_T2=$!; printf "%s\n" "$W1_T2" >> "$WORK/pids"
    sleep 2.6

    # ── Scene 2: Rofi menus — Launcher, Keybind Cheat Sheet, Settings ─────
    notify-send -a mono "Rofi Menus" "2 · App launcher, live keybinds & settings"
    ~/.config/hypr/scripts/menu.sh launcher >/dev/null 2>&1 </dev/null &
    RF=$!
    wait_rofi; sleep 0.7
    xdotool key Down; sleep 0.35
    xdotool key Down; sleep 0.35
    xdotool type --delay 85 "wall"
    sleep 1.1
    xdotool key Escape
    wait "$RF" 2>/dev/null || kill "$RF" 2>/dev/null || true
    sleep 0.5

    # Searchable keybind cheat sheet (82 live binds from modules/binds.lua)
    ~/.config/hypr/scripts/menu.sh keybinds >/dev/null 2>&1 </dev/null &
    RF=$!
    wait_rofi; sleep 0.8
    xdotool key Down; sleep 0.3
    xdotool key Down; sleep 0.3
    xdotool type --delay 90 "record"
    sleep 1.5
    xdotool key Down; sleep 0.5
    xdotool key Escape
    wait "$RF" 2>/dev/null || kill "$RF" 2>/dev/null || true
    sleep 0.5

    # Settings menu (Super + I)
    ~/.config/hypr/scripts/settings.sh >/dev/null 2>&1 </dev/null &
    RF=$!
    wait_rofi; sleep 0.8
    xdotool key Down; sleep 0.35
    xdotool key Down; sleep 0.35
    xdotool key Down; sleep 0.55
    xdotool key Escape
    wait "$RF" 2>/dev/null || kill "$RF" 2>/dev/null || true
    sleep 0.6

    # ── Scene 3: Workspace 2 — Thunar, Wallpaper Grid, Screenshots & REC ──
    swaymsg "workspace number 2" >/dev/null
    sleep 0.4
    notify-send -a mono "Workspace 2" "3 · Wallpapers (incl. 16K), Thunar, OSD & record.sh"

    # Left terminal showing interactive commands as they execute
    MONO_NO_BANNER=1 foot -c "$WORK/foot.ini" -D "$REPO" -e "$WORK/scene3.sh" >/dev/null 2>&1 </dev/null &
    W2_T1=$!; printf "%s\n" "$W2_T1" >> "$WORK/pids"
    sleep 1.0

    thunar "$HOME/Pictures/Wallpapers" >/dev/null 2>&1 </dev/null &
    W2_TH=$!; printf "%s\n" "$W2_TH" >> "$WORK/pids"
    sleep 1.6

    # Open the 3x2 Wallpaper Picker thumbnail grid, pick mono-03.jpg, then restore 16K!
    ~/.config/hypr/scripts/wallpaper.sh pick >/dev/null 2>&1 </dev/null &
    RF=$!
    wait_rofi; sleep 1.1
    xdotool key Right; sleep 0.65
    xdotool key Right; sleep 0.65
    xdotool key Down;  sleep 0.65
    xdotool key Up;    sleep 0.55
    xdotool key Return
    wait "$RF" 2>/dev/null || true
    sleep 1.4
    ~/.config/hypr/scripts/wallpaper.sh set "$HOME/Pictures/Wallpapers/mono-06-16k.jpg" >/dev/null 2>&1
    touch "$WORK/wall.done"

    # Let the left terminal finish volume OSD + grim screenshot + inner record.sh test
    while [ ! -f "$WORK/s3.done" ]; do sleep 0.2; done

    # ── Scene 4: Workspace 3 — Full Automated Test Suite Live ─────────────
    swaymsg "workspace number 3" >/dev/null
    sleep 0.4
    notify-send -a mono "Workspace 3 · Verification" "4 · Running install-sandbox.sh & run-all.sh live"

    MONO_NO_BANNER=1 foot -c "$WORK/foot.ini" -D "$REPO" -e "$WORK/scene4.sh" >/dev/null 2>&1 </dev/null &
    W3_T1=$!; printf "%s\n" "$W3_T1" >> "$WORK/pids"

    while [ ! -f "$WORK/s4.done" ]; do sleep 0.25; done
    sleep 2.2

    # Brief finale: Power Menu (Super + Escape) over the passing test results
    ~/.config/hypr/scripts/powermenu.sh >/dev/null 2>&1 </dev/null &
    RF=$!
    wait_rofi; sleep 0.8
    xdotool key Down; sleep 0.4
    xdotool key Down; sleep 0.8
    xdotool key Escape
    wait "$RF" 2>/dev/null || kill "$RF" 2>/dev/null || true
    sleep 2.0

    # Stop outer wf-recorder cleanly via SIGINT
    kill -INT "$OUTER_PID" 2>/dev/null || true
    for _ in $(seq 1 40); do kill -0 "$OUTER_PID" 2>/dev/null || break; sleep 0.1; done

    kill "$W1_T1" "$W1_T2" "$W2_T1" "$W2_TH" "$W3_T1" "$SWAY_PID" 2>/dev/null || true
'

printf '==> Optimizing MP4 for universal browser playback (%s)…\n' "$OUT"
mkdir -p "$(dirname "$OUT")"
ffmpeg -y -loglevel error -i "$RAW_MP4" \
    -c:v libx264 -preset medium -crf 22 -pix_fmt yuv420p -movflags +faststart \
    "$OUT"

ls -lh "$OUT"
ffprobe -v error -show_entries format=duration,size:stream=codec_name,profile,pix_fmt,width,height,r_frame_rate "$OUT"
