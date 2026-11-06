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

-- ── 4. Smart Single-Window Gallery Framing ─────────────────────────────────
-- Optional (`smart_single_window = true` in user.lua): gives a solo tiled
-- window generous 20px gallery margins and smoothly springs back to `gaps_out`
-- the instant a second window joins the workspace.
if enabled("smart_single_window", false) then
    local base_out = u.gaps_out or 10
    local function sync_solo_frame()
        if hl.get_active_workspace == nil or hl.get_workspace_windows == nil then return end
        local ws = hl.get_active_workspace()
        if ws == nil or ws.id == nil then return end
        local wins = hl.get_workspace_windows(ws.id) or {}
        local tiled = 0
        for _, win in ipairs(wins) do
            if not win.floating then tiled = tiled + 1 end
        end
        hl.config({ general = { gaps_out = (tiled == 1) and math.max(base_out, 20) or base_out } })
    end
    hl.on("window.open", sync_solo_frame)
    hl.on("window.destroy", sync_solo_frame)
    hl.on("window.move_to_workspace", sync_solo_frame)
    hl.on("workspace.active", sync_solo_frame)
end
