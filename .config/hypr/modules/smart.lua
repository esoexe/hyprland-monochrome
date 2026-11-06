-- ═══════════════════════════════════════════════════════════════════════════
--  modules/smart.lua — event-driven in-compositor automation (Hyprland 0.55+)
--
--  Uses Hyprland's native Lua `hl.on(...)` hooks instead of external polling
--  daemons: zero background CPU, instant reaction, toggled via modules/user.lua.
--  Docs: https://wiki.hypr.land/configuring/core/advanced-configuration/events/
-- ═══════════════════════════════════════════════════════════════════════════

local u       = user or {}
local home    = os.getenv("HOME") or ""
local scripts = home .. "/.config/hypr/scripts/"

local function enabled(key, default)
    if u[key] == nil then return default end
    return u[key]
end

-- ── 1. Screen-Share Privacy Shield ─────────────────────────────────────────
-- Automatically pauses Dunst notifications the instant a portal screen-share
-- starts (Zoom, Discord, OBS, browser) so private messages never leak live,
-- and restores notifications when sharing stops.
if enabled("smart_screenshare_dnd", true) then
    hl.on("screenshare.state", function(active, _kind, _name)
        if active then
            hl.exec_cmd(scripts .. "focus.sh screenshare-on")
        else
            hl.exec_cmd(scripts .. "focus.sh screenshare-off")
        end
    end)
end

-- ── 2. Monitor Hot-Plug Recovery ───────────────────────────────────────────
-- Re-applies the active monochrome wallpaper to newly connected displays and
-- refreshes Waybar geometry without needing a manual reload.
if enabled("smart_monitor_hotplug", true) then
    hl.on("monitor.added", function(_m)
        hl.exec_cmd("sleep 0.5 && " .. scripts .. "wallpaper.sh --restore")
        hl.exec_cmd("pkill -SIGUSR2 -x waybar 2>/dev/null || true")
    end)
    hl.on("monitor.removed", function(_m)
        hl.exec_cmd("pkill -SIGUSR2 -x waybar 2>/dev/null || true")
    end)
end

-- ── 3. Urgent Window Toast ─────────────────────────────────────────────────
-- Surfaces a discreet monochrome notification when a background workspace
-- requests attention (Super + U jumps directly to it).
if enabled("smart_urgent_notify", true) then
    hl.on("window.urgent", function(w)
        if w ~= nil and w.title ~= nil and w.title ~= "" then
            local safe = tostring(w.title):gsub("[\"\\`$]", "")
            hl.exec_cmd('notify-send -a mono -u low "Window needs attention" "' .. safe .. '  (Super + U)"')
        end
    end)
end
