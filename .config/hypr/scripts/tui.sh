#!/usr/bin/env bash
# Pure ISO C11 Monochrome Terminal Control Deck (`mono-tui`, Super + Ctrl + M).
#
#   tui.sh              launch interactive 6-tab TUI inside floating Kitty
#   tui.sh --direct     run mono-tui directly on current TTY
#   tui.sh --snapshot   print single monochrome dashboard frame to stdout
#   tui.sh --json       emit full system + compositor telemetry as JSON
#   tui.sh --bench      run CLOCK_MONOTONIC Pure C11 microsecond benchmark

set -u
. "$(dirname "$0")/lib.sh"

resolve_tui_bin() {
    local cand
    for cand in \
        "$HYPR_DIR/bin/mono-tui" \
        "$SCRIPTS_DIR/../../../bin/mono-tui" \
        "$CACHE_DIR/mono-tui"
    do
        if [ -x "$cand" ]; then
            printf '%s\n' "$cand"
            return 0
        fi
    done

    local src="$HYPR_DIR/src/mono-tui.c"
    local cc_bin=""
    if [ -f "$src" ]; then
        if have cc; then cc_bin="cc"
        elif have gcc; then cc_bin="gcc"
        elif have clang; then cc_bin="clang"
        fi
        if [ -n "$cc_bin" ]; then
            mkdir -p "$CACHE_DIR"
            if "$cc_bin" -std=c11 -O2 "$src" -o "$CACHE_DIR/mono-tui" -lm 2>/dev/null; then
                printf '%s\n' "$CACHE_DIR/mono-tui"
                return 0
            fi
        fi
    fi
    return 1
}

TUI_BIN="$(resolve_tui_bin)" || {
    notify "Control Deck" "mono-tui compiler unavailable (install gcc or clang)" "dialog-error"
    exit 1
}

case "${1:-}" in
    --snapshot|snapshot) exec "$TUI_BIN" --snapshot ;;
    --json|json)         exec "$TUI_BIN" --json ;;
    --bench|bench)       exec "$TUI_BIN" --bench ;;
    --direct|direct)     exec "$TUI_BIN" ;;
    *)
        TERM_FLOAT="$(mono_var terminal_float 'kitty --class kitty-float')"
        if have kitty; then
            exec $TERM_FLOAT -e "$TUI_BIN"
        else
            exec "$TUI_BIN"
        fi
        ;;
esac
