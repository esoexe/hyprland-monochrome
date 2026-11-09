#!/usr/bin/env bash
# Optional SDDM theme installer. Does not install, enable, or restart SDDM.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE="$ROOT/sddm/MonochromeLogin"
THEME_ID="MonochromeLogin"
THEME_DIR="/usr/share/sddm/themes/$THEME_ID"
CONF_DIR="/etc/sddm.conf.d"
CONF="$CONF_DIR/99-hyprland-monochrome.conf"
BACKUP_ROOT="/var/backups/hyprland-monochrome/sddm"

usage() {
    cat <<'EOF'
Usage:
  sudo ./tools/install-sddm-theme.sh --install   Install theme and select it in SDDM
  ./tools/install-sddm-theme.sh --preview        Preview without changing system configuration
  sudo ./tools/install-sddm-theme.sh --remove    Remove override and restore the previous theme if backed up
  ./tools/install-sddm-theme.sh --help

This affects SDDM only. It never installs, enables, starts, stops, or restarts a display manager.
EOF
}

mode="${1:---help}"
case "$mode" in
    --help|-h) usage; exit 0 ;;
    --install|--preview|--remove) ;;
    *) usage >&2; exit 2 ;;
esac

[ -d "$SOURCE" ] || { echo "Theme source missing: $SOURCE" >&2; exit 1; }

preview() {
    local greeter metadata tmp rc
    if command -v sddm-greeter-qt6 >/dev/null 2>&1; then
        greeter="$(command -v sddm-greeter-qt6)"
        metadata="$SOURCE/metadata-qt6.desktop"
    elif command -v sddm-greeter >/dev/null 2>&1; then
        greeter="$(command -v sddm-greeter)"
        metadata="$SOURCE/metadata-qt5.desktop"
    else
        echo "No SDDM greeter preview binary found. Install SDDM to preview this theme." >&2
        return 1
    fi
    tmp="$(mktemp -d)"
    mkdir -p "$tmp/assets"
    cp "$SOURCE/Main.qml" "$SOURCE/theme.conf" "$tmp/"
    cp "$metadata" "$tmp/metadata.desktop"
    cp "$ROOT/wallpapers/mono-live-01-orbital.gif" "$tmp/assets/orbital.gif"
    if [[ "$greeter" == *qt6* ]]; then
        if "$greeter" --test-mode --theme "$tmp"; then rc=0; else rc=$?; fi
    else
        if "$greeter" --test --theme "$tmp"; then rc=0; else rc=$?; fi
    fi
    rm -rf "$tmp"
    return "$rc"
}

if [ "$mode" = "--preview" ]; then
    preview
fi

if [ "$(id -u)" -ne 0 ]; then
    command -v sudo >/dev/null 2>&1 || { echo "Please run this with sudo." >&2; exit 1; }
    exec sudo bash "$0" "$mode"
fi

if [ "$mode" = "--install" ]; then
    command -v sddm >/dev/null 2>&1 || {
        echo "SDDM is not installed. This optional theme does not install or enable a display manager." >&2
        exit 1
    }

    mkdir -p "$BACKUP_ROOT"
    stamp="$(date '+%Y%m%d-%H%M%S')"
    backup="$BACKUP_ROOT/$stamp"
    mkdir -p "$backup"

    [ ! -e "$CONF" ] || cp -a "$CONF" "$backup/99-hyprland-monochrome.conf"
    [ ! -d "$THEME_DIR" ] || cp -a "$THEME_DIR" "$backup/MonochromeLogin"
    printf '%s\n' "$backup" > "$BACKUP_ROOT/last-backup"

    greeter_qt=5
    command -v sddm-greeter-qt6 >/dev/null 2>&1 && greeter_qt=6
    if [ "$greeter_qt" = 6 ]; then
        metadata="$SOURCE/metadata-qt6.desktop"
    else
        metadata="$SOURCE/metadata-qt5.desktop"
    fi
    [ -f "$metadata" ] || { echo "Missing SDDM metadata: $metadata" >&2; exit 1; }

    stage="$THEME_DIR.new.$stamp"
    rm -rf "$stage"
    install -d -m 0755 "$stage/assets"
    install -m 0644 "$SOURCE/Main.qml" "$stage/Main.qml"
    install -m 0644 "$SOURCE/theme.conf" "$stage/theme.conf"
    install -m 0644 "$metadata" "$stage/metadata.desktop"
    install -m 0644 "$ROOT/wallpapers/mono-live-01-orbital.gif" "$stage/assets/orbital.gif"
    rm -rf "$THEME_DIR"
    mv "$stage" "$THEME_DIR"

    install -d -m 0755 "$CONF_DIR"
    cat > "$CONF" <<EOF
[Theme]
Current=$THEME_ID
EOF
    chmod 0644 "$CONF"
    printf 'Installed Monochrome SDDM theme (Qt %s).\n' "$greeter_qt"
    printf 'Backup: %s\n' "$backup"
    echo "SDDM service state was not changed. Preview before reboot with the greeter's test mode."
    exit 0
fi

if [ "$mode" = "--remove" ]; then
    backup=""
    [ ! -r "$BACKUP_ROOT/last-backup" ] || backup="$(cat "$BACKUP_ROOT/last-backup")"
    rm -f "$CONF"
    rm -rf "$THEME_DIR"
    if [ -n "$backup" ] && [ -d "$backup" ]; then
        [ ! -f "$backup/99-hyprland-monochrome.conf" ] || cp -a "$backup/99-hyprland-monochrome.conf" "$CONF"
        [ ! -d "$backup/MonochromeLogin" ] || cp -a "$backup/MonochromeLogin" "$THEME_DIR"
        echo "Restored previous SDDM files from $backup"
    else
        echo "Removed Monochrome SDDM override and theme. No prior files needed restoring."
    fi
    echo "SDDM service state was not changed. Apply changes at your next normal login/reboot."
fi
