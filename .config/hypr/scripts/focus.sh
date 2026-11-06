#!/usr/bin/env bash
# Deep-Work Focus Engine — Pomodoro timer, Zen distraction-free mode & DND.
#
#   focus.sh                 (Super + Shift + Z)  Rofi focus & Zen menu
#   focus.sh zen             (Super + Z)          toggle distraction-free Zen mode
#   focus.sh start [mins] [label]                 start countdown (default 25m)
#   focus.sh pause                                pause / resume active timer
#   focus.sh stop                                 cancel active timer
#   focus.sh toggle [mins] [label]                start if idle, stop if running
#   focus.sh dnd                                  toggle Dunst Do-Not-Disturb
#   focus.sh status [--waybar]                    current state / Waybar JSON

set -u
. "$(dirname "$0")/lib.sh"

case "${1:-}" in --*) set -- "${1#--}" "${@:2}" ;; esac

FOCUS_STATE="$CACHE_DIR/focus.state"
ZEN_STATE="$CACHE_DIR/zen.state"
SHARE_STATE="$CACHE_DIR/screenshare-dnd.state"

refresh_bar() {
    pkill -RTMIN+9 -x waybar 2>/dev/null || true
}

read_focus_state() {
    mode="" end_epoch=0 rem_secs=0 label="Deep Work" watcher_pid=""
    [ -f "$FOCUS_STATE" ] || return 1
    while IFS='=' read -r k v; do
        case "$k" in
            mode)        mode="$v" ;;
            end_epoch)   end_epoch="$v" ;;
            rem_secs)    rem_secs="$v" ;;
            label)       label="$v" ;;
            watcher_pid) watcher_pid="$v" ;;
        esac
    done < "$FOCUS_STATE"
    [ -n "$mode" ]
}

write_focus_state() {
    printf 'mode=%s\nend_epoch=%s\nrem_secs=%s\nlabel=%s\nwatcher_pid=%s\n' \
        "$1" "$2" "$3" "$4" "${5:-}" > "$FOCUS_STATE"
}

kill_watcher() {
    if [ -n "${watcher_pid:-}" ]; then
        kill "$watcher_pid" 2>/dev/null || true
    fi
}

spawn_watcher() { # spawn_watcher <seconds> <label> -> echoes background PID
    local secs="$1" lbl="$2"
    (
        sleep "$secs" || exit 0
        rm -f "$FOCUS_STATE"
        if [ ! -f "$ZEN_STATE" ] && have dunstctl; then
            dunstctl set-paused false >/dev/null 2>&1 || true
        fi
        pkill -RTMIN+9 -x waybar 2>/dev/null || true
        if have notify-send; then
            notify-send -a "mono" -i "appointment-soon" \
                "Focus complete" "$lbl finished — take a short break." 2>/dev/null || true
        fi
    ) </dev/null >/dev/null 2>&1 &
    printf '%s' "$!"
}

fmt_mmss() {
    local s="$1"
    [ "$s" -lt 0 ] && s=0
    printf '%02d:%02d' $(( s / 60 )) $(( s % 60 ))
}

cmd_start() {
    local mins="${1:-25}" lbl="${2:-Deep Work}"
    case "$mins" in ''|*[!0-9]*) mins=25 ;; esac
    [ "$mins" -lt 1 ] && mins=1

    if read_focus_state; then
        kill_watcher
    fi

    local secs=$(( mins * 60 ))
    local now end pid
    now="$(date +%s)"
    end=$(( now + secs ))
    pid="$(spawn_watcher "$secs" "$lbl")"
    write_focus_state "active" "$end" "$secs" "$lbl" "$pid"
    refresh_bar
    notify "Focus · $lbl" "${mins}m session started (Super + Shift + Z to manage)"
}

cmd_pause() {
    if ! read_focus_state; then
        cmd_start 25 "Deep Work"
        return
    fi
    local now
    now="$(date +%s)"
    if [ "$mode" = "active" ]; then
        local rem=$(( end_epoch - now ))
        [ "$rem" -lt 1 ] && rem=1
        kill_watcher
        write_focus_state "paused" "0" "$rem" "$label" ""
        refresh_bar
        notify "Focus paused" "$label — $(fmt_mmss "$rem") remaining"
    else
        local secs="${rem_secs:-60}"
        [ "$secs" -lt 1 ] && secs=60
        local end=$(( now + secs ))
        local pid
        pid="$(spawn_watcher "$secs" "$label")"
        write_focus_state "active" "$end" "$secs" "$label" "$pid"
        refresh_bar
        notify "Focus resumed" "$label — $(fmt_mmss "$secs") remaining"
    fi
}

cmd_stop() {
    if read_focus_state; then
        kill_watcher
        rm -f "$FOCUS_STATE"
        refresh_bar
        notify "Focus stopped" "$label session ended"
    else
        rm -f "$FOCUS_STATE"
        refresh_bar
    fi
}

cmd_zen() {
    if [ -f "$ZEN_STATE" ]; then
        local bar_was_on=0
        while IFS='=' read -r k v; do
            [ "$k" = "bar_was_on" ] && bar_was_on="$v"
        done < "$ZEN_STATE"
        rm -f "$ZEN_STATE"

        have dunstctl && dunstctl set-paused false >/dev/null 2>&1 || true

        local gi go bs
        gi="$(mono_var gaps_in 5)"
        go="$(mono_var gaps_out 10)"
        bs="$(mono_var border_size 1)"
        if have hyprctl; then
            hyprctl --batch "keyword general:gaps_in $gi; keyword general:gaps_out $go; keyword general:border_size $bs; keyword decoration:dim_strength 0.08" >/dev/null 2>&1 || true
        fi

        if [ "$bar_was_on" = "1" ]; then
            if pgrep -x waybar >/dev/null 2>&1; then
                pkill -SIGUSR1 -x waybar 2>/dev/null || true
            elif have waybar; then
                ( setsid waybar </dev/null >/dev/null 2>&1 & )
            fi
        fi
        refresh_bar
        notify "Zen mode off" "Bar, gaps & notifications restored"
    else
        local bar_was_on=0
        if pgrep -x waybar >/dev/null 2>&1; then
            bar_was_on=1
            pkill -SIGUSR1 -x waybar 2>/dev/null || true
        fi
        printf 'bar_was_on=%s\n' "$bar_was_on" > "$ZEN_STATE"

        if have hyprctl; then
            hyprctl --batch "keyword general:gaps_in 0; keyword general:gaps_out 28; keyword general:border_size 1; keyword decoration:dim_inactive true; keyword decoration:dim_strength 0.18" >/dev/null 2>&1 || true
        fi
        notify "Zen mode on" "Distractions muted · Super + Z to restore"
        sleep 0.15
        have dunstctl && dunstctl set-paused true >/dev/null 2>&1 || true
        refresh_bar
    fi
}

cmd_dnd() {
    if ! have dunstctl; then
        notify "Do Not Disturb" "dunstctl is not installed" "dialog-error"
        exit 1
    fi
    if [ "$(dunstctl is-paused 2>/dev/null)" = "true" ]; then
        dunstctl set-paused false >/dev/null 2>&1 || true
        notify "Notifications resumed" "Do Not Disturb is off"
    else
        notify "Do Not Disturb on" "Notifications paused"
        sleep 0.15
        dunstctl set-paused true >/dev/null 2>&1 || true
    fi
    refresh_bar
}

cmd_status() {
    local waybar_json=0
    [ "${1:-}" = "--waybar" ] && waybar_json=1

    local now rem mmss
    now="$(date +%s)"

    if read_focus_state; then
        if [ "$mode" = "active" ]; then
            rem=$(( end_epoch - now ))
            if [ "$rem" -le 0 ]; then
                rm -f "$FOCUS_STATE"
            else
                mmss="$(fmt_mmss "$rem")"
                if [ "$waybar_json" = "1" ]; then
                    jq -cn --arg t "◎ $mmss" --arg tip "Focus · $label ($mmss left)\nLeft-click: pause · Right-click: stop" \
                        '{text: $t, class: "on", tooltip: $tip}'
                else
                    printf 'active %s (%s)\n' "$mmss" "$label"
                fi
                return 0
            fi
        elif [ "$mode" = "paused" ]; then
            mmss="$(fmt_mmss "${rem_secs:-0}")"
            if [ "$waybar_json" = "1" ]; then
                jq -cn --arg t "◌ $mmss" --arg tip "Paused · $label ($mmss left)\nLeft-click: resume · Right-click: stop" \
                    '{text: $t, class: "paused", tooltip: $tip}'
            else
                printf 'paused %s (%s)\n' "$mmss" "$label"
            fi
            return 0
        fi
    fi

    if [ -f "$ZEN_STATE" ]; then
        if [ "$waybar_json" = "1" ]; then
            jq -cn '{text: "◌ ZEN", class: "zen", tooltip: "Zen mode active (Super + Z to exit)"}'
        else
            printf 'zen\n'
        fi
        return 0
    fi

    if [ "$waybar_json" = "1" ]; then
        printf '{"text":"","class":"off","tooltip":""}\n'
    else
        printf 'off\n'
    fi
}

cmd_menu() {
    local state_msg="Idle — pick a focus sprint or toggle distraction-free Zen mode"
    local now
    now="$(date +%s)"
    if read_focus_state; then
        if [ "$mode" = "active" ]; then
            state_msg="Active: $label ($(fmt_mmss $(( end_epoch - now ))) remaining)"
        else
            state_msg="Paused: $label ($(fmt_mmss "${rem_secs:-0}") remaining)"
        fi
    elif [ -f "$ZEN_STATE" ]; then
        state_msg="Zen Mode is ON — top bar hidden & notifications paused"
    fi

    local items=""
    if [ -f "$FOCUS_STATE" ]; then
        items+="⏯  Pause / Resume Focus Timer\n"
        items+="⏹  Stop Focus Timer\n"
    fi
    items+="◎  Start 25m Deep Work (Pomodoro)\n"
    items+="◎  Start 50m Extended Deep Work\n"
    items+="☕  Start 5m Short Break\n"
    items+="◌  Toggle Zen Mode (Super + Z)\n"
    items+="🔕  Toggle Do Not Disturb"

    local pick
    pick="$(printf '%b' "$items" | rofi_menu settings -dmenu -i -p "focus:" -mesg "$state_msg")" || exit 0
    case "$pick" in
        *"Pause / Resume"*) cmd_pause ;;
        *"Stop Focus"*)     cmd_stop ;;
        *"25m Deep Work"*)  cmd_start 25 "Deep Work" ;;
        *"50m Extended"*)   cmd_start 50 "Extended Focus" ;;
        *"5m Short Break"*) cmd_start 5  "Short Break" ;;
        *"Zen Mode"*)       cmd_zen ;;
        *"Do Not Disturb"*) cmd_dnd ;;
    esac
}

case "${1:-menu}" in
    menu)   cmd_menu ;;
    start)  cmd_start "${2:-25}" "${3:-Deep Work}" ;;
    pause)  cmd_pause ;;
    stop)   cmd_stop ;;
    toggle)
        if [ -f "$FOCUS_STATE" ]; then cmd_stop; else cmd_start "${2:-25}" "${3:-Deep Work}"; fi ;;
    zen)    cmd_zen ;;
    dnd)    cmd_dnd ;;
    screenshare-on)
        if have dunstctl && [ "$(dunstctl is-paused 2>/dev/null)" != "true" ]; then
            notify "Screen share privacy" "Notifications auto-muted while sharing"
            sleep 0.15
            printf 'auto_muted=1\n' > "$SHARE_STATE"
            dunstctl set-paused true >/dev/null 2>&1 || true
        fi ;;
    screenshare-off)
        if [ -f "$SHARE_STATE" ]; then
            rm -f "$SHARE_STATE"
            if [ ! -f "$ZEN_STATE" ] && have dunstctl; then
                dunstctl set-paused false >/dev/null 2>&1 || true
                notify "Screen share ended" "Notifications resumed"
            fi
        fi ;;
    status) cmd_status "${2:-}" ;;
    *)
        echo "usage: focus.sh {menu|start [mins] [label]|pause|stop|toggle|zen|dnd|status [--waybar]}" >&2
        exit 1 ;;
esac
