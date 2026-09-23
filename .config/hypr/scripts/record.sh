#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════════
#  record.sh — screen recording for hyprland-monochrome.
#
#  Uses whichever recorder you have installed:
#
#      gpu-screen-recorder   GPU accelerated, instant replay, pause   (preferred)
#      wf-recorder           software / VAAPI, one audio source       (fallback)
#
#  Usage
#      record.sh toggle [screen|region]   start, or stop if running   ← keybind
#      record.sh start  [screen|region]   start recording
#      record.sh stop                     stop, save, notify, copy the path
#      record.sh pause                    pause / resume (gpu-screen-recorder)
#      record.sh replay                   save the last N seconds (instant replay)
#      record.sh status [--waybar]        one line, or JSON for the Waybar module
#      record.sh audio [show|next|off|desktop|mic|both]
#      record.sh open | path              open / print the recordings folder
#
#  Keybinds (see modules/binds.lua):
#      mod + ALT + PRINT           record the screen
#      mod + ALT + SHIFT + PRINT   record a selected area
#      mod + ALT + S               save the last minute
#      mod + ALT + P               pause / resume
#
#  Values come from modules/user.lua: recording_dir, record_fps, record_audio,
#  replay_seconds, record_backend.
# ═══════════════════════════════════════════════════════════════════════════

set -u
. "$(dirname "$0")/lib.sh"

# Tolerate --flags as well as plain arguments (handy in keybinds).
case "${1:-}" in --*) set -- "${1#--}" "${@:2}" ;; esac

STATE="$CACHE_DIR/record.state"
LOG="$CACHE_DIR/record.log"
PAUSED="$CACHE_DIR/record.paused"
AUDIO_FILE="$CACHE_DIR/record.audio"

# ── State helpers ──────────────────────────────────────────────────────────

state_get() {  # state_get <key>  →  the value, or nothing
    sed -n "s/^$1=//p" "$STATE" 2>/dev/null | head -1
}

rec_alive() { [ -n "${1:-}" ] && kill -0 "$1" 2>/dev/null; }

rec_pid() { state_get pid; }

recording() { rec_alive "$(rec_pid)"; }

is_replay() { [ -n "$(state_get replay)" ]; }

forget() { rm -f "$STATE" "$PAUSED"; }

backend_name() {  # what is running, or what we would use
    local b
    b="$(state_get backend)"
    [ -n "$b" ] && { printf '%s' "$b"; return; }
    b="$(mono_var record_backend auto)"
    case "$b" in
        gsr|gpu-screen-recorder) printf 'gsr'; return ;;
        wf|wf-recorder)          printf 'wf-recorder'; return ;;
    esac
    if have gpu-screen-recorder; then printf 'gsr'
    elif have wf-recorder;       then printf 'wf-recorder'
    else printf ''; fi
}

dir() { mono_var recording_dir "$HOME/Videos/Recordings"; }

audio_mode() {  # off | desktop | mic | both
    local m
    m="$(cat "$AUDIO_FILE" 2>/dev/null)"
    [ -z "$m" ] && m="$(mono_var record_audio desktop)"
    case "$m" in
        off|desktop|mic|both) printf '%s' "$m" ;;
        *) printf 'desktop' ;;
    esac
}

audio_label() {
    case "$1" in
        off)     printf 'no audio' ;;
        desktop) printf 'desktop audio' ;;
        mic)     printf 'microphone' ;;
        both)    printf 'desktop + microphone' ;;
    esac
}

elapsed() {  # → "0:42", "1:02:03"
    local start secs
    start="$(state_get started)"
    [ -z "$start" ] && { printf '0:00'; return; }
    secs=$(( $(date +%s) - start ))
    [ "$secs" -lt 0 ] && secs=0
    if [ "$secs" -ge 3600 ]; then
        printf '%d:%02d:%02d' $(( secs / 3600 )) $(( (secs % 3600) / 60 )) $(( secs % 60 ))
    else
        printf '%d:%02d' $(( secs / 60 )) $(( secs % 60 ))
    fi
}

human_time() { printf '%d:%02d' $(( $1 / 60 )) $(( $1 % 60 )); }

# ── Launching ──────────────────────────────────────────────────────────────

# Start a recorder detached, and hand back its real PID.
# `setsid` forks when the shell has job control on (running this by hand in a
# terminal), which makes $! the parent — so fall back to looking the process up
# by its unique output file, or by name.
# Sets LAUNCH_PID instead of printing it: launching inside "$( )" would put the
# recorder in a subshell of a subshell, and the PID would point at nothing.
LAUNCH_PID=""
launch() {  # launch <unique-path-or-name-hint> <command…>
    local hint="$1"; shift
    local pidfile="$CACHE_DIR/record.pid" pid real i

    rm -f "$pidfile"

    # Job control is enabled around the fork on purpose: a background job started
    # *without* it inherits SIGINT and SIGQUIT set to ignore, and then "stop"
    # could never reach the recorder (this bit us in testing). With job control
    # the recorder gets its own process group and $! is its exact PID.
    (
        set -m
        "$@" >"$LOG" 2>&1 </dev/null &
        printf '%s' "$!" > "$pidfile"
        disown
    ) 2>/dev/null

    pid=""
    i=0
    while [ "$i" -lt 20 ]; do
        sleep 0.1; i=$((i + 1))
        pid="$(cat "$pidfile" 2>/dev/null)"
        if [ -z "$pid" ] || ! rec_alive "$pid"; then
            # Belt and braces: if the launch ever goes through something that
            # forks, find the recorder by its unique output path instead.
            real="$(pgrep -f -u "$(id -u)" -- "$hint" 2>/dev/null | tail -1)"
            [ -n "$real" ] && pid="$real"
        fi
        rec_alive "$pid" && break
    done

    rm -f "$pidfile"
    LAUNCH_PID="$pid"
}

# Wait a moment to catch a recorder that dies on startup (no GPU permissions,
# wrong portal, bad codec…), and report what it said.
confirm_started() {  # confirm_started <pid> <label>
    local pid="$1" label="$2" i=0
    while [ "$i" -lt 10 ]; do
        rec_alive "$pid" || break
        sleep 0.1; i=$((i + 1))
    done
    if rec_alive "$pid"; then
        return 0
    fi
    notify "$label failed" "$(tail -2 "$LOG" 2>/dev/null | tr '\n' ' ')" "dialog-error"
    forget
    return 1
}

# ── Backend command lines ──────────────────────────────────────────────────

audio_args() {  # audio_args <backend> <mode>  →  NUL separated flags
    local backend="$1" mode="$2"
    [ "$mode" = "off" ] && return 0
    case "$backend" in
        gsr)
            case "$mode" in
                desktop) printf '%s\0' -a default_output ;;
                mic)     printf '%s\0' -a default_input ;;
                both)    printf '%s\0' -a "default_output|default_input" ;;
            esac ;;
        wf-recorder)
            case "$mode" in
                desktop|both)
                    # Bare --audio in wf-recorder captures default_source (mic);
                    # desktop sound is the default sink's .monitor source.
                    local sink; sink="$(default_sink)"
                    if [ -n "$sink" ]; then printf '%s\0' "--audio=${sink}.monitor"
                    else printf '%s\0' --audio; fi ;;
                mic)
                    # wf-recorder takes a single device; an empty result from
                    # pactl would turn into "--audio=", so fall back to default.
                    local src; src="$(default_source)"
                    if [ -n "$src" ]; then printf '%s\0' "--audio=$src"
                    else printf '%s\0' --audio; fi ;;
            esac ;;
    esac
}

default_source() { have pactl && pactl get-default-source 2>/dev/null || true; }
default_sink()   { have pactl && pactl get-default-sink   2>/dev/null || true; }

# Region selection. gpu-screen-recorder wants "WxH+X+Y", wf-recorder takes what
# slurp prints ("X,Y WxH"). Exit 2 = the user cancelled.
pick_region() {
    local backend="$1" geom
    have slurp || { notify "Recording failed" "slurp is not installed" "dialog-error"; return 1; }
    case "$backend" in
        gsr) geom="$(slurp -f '%wx%h+%x+%y' 2>/dev/null)" ;;
        *)   geom="$(slurp 2>/dev/null)" ;;
    esac
    [ -z "$geom" ] && return 2
    printf '%s' "$geom"
}

write_state() {  # write_state pid backend mode audio file [replay]
    {
        printf 'pid=%s\n'     "$1"
        printf 'backend=%s\n' "$2"
        printf 'mode=%s\n'    "$3"
        printf 'audio=%s\n'   "$4"
        printf 'file=%s\n'    "$5"
        [ "${6:-}" = "1" ] && printf 'replay=1\n'
        printf 'started=%s\n' "$(date +%s)"
    } > "$STATE"
}

# ── Actions ────────────────────────────────────────────────────────────────

start() {
    local mode="${1:-screen}" backend audio fps savedir file geom rc pid

    if recording; then
        notify "Already recording" "Press the keybind again to stop"
        return 1
    fi
    forget

    backend="$(backend_name)"
    if [ -z "$backend" ]; then
        notify "No screen recorder found" "Install wf-recorder or gpu-screen-recorder" "dialog-error"
        return 1
    fi

    audio="$(audio_mode)"
    fps="$(mono_var record_fps 60)"
    savedir="$(dir)"
    mkdir -p "$savedir" 2>/dev/null || true
    file="$savedir/rec-$(date +%Y-%m-%d_%H-%M-%S).mp4"

    case "$mode" in
        region)
            geom="$(pick_region "$backend")"; rc=$?
            [ "$rc" = "2" ] && return 0            # Escape = do nothing
            [ "$rc" = "1" ] && return 1 ;;
        screen|"") mode="screen" ;;
        *) echo "usage: record.sh start|toggle [screen|region]" >&2; return 1 ;;
    esac

    # Command line. gpu-screen-recorder takes its audio sources before -o;
    # wf-recorder just appends them. -D (--no-damage) keeps wf-recorder ticking
    # on a static screen so SIGINT finalizes the MP4 immediately instead of
    # blocking in wl_display_dispatch until SIGKILL truncates the file.
    local -a base_cmd=() cmd=() extra=()
    case "$backend" in
        gsr)
            if [ "$mode" = "region" ]; then
                base_cmd=(gpu-screen-recorder -w region -region "$geom" -f "$fps" -c mp4)
            else
                base_cmd=(gpu-screen-recorder -w screen -f "$fps" -c mp4)
            fi ;;
        wf-recorder)
            base_cmd=(wf-recorder -y -D -f "$file" -r "$fps")
            [ "$mode" = "region" ] && base_cmd+=(-g "$geom") ;;
    esac

    cmd=("${base_cmd[@]}")
    while IFS= read -r -d '' arg; do extra+=("$arg"); done < <(audio_args "$backend" "$audio")
    [ "${#extra[@]}" -gt 0 ] && cmd+=("${extra[@]}")

    if [ "$backend" = "wf-recorder" ]; then
        [ "$audio" = "both" ] && notify "Recording audio" \
            "wf-recorder records one source — using desktop audio" "dialog-information"
    else
        cmd+=(-o "$file")
    fi

    launch "$file" "${cmd[@]}"
    pid="$LAUNCH_PID"
    if [ "$backend" = "wf-recorder" ] && [ "$audio" != "off" ]; then
        # wf-recorder aborts in pa_simple when no PulseAudio/PipeWire server is
        # reachable; fall back cleanly to video-only rather than dropping the take.
        local j=0
        while [ "$j" -lt 10 ]; do
            rec_alive "$pid" || break
            sleep 0.1; j=$((j + 1))
        done
        if ! rec_alive "$pid" && grep -qiE 'pa_simple|pulseaudio|connection refused' "$LOG" 2>/dev/null; then
            audio="off"
            launch "$file" "${base_cmd[@]}"
            pid="$LAUNCH_PID"
            notify "Audio unavailable" "No audio server reachable — recording video only" "dialog-information"
        fi
    fi
    confirm_started "$pid" "Recording" || return 1

    write_state "$pid" "$backend" "$mode" "$audio" "$file"
    notify "Recording started" "$(basename "$file") · $(audio_label "$audio")"
}

stop() {
    local pid file started secs size i

    if [ -z "$(rec_pid)" ]; then
        notify "Not recording" "Nothing to stop" "dialog-information"
        return 1
    fi

    pid="$(rec_pid)"
    file="$(state_get file)"
    started="$(state_get started)"

    if rec_alive "$pid"; then
        kill -INT "$pid" 2>/dev/null || true          # SIGINT = save and quit
        i=0
        while rec_alive "$pid" && [ "$i" -lt 30 ]; do sleep 0.1; i=$((i + 1)); done
        if rec_alive "$pid"; then                     # stubborn? escalate
            kill -TERM "$pid" 2>/dev/null || true
            i=0
            while rec_alive "$pid" && [ "$i" -lt 20 ]; do sleep 0.1; i=$((i + 1)); done
        fi
        if rec_alive "$pid"; then
            kill -KILL "$pid" 2>/dev/null || true
            notify "Recorder had to be killed" "The file may be truncated" "dialog-warning"
        fi
    fi

    local was_replay=0
    is_replay && was_replay=1
    forget

    if [ "$was_replay" = "1" ]; then
        notify "Instant replay off" "Press the replay key to start it again"
        return 0
    fi

    secs=$(( $(date +%s) - ${started:-$(date +%s)} ))

    if [ -f "$file" ]; then
        size="$(du -h "$file" 2>/dev/null | cut -f1)"
        if have wl-copy; then printf '%s' "$file" | wl-copy --type text/plain 2>/dev/null || true; fi
        notify "Recording saved" "$(basename "$file") · $(human_time "$secs") · ${size:-?} (path copied)"
    else
        notify "No file was written" "See $LOG for what the recorder said" "dialog-warning"
    fi
}

pause() {
    local pid
    if ! recording; then
        notify "Not recording" "Nothing to pause" "dialog-information"
        return 1
    fi
    pid="$(rec_pid)"
    case "$(state_get backend)" in
        gsr)
            kill -USR2 "$pid" 2>/dev/null || true     # SIGUSR2 = pause / unpause
            if [ -f "$PAUSED" ]; then
                rm -f "$PAUSED"
                notify "Recording resumed" "$(basename "$(state_get file)")"
            else
                : > "$PAUSED"
                notify "Recording paused" "Press again to resume"
            fi ;;
        *)
            notify "Pause is not supported" "wf-recorder cannot pause — try gpu-screen-recorder" "dialog-information" ;;
    esac
}

replay() {
    local backend seconds savedir file pid

    backend="$(backend_name)"
    case "$backend" in
        gsr) ;;
        "")  notify "No screen recorder found" "Install gpu-screen-recorder" "dialog-error"; return 1 ;;
        *)   notify "Instant replay needs gpu-screen-recorder" "wf-recorder keeps no buffer" "dialog-information"; return 1 ;;
    esac

    seconds="$(mono_var replay_seconds 60)"

    # Already buffering → save what is in the buffer (SIGUSR1).
    if is_replay && rec_alive "$(rec_pid)"; then
        pid="$(rec_pid)"
        : > "$LOG"
        kill -USR1 "$pid" 2>/dev/null || true
        local i=0
        while [ "$i" -lt 50 ]; do
            sleep 0.1; i=$((i + 1))
            [ -s "$LOG" ] && break
        done
        file="$(tail -1 "$LOG" 2>/dev/null)"
        if [ -n "$file" ] && [ -f "$file" ]; then
            if have wl-copy; then printf '%s' "$file" | wl-copy --type text/plain 2>/dev/null || true; fi
            notify "Replay saved" "$(basename "$file") ($(du -h "$file" 2>/dev/null | cut -f1), path copied)"
        else
            notify "Replay saved" "Stored in $(dir)"
        fi
        return 0
    fi

    if recording; then
        notify "Already recording" "Stop the recording before starting the replay buffer"
        return 1
    fi

    forget
    savedir="$(dir)"; mkdir -p "$savedir" 2>/dev/null || true

    local -a cmd=(gpu-screen-recorder -w screen -f "$(mono_var record_fps 60)" -c mp4 -r "$seconds") extra=()
    while IFS= read -r -d '' arg; do extra+=("$arg"); done < <(audio_args gsr "$(audio_mode)")
    [ "${#extra[@]}" -gt 0 ] && cmd+=("${extra[@]}")
    cmd+=(-o "$savedir")

    launch "gpu-screen-recorder" "${cmd[@]}"
    pid="$LAUNCH_PID"
    confirm_started "$pid" "Instant replay" || return 1

    write_state "$pid" gsr screen "$(audio_mode)" "$savedir" 1
    notify "Instant replay on" "Keeping the last ${seconds}s — press again to save"
}

status() {
    local pid backend mode file audio

    if ! recording; then
        [ -n "$(rec_pid)" ] && forget              # the recorder died: clean up
        if [ "${1:-}" = "--waybar" ]; then
            printf '{"text":"","alt":"","class":"off","tooltip":"Not recording — %s + Alt + Print"}\n' \
                "$(mono_var mainMod SUPER)"
        else
            printf 'idle\n'
        fi
        return 0
    fi

    backend="$(state_get backend)"
    mode="$(state_get mode)"
    audio="$(state_get audio)"
    file="$(state_get file)"

    if is_replay; then
        if [ "${1:-}" = "--waybar" ]; then
            printf '{"text":"● buf","alt":"replay","class":"on","tooltip":"Instant replay running — keeping the last %ss\\nClick to save"}\n' \
                "$(mono_var replay_seconds 60)"
        else
            printf 'replay buffer (%s) → %s\n' "$backend" "$file"
        fi
        return 0
    fi

    if [ -f "$PAUSED" ]; then
        if [ "${1:-}" = "--waybar" ]; then
            printf '{"text":"▮▮ %s","alt":"paused","class":"paused","tooltip":"Recording paused at %s\\n%s"}\n' \
                "$(elapsed)" "$(elapsed)" "$(basename "$file")"
        else
            printf 'paused %s (%s, %s)\n' "$(elapsed)" "$mode" "$backend"
        fi
        return 0
    fi

    if [ "${1:-}" = "--waybar" ]; then
        printf '{"text":"● %s","alt":"rec","class":"on","tooltip":"Recording %s · %s · %s\\n%s"}\n' \
            "$(elapsed)" "$(elapsed)" "$mode" "$(audio_label "$audio")" "$(basename "$file")"
    else
        printf 'recording %s (%s, %s, %s) → %s\n' \
            "$(elapsed)" "$mode" "$backend" "$(audio_label "$audio")" "$file"
    fi
}

audio_cmd() {
    local want="${1:-show}" mode
    mode="$(audio_mode)"
    case "$want" in
        show) printf '%s\n' "$mode"; return 0 ;;
        next)
            case "$mode" in
                desktop) mode="both" ;;
                both)    mode="mic" ;;
                mic)     mode="off" ;;
                off)     mode="desktop" ;;
            esac ;;
        off|desktop|mic|both) mode="$want" ;;
        *) echo "usage: record.sh audio [show|next|off|desktop|mic|both]" >&2; return 1 ;;
    esac
    printf '%s\n' "$mode" > "$AUDIO_FILE"
    if recording; then
        notify "Recording audio → $(audio_label "$mode")" "Applies to the next recording"
    else
        notify "Recording audio → $(audio_label "$mode")" "New recordings will capture this"
    fi
}

# ── Dispatch ───────────────────────────────────────────────────────────────

case "${1:-toggle}" in
    start)  shift; start "${1:-screen}" ;;
    stop)   stop ;;
    toggle)
        shift
        if recording; then stop; else start "${1:-screen}"; fi ;;
    pause|resume|pausetoggle) pause ;;
    replay) replay ;;
    status) shift; status "${1:-}" ;;
    audio)  shift; audio_cmd "${1:-show}" ;;
    path)   dir ;;
    open)   mkdir -p "$(dir)" 2>/dev/null || true; setsid xdg-open "$(dir)" >/dev/null 2>&1 & ;;
    *)
        cat >&2 <<EOF
usage: record.sh {toggle|start|stop|pause|replay|status|audio|open|path} [screen|region]

  toggle [screen|region]   start, or stop when already recording   (keybind)
  start  [screen|region]   start recording (region = drag to select)
  stop                     stop, save, and copy the file path
  pause                    pause / resume (gpu-screen-recorder only)
  replay                   save the last N seconds (gpu-screen-recorder only)
  status [--waybar]        one line of state, or JSON for Waybar
  audio [show|next|off|desktop|mic|both]
  open | path              open / print the recordings folder
EOF
        exit 1 ;;
esac
