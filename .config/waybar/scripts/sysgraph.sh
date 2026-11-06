#!/usr/bin/env bash
# Waybar Live CPU Micro-Sparkline (Braun / Teenage Engineering monochrome style).
# Reads /proc/stat ticks, maintains a 6-sample rolling buffer in XDG_CACHE_HOME,
# and emits compact Waybar JSON:  {"text":"▁▂▄▆▃▂ 18%","tooltip":"..."}

set -u

CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/hyprland-monochrome"
mkdir -p "$CACHE" 2>/dev/null || true
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

# Shift 6-value rolling history cleanly without unquoted word-splitting
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

load="$(awk '{print $1 " " $2 " " $3}' /proc/loadavg 2>/dev/null || echo '0.00')"
mem="$(awk '/^MemTotal:/ {t=$2} /^MemAvailable:/ {a=$2} END {if (t>0) printf "%.1f / %.1f GiB (%d%%)", (t-a)/1048576, t/1048576, (t-a)*100/t}' /proc/meminfo 2>/dev/null)"

jq -cn \
    --arg text " $spark ${pct}%" \
    --arg tip "CPU ${pct}%  ·  load ${load}\nRAM ${mem}\nClick: toggle floating btop++ drawer (Super + Shift + B)" \
    '{text: $text, tooltip: $tip}'
