#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════════
#  open-app.sh — Universal Application, AppImage, Flatpak, Wine & File Opener
#
#  Ensures this desktop can launch ANY app or open ANY file on ANY Linux PC:
#    • Native Wayland & X11/XWayland `.desktop` apps
#    • Flatpak (`flatpak run ...`), Snap, Nix, and CLI tools
#    • `.AppImage` binaries (auto-indexed into ~/.local/share/applications AND
#      automatically retried with `--appimage-extract-and-run` if libfuse2 is
#      absent on the host!)
#    • Windows `.exe` / `.msi` executables via Wine / Bottles / umu-run
#    • Smart fallbacks for Browser (`Super+B`), File Manager (`Super+E`), and
#      Terminal (`Super+Return`) if the preferred app in `user.lua` is missing
#      on a fresh machine
#    • Universal file picker / opener (`Super+Shift+O`) across images, PDFs,
#      videos, archives, code, URLs, and directories
# ═══════════════════════════════════════════════════════════════════════════
set -euo pipefail
# shellcheck source=lib.sh
. "$(dirname "$0")/lib.sh"

# Launch detached cleanly under UWSM (if active) or setsid
launch_detached() {
    if have uwsm && [ -n "${UWSM_FINALIZE_VARNAMES:-}" ]; then
        setsid -f uwsm app -- "$@" >/dev/null 2>&1 < /dev/null &
    else
        setsid -f "$@" >/dev/null 2>&1 < /dev/null &
    fi
}

# Resolve first installed binary from space-separated command + fallback list
resolve_cmd() {
    local candidate bin
    for candidate in "$@"; do
        [ -n "$candidate" ] || continue
        bin="${candidate%% *}"
        if have "$bin"; then
            printf '%s' "$candidate"
            return 0
        fi
    done
    return 1
}

# Auto-index `.AppImage` files in ~/Applications & ~/.local/bin into .desktop entries
sync_appimages() {
    local apps_dir="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
    mkdir -p "$apps_dir"
    local dir f base slug desktop
    for dir in "$HOME/Applications" "$HOME/.local/bin"; do
        [ -d "$dir" ] || continue
        for f in "$dir"/*.AppImage "$dir"/*.appimage; do
            [ -f "$f" ] || continue
            [ -x "$f" ] || chmod +x "$f" 2>/dev/null || true
            base="${f##*/}"
            base="${base%.AppImage}"
            base="${base%.appimage}"
            slug="$(printf '%s' "$base" | tr -cs '[:alnum:]' '-' | tr '[:upper:]' '[:lower:]')"
            slug="${slug#-}"
            slug="${slug%-}"
            [ -n "$slug" ] || continue
            desktop="$apps_dir/appimage-${slug}.desktop"
            cat > "$desktop" <<EOF
[Desktop Entry]
Type=Application
Name=${base} (AppImage)
Exec=${SCRIPTS_DIR}/open-app.sh appimage "${f}" %U
Icon=application-x-executable
Terminal=false
Categories=Utility;Application;
Comment=Auto-indexed AppImage (${f})
EOF
        done
    done
}

# Run an AppImage with automatic FUSE-less fallback (`--appimage-extract-and-run`)
run_appimage() {
    local target="$1"; shift || true
    if [ ! -f "$target" ]; then
        notify "AppImage not found" "$target"
        return 1
    fi
    [ -x "$target" ] || chmod +x "$target" 2>/dev/null || true
    if [ -e /dev/fuse ] && { ldconfig -p 2>/dev/null | grep -q 'libfuse\.so\.2' || [ -f /usr/lib/libfuse.so.2 ] || [ -f /lib/x86_64-linux-gnu/libfuse.so.2 ]; }; then
        launch_detached "$target" "$@"
    else
        APPIMAGE_EXTRACT_AND_RUN=1 launch_detached "$target" --appimage-extract-and-run "$@"
    fi
}

# Smart Browser launch (`Super + B`) — works regardless of which browser a PC has
launch_browser() {
    local pref cmd startpage
    pref="$(mono_var browser firefox)"
    startpage="$CONFIG_DIR/browser/opencode/startpage.html"

    cmd="$(resolve_cmd \
        "$pref" \
        firefox zen-browser librewolf floorp waterfox \
        brave brave-browser chromium chromium-browser \
        google-chrome-stable google-chrome vivaldi-stable vivaldi \
        microsoft-edge-stable qutebrowser falkon epiphany)" || cmd=""

    if [ -n "$cmd" ]; then
        if [ "$#" -gt 0 ]; then
            # shellcheck disable=SC2086
            launch_detached $cmd "$@"
        elif [ -f "$startpage" ] && [ "${MONO_BROWSER_STARTPAGE:-0}" = "1" ]; then
            # shellcheck disable=SC2086
            launch_detached $cmd "$startpage"
        else
            # shellcheck disable=SC2086
            launch_detached $cmd
        fi
        return 0
    fi

    # Flatpak browser fallback
    if have flatpak; then
        local fp
        for fp in org.mozilla.firefox io.gitlab.librewolf-community app.zen_browser.zen com.brave.Browser org.chromium.Chromium; do
            if flatpak info "$fp" >/dev/null 2>&1; then
                launch_detached flatpak run "$fp" "$@"
                return 0
            fi
        done
    fi

    notify "No browser found" "Install firefox, brave, or chromium"
    return 1
}

# Smart File Manager launch (`Super + E`) — works on any Linux desktop
launch_files() {
    local pref cmd target="${1:-$HOME}"
    pref="$(mono_var file_manager thunar)"
    cmd="$(resolve_cmd "$pref" thunar nemo pcmanfm-qt pcmanfm nautilus dolphin cosmic-files)" || cmd=""
    if [ -n "$cmd" ]; then
        # shellcheck disable=SC2086
        launch_detached $cmd "$target"
        return 0
    fi
    # Terminal file browser fallback (`yazi` / `ranger` / `nvim`)
    local term
    term="$(resolve_cmd "$(mono_var terminal kitty)" kitty foot alacritty wezterm xterm)" || term=""
    if [ -n "$term" ]; then
        if have yazi; then
            # shellcheck disable=SC2086
            launch_detached $term -e yazi "$target"
            return 0
        elif have nvim; then
            # shellcheck disable=SC2086
            launch_detached $term -e nvim "$target"
            return 0
        fi
    fi
    notify "No file manager found" "Install thunar or yazi"
    return 1
}

# Smart Terminal launch (`Super + Return`)
launch_terminal() {
    local pref cmd
    pref="$(mono_var terminal kitty)"
    cmd="$(resolve_cmd "$pref" kitty foot alacritty wezterm ghostty konsole xfce4-terminal xterm)" || {
        notify "No terminal found" "Install kitty, foot, or alacritty"
        return 1
    }
    # shellcheck disable=SC2086
    launch_detached $cmd "$@"
}

# Open ANY target (URL, AppImage, Flatpak ID, .desktop, .exe, folder, media, PDF, code)
open_target() {
    local target="$1"; shift || true
    [ -n "$target" ] || return 0

    # 1. Expand leading ~/
    case "$target" in
        \~/*) target="$HOME/${target#\~/}" ;;
    esac

    # 2. URL schemes
    case "$target" in
        http://*|https://*|file://*)
            launch_browser "$target"
            return $?
            ;;
        mailto:*|magnet:*|tg:*|discord:*|spotify:*|steam:*)
            have xdg-open && { launch_detached xdg-open "$target"; return 0; }
            ;;
    esac

    # 3. Direct executable command in PATH (when no file with that name exists)
    if [ ! -e "$target" ] && have "${target%% *}"; then
        # shellcheck disable=SC2086
        launch_detached $target "$@"
        return 0
    fi

    # 4. Flatpak reverse-DNS ID (e.g. `org.mozilla.firefox`, `md.obsidian.Obsidian`)
    if [ ! -e "$target" ] && have flatpak; then
        case "$target" in
            *.*.*)
                if flatpak info "$target" >/dev/null 2>&1; then
                    launch_detached flatpak run "$target" "$@"
                    return 0
                fi
                ;;
        esac
    fi

    if [ ! -e "$target" ]; then
        notify "Cannot open" "Not found: $target"
        return 1
    fi

    # 5. Directory
    if [ -d "$target" ]; then
        launch_files "$target"
        return $?
    fi

    # 6. File extension routing (monochrome-first viewers with universal fallbacks)
    local lower="${target,,}"
    case "$lower" in
        *.appimage)
            run_appimage "$target" "$@"
            return $?
            ;;
        *.desktop)
            if have gio; then
                launch_detached gio launch "$target" "$@"
            elif have gtk-launch; then
                launch_detached gtk-launch "$(basename "$target" .desktop)" "$@"
            else
                have xdg-open && launch_detached xdg-open "$target"
            fi
            return 0
            ;;
        *.flatpakref)
            if have flatpak; then
                launch_detached flatpak install --from "$target"
                return 0
            fi
            ;;
        *.exe|*.msi)
            if have umu-run; then
                launch_detached umu-run "$target" "$@"
            elif have wine; then
                launch_detached wine "$target" "$@"
            elif have bottles-cli; then
                launch_detached bottles-cli run -e "$target"
            else
                notify "Windows executable" "Install 'wine' or 'bottles' to run ${target##*/}"
                return 1
            fi
            return 0
            ;;
        *.png|*.jpg|*.jpeg|*.webp|*.gif|*.bmp|*.svg|*.avif|*.tiff)
            if have imv; then launch_detached imv "$target"; return 0; fi
            if haveimv_wayland=$(command -v imv-wayland 2>/dev/null); then launch_detached "$haveimv_wayland" "$target"; return 0; fi
            if have swayimg; then launch_detached swayimg "$target"; return 0; fi
            if have feh; then launch_detached feh "$target"; return 0; fi
            ;;
        *.pdf|*.epub|*.djvu|*.cbz)
            if have zathura; then launch_detached zathura "$target"; return 0; fi
            if have evince; then launch_detached evince "$target"; return 0; fi
            if have okular; then launch_detached okular "$target"; return 0; fi
            launch_browser "$target"
            return $?
            ;;
        *.mp4|*.mkv|*.webm|*.mov|*.avi|*.mp3|*.flac|*.ogg|*.opus|*.wav|*.m4a)
            if have mpv; then launch_detached mpv "$target"; return 0; fi
            if have vlc; then launch_detached vlc "$target"; return 0; fi
            if have cellulloid; then launch_detached cellulloid "$target"; return 0; fi
            ;;
        *.zip|*.tar|*.tar.gz|*.tgz|*.tar.xz|*.tar.zst|*.7z|*.rar)
            if have file-roller; then launch_detached file-roller "$target"; return 0; fi
            if have ark; then launch_detached ark "$target"; return 0; fi
            launch_files "$(dirname "$target")"
            return $?
            ;;
        *.html|*.htm)
            launch_browser "$target"
            return $?
            ;;
        *.md|*.txt|*.json|*.toml|*.yaml|*.yml|*.lua|*.sh|*.zsh|*.c|*.cpp|*.h|*.hpp|*.rs|*.go|*.js|*.ts|*.css|*.conf|*.ini|*.log)
            local editor term
            editor="$(mono_var editor nvim)"
            term="$(resolve_cmd "$(mono_var terminal kitty)" kitty foot alacritty wezterm xterm)" || term=""
            if [ -n "$term" ] && have "$editor"; then
                # shellcheck disable=SC2086
                launch_detached $term -e "$editor" "$target"
                return 0
            fi
            ;;
    esac

    # 7. Generic XDG fallback
    if have xdg-open; then
        launch_detached xdg-open "$target"
        return 0
    fi
    if have gio; then
        launch_detached gio open "$target"
        return 0
    fi
    notify "No handler found" "Could not open ${target##*/}"
    return 1
}

# Interactive Rofi Universal Picker (`Super + Shift + O`): apps, Flatpaks, AppImages & recent files
picker_menu() {
    sync_appimages
    local entries=()

    # Collect Flatpak apps explicitly too
    if have flatpak; then
        local app_id app_name
        while IFS=$'\t' read -r app_name app_id; do
            [ -n "$app_id" ] && entries+=("󰏖  Flatpak: ${app_name}  ::  ${app_id}")
        done < <(flatpak list --app --columns=name,application 2>/dev/null || true)
    fi

    # Collect AppImages in ~/Applications & ~/Downloads
    local f
    for f in "$HOME"/Applications/*.AppImage "$HOME"/Applications/*.appimage "$HOME"/Downloads/*.AppImage "$HOME"/Downloads/*.appimage; do
        [ -f "$f" ] && entries+=("󰀻  AppImage: ${f##*/}  ::  ${f}")
    done

    # Collect top-level files in common user directories (fast depth<=2 scan)
    if have find; then
        local p
        while IFS= read -r p; do
            [ -n "$p" ] && entries+=("󰈔  File: ${p#"$HOME"/}  ::  ${p}")
        done < <(find "$HOME/Documents" "$HOME/Downloads" "$HOME/Pictures" "$HOME/Videos" "$HOME/Desktop" \
            -maxdepth 2 -type f -not -name '.*' 2>/dev/null | head -n 160 || true)
    fi

    entries+=(
        "󰖟  Open Browser (OpenCode Startpage)  ::  ${CONFIG_DIR}/browser/opencode/startpage.html"
        "󰉋  Browse Files in Rofi               ::  __rofi_filebrowser__"
        "󰘳  Launch Any App / CLI (Rofi Combi)  ::  __rofi_launcher__"
    )

    local choice payload
    choice="$(printf '%s\n' "${entries[@]}" | rofi_menu list -dmenu -i -p "open anything (file / url / appimage / flatpak / cmd)")" || exit 0
    [ -n "$choice" ] || exit 0

    case "$choice" in
        *"  ::  "*) payload="${choice##*  ::  }" ;;
        *)          payload="$choice" ;;
    esac

    case "$payload" in
        __rofi_filebrowser__)
            rofi_menu launcher -show filebrowser -display-filebrowser "files"
            ;;
        __rofi_launcher__)
            "$SCRIPTS_DIR/menu.sh" launcher
            ;;
        *)
            open_target "$payload"
            ;;
    esac
}

case "${1:-picker}" in
    open)           shift; open_target "${1:-}" "${@:2}" ;;
    appimage)       shift; run_appimage "${1:-}" "${@:2}" ;;
    sync-appimages) sync_appimages ;;
    browser)        shift; launch_browser "$@" ;;
    files)          shift; launch_files "${1:-$HOME}" ;;
    terminal)       shift; launch_terminal "$@" ;;
    picker|menu)    picker_menu ;;
    *)
        open_target "$@"
        ;;
esac
