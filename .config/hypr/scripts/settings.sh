#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════════
#  Monochrome settings menu (Super + I)
#
#    System information · Network · Bluetooth · Monitors · Sound
#    Night shift · Wallpaper · Hyprland / config files
#
#  Uses nothing but nmcli / bluetoothctl / hyprctl / wpctl / pactl, so it keeps
#  working even if you swap out other parts of the setup.
#
#  Menu entries are "key<TAB>label" — rofi only shows the label, we act on the key.
# ═══════════════════════════════════════════════════════════════════════════

set -u
. "$(dirname "$0")/lib.sh"

# Tolerate --flags as well as plain arguments (handy in keybinds).
case "${1:-}" in --*) set -- "${1#--}" "${@:2}" ;; esac

have rofi || exit 1

MON_STATE="$CACHE_DIR/monitors"
mkdir -p "$MON_STATE" 2>/dev/null || true

TAB=$'\t'

# ── rofi plumbing ──────────────────────────────────────────────────────────

pick() {  # pick <menu> <prompt> [extra rofi args…]  → prints the full line
    local menu="$1" prompt="$2"; shift 2
    rofi_menu "$menu" -dmenu -i -display-columns 2 -display-column-separator "$TAB" -p "$prompt" "$@"
}

choose() {  # choose <menu> <prompt> [entries…]  → prints the key (column 1)
    local menu="$1" prompt="$2"; shift 2
    local line
    line="$(printf '%s\n' "$@" | pick "$menu" "$prompt")" || return 1
    [ -z "$line" ] && return 1
    printf '%s' "${line%%$TAB*}"
}

show() {  # show <menu> <prompt> [lines…]  — read-only viewer, Enter closes
    printf '%s\n' "$@" | pick "$menu" "$prompt" >/dev/null 2>&1 || true
}

open_file() {
    local f="$1"
    [ -e "$f" ] || { notify "Missing file" "$f" "dialog-error"; return 1; }
    if have code;     then setsid code "$f" >/dev/null 2>&1 &
    elif have codium; then setsid codium "$f" >/dev/null 2>&1 &
    elif have kitty;  then setsid kitty -e "${VISUAL:-nvim}" "$f" >/dev/null 2>&1 &
    else notify "No editor found" "Edit $f manually" "dialog-warning"; fi
}

# ═══════════════════════════ 1. SYSTEM INFORMATION ═════════════════════════

system_info() {
    local batt info
    batt="$(for b in /sys/class/power_supply/BAT*; do
                [ -r "$b/capacity" ] && printf '%s%% (%s)' "$(cat "$b/capacity")" "$(cat "$b/status")"
            done 2>/dev/null | head -1)"

    info="$(printf '%s\n' \
        "Host         $(hostnamectl --static 2>/dev/null || hostname)" \
        "Kernel       $(uname -r)" \
        "Uptime       $(uptime -p 2>/dev/null | sed 's/^up //')" \
        "Session      Hyprland $(hyprctl version -j 2>/dev/null | jq -r '.tag // "?"')" \
        "CPU          $(awk -F': ' '/model name/ {print $2; exit}' /proc/cpuinfo 2>/dev/null)" \
        "GPU          $(lspci 2>/dev/null | awk -F': ' '/VGA|3D controller/ {print $3; exit}')" \
        "Memory       $(free -h | awk '/^Mem:/ {print $3 " / " $2}')" \
        "Disk /home   $(df -h "$HOME" 2>/dev/null | awk 'NR==2 {print $3 " / " $2 " (" $5 " used)"}')" \
        "Packages     $(pacman -Q 2>/dev/null | wc -l) (pacman)" \
        "Local IP     $(ip -4 -o addr show scope global 2>/dev/null | awk '{print $2 " " $4; exit}')" \
        "Battery      ${batt:-—}")"

    local key line
    line="$(printf '%s\n' "$info" \
        "copy${TAB}󰼏  Copy everything to the clipboard" \
        "report${TAB}󰣹  Full report (fastfetch)" | pick settings "system info:")" || return 0
    key="${line%%$TAB*}"
    case "$key" in
        copy)   printf '%s\n' "$info" | wl-copy 2>/dev/null && notify "System info" "Copied to clipboard" ;;
        report) if have fastfetch && have kitty; then
                    setsid kitty --class kitty-float -e fastfetch >/dev/null 2>&1 &
                else notify "fastfetch missing" "sudo pacman -S fastfetch" "dialog-warning"; fi ;;
    esac
}

# ═════════════════════════════════ 2. NETWORK ═════════════════════════════

wifi_radio() { nmcli -t -f WIFI general 2>/dev/null | head -1; }

connect_ssid() {
    local ssid="$1" sec="$2" out pw
    if [ "$sec" = "open" ]; then
        out="$(nmcli device wifi connect "$ssid" 2>&1)"
    else
        pw="$(rofi_menu settings -dmenu -password -i -p "password for ${ssid}:")" || return 0
        [ -z "$pw" ] && return 0
        out="$(nmcli device wifi connect "$ssid" password "$pw" 2>&1)"
    fi
    if printf '%s' "$out" | grep -qi 'successfully activated'; then
        notify "Wi-Fi" "Connected to ${ssid}" network-wireless
    else
        notify "Wi-Fi failed" "$out" dialog-error
    fi
}

network_menu() {
    while :; do
        local key
        key="$(choose settings "network:" \
            "radio$TAB󰖩  Wi-Fi: $(wifi_radio)" \
            "list$TAB󰖩  Connect to a network" \
            "saved$TAB󰈀  Saved connections" \
            "details$TAB󰋼  Connection details")" || return 0

        case "$key" in
            radio)
                if [ "$(wifi_radio)" = "enabled" ]; then nmcli radio wifi off; notify "Wi-Fi" "Off"
                else nmcli radio wifi on; notify "Wi-Fi" "On"; fi ;;

            list)
                nmcli device wifi rescan >/dev/null 2>&1
                local line ssid sec
                local -a entries=("refresh$TAB󰑐  Rescan")
                while IFS=: read -r inuse ssid signal security; do
                    [ -z "${ssid:-}" ] && continue
                    [ "$security" = "" ] && security="open"
                    entries+=("$ssid$TAB$(printf '%s  %s  %s%%' "$([ "$inuse" = "*" ] && echo '●' || echo ' ')" "$ssid" "$signal")")
                done < <(nmcli -t -e no -f IN-USE,SSID,SIGNAL,SECURITY device wifi list 2>/dev/null)
                if [ "${#entries[@]}" -le 1 ]; then
                    notify "Wi-Fi" "No networks found" "dialog-information"; continue
                fi
                line="$(printf '%s\n' "${entries[@]}" | pick settings "wi-fi:")" || continue
                ssid="${line%%$TAB*}"
                [ "$ssid" = "refresh" ] && continue
                sec="$(nmcli -t -e no -f SSID,SECURITY device wifi list 2>/dev/null |
                       awk -F: -v s="$ssid" '$1 == s { print ($2 == "" ? "open" : $2); exit }')"
                connect_ssid "$ssid" "${sec:-open}" ;;

            saved)
                local line name act
                line="$(nmcli -t -e no -f NAME,TYPE,DEVICE connection show 2>/dev/null |
                        awk -F: '$1 != "" { printf "%s\t%s  (%s)\n", $1, $2, ($3 == "" ? "-" : $3) }' |
                        pick settings "connection:")" || continue
                name="${line%%$TAB*}"
                [ -z "$name" ] && continue
                act="$(choose settings "$name:" \
                    "up$TAB󰖩  Connect" \
                    "down$TAB󰂲  Disconnect" \
                    "del$TAB󰆴  Forget")" || continue
                case "$act" in
                    up)   nmcli connection up "$name"   >/dev/null 2>&1 && notify "Network" "$name connected" ;;
                    down) nmcli connection down "$name" >/dev/null 2>&1 && notify "Network" "$name disconnected" ;;
                    del)  nmcli connection delete "$name" >/dev/null 2>&1 && notify "Network" "$name forgotten" ;;
                esac ;;

            details)
                local details
                details="$(printf '%s\n' "── devices ──" "$(nmcli device status 2>/dev/null)" \
                                          "── addresses ──" "$(ip -brief addr 2>/dev/null)" \
                                          "── routes ──" "$(ip route 2>/dev/null)")"
                printf '%s\n' "$details" | wl-copy 2>/dev/null
                show settings "details (copied to clipboard)" "$details" ;;
        esac
    done
}

# ════════════════════════════════ 3. BLUETOOTH ════════════════════════════

bt_powered() { bluetoothctl show 2>/dev/null | grep -qi 'Powered: yes' && echo on || echo off; }

bt_device_list() {
    bluetoothctl devices 2>/dev/null | while read -r _ mac name; do
        local info state
        info="$(bluetoothctl info "$mac" 2>/dev/null)"
        if printf '%s' "$info" | grep -q 'Connected: yes'; then state="connected"
        elif printf '%s' "$info" | grep -q 'Paired: yes';    then state="paired"
        else state="available"; fi
        printf '%s\t%s  (%s)\n' "$mac" "${name:-$mac}" "$state"
    done
}

bt_device_menu() {
    local mac="$1" name="$2"
    while :; do
        local info key
        info="$(bluetoothctl info "$mac" 2>/dev/null | grep -E 'Name|Connected|Paired|Trusted|Battery' | sed 's/^[[:space:]]*//')"
        key="$(choose settings "$name:" \
            "connect$TAB󰂱  Connect" \
            "disconnect$TAB󰂲  Disconnect" \
            "trust$TAB󰒓  Trust / untrust" \
            "remove$TAB󰆴  Remove / forget" \
            "info$TAB󰋼  $info")" || return 0
        case "$key" in
            connect)    bluetoothctl connect "$mac" >/dev/null 2>&1 && notify "Bluetooth" "$name connected" || notify "Bluetooth" "Could not connect to $name" dialog-error ;;
            disconnect) bluetoothctl disconnect "$mac" >/dev/null 2>&1 && notify "Bluetooth" "$name disconnected" ;;
            trust)      if bluetoothctl info "$mac" 2>/dev/null | grep -q 'Trusted: yes'; then
                            bluetoothctl untrust "$mac" >/dev/null 2>&1; notify "Bluetooth" "$name untrusted"
                        else bluetoothctl trust "$mac" >/dev/null 2>&1; notify "Bluetooth" "$name trusted"; fi ;;
            remove)     bluetoothctl remove "$mac" >/dev/null 2>&1 && notify "Bluetooth" "$name forgotten" && return 0 ;;
            info)       continue ;;
        esac
    done
}

bluetooth_menu() {
    while :; do
        local key line mac name
        key="$(choose settings "bluetooth:" \
            "power$TAB󰂯  Bluetooth: $(bt_powered)" \
            "scan$TAB󰂰  Scan for new devices" \
            "devices$TAB󰂱  Devices")" || return 0

        case "$key" in
            power)
                if [ "$(bt_powered)" = "on" ]; then bluetoothctl power off >/dev/null 2>&1; notify "Bluetooth" "Off"
                else bluetoothctl power on >/dev/null 2>&1; notify "Bluetooth" "On"; fi ;;
            scan)
                notify "Bluetooth" "Scanning for 10 seconds…" bluetooth
                ( bluetoothctl --timeout 10 scan on >/dev/null 2>&1
                  notify "Bluetooth" "Scan finished" bluetooth ) & ;;
            devices)
                line="$(bt_device_list | pick settings "device:")" || continue
                mac="${line%%$TAB*}"
                name="$(printf '%s' "$line" | sed -e "s/^${mac}${TAB}//" -e 's/  (.*)$//')"
                [ -n "$mac" ] && bt_device_menu "$mac" "${name:-$mac}" ;;
        esac
    done
}

# ═════════════════════════════════ 4. MONITORS ════════════════════════════

# Choices are remembered in $MON_STATE/<output> and written to a real config
# file, so they survive a reboot (loaded by modules/monitors.lua).

mon_set() {  # mon_set <output> <key> <value>
    local file="$MON_STATE/$1" mode position scale transform
    if [ -f "$file" ]; then IFS='|' read -r mode position scale transform < "$file"; fi
    mode="${mode:-preferred}"; position="${position:-auto}"; scale="${scale:-auto}"; transform="${transform:-}"
    case "$2" in
        mode)      mode="$3" ;;
        position)  position="$3" ;;
        scale)     scale="$3" ;;
        transform) transform="$3" ;;
    esac
    printf '%s|%s|%s|%s' "$mode" "$position" "$scale" "$transform" > "$file"
}

mon_write_config() {
    local out="$HYPR_DIR/modules/monitors-user.lua" f name mode position scale transform scale_lua
    {
        echo "-- ═══════════════════════════════════════════════════════════════════"
        echo "--  modules/monitors-user.lua — written by the settings menu (Super + I)"
        echo "--  Loaded automatically by modules/monitors.lua. Edit by hand freely."
        echo "-- ═══════════════════════════════════════════════════════════════════"
        for f in "$MON_STATE"/*; do
            [ -f "$f" ] || continue
            name="$(basename "$f")"
            IFS='|' read -r mode position scale transform < "$f"
            [ -z "${mode:-}" ] && mode="preferred"
            [ -z "${position:-}" ] && position="auto"
            [ -z "${scale:-}" ] && scale="auto"
            if [[ "$scale" =~ ^[0-9.]+$ ]]; then scale_lua="$scale"; else scale_lua="\"$scale\""; fi
            if [ -n "${transform:-}" ]; then
                printf 'hl.monitor({ output = "%s", mode = "%s", position = "%s", scale = %s, transform = %s })\n' \
                    "$name" "$mode" "$position" "$scale_lua" "$transform"
            else
                printf 'hl.monitor({ output = "%s", mode = "%s", position = "%s", scale = %s })\n' \
                    "$name" "$mode" "$position" "$scale_lua"
            fi
        done
    } > "$out"
    hyprctl reload >/dev/null 2>&1
    notify "Monitors" "Saved to modules/monitors-user.lua and applied" video-display
}

mon_disable() {
    local name="$1" out="$HYPR_DIR/modules/monitors-user.lua"
    rm -f "$MON_STATE/$name"
    mon_write_config
    printf 'hl.monitor({ output = "%s", disabled = true })\n' "$name" >> "$out"
    hyprctl reload >/dev/null 2>&1
    notify "Monitors" "$name disabled" video-display
}

monitor_menu() {
    local name="$1"
    while :; do
        local key choice
        key="$(choose settings "$name:" \
            "mode$TAB󰍹  Resolution & refresh rate" \
            "scale$TAB󰍹  Scale (HiDPI)" \
            "rotate$TAB󰍹  Rotation" \
            "position$TAB󰍹  Position in the layout" \
            "toggle$TAB󰍹  Enable / disable this monitor")" || return 0

        case "$key" in
            mode)
                local -a modes=("preferred${TAB}preferred (recommended)")
                while IFS= read -r m; do
                    [ -n "$m" ] && modes+=("$m${TAB}$m")
                done < <(hyprctl monitors all -j 2>/dev/null |
                         jq -r --arg n "$name" '.[] | select(.name==$n) | .availableModes[]?' 2>/dev/null |
                         sed 's/Hz$//' | awk 'NF')
                choice="$(choose settings "mode:" "${modes[@]}")" || continue
                [ -n "$choice" ] && { mon_set "$name" mode "$choice"; mon_write_config; } ;;
            scale)
                choice="$(choose settings "scale:" "auto${TAB}auto (Hyprland decides)" "1${TAB}1.0" "1.25${TAB}1.25" "1.5${TAB}1.5" "1.75${TAB}1.75" "2${TAB}2.0")" || continue
                [ -n "$choice" ] && { mon_set "$name" scale "$choice"; mon_write_config; } ;;
            rotate)
                choice="$(choose settings "rotation:" \
                    "0${TAB}normal" "1${TAB}90°" "2${TAB}180°" "3${TAB}270°" \
                    "4${TAB}flipped" "5${TAB}flipped 90°" "6${TAB}flipped 180°" "7${TAB}flipped 270°")" || continue
                [ -n "$choice" ] && { mon_set "$name" transform "$choice"; mon_write_config; } ;;
            position)
                choice="$(choose settings "position:" \
                    "auto${TAB}auto" "auto-right${TAB}to the right" "auto-left${TAB}to the left" \
                    "auto-up${TAB}above" "auto-down${TAB}below" "0x0${TAB}0x0 (origin)")" || continue
                [ -n "$choice" ] && { mon_set "$name" position "$choice"; mon_write_config; } ;;
            toggle)
                if hyprctl monitors all -j 2>/dev/null | jq -e --arg n "$name" '.[] | select(.name==$n) | (.disabled // false)' >/dev/null 2>&1; then
                    mon_set "$name" mode preferred; mon_write_config
                else
                    mon_disable "$name"
                fi ;;
        esac
    done
}

monitors_menu() {
    local line name
    line="$(hyprctl monitors -j 2>/dev/null |
            jq -r '.[] | "\(.name)\t\(.width)x\(.height)@\(.refreshRate|floor)Hz · scale \(.scale) · \(.x),\(.y)"' |
            pick settings "monitor:")" || return 0
    name="${line%%$TAB*}"
    [ -n "$name" ] && monitor_menu "$name"
}

# ══════════════════════════════════ 5. SOUND ══════════════════════════════

sink_list() {  # "ID<TAB>name"
    wpctl status 2>/dev/null | awk '/Sinks:/{f=1;next} /Sources:|Filters:|Streams:|Video|Devices:/{f=0} f' |
    sed -e 's/[│├└─]//g' -e 's/\[vol:[^]]*\]//g' -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//' |
    awk '{
        star = ($1 == "*") ? "*" : ""
        if (star) $1 = ""
        id = $1; sub(/\.$/, "", id); $1 = ""
        name = $0; sub(/^[[:space:]]+/, "", name)
        if (name != "" && id ~ /^[0-9]+$/) printf "%s%s\t%s\n", star, id, name
    }'
}

app_streams() {  # "index<TAB>app · volume"
    have pactl || return 0
    pactl list sink-inputs 2>/dev/null | awk '
        /^Sink Input #/        { idx = substr($3, 2); app = "(unknown)"; vol = "?" }
        /application.name = /  { app = $3; gsub(/"/, "", app) }
        /^[[:space:]]*Volume:/ { if (vol == "?" && match($0, /[0-9]+%/)) vol = substr($0, RSTART, RLENGTH) }
        /^$/                   { if (idx != "") { printf "%s\t%s · %s\n", idx, app, vol; idx = "" } }
    '
}

app_stream_menu() {
    local idx="$1" app="$2" key
    key="$(choose settings "$app:" \
        "up$TAB󰝝  Louder (+5%)" \
        "down$TAB󰝞  Quieter (-5%)" \
        "mute$TAB󰝟  Mute / unmute")" || return 0
    case "$key" in
        up)   pactl set-sink-input-volume "$idx" +5%   >/dev/null 2>&1 ;;
        down) pactl set-sink-input-volume "$idx" -5%   >/dev/null 2>&1 ;;
        mute) pactl set-sink-input-mute   "$idx" toggle >/dev/null 2>&1 ;;
    esac
}

sound_menu() {
    while :; do
        local key line id
        key="$(choose settings "sound:" \
            "up$TAB󰕾  Volume up" \
            "down$TAB󰖀  Volume down" \
            "mute$TAB󰝟  Mute / unmute output" \
            "mic$TAB󰍬  Mute / unmute microphone" \
            "sink$TAB󰓃  Choose output device" \
            "apps$TAB󰕾  Volume per application" \
            "mixer$TAB󰒓  Open the full mixer (pavucontrol)")" || return 0

        case "$key" in
            up)    "$SCRIPTS_DIR/volume.sh" up ;;
            down)  "$SCRIPTS_DIR/volume.sh" down ;;
            mute)  "$SCRIPTS_DIR/volume.sh" mute ;;
            mic)   "$SCRIPTS_DIR/volume.sh" mic ;;
            sink)
                line="$(sink_list | pick settings "output:")" || continue
                id="$(printf '%s' "${line%%$TAB*}" | tr -d '*')"
                [ -n "$id" ] && { wpctl set-default "$id" >/dev/null 2>&1; notify "Sound" "Default output changed" audio-volume-high; } ;;
            apps)
                line="$(app_streams | pick settings "app:")" || continue
                id="${line%%$TAB*}"
                [ -n "$id" ] && app_stream_menu "$id" "$(printf '%s' "$line" | cut -f2)" ;;
            mixer)
                if have pavucontrol; then setsid pavucontrol >/dev/null 2>&1 &
                else notify "pavucontrol missing" "sudo pacman -S pavucontrol" "dialog-warning"; fi ;;
        esac
    done
}

# ══════════════════════════════ 6. NIGHT SHIFT ════════════════════════════

nightshift_menu() {
    local key
    key="$(choose settings "night shift:" \
        "toggle$TAB󰖔  Toggle on / off" \
        "2700$TAB󰖔  Warmest · 2700K" \
        "3000$TAB󰖔  3000K" \
        "3500$TAB󰖔  3500K" \
        "4000$TAB󰖔  4000K (recommended)" \
        "5000$TAB󰖔  5000K" \
        "off$TAB󰖙  Turn off")" || return 0
    case "$key" in
        off) "$SCRIPTS_DIR/nightshift.sh" off ;;
        *)   "$SCRIPTS_DIR/nightshift.sh" "$key" ;;
    esac
}

# ═══════════════════════ 7. HYPRLAND / CONFIG FILES ═══════════════════════

hyprland_menu() {
    while :; do
        local key
        key="$(choose settings "hyprland:" \
            "reload$TAB󰑐  Reload Hyprland config" \
            "user$TAB󰏫  Edit user settings (user.lua)" \
            "binds$TAB󰏫  Edit keybinds (binds.lua)" \
            "appearance$TAB󰏫  Edit appearance (appearance.lua)" \
            "rules$TAB󰏫  Edit window rules (rules.lua)" \
            "monitors$TAB󰏫  Edit monitor config" \
            "waybar$TAB󰏫  Edit Waybar config" \
            "rofi$TAB󰏫  Edit Rofi theme" \
            "bar$TAB󰔄  Restart the top bar" \
            "wallpaper$TAB󰔄  Restart the wallpaper daemon" \
            "keybinds$TAB󰌌  Show all keybinds" \
            "shots$TAB󰄄  Open the screenshots folder" \
            "walls$TAB󰄄  Open the wallpapers folder")" || return 0

        case "$key" in
            reload)     hyprctl reload >/dev/null 2>&1; notify "Hyprland" "Config reloaded" ;;
            user)       open_file "$HYPR_DIR/modules/user.lua" ;;
            binds)      open_file "$HYPR_DIR/modules/binds.lua" ;;
            appearance) open_file "$HYPR_DIR/modules/appearance.lua" ;;
            rules)      open_file "$HYPR_DIR/modules/rules.lua" ;;
            monitors)   open_file "$HYPR_DIR/modules/monitors-user.lua" ;;
            waybar)     open_file "$CONFIG_DIR/waybar/config.jsonc" ;;
            rofi)       open_file "$ROFI_DIR/theme.rasi" ;;
            bar)        pkill -x waybar >/dev/null 2>&1; setsid waybar >/dev/null 2>&1 &
                        sleep 0.3; notify "Waybar" "Restarted" ;;
            wallpaper)  pkill -x awww-daemon >/dev/null 2>&1; sleep 0.3
                        setsid awww-daemon >/dev/null 2>&1 & sleep 0.6
                        "$SCRIPTS_DIR/wallpaper.sh" restore ;;
            keybinds)   setsid "$SCRIPTS_DIR/menu.sh" keybinds >/dev/null 2>&1 & ;;
            shots)      setsid xdg-open "$(mono_var screenshot_dir "$HOME/Pictures/Screenshots")" >/dev/null 2>&1 & ;;
            walls)      setsid xdg-open "$(mono_var wallpaper_dir "$HOME/Pictures/Wallpapers")" >/dev/null 2>&1 & ;;
        esac
    done
}

# ═══════════════════ 8. SCREENSHOTS & RECORDING ═══════════════════════════

capture_menu() {
    local rec audio key dir

    dir="$(mono_var recording_dir "$HOME/Videos/Recordings")"

    audio="$("$SCRIPTS_DIR/record.sh" audio show 2>/dev/null)"
    case "$audio" in
        off)     audio="no audio" ;;
        desktop) audio="desktop audio" ;;
        mic)     audio="microphone" ;;
        both)    audio="desktop + microphone" ;;
    esac

    rec="$("$SCRIPTS_DIR/record.sh" status 2>/dev/null)"
    case "$rec" in ""|idle) rec="idle" ;; esac

    key="$(choose settings "capture · ${rec%% (*}:" \
        "shoot$TAB  Screenshot: select an area" \
        "full$TAB  Screenshot: whole screen" \
        "ocr$TAB⌕  OCR: extract text from screen region" \
        "edit$TAB✎  Screenshot: select & annotate" \
        "toggle$TAB  Start / stop recording" \
        "region$TAB  Record a selected area" \
        "replay$TAB  Save the last minute (instant replay)" \
        "pause$TAB  Pause / resume recording" \
        "audio$TAB  Audio: $audio (change)" \
        "recs$TAB  Open the recordings folder" \
        "shots$TAB  Open the screenshots folder")" || return 0

    case "${key%%"$TAB"*}" in
        shoot)  "$SCRIPTS_DIR/screenshot.sh" region ;;
        full)   "$SCRIPTS_DIR/screenshot.sh" full ;;
        ocr)    "$SCRIPTS_DIR/screenshot.sh" ocr ;;
        edit)   "$SCRIPTS_DIR/screenshot.sh" edit ;;
        toggle) "$SCRIPTS_DIR/record.sh" toggle ;;
        region) "$SCRIPTS_DIR/record.sh" start region ;;
        replay) "$SCRIPTS_DIR/record.sh" replay ;;
        pause)  "$SCRIPTS_DIR/record.sh" pause ;;
        audio)  "$SCRIPTS_DIR/record.sh" audio next ;;
        recs)   setsid xdg-open "$dir" >/dev/null 2>&1 & ;;
        shots)  setsid xdg-open "$(mono_var screenshot_dir "$HOME/Pictures/Screenshots")" >/dev/null 2>&1 & ;;
    esac
}

# ════════════════════════════════ MAIN MENU ═══════════════════════════════

while :; do
    key="$(choose settings "settings:" \
        "palette$TAB⌘  Command palette & math calculator" \
        "focus$TAB◎  Deep-work focus & Zen mode" \
        "drawers$TAB▣  Named floating drawers (term / btop / notes)" \
        "sessions$TAB💾  Workspace session snapshots (save / restore)" \
        "layout$TAB⊞  Tiling layouts & gap presets" \
        "doctor$TAB✓  System & monochrome theme doctor" \
        "system$TAB󰍹  System information" \
        "network$TAB󰖩  Network" \
        "bluetooth$TAB󰂯  Bluetooth" \
        "monitors$TAB󰍹  Monitors" \
        "sound$TAB󰕾  Sound" \
        "nightshift$TAB󰖔  Night shift (blue light filter)" \
        "capture$TAB󰄄  Screenshots, OCR & recording" \
        "wallpaper$TAB󰸉  Wallpaper…" \
        "hyprland$TAB󰒓  Hyprland / config files")" || exit 0

    case "$key" in
        palette)    "$SCRIPTS_DIR/palette.sh"; exit 0 ;;
        focus)      "$SCRIPTS_DIR/focus.sh" menu ;;
        drawers)    "$SCRIPTS_DIR/scratchpad.sh" menu; exit 0 ;;
        sessions)   "$SCRIPTS_DIR/workspace-session.sh" menu ;;
        layout)     "$SCRIPTS_DIR/layout.sh" menu ;;
        doctor)     "$SCRIPTS_DIR/doctor.sh" --rofi ;;
        system)     system_info ;;
        network)    network_menu ;;
        bluetooth)  bluetooth_menu ;;
        monitors)   monitors_menu ;;
        sound)      sound_menu ;;
        nightshift) nightshift_menu ;;
        capture)    capture_menu ;;
        wallpaper)  "$SCRIPTS_DIR/wallpaper.sh" pick ;;
        hyprland)   hyprland_menu ;;
    esac
done
