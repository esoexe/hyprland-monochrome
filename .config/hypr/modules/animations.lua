-- ═══════════════════════════════════════════════════════════════════════════
--  modules/animations.lua — physics-driven spring & cubic-Bezier choreography
--
--  Four selectable profiles in modules/user.lua (`motion_profile`):
--    "fluid"     (default) Apple-style damped springs + velvet parallax
--    "snappy"              140ms critically damped studio precision
--    "cinematic"           deep showcase parallax + liquid border shimmer
--    "minimal"             zero motion / battery-saver mode
--
--  Switch live anytime with `Super + Shift + M` or `motion.sh menu`.
--  Docs: https://wiki.hypr.land/configuring/core/animations/
-- ═══════════════════════════════════════════════════════════════════════════

local u       = user or {}
local profile = u.motion_profile or "fluid"
local loop_on = u.border_loop
if loop_on == nil then
    loop_on = (profile == "cinematic")
end

if profile == "minimal" then
    hl.config({ animations = { enabled = false } })
    return
end

hl.config({ animations = { enabled = true } })

-- ── Physical Spring & Cubic-Bezier Curves ─────────────────────────────────
-- Springs (Hyprland 0.55+): mass = 1, stiffness = speed, damping = settle.
hl.curve("silkSpring",   { type = "spring", mass = 1.0, stiffness = 175, damping = 22 })
hl.curve("drawerSpring", { type = "spring", mass = 1.0, stiffness = 215, damping = 25 })
hl.curve("snappySpring", { type = "spring", mass = 1.0, stiffness = 310, damping = 30 })

hl.curve("expoOut",      { type = "bezier", points = { { 0.16, 1.00 }, { 0.30, 1.00 } } })
hl.curve("quintOut",     { type = "bezier", points = { { 0.22, 1.00 }, { 0.36, 1.00 } } })
hl.curve("quintInOut",   { type = "bezier", points = { { 0.65, 0.00 }, { 0.35, 1.00 } } })
hl.curve("velvetPop",    { type = "bezier", points = { { 0.12, 1.04 }, { 0.28, 1.00 } } })
hl.curve("monoSnap",     { type = "bezier", points = { { 0.15, 0.00 }, { 0.10, 1.00 } } })
hl.curve("monoLinear",   { type = "bezier", points = { { 0.00, 0.00 }, { 1.00, 1.00 } } })

-- Speed multiplier & parallax depth per profile
local k       = 1.0
local win_pop = "popin 86%"
local ws_par  = "slidefade 18%"
local win_crv = "silkSpring"

if profile == "snappy" then
    k       = 0.58
    win_pop = "popin 93%"
    ws_par  = "slidefade 10%"
    win_crv = "snappySpring"
elseif profile == "cinematic" then
    k       = 1.38
    win_pop = "popin 78%"
    ws_par  = "slidefade 26%"
    win_crv = "silkSpring"
end

-- ── Full 22-Leaf Hyprland Animation Tree ──────────────────────────────────
hl.animation({ leaf = "global",             enabled = true, speed = 6.0 * k, bezier = "quintInOut" })

-- Windows: physical spring open & retile, crisp exponential close
hl.animation({ leaf = "windows",            enabled = true, speed = 4.8 * k, spring = win_crv, style = win_pop })
hl.animation({ leaf = "windowsIn",          enabled = true, speed = 4.6 * k, spring = win_crv, style = win_pop })
hl.animation({ leaf = "windowsOut",         enabled = true, speed = 3.4 * k, bezier = "monoSnap", style = "popin 91%" })
hl.animation({ leaf = "windowsMove",        enabled = true, speed = 4.8 * k, spring = win_crv })

-- Layers: Rofi popin, Waybar top-slide, Dunst right-slide (paired with layer_rules)
hl.animation({ leaf = "layers",             enabled = true, speed = 3.8 * k, bezier = "expoOut" })
hl.animation({ leaf = "layersIn",           enabled = true, speed = 3.8 * k, bezier = "velvetPop", style = "popin 92%" })
hl.animation({ leaf = "layersOut",          enabled = true, speed = 2.8 * k, bezier = "monoSnap",  style = "fade" })

-- Opacity, focus-dimming, shadows & popups
hl.animation({ leaf = "fade",               enabled = true, speed = 3.8 * k, bezier = "expoOut" })
hl.animation({ leaf = "fadeIn",             enabled = true, speed = 3.8 * k, bezier = "expoOut" })
hl.animation({ leaf = "fadeOut",            enabled = true, speed = 3.0 * k, bezier = "monoSnap" })
hl.animation({ leaf = "fadeSwitch",         enabled = true, speed = 3.6 * k, bezier = "quintOut" })
hl.animation({ leaf = "fadeShadow",         enabled = true, speed = 4.0 * k, bezier = "quintOut" })
hl.animation({ leaf = "fadeDim",            enabled = true, speed = 4.2 * k, bezier = "quintOut" })
hl.animation({ leaf = "fadeLayersIn",       enabled = true, speed = 3.2 * k, bezier = "expoOut" })
hl.animation({ leaf = "fadeLayersOut",      enabled = true, speed = 2.6 * k, bezier = "monoSnap" })
hl.animation({ leaf = "fadePopupsIn",       enabled = true, speed = 2.8 * k, bezier = "expoOut" })
hl.animation({ leaf = "fadePopupsOut",      enabled = true, speed = 2.2 * k, bezier = "monoSnap" })
hl.animation({ leaf = "fadeDpms",           enabled = true, speed = 5.0 * k, bezier = "quintInOut" })

-- Active window border luminance & optional continuous liquid-silver shimmer
hl.animation({ leaf = "border",             enabled = true, speed = 5.0 * k, bezier = "expoOut" })
if loop_on then
    hl.animation({ leaf = "borderangle",    enabled = true, speed = 55.0,    bezier = "monoLinear", style = "loop" })
else
    hl.animation({ leaf = "borderangle",    enabled = true, speed = 18.0,    bezier = "expoOut",    style = "once" })
end

-- Workspaces & named scratchpad drawers (vertical spring drop-in)
hl.animation({ leaf = "workspaces",         enabled = true, speed = 5.2 * k, bezier = "expoOut",      style = ws_par })
hl.animation({ leaf = "workspacesIn",       enabled = true, speed = 5.2 * k, bezier = "expoOut",      style = ws_par })
hl.animation({ leaf = "workspacesOut",      enabled = true, speed = 4.8 * k, bezier = "quintOut",     style = ws_par })
hl.animation({ leaf = "specialWorkspace",   enabled = true, speed = 4.6 * k, spring = "drawerSpring", style = "slidefadevert" })
hl.animation({ leaf = "specialWorkspaceIn", enabled = true, speed = 4.6 * k, spring = "drawerSpring", style = "slidefadevert" })
hl.animation({ leaf = "specialWorkspaceOut",enabled = true, speed = 3.8 * k, bezier = "monoSnap",     style = "slidefadevert" })

-- Cursor magnifier (Super + =/-) & hot-plug monitor zoom
hl.animation({ leaf = "zoomFactor",         enabled = true, speed = 5.0 * k, bezier = "expoOut" })
hl.animation({ leaf = "monitorAdded",       enabled = true, speed = 5.5 * k, bezier = "expoOut" })
