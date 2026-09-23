#!/usr/bin/env bash
# Apply (or re-apply) the monochrome Spotify theme through Spicetify.
#
#   spotify-theme.sh          apply the theme
#   spotify-theme.sh restore  put the original Spotify files back
#
# Spicetify itself lives in the AUR:  yay -S spicetify-cli
# Run this again after every Spotify update (or add it to modules/autostart.lua).

set -u
. "$(dirname "$0")/lib.sh"

# Tolerate --flags as well as plain arguments (handy in keybinds).
case "${1:-}" in --*) set -- "${1#--}" "${@:2}" ;; esac

THEME="monochrome"
SCHEME="mono"

if ! have spicetify; then
    notify "Spicetify not installed" "Optional: yay -S spicetify-cli" "dialog-warning"
    exit 1
fi

# ── Locate Spotify (native package or Flatpak) ────────────────────────────
spotify_path=""
prefs_path=""

for candidate in /opt/spotify /usr/share/spotify /usr/lib/spotify; do
    [ -d "$candidate" ] && spotify_path="$candidate" && break
done

for candidate in \
    "$HOME/.var/app/com.spotify.Client/config/spotify/prefs" \
    "$HOME/.config/spotify/prefs"
do
    [ -f "$candidate" ] && prefs_path="$candidate" && break
done

flatpak_app=""
for candidate in \
    "$HOME/.local/share/flatpak/app/com.spotify.Client/current/active/files/extra/share/spotify" \
    "/var/lib/flatpak/app/com.spotify.Client/current/active/files/extra/share/spotify"
do
    [ -d "$candidate" ] && flatpak_app="$candidate" && break
done

if [ -n "$flatpak_app" ]; then
    spicetify config spotify_path "$flatpak_app" >/dev/null 2>&1
    [ -n "$prefs_path" ] && spicetify config prefs_path "$prefs_path" >/dev/null 2>&1
elif [ -n "$spotify_path" ]; then
    spicetify config spotify_path "$spotify_path" >/dev/null 2>&1
fi

# ── Restore ───────────────────────────────────────────────────────────────
if [ "${1:-apply}" = "restore" ]; then
    spicetify restore >/dev/null 2>&1 && notify "Spotify" "Original files restored"
    exit 0
fi

# ── Apply ─────────────────────────────────────────────────────────────────
spicetify config current_theme "$THEME" color_scheme "$SCHEME" \
           inject_css 1 replace_colors 1 overwrite_assets 1 >/dev/null 2>&1

# First run on a fresh install needs a backup before it can patch anything.
if [ ! -d "$HOME/.config/spicetify/Backup" ] && [ ! -d "$HOME/.spicetify/Backup" ]; then
    output="$(spicetify backup apply 2>&1)"
else
    output="$(spicetify apply 2>&1)"
fi

if printf '%s' "$output" | grep -qiE 'error|not found|no such file'; then
    notify "Spotify theme failed" "$(printf '%s' "$output" | tail -3)" "dialog-error"
    exit 1
fi

notify "Spotify" "Monochrome theme applied" spotify
