#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════════
#  test/smoke-test.sh — exercise every script without a running Hyprland.
#
#  Builds a throw-away HOME, stubs all external tools (hyprctl, rofi, awww,
#  wpctl, nmcli, bluetoothctl …) and checks that each script does the right
#  thing. Useful after editing anything in .config/hypr/scripts/.
#
#      ./test/smoke-test.sh          run everything
#      ./test/smoke-test.sh -v       also dump the stub call log at the end
#
#  Nothing outside the temporary directory is touched.
# ═══════════════════════════════════════════════════════════════════════════

set -uo pipefail

REPO="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
VERBOSE=0
[ "${1:-}" = "-v" ] && VERBOSE=1

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

export HOME="$WORK/home"
export XDG_CONFIG_HOME="$HOME/.config"
export XDG_CACHE_HOME="$HOME/.cache"
BIN="$WORK/bin"
CALLS="$WORK/calls.log"
REPLY="$HOME/.rofi-reply"
ROFI_STDIN="$WORK/rofi-stdin"
mkdir -p "$BIN" "$HOME/Pictures/Wallpapers" "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME" \
         "$XDG_CONFIG_HOME/hypr/modules" "$XDG_CONFIG_HOME/hypr/scripts" \
         "$XDG_CONFIG_HOME/rofi" "$XDG_CONFIG_HOME/waybar/scripts"

C="$XDG_CONFIG_HOME/hypr/scripts"

# ── output encoding ────────────────────────────────────────────────────────
# The ✓ / ✗ / box-drawing characters below need a UTF-8 locale. A bare shell in
# CI, cron or a plain TTY often has none (LC_CTYPE=POSIX), and bash then fails
# to write them. Pick a UTF-8 locale when one exists; fall back to ASCII.
if [ "$(locale charmap 2>/dev/null)" != "UTF-8" ]; then
    for _loc in C.UTF-8 C.utf8 en_US.UTF-8 en_GB.UTF-8; do
        if locale -a 2>/dev/null | grep -qx "$_loc"; then
            export LC_ALL="$_loc"; break
        fi
    done
    unset _loc
fi
UNICODE=0
[ "$(locale charmap 2>/dev/null)" = "UTF-8" ] && UNICODE=1
if [ "$UNICODE" = "1" ]; then
    M_OK='\033[32m✓\033[0m'; M_BAD='\033[31m✗\033[0m'
    M_RULE='────────────────────────────────────────────'
else
    M_OK='\033[32mok\033[0m'; M_BAD='\033[31m!!\033[0m'
    M_RULE='--------------------------------------------'
fi

PASS=0; FAIL=0
green() { printf '  %b %s\n' "$M_OK" "$1"; PASS=$((PASS + 1)); }
red()   { printf '  %b %s\n' "$M_BAD" "$1"; printf '      %s\n' "${2:-}" | head -8; FAIL=$((FAIL + 1)); }

contains() {  # contains <desc> <needle> <haystack>
    if grep -qF -- "$2" <<<"$3"; then green "$1"
    else red "$1" "expected to find: $2 | got: $(printf '%s' "$3" | head -3)"; fi
}
lacks() {     # lacks <desc> <needle> <haystack>
    if grep -qF -- "$2" <<<"$3"; then red "$1" "expected NOT to find: $2"
    else green "$1"; fi
}
eq() {        # eq <desc> <expected> <actual>
    if [ "$2" = "$3" ]; then green "$1"; else red "$1" "expected [$2] | got [$3]"; fi
}

# ── Stubs ─────────────────────────────────────────────────────────────────
stub() {  # stub <name> <body…>
    local name="$1"; shift
    {
        printf '#!/usr/bin/env bash\n'
        printf 'printf "%%s %%s\\n" "%s" "$*" >> "%s"\n' "$name" "$CALLS"
        printf '%s\n' "$@"
    } > "$BIN/$name"
    chmod +x "$BIN/$name"
}

: > "$CALLS"

# rofi consumes the reply queue (one line per call); an empty queue == Escape.
stub rofi 'timeout 0.5 cat > "$ROFI_STDIN" 2>/dev/null || true      # capture the menu list
if [ -s "'"$REPLY"'" ]; then
  head -1 "'"$REPLY"'"
  sed -i "1d" "'"$REPLY"'"
  exit 0
fi
exit 1'

stub hyprctl 'case "${1:-}" in
  activewindow) cat <<JSON
{"class":"kitty","floating":false,"monitor":0,"at":[100,100],"size":[800,600]}
JSON
;;
  monitors) cat <<JSON
[{"id":0,"name":"eDP-1","width":1920,"height":1080,"scale":1,"x":0,"y":0,"refreshRate":60,
  "availableModes":["1920x1080@60Hz","1600x900@60Hz"]}]
JSON
;;
  clients) if [ -n "${FAKE_CLIENTS:-}" ]; then cat "$FAKE_CLIENTS"; else echo "[]"; fi ;;
  version) echo "Hyprland 0.56.2 (main, abc123)" ;;
  binds) [ -n "${FAKE_BINDS:-}" ] && cat "$FAKE_BINDS" ;;
  configerrors) echo "no errors" ;;
  getoption) echo "float: 1" ;;
  *) : ;;
esac'

stub awww 'exit 0'
stub awww-daemon 'sleep 1'
stub hyprsunset 'sleep 1'
stub notify-send ':'
stub grim 'for last; do :; done; printf png > "$last"'
stub slurp 'echo 100x100+10+10'
stub wl-copy 'cat > /dev/null'
stub wpctl 'case "$1" in
  get-volume) echo "Volume: 0.42" ;;
  inspect) echo "node.description = \"Speakers\"" ;;
  status) printf "Sinks:\n │  * 42. Speakers  [vol: 0.42]\n" ;;
  *) : ;;
esac'
stub nmcli 'echo "successfully activated"; case "$1 $2" in
  "-t -f") echo enabled ;;
  "device wifi") echo "*:MyNetwork:80:WPA2" ;;
  *) : ;;
esac'
stub bluetoothctl 'case "$1" in
  show) echo "Powered: yes" ;;
  devices) echo "Device AA:BB:CC:DD:EE:FF Headphones" ;;
  info) printf "Name: Headphones\nConnected: yes\nPaired: yes\nBattery: 80\n" ;;
  *) : ;;
esac'
stub playerctl 'echo Playing'
stub pactl ':'
stub brightnessctl ':'
stub systemctl ':'
stub loginctl ':'
stub pgrep 'exit 1'
stub pkill 'exit 0'
stub xdg-open ':'
stub fastfetch ':'
stub pavucontrol 'sleep 1'
stub kitty 'sleep 1'
stub waybar 'sleep 1'
stub checkupdates 'echo "pkg 1.0 -> 1.1"'
stub hostnamectl 'echo testhost'
stub hostname 'echo testhost'
stub uptime 'echo " 10:00:00 up 2 hours,  1 user,  load average: 0.10, 0.20, 0.30"'
stub free 'printf "               total        used        free\nMem:            16Gi       4.2Gi       9.1Gi\n"'
stub df 'printf "Filesystem      Size  Used Avail Use%% Mounted on\n/dev/sda1       500G  120G  380G  24%% /home\n"'
stub lspci 'echo "00:02.0 VGA compatible controller: Intel Corporation Iris Xe Graphics"'
stub ip 'echo "1: lo    inet 127.0.0.1/8"; echo "2: wlan0    inet 192.168.1.42/24 brd 192.168.1.255 scope global wlan0"'
stub uname 'exec /usr/bin/uname "$@" 2>/dev/null || echo 6.10.0-test'

for c in bash sh jq cat sed awk grep head tail printf date uname df free lspci hostnamectl uptime ip rm mkdir tr cut du wc sleep kill basename dirname id ls; do
    [ -x "$BIN/$c" ] && continue
    real="$(command -v "$c" 2>/dev/null || true)"
    [ -n "$real" ] || continue
    ln -sf "$real" "$BIN/$c"
done

export PATH="$BIN:$PATH"
export FAKE_BINDS="${FAKE_BINDS:-}"
export ROFI_STDIN

# ── Fake config tree ──────────────────────────────────────────────────────
cp "$REPO/.config/hypr/hyprland.lua" "$XDG_CONFIG_HOME/hypr/"
cp "$REPO"/.config/hypr/modules/*.lua "$XDG_CONFIG_HOME/hypr/modules/"
# Ensure Pure C11 tools & runtime calculator are built before running isolated tests
if [ ! -x "$REPO/bin/mono-calc" ] || [ ! -x "$REPO/bin/mono-ipc" ] || [ ! -x "$REPO/bin/mono-tui" ] || \
   [ ! -x "$REPO/bin/wallpaper-gen" ] || [ ! -x "$REPO/bin/wallpaper-anim" ] || \
   [ ! -x "$REPO/bin/mono-util" ]; then
    make -C "$REPO" -s -j"$(nproc 2>/dev/null || echo 2)"
fi

cp "$REPO"/.config/hypr/scripts/*.sh "$C"
cp "$REPO"/.config/waybar/scripts/*.sh "$XDG_CONFIG_HOME/waybar/scripts/"
mkdir -p "$XDG_CONFIG_HOME/gtk-3.0" "$XDG_CONFIG_HOME/gtk-4.0" \
         "$XDG_CONFIG_HOME/qt5ct/colors" "$XDG_CONFIG_HOME/qt6ct/colors" \
         "$XDG_CONFIG_HOME/nvim/colors" "$XDG_CONFIG_HOME/kitty" \
         "$XDG_CONFIG_HOME/hypr/shaders" "$XDG_CONFIG_HOME/hypr/src" \
         "$XDG_CONFIG_HOME/hypr/bin"
cp "$REPO/.config/hypr/src/mono-calc.c" "$XDG_CONFIG_HOME/hypr/src/"
cp "$REPO/.config/hypr/src/mono-ipc.c" "$XDG_CONFIG_HOME/hypr/src/"
cp "$REPO/.config/hypr/src/mono-tui.c" "$XDG_CONFIG_HOME/hypr/src/"
cp "$REPO/bin/mono-calc" "$XDG_CONFIG_HOME/hypr/bin/"
cp "$REPO/bin/mono-ipc" "$XDG_CONFIG_HOME/hypr/bin/"
cp "$REPO/bin/mono-tui" "$XDG_CONFIG_HOME/hypr/bin/"
cp "$REPO"/.config/hypr/shaders/*.frag "$XDG_CONFIG_HOME/hypr/shaders/"
cp "$REPO/.config/gtk-3.0/gtk.css" "$XDG_CONFIG_HOME/gtk-3.0/"
cp "$REPO/.config/gtk-4.0/gtk.css" "$XDG_CONFIG_HOME/gtk-4.0/"
cp "$REPO/.config/qt5ct/colors/Monochrome.conf" "$XDG_CONFIG_HOME/qt5ct/colors/"
cp "$REPO/.config/qt6ct/colors/Monochrome.conf" "$XDG_CONFIG_HOME/qt6ct/colors/"
cp "$REPO/.config/nvim/colors/monochrome.lua" "$XDG_CONFIG_HOME/nvim/colors/"
cp "$REPO/.config/kitty/theme.conf" "$XDG_CONFIG_HOME/kitty/"
cp "$REPO/.config/waybar/style.css" "$XDG_CONFIG_HOME/waybar/"
cp -r "$REPO/.config/browser" "$XDG_CONFIG_HOME/"
cp -r "$REPO/.config/opencode" "$XDG_CONFIG_HOME/"
chmod +x "$C"/*.sh "$XDG_CONFIG_HOME"/waybar/scripts/*.sh
: > "$XDG_CONFIG_HOME/rofi/theme.rasi"

for n in 1 2 3; do : > "$HOME/Pictures/Wallpapers/mono-0$n.jpg"; done
touch "$HOME/Pictures/Wallpapers/notes.txt"        # not an image: must be ignored

queue() { printf '%s\n' "$@" > "$REPLY"; }

cd "$WORK" || exit 1        # scripts must never write into the repository

printf '\n\033[1mhyprland-monochrome smoke test\033[0m\n'
printf 'workspace: %s\n\n' "$WORK"

# ═══════════════════════ 1. lib.sh ════════════════════════════════════════
printf '\033[1m1. lib.sh helpers\033[0m\n'
. "$C/lib.sh"

eq "mono_var reads a number"                      "5"   "$(mono_var gaps_in)"
eq "mono_var reads a plain string"                "kitty" "$(mono_var terminal)"
eq "mono_var keeps --flags inside strings"        "kitty --class kitty-float" "$(mono_var terminal_float)"
eq "mono_var expands home .. \"…\" paths"          "$HOME/Pictures/Wallpapers" "$(mono_var wallpaper_dir)"
eq "mono_var strips trailing comments"            "0.92" "$(mono_var terminal_opacity)"
eq "mono_var strips the trailing comma"           "SUPER" "$(mono_var mainMod)"
eq "mono_var falls back for unknown keys"         "fallback" "$(mono_var not_a_key fallback)"
eq "monitors are reported in logical pixels"      "0	0	1920	1080" "$(monitor_geometry 0)"
eq "win_class reads the focused window"           "kitty" "$(win_class)"

# ═══════════════════════ 2. menu.sh ═══════════════════════════════════════
printf '\n\033[1m2. menu.sh (launcher / keybind cheat sheet)\033[0m\n'

cat > "$WORK/binds.json" <<'JSON'
[ { "key": "D", "keycode": 40, "modmask": 64, "description": "App launcher" },
  { "key": "E", "keycode": 26, "modmask": 65, "description": "File manager" },
  { "key": "",  "keycode": 82, "modmask": 68, "description": "Zoom out" },
  { "key": "Q", "keycode": 24, "modmask": 64, "description": "" } ]
JSON
jq -e . "$WORK/binds.json" >/dev/null 2>&1 || { printf "  fixture binds.json is invalid JSON!\n"; exit 1; }
: > "$CALLS"
FAKE_BINDS="$WORK/binds.json" timeout 15 "$C/menu.sh" keybinds >/dev/null 2>&1
rofi_call="$(grep '^rofi' "$CALLS" | tail -1)"
sheet="$(cat "$ROFI_STDIN" 2>/dev/null)"
contains "cheat sheet shows Super + D (modmask 64)"        "Super + D"          "$sheet"
contains "shift-modified binds render correctly"           "Super + Shift + E"  "$sheet"
contains "keyless binds fall back to the keycode"          "Super + Ctrl + code 82" "$sheet"
contains "ctrl+super renders as one entry"                 "Super + Ctrl"       "$sheet"
lacks    "binds with an empty description are dropped"    "Super + Q"          "$sheet"
contains "the list is handed to rofi"                      "dmenu"              "$rofi_call"
lacks    "descriptions stay visible and searchable"        "-display-columns 1" "$rofi_call"

: > "$CALLS"
timeout 15 "$C/menu.sh" launcher >/dev/null 2>&1
contains "launcher asks rofi for drun"                     "-show drun" "$(cat "$CALLS")"

# ═══════════════════════ 3. wallpaper.sh ══════════════════════════════════
printf '\n\033[1m3. wallpaper.sh\033[0m\n'

: > "$CALLS"
timeout 15 "$C/wallpaper.sh" set "$HOME/Pictures/Wallpapers/mono-01.jpg" >/dev/null 2>&1
contains "set pushes the image through awww"  "awww img $HOME/Pictures/Wallpapers/mono-01.jpg" "$(cat "$CALLS")"
eq "set records it for the next login"        "$HOME/Pictures/Wallpapers/mono-01.jpg" "$(cat "$XDG_CACHE_HOME/hyprland-monochrome/wallpaper" 2>/dev/null)"

: > "$CALLS"
timeout 15 "$C/wallpaper.sh" restore >/dev/null 2>&1
contains "restore re-applies the recorded image" "mono-01.jpg" "$(cat "$CALLS")"

: > "$CALLS"
timeout 15 "$C/wallpaper.sh" solid 101010 >/dev/null 2>&1
contains "solid generates a flat colour image"   "solid-101010.png" "$(cat "$CALLS")"

: > "$CALLS"
queue "mono-02.jpg"
timeout 15 "$C/wallpaper.sh" pick >/dev/null 2>&1
contains "picker applies the chosen thumbnail"   "awww img $HOME/Pictures/Wallpapers/mono-02.jpg" "$(cat "$CALLS")"
contains "picker sends rofi dmenu icon protocol" "@icon@" "$(tr '\0\037' '@@' < "$ROFI_STDIN" 2>/dev/null)"

: > "$CALLS"
timeout 15 "$C/wallpaper.sh" next >/dev/null 2>&1
contains "next cycles to another wallpaper"      "awww img" "$(cat "$CALLS")"

# ═══════════════════════ 4. screenshot.sh ════════════════════════════════
printf '\n\033[1m4. screenshot.sh\033[0m\n'
: > "$CALLS"
timeout 15 "$C/screenshot.sh" full >/dev/null 2>&1
eq "full screenshot writes exactly one file" "1" "$(ls "$HOME/Pictures/Screenshots" 2>/dev/null | wc -l)"
contains "the screenshot is copied to the clipboard" "wl-copy" "$(cat "$CALLS")"

: > "$CALLS"
timeout 15 "$C/screenshot.sh" window >/dev/null 2>&1
contains "window shots use the focused geometry" "-g 800x600+100+100" "$(cat "$CALLS")"

: > "$CALLS"
timeout 15 "$C/screenshot.sh" region >/dev/null 2>&1
contains "region shots use the slurp selection"  "-g 100x100+10+10" "$(cat "$CALLS")"

# ═══════════════════════ 5. volume.sh ════════════════════════════════════
printf '\n\033[1m5. volume.sh\033[0m\n'
eq "status reports the current level" "42%" "$(timeout 15 "$C/volume.sh" status)"
: > "$CALLS"; timeout 15 "$C/volume.sh" up   >/dev/null 2>&1
contains "volume up is capped at 150%" "set-volume -l 1.5 @DEFAULT_AUDIO_SINK@ 5%+" "$(cat "$CALLS")"
: > "$CALLS"; timeout 15 "$C/volume.sh" down >/dev/null 2>&1
contains "volume down steps by 5%"     "set-volume @DEFAULT_AUDIO_SINK@ 5%-"        "$(cat "$CALLS")"
: > "$CALLS"; timeout 15 "$C/volume.sh" mic  >/dev/null 2>&1
contains "mic toggles the default source" "@DEFAULT_AUDIO_SOURCE@ toggle"          "$(cat "$CALLS")"

# ═══════════════════════ 6. window helpers ═══════════════════════════════
printf '\n\033[1m6. window helpers\033[0m\n'
: > "$CALLS"; timeout 15 "$C/float-center.sh" >/dev/null 2>&1
contains "float toggle uses the dispatcher"  'window.float({ action = "toggle" })' "$(cat "$CALLS")"
: > "$CALLS"; timeout 15 "$C/opacity.sh" >/dev/null 2>&1
contains "opacity cycles via set_prop"       'set_prop({ prop = "opacity"'        "$(cat "$CALLS")"
: > "$CALLS"; timeout 15 "$C/zoom.sh" in >/dev/null 2>&1
contains "zoom changes cursor:zoom_factor"   "cursor = { zoom_factor"             "$(cat "$CALLS")"
: > "$CALLS"; timeout 15 "$C/nightshift.sh" 3000 >/dev/null 2>&1
contains "nightshift drives hyprsunset"      "hyprsunset temperature 3000"        "$(cat "$CALLS")"
: > "$CALLS"; timeout 15 "$C/waybar-toggle.sh" >/dev/null 2>&1
contains "waybar toggle starts the bar when stopped" "waybar" "$(cat "$CALLS")"
: > "$CALLS"; timeout 15 "$C/scratchterm.sh" >/dev/null 2>&1
contains "dropdown terminal toggles the special workspace" "toggle_special" "$(cat "$CALLS")"

# ═══════════════════════ 7. powermenu.sh ═════════════════════════════════
printf '\n\033[1m7. powermenu.sh\033[0m\n'
: > "$CALLS"; queue "$(printf '\tshutdown\tShut down')"
timeout 15 bash -c "false" 2>/dev/null            # (no-op, keeps output tidy)
printf 'no\n' >> "$REPLY"
timeout 15 "$C/powermenu.sh" >/dev/null 2>&1
contains "the menu offers every session action" "powermenu.rasi" "$(cat "$CALLS")"
lacks    "declining the confirmation is safe"   "poweroff"       "$(cat "$CALLS")"

: > "$CALLS"; queue "$(printf '\tshutdown\tShut down')" "yes"
timeout 15 "$C/powermenu.sh" >/dev/null 2>&1
contains "confirming shuts the machine down" "systemctl poweroff" "$(cat "$CALLS")"

: > "$CALLS"; queue "$(printf '\tlock\tLock screen')"
timeout 15 "$C/powermenu.sh" >/dev/null 2>&1
lacks    "locking never asks for confirmation" "are you sure" "$(cat "$CALLS")"
contains "locking starts hyprlock"             "hyprlock"      "$(cat "$CALLS")"

# ═══════════════════════ 8. session.sh ═══════════════════════════════════
printf '\n\033[1m8. session.sh\033[0m\n'
: > "$CALLS"; timeout 15 "$C/session.sh" reboot >/dev/null 2>&1
contains "reboot calls systemctl"              "systemctl reboot" "$(cat "$CALLS")"
: > "$CALLS"; timeout 15 "$C/session.sh" quit >/dev/null 2>&1
contains "quit falls back to the exit dispatcher" "hl.dsp.exit()" "$(cat "$CALLS")"
: > "$CALLS"; timeout 15 "$C/session.sh" suspend >/dev/null 2>&1
contains "suspend locks first, then suspends" "systemctl suspend" "$(cat "$CALLS")"

# ═══════════════════════ 9. settings.sh ══════════════════════════════════
printf '\n\033[1m9. settings.sh\033[0m\n'
: > "$CALLS"; queue "system" "copy"
timeout 20 "$C/settings.sh" >/dev/null 2>&1
contains "settings menu renders through rofi"     "settings.rasi" "$(cat "$CALLS")"
contains "system info collects host details"      "hostnamectl"   "$(cat "$CALLS")"
contains "copying the report uses the clipboard"  "wl-copy"       "$(cat "$CALLS")"

: > "$CALLS"; queue "network" "radio"
timeout 20 "$C/settings.sh" >/dev/null 2>&1
contains "network toggles the wifi radio"         "radio wifi"    "$(cat "$CALLS")"

: > "$CALLS"; queue "sound" "up"
timeout 20 "$C/settings.sh" >/dev/null 2>&1
contains "sound changes the volume"               "set-volume"    "$(cat "$CALLS")"

: > "$CALLS"; queue "bluetooth" "devices"
timeout 20 "$C/settings.sh" >/dev/null 2>&1
contains "bluetooth lists devices"                "bluetoothctl devices" "$(cat "$CALLS")"

: > "$CALLS"; queue "monitors"
timeout 20 "$C/settings.sh" >/dev/null 2>&1
contains "monitors reads the live outputs"        "hyprctl monitors"     "$(cat "$CALLS")"

: > "$CALLS"; queue "nightshift" "3500"
timeout 20 "$C/settings.sh" >/dev/null 2>&1
contains "night shift forwards the temperature"   "hyprsunset temperature 3500" "$(cat "$CALLS")"

: > "$CALLS"; queue "hyprland" "reload"
timeout 20 "$C/settings.sh" >/dev/null 2>&1
contains "hyprland section can reload the config" "hyprctl reload" "$(cat "$CALLS")"

# The monitor config writer must produce valid, loadable Lua.
: > "$CALLS"
printf 'eDP-1\t1920x1080@60 · scale 1 · 0,0\n' > "$REPLY"
# (menu-driven write is covered by the generated file below)
MONOUT="$XDG_CONFIG_HOME/hypr/modules/monitors-user.lua"
printf -- '-- test\nhl.monitor({ output = "eDP-1", mode = "1920x1080@60", position = "auto", scale = 1 })\n' > "$MONOUT"
if command -v luac5.4 >/dev/null 2>&1 || command -v luac >/dev/null 2>&1; then
    lua_bin="$(command -v luac5.4 || command -v luac)"
    "$lua_bin" -p "$MONOUT" 2>/dev/null && green "generated monitor config is valid Lua" \
                                       || red "generated monitor config is valid Lua" "$("$lua_bin" -p "$MONOUT" 2>&1)"
else
    green "generated monitor config is valid Lua (skipped: no luac installed)"
fi

# ═══════════════════════ 10. waybar module ═══════════════════════════════
printf '\n\033[1m10. waybar updates module\033[0m\n'
out="$(timeout 15 "$XDG_CONFIG_HOME/waybar/scripts/updates.sh" 2>/dev/null)"
contains "updates module emits waybar JSON" '"text"'  "$out"
contains "and reports the pending count"    '1'       "$out"

# ═══════════════════════ 11. Hyprland Lua config ═════════════════════════
printf '\n\033[1m11. Hyprland Lua config\033[0m\n'
if command -v luac5.4 >/dev/null 2>&1 || command -v luac >/dev/null 2>&1; then
    lua_bin="$(command -v luac5.4 || command -v luac)"
    for f in "$REPO"/.config/hypr/hyprland.lua "$REPO"/.config/hypr/modules/*.lua; do
        if "$lua_bin" -p "$f" 2>/dev/null; then green "parses: ${f#"$REPO"/}"
        else red "parses: ${f#"$REPO"/}" "$("$lua_bin" -p "$f" 2>&1 | head -2)"; fi
    done
else
    green "Lua config parsing (skipped: no luac installed)"
fi

# ═══════════════════════ 12. record.sh ═══════════════════════════════════
printf '\n\033[1m12. record.sh (screen recording)\033[0m\n'

# Fake recorders. They log the exact command line, drop the output file where
# the script asked for it, and quit on SIGINT — same contract as the real ones.
cat > "$BIN/gpu-screen-recorder" <<EOF
#!/usr/bin/env bash
printf 'gpu-screen-recorder %s\n' "\$*" >> "$CALLS"
out=""; prev=""
for a in "\$@"; do [ "\$prev" = "-o" ] && out="\$a"; prev="\$a"; done
trap '[ -n "\$out" ] && [ ! -d "\$out" ] && printf video > "\$out"; exit 0' INT
trap 'printf "%s/replay-1.mp4\n" "\$out"; printf video > "\$out/replay-1.mp4" 2>/dev/null' USR1
trap ':' USR2
# -o is a file when recording and a directory in replay mode.
# Like the real recorders, the output file appears as soon as capture starts.
case "\$out" in *.mp4|*.mkv|*.webm) mkdir -p "$(dirname "\$out")"; printf header > "\$out" ;; *) mkdir -p "\$out" ;; esac
# Stay alive through signals (a real recorder keeps recording on SIGUSR1/2);
# the INT trap above is what ends this loop.
while :; do sleep 0.2; done
EOF
chmod +x "$BIN/gpu-screen-recorder"

REC_DIR="$HOME/Videos/Recordings"
REC_STATE="$XDG_CACHE_HOME/hyprland-monochrome/record.state"

: > "$CALLS"
timeout 20 "$C/record.sh" start screen >/dev/null 2>&1
contains "start records the whole screen"      "-w screen"        "$(cat "$CALLS")"
contains "audio comes from user.lua"           "-a default_output" "$(cat "$CALLS")"
contains "and it records to the mp4 file"      "-o "              "$(cat "$CALLS")"
eq "the file lands in the recordings folder"   "1" "$(ls "$REC_DIR"/rec-*.mp4 2>/dev/null | wc -l)"
eq "state remembers the pid"                   "yes" "$(grep -q '^pid=' "$REC_STATE" 2>/dev/null && echo yes || echo no)"
contains "status tells Waybar it is recording" '"class":"on"' "$(timeout 10 "$C/record.sh" status --waybar)"
contains "the elapsed time is in the bar text" '● ' "$(timeout 10 "$C/record.sh" status --waybar)"

timeout 20 "$C/record.sh" toggle >/dev/null 2>&1
eq "toggle again stops it"                     "no" "$(test -f "$REC_STATE" && echo yes || echo no)"
contains "stop reports the saved file"         "Recording saved"  "$(cat "$CALLS")"
contains "the path is copied to the clipboard" "wl-copy"          "$(cat "$CALLS")"
contains "and the bar goes quiet again"        '"class":"off"' "$(timeout 10 "$C/record.sh" status --waybar)"

# region recording
: > "$CALLS"
timeout 20 "$C/record.sh" start region >/dev/null 2>&1
contains "region recording passes the slurp geometry" "-region 100x100+10+10" "$(cat "$CALLS")"
timeout 20 "$C/record.sh" stop >/dev/null 2>&1

# audio setting persists and cycles
timeout 10 "$C/record.sh" audio both >/dev/null 2>&1
eq "audio mode is remembered"      "both" "$(cat "$XDG_CACHE_HOME/hyprland-monochrome/record.audio")"
contains "the change is announced" "desktop + microphone" "$(cat "$CALLS")"
timeout 10 "$C/record.sh" audio next >/dev/null 2>&1
eq "next cycles both → mic"        "mic"  "$(cat "$XDG_CACHE_HOME/hyprland-monochrome/record.audio")"
timeout 10 "$C/record.sh" audio desktop >/dev/null 2>&1

# instant replay: first press starts the buffer, second press saves it
: > "$CALLS"
timeout 20 "$C/record.sh" replay >/dev/null 2>&1
contains "replay starts a 60s buffer"        "-r 60" "$(cat "$CALLS")"
contains "the buffer is shown in the bar"    'buf' "$(timeout 10 "$C/record.sh" status --waybar)"
timeout 20 "$C/record.sh" replay >/dev/null 2>&1
contains "pressing again saves the replay"   "Replay saved" "$(cat "$CALLS")"
timeout 20 "$C/record.sh" stop >/dev/null 2>&1

# pause / resume
timeout 20 "$C/record.sh" start screen >/dev/null 2>&1
timeout 10 "$C/record.sh" pause >/dev/null 2>&1
contains "pause is reflected in the bar"     '"class":"paused"' "$(timeout 10 "$C/record.sh" status --waybar)"
timeout 10 "$C/record.sh" pause >/dev/null 2>&1
contains "pressing again resumes"            '"class":"on"' "$(timeout 10 "$C/record.sh" status --waybar)"
timeout 20 "$C/record.sh" stop >/dev/null 2>&1

# wf-recorder fallback
mv "$BIN/gpu-screen-recorder" "$BIN/.gsr"
cat > "$BIN/wf-recorder" <<EOF
#!/usr/bin/env bash
printf 'wf-recorder %s\n' "\$*" >> "$CALLS"
out=""; prev=""
for a in "\$@"; do [ "\$prev" = "-f" ] && out="\$a"; prev="\$a"; done
trap 'printf video > "\$out"; exit 0' INT
mkdir -p "$(dirname "\$out")" 2>/dev/null
while :; do sleep 0.2; done
EOF
chmod +x "$BIN/wf-recorder"

: > "$CALLS"
cat > "$BIN/pactl" <<'EOF'
#!/usr/bin/env bash
[ "${1:-}" = "get-default-sink" ] && echo "alsa_output.test"
EOF
chmod +x "$BIN/pactl"
timeout 20 "$C/record.sh" start region >/dev/null 2>&1
contains "wf-recorder is used when gsr is missing" "wf-recorder -y"   "$(cat "$CALLS")"
contains "…disables damage wait (-D) on idle screens" " -D "          "$(cat "$CALLS")"
contains "…with the output file"                   "-f "             "$(cat "$CALLS")"
contains "…and the default sink monitor for audio" "--audio=alsa_output.test.monitor" "$(cat "$CALLS")"
timeout 20 "$C/record.sh" stop >/dev/null 2>&1

# wf-recorder falls back to video-only if PulseAudio/PipeWire aborts on --audio
cat > "$BIN/wf-recorder" <<EOF
#!/usr/bin/env bash
printf 'wf-recorder %s\n' "\$*" >> "$CALLS"
for a in "\$@"; do
    case "\$a" in --audio*) echo "Assertion 'p' failed at ../src/pulse/simple.c:456, function pa_simple_get_latency(). Aborting." >&2; exit 134 ;; esac
done
out=""; prev=""
for a in "\$@"; do [ "\$prev" = "-f" ] && out="\$a"; prev="\$a"; done
trap 'printf video > "\$out"; exit 0' INT
mkdir -p "$(dirname "\$out")" 2>/dev/null
while :; do sleep 0.2; done
EOF
chmod +x "$BIN/wf-recorder"
: > "$CALLS"
timeout 20 "$C/record.sh" start screen >/dev/null 2>&1
contains "wf-recorder retries without audio when pa_simple fails" "Audio unavailable" "$(cat "$CALLS")"
timeout 20 "$C/record.sh" stop >/dev/null 2>&1

timeout 20 "$C/record.sh" replay >/dev/null 2>&1
contains "instant replay explains it needs gsr" "gpu-screen-recorder" "$(cat "$CALLS")"

# no recorder at all (isolate PATH="$BIN" so a real /usr/bin/wf-recorder on the host does not leak in)
mv "$BIN/wf-recorder" "$BIN/.wf"
: > "$CALLS"
PATH="$BIN" "$C/record.sh" start screen >/dev/null 2>&1
eq "no recorder installed fails politely" "1" "$?"
contains "…and says what to install" "Install wf-recorder or gpu-screen-recorder" "$(cat "$CALLS")"
mv "$BIN/.gsr" "$BIN/gpu-screen-recorder"
mv "$BIN/.wf"  "$BIN/wf-recorder"
eq "record.sh path prints the folder" "$REC_DIR" "$(timeout 10 "$C/record.sh" path)"

# ═══════════════════════ 13. wallpaper generator (Pure C11) ═════════════
printf '\n\033[1m13. wallpapers/generate.c (Pure C11)\033[0m\n'
# run it in the throw-away workspace, so nothing is written into the repo
GEN_DIR="$WORK/wallpapers"
mkdir -p "$GEN_DIR"
cp "$REPO/bin/wallpaper-gen" "$GEN_DIR/"

if timeout 60 "$GEN_DIR/wallpaper-gen" 320 180 --only 6 --suffix -test >/dev/null 2>&1; then
    green "renders a design on demand"
else
    red "renders a design on demand" "$(cd "$GEN_DIR" && timeout 60 ./wallpaper-gen 320 180 --only 6 2>&1 | tail -2)"
fi
eq "…writes exactly the file asked for" "1" "$(find "$GEN_DIR" -maxdepth 1 -name 'mono-06-test.jpg' | wc -l)"
eq "…at the requested size" "320x180" "$("$REPO/bin/mono-util" image-size "$GEN_DIR/mono-06-test.jpg" 2>/dev/null || echo 'bad-jpg')"
others=0
for f in "$GEN_DIR"/mono-0[1-5].jpg; do [ -e "$f" ] && others=$((others + 1)); done
eq "…and leaves the other designs alone" "0" "$others"

# --only accepts bare numbers ("--only 3" -> mono-03.jpg)
timeout 60 "$GEN_DIR/wallpaper-gen" 160 90 --only 3 >/dev/null 2>&1
eq "a bare number selects a design" "1" "$(find "$GEN_DIR" -maxdepth 1 -name 'mono-03.jpg' | wc -l)"

# verify procedural designs (mono-07 solar eclipse, mono-10 silk ribbon, mono-11 singularity, mono-14 bauhaus)
timeout 60 "$GEN_DIR/wallpaper-gen" 160 90 --only 7 >/dev/null 2>&1
eq "mono-07 eclipse renders cleanly" "1" "$(find "$GEN_DIR" -maxdepth 1 -name 'mono-07.jpg' | wc -l)"
timeout 60 "$GEN_DIR/wallpaper-gen" 160 90 --only 10 >/dev/null 2>&1
eq "mono-10 silk ribbon renders cleanly" "1" "$(find "$GEN_DIR" -maxdepth 1 -name 'mono-10.jpg' | wc -l)"
timeout 60 "$GEN_DIR/wallpaper-gen" 160 90 --only 11 >/dev/null 2>&1
eq "mono-11 singularity renders cleanly" "1" "$(find "$GEN_DIR" -maxdepth 1 -name 'mono-11.jpg' | wc -l)"
timeout 60 "$GEN_DIR/wallpaper-gen" 160 90 --only 14 >/dev/null 2>&1
eq "mono-14 bauhaus compass renders cleanly" "1" "$(find "$GEN_DIR" -maxdepth 1 -name 'mono-14.jpg' | wc -l)"

timeout 60 "$GEN_DIR/wallpaper-gen" 160 90 --only nope >/dev/null 2>&1
eq "an unknown --only fails loudly" "1" "$?"

# ═══════════════════════ 14. focus & zen engine ══════════════════════════
printf '\n\033[1m14. focus.sh\033[0m\n'
: > "$CALLS"
contains "idle focus status is off" '"class":"off"' "$(timeout 10 "$C/focus.sh" status --waybar)"
timeout 10 "$C/focus.sh" start 25 "Sprint" >/dev/null 2>&1
contains "active timer emits Waybar 'on' pill" '"class":"on"' "$(timeout 10 "$C/focus.sh" status --waybar | tr -d '\n')"
contains "active timer shows label in tooltip" 'Sprint'       "$(timeout 10 "$C/focus.sh" status --waybar)"
timeout 10 "$C/focus.sh" pause >/dev/null 2>&1
contains "pause transitions pill to 'paused'"  '"class":"paused"' "$(timeout 10 "$C/focus.sh" status --waybar | tr -d '\n')"
timeout 10 "$C/focus.sh" stop >/dev/null 2>&1
eq "stop returns status to off" "off" "$(timeout 10 "$C/focus.sh" status)"

timeout 10 "$C/focus.sh" zen >/dev/null 2>&1
eq "zen toggle enters zen state" "zen" "$(timeout 10 "$C/focus.sh" status)"
contains "zen sets generous outer framing" "gaps_out 28" "$(cat "$CALLS")"
timeout 10 "$C/focus.sh" zen >/dev/null 2>&1
eq "second zen toggle restores workspace" "off" "$(timeout 10 "$C/focus.sh" status)"

# ═══════════════════════ 15. layout & gap presets ════════════════════════
printf '\n\033[1m15. layout.sh\033[0m\n'
: > "$CALLS"
timeout 10 "$C/layout.sh" set dwindle >/dev/null 2>&1
timeout 10 "$C/layout.sh" cycle >/dev/null 2>&1
contains "cycle advances dwindle -> master" "master" "$(timeout 10 "$C/layout.sh" status)"
timeout 10 "$C/layout.sh" cycle >/dev/null 2>&1
contains "cycle advances master -> scrolling" "scrolling" "$(timeout 10 "$C/layout.sh" status)"
timeout 10 "$C/layout.sh" gaps airy >/dev/null 2>&1
contains "airy preset applies 10/22px gaps" "gaps_out 22" "$(cat "$CALLS")"
contains "status reports active gap preset" "(airy)" "$(timeout 10 "$C/layout.sh" status)"
timeout 10 "$C/layout.sh" gaps default >/dev/null 2>&1

# ═══════════════════════ 16. command palette & math ══════════════════════
printf '\n\033[1m16. palette.sh\033[0m\n'
eq "calc evaluates pixel count" "2073600" "$(timeout 10 "$C/palette.sh" calc '= 1920 * 1080')"
eq "calc evaluates hex()"       "0xff"    "$(timeout 10 "$C/palette.sh" calc 'hex(255)')"
eq "calc evaluates math.sqrt"   "16"      "$(timeout 10 "$C/palette.sh" calc 'sqrt(256)')"
timeout 10 "$C/palette.sh" calc "__import__('os').system('id')" >/dev/null 2>&1
eq "calc rejects unsafe code injection" "1" "$?"
contains "palette list includes Focus actions" "Deep Work" "$(timeout 10 "$C/palette.sh" list)"
contains "palette list includes OCR action"    "OCR Screen Region" "$(timeout 10 "$C/palette.sh" list)"

# ═══════════════════════ 17. screenshot OCR ══════════════════════════════
printf '\n\033[1m17. screenshot.sh ocr\033[0m\n'
cat > "$BIN/tesseract" <<'EOF'
#!/usr/bin/env bash
printf 'MONOCHROME OCR TEST 2026\n'
EOF
chmod +x "$BIN/tesseract"
: > "$CALLS"
OCR_SAMPLE="$WORK/ocr-sample.png"
printf 'png' > "$OCR_SAMPLE"
OCR_OUT="$(timeout 10 "$C/screenshot.sh" ocr-file "$OCR_SAMPLE")"
eq "ocr-file extracts and prints text" "MONOCHROME OCR TEST 2026" "$OCR_OUT"
contains "ocr notifies with extracted preview" "OCR copied to clipboard" "$(cat "$CALLS")"

# ═══════════════════════ 18. scratchpad & sessions ═══════════════════════
printf '\n\033[1m18. scratchpad.sh & workspace-session.sh\033[0m\n'
: > "$CALLS"
stub nvim 'sleep 0.1'
timeout 10 "$C/scratchpad.sh" notes >/dev/null 2>&1
eq "scratchpad notes creates ~/Documents/scratch.md" "yes" "$(test -f "$HOME/Documents/scratch.md" && echo yes || echo no)"
contains "scratchpad notes toggles special:notes" "togglespecialworkspace notes" "$(cat "$CALLS")"
eq "scratchpad status reports idle when drawers closed" "idle" "$(timeout 10 "$C/scratchpad.sh" status)"

: > "$CALLS"
cat > "$WORK/clients.json" <<'JSON'
[ { "mapped": true, "class": "kitty", "floating": false, "title": "nvim", "workspace": { "name": "2" } } ]
JSON
SESS_OUT="$(FAKE_CLIENTS="$WORK/clients.json" timeout 10 "$C/workspace-session.sh" save coding)"
contains "workspace-session saves open client" "saved coding (1 windows)" "$SESS_OUT"
contains "workspace-session list shows profile" "coding" "$(timeout 10 "$C/workspace-session.sh" list)"
timeout 10 "$C/workspace-session.sh" restore coding >/dev/null 2>&1
contains "workspace-session restore dispatches onto saved workspace" "[workspace 2 silent] kitty" "$(cat "$CALLS")"
timeout 10 "$C/workspace-session.sh" delete coding >/dev/null 2>&1
eq "workspace-session delete removes snapshot" "" "$(timeout 10 "$C/workspace-session.sh" list)"

# ═══════════════════════ 19. brightness & sysgraph ═══════════════════════
printf '\n\033[1m19. brightness.sh & waybar/scripts/sysgraph.sh\033[0m\n'
cat > "$BIN/brightnessctl" <<EOF
#!/usr/bin/env bash
printf 'brightnessctl %s\n' "\$*" >> "$CALLS"
if [ "\${1:-}" = "-m" ]; then
    echo "intel_backlight,backlight,75,75%,100"
fi
EOF
chmod +x "$BIN/brightnessctl"
: > "$CALLS"
eq "brightness status reads percentage" "75%" "$(timeout 10 "$C/brightness.sh" status)"
timeout 10 "$C/brightness.sh" up >/dev/null 2>&1
contains "brightness up shows Dunst progress bar" "Brightness · 75%" "$(cat "$CALLS")"

SYSG_JSON="$(timeout 10 "$XDG_CONFIG_HOME/waybar/scripts/sysgraph.sh")"
contains "sysgraph emits valid JSON text" '"text":' "$SYSG_JSON"
contains "sysgraph tooltip mentions btop++ drawer" 'btop++ drawer' "$SYSG_JSON"

# ═══════════════════════ 20. smart.lua & doctor.sh ═══════════════════════
printf '\n\033[1m20. smart.lua reactive hooks & doctor.sh\033[0m\n'
cat > "$BIN/dunstctl" <<EOF
#!/usr/bin/env bash
printf 'dunstctl %s\n' "\$*" >> "$CALLS"
if [ "\${1:-}" = "is-paused" ]; then echo "false"; fi
EOF
chmod +x "$BIN/dunstctl"
: > "$CALLS"
timeout 10 "$C/focus.sh" screenshare-on >/dev/null 2>&1
contains "screenshare-on auto-pauses Dunst" "dunstctl set-paused true" "$(cat "$CALLS")"
timeout 10 "$C/focus.sh" screenshare-off >/dev/null 2>&1
contains "screenshare-off resumes Dunst" "dunstctl set-paused false" "$(cat "$CALLS")"

LUA_BIN="$(command -v lua5.4 || command -v lua || true)"
if [ -n "$LUA_BIN" ]; then
    LUA_EVENTS="$("$LUA_BIN" -e '
        package.path = os.getenv("XDG_CONFIG_HOME") .. "/hypr/?.lua;" .. package.path
        local events, envs = {}, {}
        local function mock()
            return setmetatable({}, {
                __call = function() return mock() end,
                __index = function() return mock() end,
            })
        end
        hl = setmetatable({
            on = function(ev, _fn) table.insert(events, ev) end,
            env = function(k, v) envs[k] = v end,
            define_submap = function(_n, _r, fn) if fn then fn() end end,
        }, { __index = function() return mock() end })
        dofile(os.getenv("XDG_CONFIG_HOME") .. "/hypr/hyprland.lua")
        print("EVENTS=" .. table.concat(events, ",") .. " QT=" .. tostring(envs.QT_QPA_PLATFORMTHEME))
    ' 2>&1)"
    contains "smart.lua registers screenshare.state hook" "screenshare.state" "$LUA_EVENTS"
    contains "smart.lua registers monitor.added hook"     "monitor.added"     "$LUA_EVENTS"
    contains "hyprland.lua exports QT_QPA_PLATFORMTHEME=qt6ct" "QT=qt6ct"     "$LUA_EVENTS"
fi

DOC_OUT="$(timeout 10 "$C/doctor.sh" --plain)"
contains "doctor verifies 7/7 monochrome toolkits" "7/7 toolkits themed" "$DOC_OUT"

# Monochrome purity audit across Qt5/Qt6 & Neovim palettes (R == G == B for every #rrggbb)
PURE_CHECK="$("$REPO/bin/mono-util" check-greyscale "$REPO")"
eq "Qt5, Qt6 & Neovim palettes are 100% pure greyscale (R==G==B)" "pure" "$PURE_CHECK"

# ═══════════════════════ 21. kinetic motion, shaders & live loops ════════
printf '\n\033[1m21. motion.sh, shader.sh, pip-corner.sh & animate.c (Pure C11)\033[0m\n'
: > "$CALLS"
contains "default motion profile is fluid" "fluid" "$(timeout 10 "$C/motion.sh" status)"
timeout 10 "$C/motion.sh" cycle >/dev/null 2>&1
contains "motion cycle advances fluid -> snappy" "snappy" "$(timeout 10 "$C/motion.sh" status)"
timeout 10 "$C/motion.sh" cycle >/dev/null 2>&1
contains "motion cycle advances snappy -> cinematic" "cinematic" "$(timeout 10 "$C/motion.sh" status)"
contains "cinematic enables orbiting borderangle loop" "borderangle,1,55,monoLinear,loop" "$(cat "$CALLS")"
timeout 10 "$C/motion.sh" set minimal >/dev/null 2>&1
contains "minimal disables animations cleanly" "animations:enabled false" "$(cat "$CALLS")"
timeout 10 "$C/motion.sh" set fluid >/dev/null 2>&1

: > "$CALLS"
timeout 10 "$C/shader.sh" set noir >/dev/null 2>&1
eq "shader status reports noir" "noir" "$(timeout 10 "$C/shader.sh" status)"
contains "shader.sh applies noir.frag via decoration:screen_shader" "noir.frag" "$(cat "$CALLS")"
timeout 10 "$C/shader.sh" cycle >/dev/null 2>&1
eq "shader cycle advances noir -> cinema-grain" "cinema-grain" "$(timeout 10 "$C/shader.sh" status)"
timeout 10 "$C/shader.sh" set off >/dev/null 2>&1
eq "shader off resets status" "off" "$(timeout 10 "$C/shader.sh" status)"

GLSL_OK=0
for frag in "$REPO"/.config/hypr/shaders/*.frag; do
    head -1 "$frag" | grep -qx '#version 300 es' &&
    grep -q 'uniform sampler2D tex;' "$frag" &&
    grep -q 'out vec4 fragColor;' "$frag" &&
    GLSL_OK=$((GLSL_OK + 1))
done
eq "all 5 GLSL ES 3.00 screen shaders match Hyprland uniform spec" "5" "$GLSL_OK"

: > "$CALLS"
timeout 10 "$C/pip-corner.sh" 2 >/dev/null 2>&1
contains "pip-corner springs window via movewindowpixel exact" "movewindowpixel exact" "$(cat "$CALLS")"

if [ -n "$LUA_BIN" ]; then
    ANIM_STATS="$("$LUA_BIN" -e '
        package.path = os.getenv("XDG_CONFIG_HOME") .. "/hypr/?.lua;" .. package.path
        local springs, leaves = 0, 0
        local function mock()
            return setmetatable({}, {
                __call = function() return mock() end,
                __index = function() return mock() end,
            })
        end
        hl = setmetatable({
            curve = function(_n, spec) if spec and spec.type == "spring" then springs = springs + 1 end end,
            animation = function(spec) if spec and spec.leaf then leaves = leaves + 1 end end,
            define_submap = function(_n, _r, fn) if fn then fn() end end,
        }, { __index = function() return mock() end })
        dofile(os.getenv("XDG_CONFIG_HOME") .. "/hypr/hyprland.lua")
        print(string.format("springs=%d leaves=%d", springs, leaves))
    ' 2>&1)"
    contains "animations.lua registers 3 physical spring curves" "springs=3" "$ANIM_STATS"
    contains "animations.lua configures 29 animation leaves"     "leaves=29" "$ANIM_STATS"
fi

: > "$HOME/Pictures/Wallpapers/mono-live-01-orbital.gif"
: > "$CALLS"
timeout 10 "$C/wallpaper.sh" live >/dev/null 2>&1
contains "wallpaper.sh live transitions at 120fps on Bezier curve" "--transition-fps 120" "$(cat "$CALLS")"
contains "wallpaper.sh live selects animated gif" "mono-live-01-orbital.gif" "$(cat "$CALLS")"

cp "$REPO/bin/wallpaper-anim" "$GEN_DIR/"
timeout 60 "$GEN_DIR/wallpaper-anim" 160 90 --frames 6 --fps 12 --only orbital >/dev/null 2>&1
eq "animate.c renders seamless multi-frame greyscale GIF" "6" "$("$REPO/bin/mono-util" gif-frames "$GEN_DIR/mono-live-01-orbital.gif" 2>/dev/null || echo 0)"
timeout 60 "$GEN_DIR/wallpaper-anim" 160 90 --frames 6 --fps 12 --only lissajous >/dev/null 2>&1
eq "animate.c renders seamless lissajous oscilloscope GIF" "6" "$("$REPO/bin/mono-util" gif-frames "$GEN_DIR/mono-live-03-lissajous.gif" 2>/dev/null || echo 0)"
timeout 60 "$GEN_DIR/wallpaper-anim" 160 90 --frames 6 --fps 12 --only 7 >/dev/null 2>&1
eq "animate.c renders numbered design (mono-07.gif) as live loop" "6" "$("$REPO/bin/mono-util" gif-frames "$GEN_DIR/mono-07.gif" 2>/dev/null || echo 0)"
eq "all 20 bundled wallpapers in wallpapers/ are live .gif loops" "20" "$(find "$REPO/wallpapers" -maxdepth 1 -name '*.gif' | wc -l | tr -d ' ')"
eq "zero static .jpg/.png wallpapers remain in wallpapers/" "0" "$(find "$REPO/wallpapers" -maxdepth 1 \( -name '*.jpg' -o -name '*.png' \) | wc -l | tr -d ' ')"
LIVE_VALID=0
for wf in "$REPO"/wallpapers/*.gif; do
    fcount="$("$REPO/bin/mono-util" gif-frames "$wf" 2>/dev/null || echo 0)"
    [ "$fcount" -ge 20 ] && LIVE_VALID=$((LIVE_VALID + 1))
done
eq "every bundled wallpaper loop has >= 20 animation frames" "20" "$LIVE_VALID"
contains "doctor verifies 5 GLSL shaders & kinetic motion" "5 GLSL ES 3.00 shaders ready" "$(timeout 10 "$C/doctor.sh" --plain)"

# ═══════════════════════ 22. Velocity, Turbo & Liquid Glass ═══════════════
printf '\n\033[1m22. perf.sh, 0-fork runtime & squircle GPU pipeline\033[0m\n'

contains "perf status defaults to turbo=off glass=kawase" "turbo=off glass=kawase" "$(timeout 10 "$C/perf.sh" status)"
: > "$CALLS"
timeout 10 "$C/perf.sh" turbo on >/dev/null 2>&1
contains "turbo on enables allow_tearing & direct_scanout" "general:allow_tearing 1" "$(cat "$CALLS")"
contains "turbo on emits Waybar TURBO pill" "⚡ TURBO" "$(timeout 10 "$C/perf.sh" status --waybar)"
: > "$CALLS"
timeout 10 "$C/perf.sh" turbo off >/dev/null 2>&1
contains "turbo off restores squircle rounding_power 4.0" "decoration:rounding_power 4.0" "$(cat "$CALLS")"
contains "turbo off clears Waybar TURBO pill" '"class":"off"' "$(timeout 10 "$C/perf.sh" status --waybar)"

: > "$CALLS"
timeout 10 "$C/perf.sh" glass xray >/dev/null 2>&1
contains "glass xray enables 1-pass xray blur" "decoration:blur:xray 1" "$(cat "$CALLS")"
: > "$CALLS"
timeout 10 "$C/perf.sh" glass acrylic >/dev/null 2>&1
contains "glass acrylic sets variant acrylic with 0.0 aberration" "decoration:blur:acrylic:aberration 0.0" "$(cat "$CALLS")"
timeout 10 "$C/perf.sh" glass kawase >/dev/null 2>&1

: > "$CALLS"
timeout 10 "$C/perf.sh" vrr on >/dev/null 2>&1
contains "perf vrr on sets misc:vrr 1" "misc:vrr 1" "$(cat "$CALLS")"
timeout 10 "$C/perf.sh" vrr fullscreen >/dev/null 2>&1

BENCH_OUT="$(timeout 15 "$C/perf.sh" bench)"
contains "perf bench measures mono_var latency" "mono_var() lookup" "$BENCH_OUT"
contains "perf bench measures sysgraph latency" "sysgraph /proc sample" "$BENCH_OUT"

: > "$CALLS"
timeout 10 "$C/shader.sh" set oled-pitch >/dev/null 2>&1
contains "shader oled-pitch applies oled-pitch.frag" "oled-pitch.frag" "$(cat "$CALLS")"
timeout 10 "$C/shader.sh" set off >/dev/null 2>&1

SYS_JSON="$(timeout 10 "$XDG_CONFIG_HOME/waybar/scripts/sysgraph.sh")"
eq "zero-fork sysgraph.sh emits valid JSON" "ok" "$(printf '%s' "$SYS_JSON" | jq -r 'if (.text | length) > 0 and (.tooltip | length) > 0 then "ok" else "bad" end')"
contains "doctor verifies Velocity engine & squircle=4.0" "Velocity engine" "$(timeout 10 "$C/doctor.sh" --plain)"

if [ -n "$LUA_BIN" ]; then
    GPU_STATS="$("$LUA_BIN" -e '
        package.path = os.getenv("XDG_CONFIG_HOME") .. "/hypr/?.lua;" .. package.path
        local gestures, scanout, async_c, squircle, snap_on = 0, 0, false, 0, false
        local function mock()
            return setmetatable({}, {
                __call = function() return mock() end,
                __index = function() return mock() end,
            })
        end
        hl = setmetatable({
            gesture = function() gestures = gestures + 1 end,
            config = function(t)
                if t.render and t.render.direct_scanout then scanout = t.render.direct_scanout end
                if t.render and t.render.async_commit then async_c = true end
                if t.decoration and t.decoration.rounding_power then squircle = t.decoration.rounding_power end
                if t.general and t.general.snap and t.general.snap.enabled then snap_on = true end
            end,
            define_submap = function(_n, a, b) local fn = type(a) == "function" and a or b; if fn then fn() end end,
        }, { __index = function() return mock() end })
        dofile(os.getenv("XDG_CONFIG_HOME") .. "/hypr/hyprland.lua")
        print(string.format("gestures=%d scanout=%d async=%s squircle=%.1f snap=%s",
            gestures, scanout, tostring(async_c), squircle, tostring(snap_on)))
    ' 2>&1)"
    contains "appearance.lua configures 6 1:1 trackpad gestures" "gestures=6" "$GPU_STATS"
    contains "appearance.lua enables direct_scanout=2 & async_commit" "scanout=2 async=true" "$GPU_STATS"
    contains "appearance.lua enables G2 squircle=4.0 & magnetic snap" "squircle=4.0 snap=true" "$GPU_STATS"
fi

# ═══════════════════════ 23. Any PC, Any App & OpenCode Browser ══════════
printf '\n\033[1m23. Universal PC hardware, universal app opener & OpenCode browser\033[0m\n'

HW_JSON="$(timeout 10 "$C/hardware.sh" status --json)"
eq "hardware.sh status --json emits valid JSON" "ok" "$(printf '%s' "$HW_JSON" | jq -r 'if (.chassis | length) > 0 and (.gpus | length) > 0 then "ok" else "bad" end')"
timeout 10 "$C/hardware.sh" power performance >/dev/null 2>&1
contains "hardware.sh power sets performance profile" '"power_profile":"performance"' "$(timeout 10 "$C/hardware.sh" status --json)"
timeout 10 "$C/hardware.sh" power balanced >/dev/null 2>&1
timeout 10 "$C/hardware.sh" write-override >/dev/null 2>&1
[ -f "$XDG_CONFIG_HOME/hypr/modules/hardware-user.lua" ] && green "hardware.sh write-override creates hardware-user.lua" || red "hardware-user.lua missing"

mkdir -p "$HOME/Applications"
printf '#!/usr/bin/env bash\nexit 0\n' > "$HOME/Applications/DemoStudio.AppImage"
timeout 10 "$C/open-app.sh" sync-appimages >/dev/null 2>&1
if [ -f "$HOME/.local/share/applications/appimage-demostudio.desktop" ] && [ -x "$HOME/Applications/DemoStudio.AppImage" ]; then
    green "open-app.sh indexes ~/Applications/*.AppImage into .desktop entries"
else
    red "open-app.sh failed to index DemoStudio.AppImage"
fi

stub chromium 'exit 0'
stub zathura 'exit 0'
: > "$CALLS"
timeout 10 "$C/open-app.sh" browser "https://opencode.ai" >/dev/null 2>&1
sleep 0.15
contains "open-app.sh browser auto-falls back to installed chromium" "chromium https://opencode.ai" "$(cat "$CALLS")"

: > "$HOME/sample.pdf"
: > "$CALLS"
timeout 10 "$C/open-app.sh" open "$HOME/sample.pdf" >/dev/null 2>&1
sleep 0.15
contains "open-app.sh routes PDF files to monochrome zathura" "zathura $HOME/sample.pdf" "$(cat "$CALLS")"

FF_PROF="$HOME/.mozilla/firefox/abcd1234.default-release"
mkdir -p "$FF_PROF"
printf 'user_pref("custom.keep.me", true);\n' > "$FF_PROF/user.js"
timeout 10 "$C/browser-theme.sh" apply --quiet >/dev/null 2>&1
[ -f "$FF_PROF/chrome/userChrome.css" ] && green "browser-theme.sh installs OpenCode userChrome.css" || red "userChrome.css missing"
[ -f "$FF_PROF/chrome/userContent.css" ] && green "browser-theme.sh installs OpenCode userContent.css" || red "userContent.css missing"
contains "browser-theme.sh preserves existing user.js prefs" 'custom.keep.me' "$(cat "$FF_PROF/user.js")"
contains "browser-theme.sh enables legacy stylesheets & OpenCode homepage" 'toolkit.legacyUserProfileCustomizations.stylesheets' "$(cat "$FF_PROF/user.js")"
contains "browser-theme.sh status reports all 4 OpenCode targets ready" 'OpenCode TUI' "$(timeout 10 "$C/browser-theme.sh" status)"
contains "doctor verifies PC Hardware & OpenCode browser" 'OpenCode browser' "$(timeout 10 "$C/doctor.sh" --plain)"

if [ -n "$LUA_BIN" ]; then
    COMPAT_STATS="$("$LUA_BIN" -e '
        package.path = os.getenv("XDG_CONFIG_HOME") .. "/hypr/?.lua;" .. package.path
        local xw_zero, nv_af, java_fix, flatpak_dir = false, false, false, false
        local function mock()
            return setmetatable({}, {
                __call = function() return mock() end,
                __index = function() return mock() end,
            })
        end
        hl = setmetatable({
            env = function(k, v)
                if k == "_JAVA_AWT_WM_NONEREPARENTING" and v == "1" then java_fix = true end
                if k == "XDG_DATA_DIRS" and tostring(v):find("flatpak", 1, true) then flatpak_dir = true end
            end,
            config = function(t)
                if t.xwayland and t.xwayland.force_zero_scaling then xw_zero = true end
                if t.opengl and t.opengl.nvidia_anti_flicker then nv_af = true end
            end,
            define_submap = function(_n, a, b) local fn = type(a) == "function" and a or b; if fn then fn() end end,
        }, { __index = function() return mock() end })
        dofile(os.getenv("XDG_CONFIG_HOME") .. "/hypr/hyprland.lua")
        print(string.format("xw_zero=%s nv_af=%s java=%s flatpak=%s",
            tostring(xw_zero), tostring(nv_af), tostring(java_fix), tostring(flatpak_dir)))
    ' 2>&1)"
    eq "hardware.lua + hyprland.lua enable XWayland zero-scale, Nvidia anti-flicker, Java & Flatpak" \
       "xw_zero=true nv_af=true java=true flatpak=true" "$COMPAT_STATS"
fi

# ═══════════════ 24. Bespoke Cursor Suite & Keyboard Studio ══════════════
printf '\n\033[1m24. Bespoke Monochrome Cursor Suite & Keyboard Type Studio\033[0m\n'

mkdir -p "$XDG_CONFIG_HOME/gtk-3.0" "$XDG_CONFIG_HOME/gtk-4.0" "$XDG_CONFIG_HOME/kitty"
cp "$REPO/.config/gtk-3.0/settings.ini" "$XDG_CONFIG_HOME/gtk-3.0/"
cp "$REPO/.config/gtk-4.0/settings.ini" "$XDG_CONFIG_HOME/gtk-4.0/"

CUR_GEN_OUT="$HOME/cursor-build"
"$REPO/bin/cursor-gen" --out-dir "$CUR_GEN_OUT" >/dev/null 2>&1
[ -f "$CUR_GEN_OUT/Monochrome-Obsidian/manifest.hl" ] \
    && [ -f "$CUR_GEN_OUT/Monochrome-Ivory/hyprcursors/left_ptr.hlc" ] \
    && [ -f "$CUR_GEN_OUT/Monochrome-Precision/cursors/left_ptr" ] \
    && [ -f "$CUR_GEN_OUT/Monochrome-Dot/hyprcursors/watch.hlc" ] \
    && green "cursor-gen builds all 4 monochrome cursor themes (hyprcursor + Xcursor)" \
    || red "cursor-gen missing expected theme outputs"

XCUR_MAGIC="$(head -c 4 "$CUR_GEN_OUT/Monochrome-Obsidian/cursors/left_ptr")"
eq "Xcursor binary carries valid Xcur header" "Xcur" "$XCUR_MAGIC"

HLC_MAGIC="$(head -c 2 "$CUR_GEN_OUT/Monochrome-Obsidian/hyprcursors/left_ptr.hlc")"
eq "hyprcursor .hlc package carries valid ZIP PK header" "PK" "$HLC_MAGIC"

: > "$CALLS"
timeout 10 "$C/cursor.sh" theme ivory >/dev/null 2>&1
contains "cursor.sh theme ivory calls hyprctl setcursor" "setcursor Monochrome-Ivory 24" "$(cat "$CALLS")"
contains "cursor.sh syncs gtk-3.0 cursor theme" "Monochrome-Ivory" "$(cat "$XDG_CONFIG_HOME/gtk-3.0/settings.ini")"
[ -f "$XDG_CONFIG_HOME/hypr/modules/cursor-user.lua" ] \
    && green "cursor.sh writes persistent cursor-user.lua" \
    || red "cursor-user.lua missing"

timeout 10 "$C/cursor.sh" cycle >/dev/null 2>&1
contains "cursor.sh cycle advances Ivory -> Precision" "theme=Monochrome-Precision" "$(timeout 10 "$C/cursor.sh" status)"

timeout 10 "$C/cursor.sh" size 32 >/dev/null 2>&1
contains "cursor.sh size updates pointer size to 32px" "size=32" "$(timeout 10 "$C/cursor.sh" status)"

timeout 10 "$C/cursor.sh" kitty beam >/dev/null 2>&1
contains "cursor.sh kitty beam updates terminal cursor config" "cursor_shape                 beam" "$(cat "$XDG_CONFIG_HOME/kitty/cursor-user.conf")"

: > "$CALLS"
timeout 10 "$C/keyboard.sh" type compact60 >/dev/null 2>&1
contains "keyboard.sh type compact60 applies caps:escape" "keyword input:kb_options caps:escape" "$(cat "$CALLS")"

timeout 10 "$C/keyboard.sh" layout colemak-dh >/dev/null 2>&1
contains "keyboard.sh layout colemak-dh sets CMK-DH Waybar pill" '"text":"CMK-DH"' "$(timeout 10 "$C/keyboard.sh" status --waybar)"

timeout 10 "$C/keyboard.sh" layout us-ara >/dev/null 2>&1
contains "keyboard.sh dual layout preserves caps:escape + grp toggle" "caps:escape,grp:alt_shift_toggle" "$(cat "$XDG_CONFIG_HOME/hypr/modules/keyboard-user.lua")"
timeout 10 "$C/keyboard.sh" next >/dev/null 2>&1
contains "keyboard.sh next switches active badge US -> ARA" '"text":"ARA"' "$(timeout 10 "$C/keyboard.sh" status --waybar)"

timeout 10 "$C/keyboard.sh" rate rapid >/dev/null 2>&1
contains "keyboard.sh rate rapid sets 55Hz / 190ms" "rate=rapid(55Hz/190ms)" "$(timeout 10 "$C/keyboard.sh" status)"
contains "doctor verifies Input & Cursor studio" "Input & Cursor" "$(timeout 10 "$C/doctor.sh" --plain)"

# ═══════════════════════ 25. Pure C11 Hyprland AF_UNIX IPC (mono-ipc) ═════
printf '\n\033[1m25. mono-ipc.c Pure C11 AF_UNIX .socket.sock / .socket2.sock & telemetry\033[0m\n'

IPC_RT="$WORK/ipc-rt"
mkdir -p "$IPC_RT"
"$REPO/bin/mono-ipc" --mock-server "$IPC_RT" "his-c11" >/dev/null 2>&1 &
IPC_SRV=$!
sleep 0.1
eq "mono-ipc queries .socket.sock j/version over AF_UNIX" "v0.56.0-c11-ipc" \
    "$(XDG_RUNTIME_DIR="$IPC_RT" HYPRLAND_INSTANCE_SIGNATURE="his-c11" timeout 5 "$REPO/bin/mono-ipc" version-tag)"
eq "mono-ipc queries .socket.sock j/activewindow over AF_UNIX" "kitty-ipc" \
    "$(XDG_RUNTIME_DIR="$IPC_RT" HYPRLAND_INSTANCE_SIGNATURE="his-c11" timeout 5 "$REPO/bin/mono-ipc" win-class)"
contains "mono-ipc streams .socket2.sock EVENT>>DATA frame" "activelayout>>" \
    "$(XDG_RUNTIME_DIR="$IPC_RT" HYPRLAND_INSTANCE_SIGNATURE="his-c11" timeout 5 "$REPO/bin/mono-ipc" watch --once)"
wait "$IPC_SRV" 2>/dev/null || true

contains "mono-ipc sysgraph emits valid Waybar JSON sparkline" '"tooltip":"CPU ' "$("$REPO/bin/mono-ipc" sysgraph)"
contains "mono-ipc focus-json encodes Waybar Pomodoro pill" '"class":"on"' "$("$REPO/bin/mono-ipc" focus-json on "24:59" "Deep Work")"
eq "zero jq calls remain across all .config runtime scripts" "0" \
    "$(grep -rn '\bjq\b' "$REPO/.config/hypr/scripts/" "$REPO/.config/waybar/scripts/" 2>/dev/null | wc -l | tr -d ' ')"

contains "mono-tui --snapshot renders Pure ISO C11 Control Deck" "Pure ISO C11 Control Deck" "$(timeout 5 "$C/tui.sh" --snapshot)"
contains "mono-tui --json emits valid compositor & CPU telemetry" '"cursor_theme":' "$(timeout 5 "$C/tui.sh" --json)"
contains "mono-tui --bench runs CLOCK_MONOTONIC microsecond audit" "telemetry_proc_sys" "$(timeout 5 "$C/tui.sh" --bench)"

contains "website showcases #c11-runtime section" 'id="c11-runtime"' "$(cat "$REPO/site/index.html")"
contains "website reports all 12 Pure C11 tools" '12 Native C11 Tools' "$(cat "$REPO/site/index.html")"
contains "website keeps sections visible without JavaScript" 'opacity: 1;' "$(cat "$REPO/site/styles.css")"
contains "website guards scroll observers for older browsers" '"IntersectionObserver" in window' "$(cat "$REPO/site/app.js")"
contains "website app.js drives interactive mono-tui & mono-ipc" '__setMonoIpcQuery' "$(cat "$REPO/site/app.js")"
eq "website keeps 0 opencode mentions" "0" \
    "$(grep -ri 'opencode' "$REPO/site/index.html" "$REPO/site/styles.css" "$REPO/site/app.js" "$REPO/site/newtab.html" 2>/dev/null | wc -l | tr -d ' ')"
eq "website keeps 0 em-dash characters" "0" \
    "$(grep -rn '—' "$REPO/site/index.html" "$REPO/site/styles.css" "$REPO/site/app.js" "$REPO/site/newtab.html" 2>/dev/null | wc -l | tr -d ' ')"

# ═══════════════════════ result ══════════════════════════════════════════
printf '\n\033[2m%s\033[0m\n' "$M_RULE"
if [ "$FAIL" -eq 0 ]; then
    printf '  \033[32m%s checks passed\033[0m\n\n' "$PASS"
    exit 0
fi
printf '  \033[32m%s passed\033[0m, \033[31m%s failed\033[0m\n\n' "$PASS" "$FAIL"
[ "$VERBOSE" = "1" ] && { printf 'stub call log:\n'; sed 's/^/    /' "$CALLS"; }
exit 1
