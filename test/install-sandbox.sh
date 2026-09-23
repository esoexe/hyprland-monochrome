#!/usr/bin/env bash
# Sandbox test for install.sh and uninstall.sh.
#
#   ./test/install-sandbox.sh
#
# Creates a throw-away HOME with a fake pre-existing config, stubs pacman/sudo/
# hyprctl, then checks that installing, re-installing, --dry-run, --link, the
# restore script and the uninstaller all behave correctly.
# Nothing outside the temporary directory is touched.
set -uo pipefail

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
else
    M_OK='\033[32mok\033[0m'; M_BAD='\033[31m!!\033[0m'
fi

REPO="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
SANDBOX=$(mktemp -d)
export HOME="$SANDBOX/home"
export XDG_CONFIG_HOME="$HOME/.config"
export XDG_CACHE_HOME="$HOME/.cache"
export XDG_DATA_HOME="$HOME/.local/share"
BIN="$SANDBOX/bin"
mkdir -p "$HOME/.config/hypr" "$HOME/.config/kitty" "$HOME/Pictures" "$BIN"

# ── pre-existing config that must be backed up, not lost ──────────────────
echo "MY OLD HYPRLAND CONFIG" > "$HOME/.config/hypr/hyprland.conf"
echo "old kitty conf"         > "$HOME/.config/kitty/kitty.conf"
echo "my shell rc"            > "$HOME/.zshrc"
mkdir -p "$HOME/.config/waybar"
echo "old bar"                > "$HOME/.config/waybar/config.jsonc"

# ── stubs ─────────────────────────────────────────────────────────────────
cat > "$BIN/pacman" <<'EOF'
#!/usr/bin/env bash
case "$1" in
  -Si) exit 0 ;;
  -S)  echo "pacman $*" >> "$PACMAN_LOG"; exit 0 ;;
  -Q)  echo "pkg1"; exit 0 ;;
  *)   exit 0 ;;
esac
EOF
cat > "$BIN/sudo" <<'EOF'
#!/usr/bin/env bash
exec "$@"
EOF
cat > "$BIN/hyprctl" <<'EOF'
#!/usr/bin/env bash
[ "$1" = configerrors ] && { echo "no errors"; exit 0; }
exit 0
EOF
for c in chsh systemctl getent bluetoothctl spicetify spotify zsh; do
    printf '#!/usr/bin/env bash\nexit 1\n' > "$BIN/$c"
done
chmod +x "$BIN"/*
export PATH="$BIN:$PATH"
export PACMAN_LOG="$SANDBOX/pacman.log"

run_test() {
    local desc="$1"; shift
    local args="$1"; shift

    rm -rf "$SANDBOX/run"; mkdir -p "$SANDBOX/run"
    export HOME="$SANDBOX/home"
    printf '  %-52s' "$desc"
    # shellcheck disable=SC2086
    out="$(cd "$REPO" && bash install.sh $args </dev/null 2>&1)"
    code=$?
    if [ "$code" -eq 0 ]; then echo "PASS (exit 0)"; else echo "FAIL (exit $code)"; printf '%s\n' "$out" | tail -12; fi
}

echo "── installer sandbox test ───────────────────────────────────"
run_test "1. full install (--yes)" "--yes"
echo "     files installed:"
for f in "$HOME/.config/hypr/hyprland.lua" "$HOME/.config/hypr/modules/binds.lua" \
         "$HOME/.config/rofi/theme.rasi" "$HOME/.config/waybar/config.jsonc" \
         "$HOME/.config/kitty/kitty.conf" "$HOME/.zshrc" "$HOME/.config/starship.toml"; do
    if [ -e "$f" ]; then printf '       %b %s\n' "$M_OK" "${f#"$HOME"/}"
    else printf '       %b %s\n' "$M_BAD" "MISSING ${f#"$HOME"/}"; fi
done
echo "     executables:"
for f in "$HOME/.config/hypr/scripts"/*.sh; do
    [ -x "$f" ] || printf '       %b %s\n' "$M_BAD" "not executable: $(basename "$f")"
done
echo "       (all scripts executable if nothing above)"
echo "     backups:"
ls "$HOME/.config-backups"/*/manifest.txt >/dev/null 2>&1 && cat "$HOME/.config-backups"/*/manifest.txt | sed 's/^/       backed up: /'
run_backup=""
for d in "$HOME"/.config-backups/*/; do
    case "$d" in
        */pristine/) continue ;;
        *) run_backup="$d"; break ;;
    esac
done
if [ -x "$HOME/.config-backups/pristine/restore.sh" ] && [ -n "$run_backup" ] && [ -x "${run_backup}restore.sh" ]; then
    printf '       %b %s\n' "$M_OK" "restore.sh present in both backup folders"
fi
echo "     wallpapers copied: $(ls "$HOME/Pictures/Wallpapers" 2>/dev/null | wc -l) file(s)"
echo "     pacman was called: $(wc -l < "$PACMAN_LOG" 2>/dev/null || echo 0) time(s)"

echo "     pristine keeps the originals:"
for f in kitty/kitty.conf zshrc_file; do :; done
if grep -q "old kitty conf" "$HOME/.config-backups/pristine/.config/kitty/kitty.conf" 2>/dev/null; then
    printf '       %b %s\n' "$M_OK" "pristine/kitty.conf still holds the user's original"
else
    printf '       %b %s\n' "$M_BAD" "pristine does NOT hold the original kitty.conf"
fi

echo
echo "── restore path ─────────────────────────────────────────────"
"$HOME/.config-backups/pristine/restore.sh" >/dev/null 2>&1
for f in "hypr/hyprland.conf" "kitty/kitty.conf"; do
    if [ -f "$HOME/.config/$f" ]; then printf '       %b %s\n' "$M_OK" "restored $f"
    else printf '       %b %s\n' "$M_BAD" "NOT restored $f"; fi
done
if [ -f "$HOME/.zshrc" ] && grep -q "my shell rc" "$HOME/.zshrc"; then
    printf '       %b %s\n' "$M_OK" "restored ~/.zshrc"
fi

echo
echo "── second run (idempotency, --link, --no-packages) ──────────"
run_test "2. rerun --yes --no-packages" "--yes --no-packages"
run_test "3. --dry-run changes nothing" "--dry-run --yes"
[ -L "$HOME/.config/hypr/modules/user.lua" ] && echo "       (dry-run correctly did not symlink)"
run_test "4. --link install" "--yes --no-packages --link"
[ -L "$HOME/.config/hypr/hyprland.lua" ] && printf '       %b %s\n' "$M_OK" "--link created symlinks"
mv "$BIN/pacman" "$BIN/.pacman-hidden"
run_test "5. --no-packages without pacman (non-Arch)" "--yes --no-packages --link"
mv "$BIN/.pacman-hidden" "$BIN/pacman"

echo
echo "── uninstall ────────────────────────────────────────────────"
printf 'y\n' | HOME="$HOME" bash "$REPO/uninstall.sh" >/dev/null 2>&1
if [ ! -e "$HOME/.config/kitty/kitty.conf" ] || [ -L "$HOME/.config/kitty" ]; then
    printf '       %b %s\n' "$M_OK" "target files removed"
else
    echo "       ? checking removal:"
    ls "$HOME/.config" | sed 's/^/         /'
fi
if [ -f "$HOME/.config/kitty/kitty.conf" ] && grep -q "old kitty conf" "$HOME/.config/kitty/kitty.conf"; then
    printf '       %b %s\n' "$M_OK" "original kitty.conf restored"
else
    printf '       %b %s\n' "$M_BAD" "the original kitty.conf was NOT restored"
    tail -5 "$SANDBOX/uninstall.log" | sed 's/^/         /'
fi

echo
echo "sandbox: $SANDBOX   (delete it when done)"
