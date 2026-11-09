#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════════
#  hyprland-monochrome — installer for Arch-based systems
#  Arch · EndeavourOS · CachyOS · Manjaro · Garuda · …
#
#  Usage:
#     ./install.sh                 install everything (asks before changes)
#     ./install.sh --yes           no questions, sane defaults
#     ./install.sh --no-packages   config files only
#     ./install.sh --link          symlink the configs instead of copying
#     ./install.sh --dry-run       show what would happen, change nothing
#     ./install.sh --help
#
#  Everything that already exists in ~/.config is backed up first to
#  ~/.config-backups/<timestamp>/ together with a restore script.
# ═══════════════════════════════════════════════════════════════════════════

set -uo pipefail

REPO_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}"
BACKUP_ROOT="$HOME/.config-backups"
TIMESTAMP="$(date '+%Y-%m-%d_%H-%M-%S')"
BACKUP_DIR="$BACKUP_ROOT/$TIMESTAMP"
PRISTINE_DIR="$BACKUP_ROOT/pristine"   # the user's true originals, written once
WALLPAPER_DIR="$HOME/Pictures/Wallpapers"
SCREENSHOT_DIR="$HOME/Pictures/Screenshots"

ASSUME_YES=0
DO_PACKAGES=1
DO_AUR=1
DO_BACKUP=1
USE_LINKS=0
DRY_RUN=0
SET_ZSH=1

# ── Output helpers ────────────────────────────────────────────────────────
BOLD=$'\033[1m'; DIM=$'\033[2m'; RED=$'\033[31m'; GREEN=$'\033[32m'
YELLOW=$'\033[33m'; BLUE=$'\033[34m'; RESET=$'\033[0m'

info()  { printf '%s==>%s %s\n' "$BLUE$BOLD" "$RESET" "$1"; }
ok()    { printf '  %s✓%s %s\n' "$GREEN" "$RESET" "$1"; }
warn()  { printf '  %s!%s %s\n' "$YELLOW" "$RESET" "$1"; }
err()   { printf '  %s✗%s %s\n' "$RED" "$RESET" "$1" >&2; }
step()  { printf '\n%s──%s %s%s%s\n' "$DIM" "$RESET" "$BOLD" "$1" "$RESET"; }

die() { err "$1"; exit 1; }

run() {  # run <cmd…> — respects --dry-run
    if [ "$DRY_RUN" = "1" ]; then
        printf '  %s[dry-run]%s %s\n' "$DIM" "$RESET" "$*"
    else
        "$@"
    fi
}

ask() {  # ask <variable> — prompt on the terminal when there is one
    # As in uninstall.sh: only reach for /dev/tty when stdin is a terminal, so
    # piped answers (tests, CI, `yes | install.sh`) actually take effect.
    local __var="$1" __reply=""
    if [ -t 0 ] && [ -r /dev/tty ]; then
        read -r __reply </dev/tty 2>/dev/null || read -r __reply || __reply=""
    else
        read -r __reply || __reply=""
    fi
    printf -v "$__var" '%s' "$__reply"
}

confirm() {  # confirm <question> [default-yes]
    [ "$ASSUME_YES" = "1" ] && return 0
    local answer=""
    printf '  %s?%s %s [%s] ' "$YELLOW" "$RESET" "$1" "${2:-y/N}"
    ask answer
    case "${answer,,}" in
        y|yes) return 0 ;;
        *)     return 1 ;;
    esac
}

usage() {
    sed -n '2,20p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
    exit 0
}

while [ $# -gt 0 ]; do
    case "$1" in
        -y|--yes)        ASSUME_YES=1 ;;
        --no-packages)   DO_PACKAGES=0 ;;
        --no-aur)        DO_AUR=0 ;;
        --no-backup)     DO_BACKUP=0 ;;
        --link)          USE_LINKS=1 ;;
        --no-zsh)        SET_ZSH=0 ;;
        --dry-run)       DRY_RUN=1 ;;
        -h|--help)       usage ;;
        *) die "Unknown option: $1   (try --help)" ;;
    esac
    shift
done

# ═════════════════════════ 1. Sanity checks ═══════════════════════════════

step "Checking your system"

[ "$(id -u)" -eq 0 ] && die "Do not run this as root. Run it as your normal user (it asks for sudo when needed)."

PKG_MANAGER=""
if command -v pacman >/dev/null 2>&1; then PKG_MANAGER="pacman"
elif command -v apt-get >/dev/null 2>&1; then PKG_MANAGER="apt"
elif command -v dnf >/dev/null 2>&1; then PKG_MANAGER="dnf"
elif command -v zypper >/dev/null 2>&1; then PKG_MANAGER="zypper"
fi

if [ "$DO_PACKAGES" = "1" ] && [ -z "$PKG_MANAGER" ]; then
    die "No supported package manager (pacman, apt-get, dnf, zypper) found — re-run with --no-packages to install the config files only."
fi

DISTRO="$(. /etc/os-release 2>/dev/null && printf '%s' "${PRETTY_NAME:-${ID:-unknown}}")"
ok "Distribution: $DISTRO (${PKG_MANAGER:-config-only})"

[ -d "$REPO_DIR/.config/hypr" ] || die "Config files not found next to this script. Extract the whole repository first."

AUR_HELPER=""
if [ "$DO_PACKAGES" = "1" ]; then
    for helper in paru yay; do
        command -v "$helper" >/dev/null 2>&1 && AUR_HELPER="$helper" && break
    done
    [ -n "$AUR_HELPER" ] && ok "AUR helper: $AUR_HELPER" || warn "No AUR helper (paru/yay) found — optional AUR packages will be skipped."
fi

# Hyprland version check: the Lua configuration needs 0.55 or newer.
if command -v hyprctl >/dev/null 2>&1; then
    HL_VERSION="$(hyprctl version 2>/dev/null | head -1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)"
    if [ -n "${HL_VERSION:-}" ]; then
        MAJOR="${HL_VERSION%%.*}"; REST="${HL_VERSION#*.}"; MINOR="${REST%%.*}"
        if [ "$MAJOR" -eq 0 ] && [ "$MINOR" -lt 55 ]; then
            warn "Hyprland $HL_VERSION found — this setup uses the Lua config format (Hyprland 0.55+)."
            warn "Update Hyprland first:  sudo pacman -Syu hyprland"
            confirm "Continue anyway?" || exit 1
        else
            ok "Hyprland $HL_VERSION (Lua config supported)"
        fi
    fi
else
    warn "Hyprland is not installed yet — it will be installed from the repositories."
fi

# Browser detection, used to fill in user.lua below.
DETECTED_BROWSER=""
for b in firefox brave-browser chromium google-chrome-stable zen-browser librewolf vivaldi microsoft-edge-stable; do
    command -v "$b" >/dev/null 2>&1 && DETECTED_BROWSER="$b" && break
done
[ -n "$DETECTED_BROWSER" ] && ok "Browser detected: $DETECTED_BROWSER" || warn "No browser found — install one, then set it in modules/user.lua."

printf '\n'
info "Plan"
printf '     • install packages from packages.txt %s\n' "$([ "$DO_PACKAGES" = 1 ] && echo "(official repos)" || echo "(skipped)")"
printf '     • install optional AUR extras %s\n' "$([ "$DO_AUR" = 1 ] && echo "(${AUR_HELPER:-none})" || echo "(skipped)")"
printf '     • back up anything it replaces to %s\n' "$BACKUP_DIR"
printf '     • install the configuration into %s and %s\n' "$CONFIG_DIR" "$HOME"
[ "$USE_LINKS" = "1" ] && printf '     • as SYMLINKS (the repo stays the source of truth)\n'
printf '\n'

confirm "Proceed with the installation?" y || { info "Aborted."; exit 0; }

# ═════════════════════════ 2. Packages ════════════════════════════════════

read_list() {  # read_list <file> — strip comments/blank lines
    [ -f "$1" ] || return 0
    sed -e 's/#.*//' -e 's/[[:space:]]*$//' -e '/^[[:space:]]*$/d' "$1"
}

if [ "$DO_PACKAGES" = "1" ]; then
    step "Installing packages"

    if ! command -v sudo >/dev/null 2>&1; then
        warn "sudo not found — skipping the repository packages (install them by hand from packages.txt)."
    elif [ "$PKG_MANAGER" = "pacman" ]; then
        mapfile -t PKGS < <(read_list "$REPO_DIR/packages.txt")
        available=(); missing=()
        for p in "${PKGS[@]}"; do
            if pacman -Si "$p" >/dev/null 2>&1; then available+=("$p"); else missing+=("$p"); fi
        done

        [ "${#missing[@]}" -gt 0 ] && warn "Not available in your repositories (skipped): ${missing[*]}"

        if [ "${#available[@]}" -gt 0 ]; then
            info "Installing ${#available[@]} packages via pacman…"
            run sudo pacman -S --needed --noconfirm "${available[@]}" || warn "pacman reported an error — check the output above."
            ok "Repository packages done"
        fi
    elif [ "$PKG_MANAGER" = "apt" ]; then
        info "Installing cross-distro packages via apt-get…"
        APT_PKGS=(hyprland waybar rofi kitty thunar dunst wl-clipboard cliphist grim slurp wf-recorder
                  jq fzf bat zathura imv mpv neovim zsh zsh-autosuggestions zsh-syntax-highlighting
                  btop pavucontrol network-manager bluez brightnessctl playerctl xdg-utils flatpak
                  file-roller unzip p7zip-full gcc make libnotify-bin fonts-noto-color-emoji xwayland)
        avail_apt=()
        for p in "${APT_PKGS[@]}"; do
            apt-cache show "$p" >/dev/null 2>&1 && avail_apt+=("$p")
        done
        [ "${#avail_apt[@]}" -gt 0 ] && run sudo apt-get install -y "${avail_apt[@]}" || warn "Some apt packages were unavailable — install Hyprland >= 0.55 manually if needed."
        ok "APT packages processed"
    elif [ "$PKG_MANAGER" = "dnf" ]; then
        info "Installing cross-distro packages via dnf…"
        run sudo dnf install -y --skip-unavailable \
            hyprland waybar rofi-wayland kitty thunar dunst wl-clipboard cliphist grim slurp wf-recorder \
            jq fzf bat zathura zathura-pdf-mupdf imv mpv neovim zsh zsh-autosuggestions zsh-syntax-highlighting \
            btop pavucontrol NetworkManager bluez brightnessctl playerctl xdg-utils flatpak file-roller \
            unzip p7zip gcc-c++ make libnotify xorg-x11-server-Xwayland power-profiles-daemon || warn "dnf reported warnings — continuing."
        ok "DNF packages processed"
    elif [ "$PKG_MANAGER" = "zypper" ]; then
        info "Installing cross-distro packages via zypper…"
        run sudo zypper --non-interactive install --no-recommends \
            hyprland waybar rofi-wayland kitty thunar dunst wl-clipboard cliphist grim slurp wf-recorder \
            jq fzf bat zathura imv mpv neovim zsh btop pavucontrol brightnessctl playerctl xdg-utils \
            flatpak file-roller unzip gcc-c++ make libnotify-tools xwayland || warn "zypper reported warnings — continuing."
        ok "Zypper packages processed"
    fi

    if [ "$DO_AUR" = "1" ] && [ -n "$AUR_HELPER" ]; then
        mapfile -t AURPKGS < <(read_list "$REPO_DIR/aur-packages.txt")
        if [ "${#AURPKGS[@]}" -gt 0 ]; then
            info "Installing optional AUR packages ($AUR_HELPER)…"
            run "$AUR_HELPER" -S --needed --noconfirm "${AURPKGS[@]}" || warn "AUR install had problems — the setup works without these."
            ok "AUR extras done"
        fi
    elif [ "$DO_AUR" = "1" ]; then
        warn "Skipping AUR packages (no paru/yay). See aur-packages.txt for what they are."
    fi
else
    step "Packages"
    warn "Skipped (--no-packages). Make sure everything in packages.txt is installed."
fi

# ═════════════════════════ 3. Backups ═════════════════════════════════════

step "Backing up existing configuration"

if [ "$DO_BACKUP" = "1" ]; then
    if [ "$DRY_RUN" = "1" ]; then
        info "Would create $BACKUP_DIR and copy replaced files there."
    else
        mkdir -p "$BACKUP_DIR"
        : > "$BACKUP_DIR/manifest.txt"
        ok "Run backup:      $BACKUP_DIR"
        ok "Original files:  $PRISTINE_DIR"
    fi
else
    warn "Backups disabled (--no-backup). Existing files will be overwritten!"
fi

manifest_add() {  # manifest_add <file> <relative path>
    [ "$DRY_RUN" = "1" ] && return 0
    printf '%s\n' "$2" >> "$1"
}

# Keep the *first* version we ever saw of a file. Re-installing never
# overwrites it, so the user's real originals survive any number of runs.
backup_to_pristine() {  # backup_to_pristine <target> <relative path>
    local dst="$1" rel="$2"
    [ "$DO_BACKUP" = "1" ] || return 0
    [ "$DRY_RUN" = "1" ] && return 0
    [ -e "$dst" ] || return 0
    [ -e "$PRISTINE_DIR/$rel" ] && return 0          # already have the original
    mkdir -p "$PRISTINE_DIR/$(dirname "$rel")"
    cp -a "$dst" "$PRISTINE_DIR/$rel" 2>/dev/null || return 0
    manifest_add "$PRISTINE_DIR/manifest.txt" "$rel"
}

is_ours() {  # is_ours <source> <target> — already installed by us?
    local src="$1" dst="$2"
    [ -L "$dst" ] && [ "$(readlink -f "$dst" 2>/dev/null)" = "$(readlink -f "$src" 2>/dev/null)" ] && return 0
    cmp -s "$src" "$dst" 2>/dev/null && return 0
    return 1
}

INSTALLED=0
SKIPPED=0

install_file() {  # install_file <source> <target>
    local src="$1" dst="$2" rel="${2#$HOME/}"

    if [ -e "$dst" ] && ! is_ours "$src" "$dst"; then
        backup_to_pristine "$dst" "$rel"                       # the original, kept forever
        if [ "$DO_BACKUP" = "1" ] && [ "$DRY_RUN" = "0" ]; then
            mkdir -p "$BACKUP_DIR/$(dirname "$rel")"
            cp -a "$dst" "$BACKUP_DIR/$rel" 2>/dev/null && manifest_add "$BACKUP_DIR/manifest.txt" "$rel"
        fi
    fi

    mkdir -p "$(dirname "$dst")"

    if [ "$USE_LINKS" = "1" ]; then
        run ln -sfn "$src" "$dst"
    else
        run cp -a "$src" "$dst"
    fi
    INSTALLED=$((INSTALLED + 1))
}

# ═════════════════════════ 4. Configuration ═══════════════════════════════

step "Installing configuration files"

# All of .config/** (hypr, waybar, rofi, kitty, dunst, gtk, wlogout, spicetify, …)
while IFS= read -r -d '' src; do
    rel="${src#$REPO_DIR/.config/}"
    case "$rel" in
        # never ship these
        *.bak|*~|*.orig) SKIPPED=$((SKIPPED + 1)); continue ;;
    esac
    install_file "$src" "$CONFIG_DIR/$rel"
done < <(find "$REPO_DIR/.config" -type f -print0)

# Shell files that live in $HOME rather than ~/.config
[ -f "$REPO_DIR/.zshrc" ] && install_file "$REPO_DIR/.zshrc" "$HOME/.zshrc"

ok "$INSTALLED files installed ($SKIPPED skipped)"

# Scripts must be executable + native Pure C11 helpers compiled
if [ "$DRY_RUN" = "0" ]; then
    chmod +x "$CONFIG_DIR"/hypr/scripts/*.sh 2>/dev/null || true
    chmod +x "$CONFIG_DIR"/waybar/scripts/*.sh 2>/dev/null || true
    chmod +x "$CONFIG_DIR"/hypr/scripts/* 2>/dev/null || true
    ok "Scripts marked executable"

    if [ "$USE_LINKS" = "0" ] && [ -f "$CONFIG_DIR/hypr/src/mono-calc.c" ]; then
        cc_bin=""
        if command -v cc >/dev/null 2>&1; then cc_bin="cc"
        elif command -v gcc >/dev/null 2>&1; then cc_bin="gcc"
        elif command -v clang >/dev/null 2>&1; then cc_bin="clang"
        fi
        if [ -n "$cc_bin" ]; then
            mkdir -p "$CONFIG_DIR/hypr/bin"
            if "$cc_bin" -std=c11 -O3 "$CONFIG_DIR/hypr/src/mono-calc.c" \
                -o "$CONFIG_DIR/hypr/bin/mono-calc" -lm 2>/dev/null; then
                ok "Compiled native Pure C11 helper (hypr/bin/mono-calc)"
            fi
            if [ -f "$CONFIG_DIR/hypr/src/mono-cursor-gen.c" ] && \
               "$cc_bin" -std=c11 -O3 "$CONFIG_DIR/hypr/src/mono-cursor-gen.c" \
                -o "$CONFIG_DIR/hypr/bin/mono-cursor-gen" -lm 2>/dev/null; then
                ok "Compiled native Pure C11 cursor compiler (hypr/bin/mono-cursor-gen)"
            fi
            if [ -f "$CONFIG_DIR/hypr/src/mono-ipc.c" ] && \
               "$cc_bin" -std=c11 -O3 "$CONFIG_DIR/hypr/src/mono-ipc.c" \
                -o "$CONFIG_DIR/hypr/bin/mono-ipc" -lm 2>/dev/null; then
                ok "Compiled native Pure C11 Hyprland IPC engine (hypr/bin/mono-ipc)"
            fi
            if [ -f "$CONFIG_DIR/hypr/src/mono-tui.c" ] && \
               "$cc_bin" -std=c11 -O3 "$CONFIG_DIR/hypr/src/mono-tui.c" \
                -o "$CONFIG_DIR/hypr/bin/mono-tui" -lm 2>/dev/null; then
                ok "Compiled native Pure C11 Terminal Control Deck (hypr/bin/mono-tui)"
            fi
        fi
    fi

    # Install the 4 bespoke Monochrome cursor suites (hyprcursor + Xcursor)
    ICONS_TARGET="${XDG_DATA_HOME:-$HOME/.local/share}/icons"
    mkdir -p "$ICONS_TARGET" 2>/dev/null || true
    if [ -d "$REPO_DIR/cursors/Monochrome-Obsidian" ]; then
        cp -a "$REPO_DIR/cursors"/Monochrome-* "$ICONS_TARGET"/ 2>/dev/null || true
        ok "Installed 4 bespoke Monochrome cursor themes (hyprcursor + Xcursor)"
    elif [ -x "$CONFIG_DIR/hypr/bin/mono-cursor-gen" ]; then
        "$CONFIG_DIR/hypr/bin/mono-cursor-gen" --out-dir "$ICONS_TARGET" >/dev/null 2>&1 \
            && ok "Generated 4 bespoke Monochrome cursor themes in $ICONS_TARGET"
    fi
    if [ -x "$CONFIG_DIR/hypr/scripts/cursor.sh" ]; then
        "$CONFIG_DIR/hypr/scripts/cursor.sh" theme Monochrome-Obsidian >/dev/null 2>&1 || true
    fi
fi

# ── Personalise: browser & Qt palette path ───────────────────────────────
USER_LUA="$CONFIG_DIR/hypr/modules/user.lua"
if [ -n "$DETECTED_BROWSER" ] && [ -f "$USER_LUA" ] && [ "$DRY_RUN" = "0" ] && [ ! -L "$USER_LUA" ]; then
    if grep -q 'browser      = "firefox"' "$USER_LUA" 2>/dev/null || grep -q 'browser = "firefox"' "$USER_LUA" 2>/dev/null; then
        sed -i -E "s|^( *browser *)= *\"firefox\"|\1= \"$DETECTED_BROWSER\"|" "$USER_LUA"
        ok "Set your browser to '$DETECTED_BROWSER' in modules/user.lua"
    fi
fi

if [ "$DRY_RUN" = "0" ]; then
    for qtc in "$CONFIG_DIR/qt5ct/qt5ct.conf" "$CONFIG_DIR/qt6ct/qt6ct.conf"; do
        if [ -f "$qtc" ] && [ ! -L "$qtc" ]; then
            sed -i "s|~/.config|$CONFIG_DIR|g" "$qtc" 2>/dev/null || true
        fi
    done
    if [ -x "$CONFIG_DIR/hypr/scripts/browser-theme.sh" ]; then
        "$CONFIG_DIR/hypr/scripts/browser-theme.sh" apply --quiet >/dev/null 2>&1 || true
        ok "Applied OpenCode browser theme & Wayland browser flags"
    fi
fi

# ── A leftover hyprlang config? ───────────────────────────────────────────
if [ -f "$CONFIG_DIR/hypr/hyprland.conf" ]; then
    backup_to_pristine "$CONFIG_DIR/hypr/hyprland.conf" ".config/hypr/hyprland.conf"
    if [ "$DO_BACKUP" = "1" ] && [ "$DRY_RUN" = "0" ]; then
        mkdir -p "$BACKUP_DIR/.config/hypr"
        cp -a "$CONFIG_DIR/hypr/hyprland.conf" "$BACKUP_DIR/.config/hypr/hyprland.conf"
        manifest_add "$BACKUP_DIR/manifest.txt" ".config/hypr/hyprland.conf"
    fi
    warn "Found ~/.config/hypr/hyprland.conf."
    warn "Hyprland 0.55+ reads hyprland.lua, so the old file is now IGNORED (a copy is in the backup)."
    warn "If you had custom lines there, port them into ~/.config/hypr/modules/*.lua."
fi

# ── Wallpapers & screenshots ──────────────────────────────────────────────
step "Wallpapers and folders"
run mkdir -p "$WALLPAPER_DIR" "$SCREENSHOT_DIR" "$HOME/Pictures"
if [ -d "$REPO_DIR/wallpapers" ]; then
    copied=0
    for wp in "$REPO_DIR"/wallpapers/*; do
        [ -f "$wp" ] || continue
        case "${wp,,}" in
            *.jpg|*.jpeg|*.png|*.webp|*.gif|*.bmp) ;;
            *) continue ;;   # skip generate.c / generate.h / animate.c and anything else non-image
        esac
        name="$(basename "$wp")"
        if [ -e "$WALLPAPER_DIR/$name" ]; then
            continue
        fi
        run cp -n "$wp" "$WALLPAPER_DIR/$name" && copied=$((copied + 1))
    done
    ok "$copied wallpapers copied to $WALLPAPER_DIR (existing files left alone)"
fi
run mkdir -p "$CONFIG_DIR/hypr/modules"

# ── Restore script for the backups ────────────────────────────────────────
write_restore_script() {  # write_restore_script <backup dir> <label>
    local dir="$1" label="$2"
    [ "$DRY_RUN" = "1" ] && return 0
    [ -d "$dir" ] || return 0
    cat > "$dir/restore.sh" <<RESTORE
#!/usr/bin/env bash
# Put back the files that hyprland-monochrome replaced ($label).
set -euo pipefail
BACKUP="\$(cd -- "\$(dirname -- "\${BASH_SOURCE[0]}")" && pwd)"
while IFS= read -r rel; do
    [ -n "\$rel" ] || continue
    src="\$BACKUP/\$rel"; dst="\$HOME/\$rel"
    [ -e "\$src" ] || continue
    mkdir -p "\$(dirname "\$dst")"
    rm -rf "\$dst"
    cp -a "\$src" "\$dst"
    printf 'restored %s\n' "\$rel"
done < "\$BACKUP/manifest.txt"
echo "Done. Log out and back in."
RESTORE
    chmod +x "$dir/restore.sh"
}

if [ "$DO_BACKUP" = "1" ]; then
    write_restore_script "$BACKUP_DIR" "files replaced on $TIMESTAMP"
    write_restore_script "$PRISTINE_DIR" "your original configuration"
    ok "Restore scripts: $PRISTINE_DIR/restore.sh  (originals)  ·  $BACKUP_DIR/restore.sh  (this run)"
fi

# ═════════════════════════ 5. Optional extras ═════════════════════════════

step "Optional extras"

# Default shell
if [ "$SET_ZSH" = "1" ] && command -v zsh >/dev/null 2>&1; then
    if [ "$(getent passwd "$USER" | cut -d: -f7)" != "$(command -v zsh)" ]; then
        if confirm "Make zsh your default shell? (recommended)" y; then
            run chsh -s "$(command -v zsh)" "$USER" || warn "chsh failed — run: chsh -s $(command -v zsh)"
        fi
    else
        ok "zsh is already the default shell"
    fi
fi

# Bluetooth service
if command -v systemctl >/dev/null 2>&1 && command -v bluetoothctl >/dev/null 2>&1; then
    if ! systemctl is-enabled bluetooth.service >/dev/null 2>&1; then
        confirm "Enable the Bluetooth service?" y && run sudo systemctl enable --now bluetooth.service
    else
        ok "Bluetooth service already enabled"
    fi
fi

# Spotify theming (optional, needs spicetify)
if command -v spicetify >/dev/null 2>&1 && command -v spotify >/dev/null 2>&1; then
    if confirm "Apply the monochrome Spotify theme now?" y; then
        run bash "$CONFIG_DIR/hypr/scripts/spotify-theme.sh" || warn "Spotify theming failed — rerun scripts/spotify-theme.sh after Spotify updates."
    fi
fi

# ═════════════════════════ 6. Verification ════════════════════════════════

step "Verifying the installation"

missing_files=0
for f in \
    "$CONFIG_DIR/hypr/hyprland.lua" \
    "$CONFIG_DIR/hypr/modules/user.lua" \
    "$CONFIG_DIR/hypr/modules/binds.lua" \
    "$CONFIG_DIR/hypr/scripts/settings.sh" \
    "$CONFIG_DIR/waybar/config.jsonc" \
    "$CONFIG_DIR/rofi/theme.rasi" \
    "$CONFIG_DIR/kitty/kitty.conf" \
    "$HOME/.zshrc"
do
    if [ -e "$f" ]; then ok "$(basename "$f")"; else err "missing: $f"; missing_files=$((missing_files + 1)); fi
done

# Syntax check the shell scripts (cheap insurance)
if [ "$DRY_RUN" = "0" ]; then
    broken=0
    for s in "$CONFIG_DIR"/hypr/scripts/*.sh "$CONFIG_DIR"/waybar/scripts/*.sh; do
        [ -f "$s" ] || continue
        bash -n "$s" 2>/dev/null || { err "syntax error in $s"; broken=$((broken + 1)); }
    done
    [ "$broken" -eq 0 ] && ok "All shell scripts parse cleanly"
fi

# If Hyprland is already running, reload and report config errors
if [ -n "${HYPRLAND_INSTANCE_SIGNATURE:-}" ] && command -v hyprctl >/dev/null 2>&1 && [ "$DRY_RUN" = "0" ]; then
    hyprctl reload >/dev/null 2>&1
    sleep 0.4
    errors="$(hyprctl configerrors 2>/dev/null)"
    if [ -z "$errors" ] || printf '%s' "$errors" | grep -qi 'no errors'; then
        ok "Hyprland reloaded with no config errors"
    else
        warn "Hyprland reported config errors:"
        printf '%s\n' "$errors" | sed 's/^/      /'
    fi
fi

# ═════════════════════════ 7. Done ════════════════════════════════════════

printf '\n'
printf '%s╭──────────────────────────────────────────────────────────────╮%s\n' "$BOLD" "$RESET"
printf '%s│%s  %sInstallation finished%s                                      %s│%s\n' "$BOLD" "$RESET" "$GREEN$BOLD" "$RESET" "$BOLD" "$RESET"
printf '%s╰──────────────────────────────────────────────────────────────╯%s\n' "$BOLD" "$RESET"
cat <<EOF

  Next steps
    1. Log out and log back in (choose the "Hyprland" session), or reboot.
    2. On first login you should see: bar on top, wallpaper, and Super + D for apps.
    3. Learn the keys:  Super + /      (or read docs/KEYBINDS.md)
    4. Make it yours:   modules/user.lua  ·  docs/CUSTOMIZE.md

  Handy commands
    ~/.config/hypr/scripts/settings.sh      settings menu (Super + I)
    ~/.config/hypr/scripts/wallpaper.sh pick
    ~/.config/hypr/scripts/menu.sh keybinds all shortcuts
    hyprctl reload                          reload after editing config

  Your old config (if any) is in:
    $PRISTINE_DIR        <- your original files, kept forever
    $BACKUP_DIR          <- what this run replaced

  Restore with:  $PRISTINE_DIR/restore.sh
  Or run:        ./uninstall.sh

EOF

if [ "$missing_files" -gt 0 ]; then
    err "$missing_files expected file(s) are missing — check the output above."
    exit 1
fi
