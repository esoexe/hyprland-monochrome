#!/usr/bin/env bash
# Waybar Live CPU Micro-Sparkline (Braun / Teenage Engineering monochrome style).
# Pure-Bash zero-subprocess /proc reader (<0.4ms execution, 0 external forks).
# Maintains a 6-sample rolling buffer in XDG_CACHE_HOME and emits Waybar JSON.

set -u

CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/hyprland-monochrome"
[ -d "$CACHE" ] || mkdir -p "$CACHE" 2>/dev/null || true
STATE="$CACHE/sysgraph.state"

read -r _ u n s idle iow irq sirq steal _ < /proc/stat
total=$(( u + n + s + idle + iow + irq + sirq + steal ))
busy=$(( total - idle - iow ))

prev_total=0
prev_busy=0
hist="0 0 0 0 0"
if [ -f "$STATE" ]; then
    {
        read -r prev_total prev_busy || true
        read -r hist || true
    } < "$STATE"
fi

dt=$(( total - prev_total ))
db=$(( busy - prev_busy ))
if [ "$prev_total" -gt 0 ] && [ "$dt" -gt 0 ]; then
    pct=$(( 100 * db / dt ))
else
    pct=0
fi
[ "$pct" -lt 0 ] && pct=0
[ "$pct" -gt 100 ] && pct=100

read -r -a arr <<<"$hist"
arr+=("$pct")
if [ "${#arr[@]}" -gt 6 ]; then
    arr=("${arr[@]: -6}")
fi
new_hist="${arr[*]}"

printf '%s %s\n%s\n' "$total" "$busy" "$new_hist" > "$STATE"

BARS=(▁ ▂ ▃ ▄ ▅ ▆ ▇ █)
spark=""
for v in "${arr[@]}"; do
    idx=$(( v * 7 / 100 ))
    [ "$idx" -lt 0 ] && idx=0
    [ "$idx" -gt 7 ] && idx=7
    spark+="${BARS[$idx]}"
done

l1="0.00"; l2="0.00"; l3="0.00"
[ -r /proc/loadavg ] && read -r l1 l2 l3 _ < /proc/loadavg
load="$l1 $l2 $l3"

mem_total=0
mem_avail=0
if [ -r /proc/meminfo ]; then
    while read -r k val _; do
        case "$k" in
            MemTotal:)     mem_total="$val" ;;
            MemAvailable:) mem_avail="$val"; break ;;
        esac
    done < /proc/meminfo
fi

mem="0.0 / 0.0 GiB (0%)"
if [ "$mem_total" -gt 0 ]; then
    used_kb=$(( mem_total - mem_avail ))
    [ "$used_kb" -lt 0 ] && used_kb=0
    u10=$(( (used_kb * 10 + 524288) / 1048576 ))
    t10=$(( (mem_total * 10 + 524288) / 1048576 ))
    mpct=$(( used_kb * 100 / mem_total ))
    if [ "$u10" -lt 10 ]; then u_str="0.${u10}"; else u_str="${u10%?}.${u10: -1}"; fi
    if [ "$t10" -lt 10 ]; then t_str="0.${t10}"; else t_str="${t10%?}.${t10: -1}"; fi
    mem="${u_str} / ${t_str} GiB (${mpct}%)"
fi

printf '{"text":" %s %d%%","tooltip":"CPU %d%%  ·  load %s\\nRAM %s\\nClick: toggle floating btop++ drawer (Super + Shift + B)\\nRight-click: system doctor · Middle-click: velocity & glass menu"}\n' \
    "$spark" "$pct" "$pct" "$load" "$mem"
