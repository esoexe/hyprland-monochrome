#!/usr/bin/env bash
# Run every check this repository has, in one go.
#
#     ./test/run-all.sh
#
#   • shell syntax + shellcheck (if installed) on every script
#   • Lua syntax (luac, if installed) on the Hyprland config
#   • JSON validity of the Waybar + fastfetch configs
#   • the script smoke test (test/smoke-test.sh)
#   • documentation drift (tools/keybind-table.py --check)
#   • preview page drift (tools/build-preview.py --check)
#   • executable bits on every file that has a shebang
#   • the Windows kit (windows/) matching the palette

set -uo pipefail
REPO="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO" || exit 1


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
    M_SKIP='\033[33m·\033[0m'; M_RULE='────────────────────────────────────────────'
else
    M_OK='\033[32mok\033[0m'; M_BAD='\033[31m!!\033[0m'
    M_SKIP='\033[33m--\033[0m'; M_RULE='--------------------------------------------'
fi

FAIL=0
SKIPPED=0
SKIP_WHAT=""
green() { printf '  %b %s\n' "$M_OK" "$1"; }
red()   { printf '  %b %s\n' "$M_BAD" "$1"; FAIL=$((FAIL + 1)); }
skip()  {  # skip <tool>  — remembered, so the summary cannot claim a full pass
    printf '  %b %s (skipped: not installed)\n' "$M_SKIP" "$1"
    SKIPPED=$((SKIPPED + 1)); SKIP_WHAT="$SKIP_WHAT $1"
}

printf '\n\033[1m1. shell syntax\033[0m\n'
while IFS= read -r f; do
    if bash -n "$f" 2>/dev/null; then green "$f"; else red "$f"; bash -n "$f"; fi
done < <(find . -name '*.sh' -not -path './.git/*' | sort)

printf '\n\033[1m2. shellcheck\033[0m\n'
if command -v shellcheck >/dev/null 2>&1; then
    mapfile -t SHELL_FILES < <(find . -name '*.sh' -not -path './.git/*' | sort)
    if out="$(shellcheck -S warning -f gcc "${SHELL_FILES[@]}" 2>&1)"; then
        green "no warnings"
    else
        red "shellcheck found issues"; printf '%s\n' "$out" | sed 's/^/      /'
    fi
else
    skip "shellcheck"
fi

printf '\n\033[1m3. Lua config\033[0m\n'
LUAC="$(command -v luac5.4 || command -v luac || true)"
if [ -n "$LUAC" ]; then
    for f in .config/hypr/hyprland.lua .config/hypr/modules/*.lua .config/nvim/init.lua .config/nvim/colors/*.lua; do
        if "$LUAC" -p "$f" 2>/dev/null; then green "$f"; else red "$f"; "$LUAC" -p "$f"; fi
    done
else
    skip "luac (pacman -S lua)"
fi

printf '\n\033[1m4. JSON + TOML configs\033[0m\n'
python3 - <<'PYJSON' && green "waybar, fastfetch, starship + windows-terminal configs parse" || FAIL=$((FAIL+1))
import json, pathlib, re, sys, tomllib
def load(p):
    s = re.sub(r"(?m)^\s*//.*$", "", open(p).read())
    return json.loads(re.sub(r"/\*.*?\*/", "", s, flags=re.S))
for p in (".config/waybar/config.jsonc", ".config/fastfetch/config.jsonc",
          "windows/windows-terminal-scheme.json", "windows/windows-terminal-settings.json"):
    load(p)
tomllib.loads(pathlib.Path(".config/starship.toml").read_text())
sys.exit(0)
PYJSON

printf '\n\033[1m5. script smoke test\033[0m\n'
if bash test/smoke-test.sh; then green "smoke test passed"; else red "smoke test failed"; fi

printf '\n\033[1m6. documentation in sync\033[0m\n'
if python3 tools/keybind-table.py --check >/dev/null 2>&1; then
    green "docs/KEYBINDS.md matches binds.lua"
else
    red "docs/KEYBINDS.md is stale — run: python3 tools/keybind-table.py"
fi

printf '\n\033[1m7. preview page in sync\033[0m\n'
if python3 tools/build-preview.py --check >/dev/null 2>&1; then
    green "docs/preview.html matches the config"
else
    red "docs/preview.html is stale — run: python3 tools/build-preview.py"
fi

printf '\n\033[1m8. executable bits\033[0m\n'
# What matters is what a fresh clone gets, so git's recorded mode is the source
# of truth. A working tree that drifted (some sync tools drop the bit) is
# repaired in place rather than failing the suite.
NOT_EXEC=""; REPAIRED=""
if git rev-parse --git-dir >/dev/null 2>&1; then
    while IFS= read -r entry; do
        mode="${entry%% *}"; rest="${entry#* }"; f="${rest#*	}"
        [ -f "$f" ] || continue
        head -c 2 "$f" 2>/dev/null | grep -q '#!' || continue
        if [ "$mode" != "100755" ]; then
            NOT_EXEC="$NOT_EXEC $f"
        elif [ ! -x "$f" ]; then
            chmod +x "$f" 2>/dev/null && REPAIRED="$REPAIRED $f"
        fi
    done < <(git ls-files -s)
else
    while IFS= read -r f; do
        head -c 2 "$f" 2>/dev/null | grep -q '#!' || continue
        [ -x "$f" ] || NOT_EXEC="$NOT_EXEC $f"
    done < <(find . -type f -not -path './.git/*' | sed 's|^\./||')
fi
if [ -n "$NOT_EXEC" ]; then
    red "committed without the executable bit:$NOT_EXEC"
else
    green "every file with a shebang is committed executable"
    [ -n "$REPAIRED" ] && green "restored the bit locally on:$REPAIRED"
fi

printf '\n\033[1m9. windows kit in sync\033[0m\n'
if python3 tools/build-windows-kit.py --check >/dev/null 2>&1; then
    green "windows/ matches the palette and modules/user.lua"
else
    red "windows/ is stale — run: python3 tools/build-windows-kit.py"
fi
if grep -qP '^\t' windows/glazewm-config.yaml 2>/dev/null; then
    red "windows/glazewm-config.yaml contains tabs (YAML forbids them)"
else
    green "windows/glazewm-config.yaml has no tabs"
fi

printf '\n\033[2m%s\033[0m\n' "$M_RULE"
if [ "$FAIL" -gt 0 ]; then printf '  \033[31m%s check(s) failed\033[0m\n\n' "$FAIL"; exit 1; fi
if [ "$SKIPPED" -gt 0 ]; then
    printf '  \033[32mall checks passed\033[0m \033[33m— %s step(s) skipped:%s\033[0m\n' "$SKIPPED" "$SKIP_WHAT"
    printf '  \033[2minstall them to check the whole project:\033[0m\n'
    printf '  \033[2m  shellcheck: pacman -S shellcheck     luac: pacman -S lua\033[0m\n\n'
    exit 0
fi
printf '  \033[32mall checks passed\033[0m\n\n'; exit 0
