-- ═══════════════════════════════════════════════════════════════════════════
--  modules/monitors.lua — monitor layout
--
--  Check your names with:  hyprctl monitors
--  The default below works everywhere: highest resolution, auto placement,
--  scale detected automatically (good for both 1080p and HiDPI panels).
-- ═══════════════════════════════════════════════════════════════════════════

-- Default: every connected screen, preferred mode, auto scale.
hl.monitor({
    output   = "",
    mode     = "preferred",
    position = "auto",
    scale    = "auto",
})


-- ── Examples — uncomment and adapt ────────────────────────────────────────
--
-- Laptop + external screen (laptop left, 1.25x scale on a HiDPI panel):
-- hl.monitor({ output = "eDP-1",   mode = "1920x1200@60", position = "0x0",    scale = 1.25 })
-- hl.monitor({ output = "HDMI-A-1", mode = "2560x1440@144", position = "auto-right", scale = 1 })
--
-- Mirror a second screen onto the laptop panel:
-- hl.monitor({ output = "HDMI-A-1", mode = "preferred", position = "auto", scale = 1, mirror = "eDP-1" })
--
-- Disable a broken / unused output:
-- hl.monitor({ output = "DP-3", disabled = true })
--
-- Force 10-bit colour + variable refresh rate on a gaming monitor:
-- hl.monitor({ output = "DP-1", mode = "2560x1440@165", position = "0x0", scale = 1, bitdepth = 10, vrr = 2 })
--
-- Reserve space so windows never hide behind the top bar (Waybar does this
-- itself with "exclusive": true, only add this if you use a non-exclusive bar):
-- hl.monitor({ output = "", reserved = { top = 40 } })


-- ── Workspaces pinned to a monitor (optional) ─────────────────────────────
-- Handy on multi-screen setups: keep 1-5 on the laptop, 6-0 on the external.
--
-- hl.workspace_rule({ workspace = "1", monitor = "eDP-1", default = true })
-- hl.workspace_rule({ workspace = "6", monitor = "HDMI-A-1", default = true })


-- ── Monitors saved from the settings menu (Super + I → Monitors) ─────────
-- The menu writes modules/monitors-user.lua. It is optional: a missing file
-- is ignored, which is why it is loaded through pcall.

local ok, err = pcall(require, "modules/monitors-user")
if not ok then
    -- no saved monitor layout yet — nothing to do
end
