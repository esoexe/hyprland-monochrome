#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════════
#  hyprland-monochrome — uninstaller / rollback
#
#     ./uninstall.sh              restore the newest backup, remove the configs
#     ./uninstall.sh --list       show available backups
#     ./uninstall.sh --from <dir> restore a specific backup
#     ./uninstall.sh --keep-files only restore backups, keep the new config too
#
#  Packages are never removed automatically — see packages.txt if you want to
#  clean those up yourself.
# ═══════════════════════════════════════════════════════════════════════════

set -uo pipefail

CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}"
BACKUP_ROOT="$HOME/.config-backups"

TARGET_FILES=(
    "hypr"
    "waybar"
    "rofi"
    "kitty"
    "dunst"
    "btop"
    "fastfetch"
    "wlogout"
    "gtk-3.0/settings.ini"
    "gtk-4.0/settings.ini"
    "spicetify/Themes/monochrome"
    "starship.toml"
    "fastfetch/config.jsonc"
    "btop/btop.conf"
    "btop/themes/mono.theme"
)

BOLD=$'\033[1m'; GREEN=$'\033[32m'; YELLOW=$'\033[33m'; DIM=$'\033[2m'; RESET=$'\033[0m'
info() { printf '%s==>%s %s\n' "$BOLD" "$RESET" "$1"; }
ok()   { printf '  %s✓%s %s\n' "$GREEN" "$RESET" "$1"; }
warn() { printf '  %s!%s %s\n' "$YELLOW" "$RESET" "$1"; }
ask() {  # ask <variable> — prompt on the terminal when there is one
    # The terminal is only consulted when stdin is itself a terminal. Answering
    # through a pipe (tests, CI, `yes | uninstall.sh`) must not be ignored, or
    # the prompt waits on /dev/tty forever.
    local __var="$1" __reply=""
    if [ -t 0 ] && [ -r /dev/tty ]; then
        read -r __reply </dev/tty 2>/dev/null || read -r __reply || __reply=""
    else
        read -r __reply || __reply=""
    fi
    printf -v "$__var" '%s' "$__reply"
}

RESTORE_FROM=""
KEEP_FILES=0

while [ $# -gt 0 ]; do
    case "$1" in
        --list)
            info "Backups in $BACKUP_ROOT:"
            ls -1 "$BACKUP_ROOT" 2>/dev/null | sed 's/^/  /' || warn "No backups found."
            printf '\n  %spristine%s = your files as they were before the first install\n' "$BOLD" "$RESET"
            exit 0 ;;
        --from)  RESTORE_FROM="${2:-}"; shift ;;
        --keep-files) KEEP_FILES=1 ;;
        -h|--help) sed -n '2,15p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) warn "Unknown option: $1" ;;
    esac
    shift
done

# ── 1. Remove the installed configuration ─────────────────────────────────
# Order matters: clear the paths we installed first, then put the user's own
# files back — otherwise the restore would be undone by the removal.
if [ "$KEEP_FILES" = "0" ]; then
    info "Removing installed configuration"
    printf '  This deletes: %s\n' "$(printf '%s ' "${TARGET_FILES[@]}")"
    printf '  Continue? [y/N] '
    answer=""
    ask answer
    if [[ "${answer,,}" == "y" || "${answer,,}" == "yes" ]]; then
        for rel in "${TARGET_FILES[@]}"; do
            target="$CONFIG_DIR/$rel"
            if [ -e "$target" ] || [ -L "$target" ]; then
                rm -rf "$target" && ok "removed ~/.config/$rel"
            fi
        done
        for f in "$HOME/.zshrc" "$HOME/.config/starship.toml"; do
            [ -e "$f" ] && rm -f "$f" && ok "removed ${f#$HOME/}"
        done
    else
        info "Kept the configuration files (restoring backups only)."
    fi
fi

# ── 2. Put the user's own files back ──────────────────────────────────────
# The "pristine" folder holds the files as they were before the very first
# install, so that is the most faithful thing to restore.
printf '\n'
if [ -z "$RESTORE_FROM" ]; then
    if [ -d "$BACKUP_ROOT/pristine" ]; then
        RESTORE_FROM="$BACKUP_ROOT/pristine"
    else
        RESTORE_FROM="$(ls -1dt "$BACKUP_ROOT"/*/ 2>/dev/null | head -1)"
    fi
fi

if [ -n "$RESTORE_FROM" ] && [ -d "$RESTORE_FROM" ]; then
    info "Restoring your files from $RESTORE_FROM"
    if [ -x "$RESTORE_FROM/restore.sh" ]; then
        "$RESTORE_FROM/restore.sh"
    elif [ -f "$RESTORE_FROM/manifest.txt" ]; then
        while IFS= read -r rel; do
            [ -n "$rel" ] || continue
            src="$RESTORE_FROM/$rel"; dst="$HOME/$rel"
            mkdir -p "$(dirname "$dst")"
            rm -rf "$dst"
            cp -a "$src" "$dst" && ok "restored $rel"
        done < "$RESTORE_FROM/manifest.txt"
    else
        warn "That backup has no manifest — nothing to restore from it."
    fi
else
    warn "No backup found in $BACKUP_ROOT (nothing to restore)."
fi

warn "Wallpapers in ~/Pictures/Wallpapers were left in place (delete them yourself if you want)."

printf '\n'
info "Done. Log out and back in."
printf '  %sLeft on disk:%s\n' "$DIM" "$RESET"
printf '    • pip/pacman packages from packages.txt (remove manually if unwanted)\n'
printf '    • backups in %s\n' "$BACKUP_ROOT"
printf '    • ~/Pictures/Wallpapers and ~/Pictures/Screenshots\n'
