#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════════
#  tools/demo.sh — a scripted walkthrough of this project.
#
#  It runs the real test suite, the real generators and a live demo of
#  record.sh, with short pauses so the output stays readable. This is what
#  docs/test-run.gif was recorded from:
#
#      bash tools/demo.sh                                   # watch it here
#      asciinema rec -c "bash tools/demo.sh" demo.cast      # record it
#
#  Nothing is faked. The compositor-side scripts are exercised through stubs —
#  the same ones the test suite uses — because a demo machine is usually not
#  running a Hyprland session.
# ═══════════════════════════════════════════════════════════════════════════

set -u
cd "$(dirname "$0")/.." || exit 1
REPO="$PWD"

B=$'\033[1m'; D=$'\033[2m'; G=$'\033[32m'; C=$'\033[36m'; R=$'\033[0m'

prompt() { printf '%s%s%s %s$%s ' "$D" "$(basename "$REPO")" "$R" "$G" "$R"; }
say()    { printf '\n%s▎ %s%s\n' "$B$C" "$1" "$R"; }
dim()    { printf '%s%s%s\n' "$D" "$1" "$R"; }
pause()  { sleep "${1:-1}"; }

cmd() {  # cmd <command…> — show the prompt, then really run it
    prompt; printf '%s%s%s\n' "$B" "$*" "$R"
    pause 0.7
    "$@"
}

DEMO="$(mktemp -d)"
cleanup() { rm -rf "$DEMO"; }
trap cleanup EXIT

# ── opening ──────────────────────────────────────────────────────────────
printf '\033[2J\033[H'
say "hyprland-monochrome — test run"
dim "A monochrome Hyprland setup for Arch-based distros."
dim "This is a recording of its test suite: shell syntax, shellcheck, Lua, JSON/TOML,"
dim "100 script checks, an install into a throw-away HOME, and screen recording."
pause 3

# ── 1. the whole suite ───────────────────────────────────────────────────
say "1 · the whole suite"
cmd bash test/run-all.sh
pause 2

# ── 2. install sandbox ───────────────────────────────────────────────────
say "2 · installing into a throw-away HOME"
dim "Backups, restore, --link and uninstall — without touching this machine."
pause 1.5
prompt; printf '%sbash test/install-sandbox.sh%s\n' "$B" "$R"
pause 0.7
bash test/install-sandbox.sh 2>&1 | tail -24 | sed 's/^/  /'
pause 2.5

# ── 3. generated docs ────────────────────────────────────────────────────
say "3 · the docs are generated from the config (C++20)"
[ -x bin/keybind-table ] || make -s -j"$(nproc 2>/dev/null || echo 2)"
cmd ./bin/keybind-table
cmd ./bin/build-preview --check
dim "docs/KEYBINDS.md — parsed out of modules/binds.lua, never hand-edited:"
pause 1.2
sed -n '/^## Applications/,/^## Window/p' docs/KEYBINDS.md | head -13 | sed 's/^/  /'
pause 2.5

# ── 4. screen recording ──────────────────────────────────────────────────
say "4 · screen recording"
dim "record.sh drives gpu-screen-recorder when it is installed and wf-recorder"
dim "otherwise. This machine has neither, so it runs against a stub recorder."
pause 2.5

# a throw-away HOME with this repo's config, plus stub tools
DEMO_HOME="$DEMO/home"
mkdir -p "$DEMO_HOME/.config/hypr/modules" "$DEMO_HOME/.config/hypr/scripts" "$DEMO/bin"
cp .config/hypr/modules/user.lua "$DEMO_HOME/.config/hypr/modules/"
cp .config/hypr/scripts/*.sh "$DEMO_HOME/.config/hypr/scripts/"
cp "$DEMO_HOME/.config/hypr/scripts/record.sh" "$DEMO/bin/"

cat > "$DEMO/bin/gpu-screen-recorder" <<'STUB'
#!/usr/bin/env bash
out=""; prev=""
for a in "$@"; do [ "$prev" = "-o" ] && out="$a"; prev="$a"; done
mkdir -p "$(dirname "$out")"; printf 'video data' > "$out"     # appears at start
trap 'printf "video data" > "$out"; exit 0' INT                # SIGINT = save
while :; do sleep 0.2; done
STUB
chmod +x "$DEMO/bin/gpu-screen-recorder"

# the notification the user would see, printed instead
cat > "$DEMO/bin/notify-send" <<'STUB'
#!/usr/bin/env bash
n=$#; args=("$@")
printf '  \033[2m[ notification ]\033[0m %s — %s\n' "${args[n-2]}" "${args[n-1]}"
STUB
chmod +x "$DEMO/bin/notify-send"

export PATH="$DEMO/bin:$PATH"
run_rec() {  # show ~/.config/hypr/scripts/…, run it against the demo HOME
    prompt
    printf '%s~/.config/hypr/scripts/record.sh %s%s\n' "$B" "$*" "$R"
    pause 0.7
    HOME="$DEMO_HOME" XDG_CONFIG_HOME="$DEMO_HOME/.config" \
        XDG_CACHE_HOME="$DEMO_HOME/.cache" PATH="$DEMO/bin:$PATH" \
        "$DEMO_HOME/.config/hypr/scripts/record.sh" "$@" 2>&1 \
        | sed -e "s|$DEMO_HOME|~|g"
}

run_rec start screen
pause 1.5
run_rec status
pause 1.5
run_rec stop
pause 1.2
dim "  the file, in ~/Videos/Recordings:"
ls -1 "$DEMO_HOME/Videos/Recordings" 2>/dev/null | sed 's/^/    /'
pause 2

# ── wrap up ──────────────────────────────────────────────────────────────
say "That is the whole loop."
dim "Everything you just watched is reproducible:"
dim "   make test                      # C++20 + shellcheck + Lua + 178 script checks"
dim "   make docs                      # rebuild KEYBINDS.md, windows/* & preview.html"
dim "   Super + Alt + Print            # screen recording, and Super + Alt + S for a replay"
pause 3
printf '\n'
