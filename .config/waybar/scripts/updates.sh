#!/usr/bin/env bash
# Waybar module: pending pacman updates (official repos + AUR, if an AUR helper
# is installed). Hidden automatically when there is nothing to do.

set -u

count=0
repo=0
aur=0

if command -v checkupdates >/dev/null 2>&1; then
    repo="$(checkupdates 2>/dev/null | wc -l)"
else
    repo="$(pacman -Qu 2>/dev/null | wc -l)"
fi
count="$repo"

for helper in paru yay; do
    if command -v "$helper" >/dev/null 2>&1; then
        aur="$("$helper" -Qua 2>/dev/null | wc -l)"
        count=$((count + aur))
        break
    fi
done

if [ "$count" -le 0 ]; then
    printf '{"text":"","tooltip":"Everything is up to date","class":"updated"}\n'
    exit 0
fi

icon="󰚰"
printf '{"text":"%s %s","tooltip":"%s update(s) available\\n%s in the official repos, %s from the AUR\\n\\nClick to run pacman -Syu","class":"pending"}\n' \
    "$icon" "$count" "$count" "$repo" "$aur"
