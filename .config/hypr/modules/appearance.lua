-- ═══════════════════════════════════════════════════════════════════════════
--  modules/appearance.lua — gaps, borders, blur, animations, input
--
--  Monochrome palette:  pure black surfaces, white text, grey hairlines.
--  Tweak the numbers in modules/user.lua first — this file reads from there.
-- ═══════════════════════════════════════════════════════════════════════════

local u = user or {}

local function num(key, fallback) return u[key] or fallback end

local gaps_in        = num("gaps_in", 5)
local gaps_out       = num("gaps_out", 10)
local border_size    = num("border_size", 2)
local rounding       = num("rounding", 10)
local rounding_power = num("rounding_power", 2.0)
local blur_on        = u.blur_enabled
if blur_on == nil then blur_on = true end


-- ─────────────────────────────── General ───────────────────────────────────

hl.config({
    general = {
        gaps_in     = gaps_in,
        gaps_out    = gaps_out,
        border_size = border_size,

        -- Focused window: white-to-grey hairline gradient. Unfocused: dark grey.
        col = {
            active_border   = { colors = { "rgba(ffffffe6)", "rgba(9a9a9a99)" }, angle = 45 },
            inactive_border = "rgba(3a3a3a99)",
        },

        resize_on_border     = true,   -- grab window edges with the mouse
        extend_border_grab_area = 12,
        hover_icon_on_border = true,
        layout               = "dwindle",
        allow_tearing        = false,  -- set true only for competitive gaming
        no_border_on_floating = false,
    },

    decoration = {
        rounding       = rounding,
        rounding_power = rounding_power,

        active_opacity   = num("active_opacity", 1.0),
        inactive_opacity = num("inactive_opacity", 0.94),
        fullscreen_opacity = 1.0,

        -- Faint dimming of unfocused windows keeps focus obvious without noise.
        dim_inactive = true,
        dim_strength = 0.06,

        shadow = {
            enabled      = true,
            range        = 18,
            render_power = 3,
            color        = "rgba(00000055)",
            offset       = { 0, 4 },
        },

        blur = {
            enabled  = blur_on,
            size     = 6,
            passes   = 2,
            noise    = 0.02,   -- very subtle grain, keeps big blurs from banding
            contrast = 1.0,
            brightness = 1.0,
            vibrancy = 0.0,    -- 0 keeps the blur strictly monochrome
            xray     = false,
        },
    },

    animations = { enabled = true },
})


-- ────────────────────────────── Animations ─────────────────────────────────

hl.curve("monoOut",    { type = "bezier", points = { { 0.16, 1.0 }, { 0.3, 1.0 } } })
hl.curve("monoInOut",  { type = "bezier", points = { { 0.65, 0.05 }, { 0.36, 1.0 } } })
hl.curve("monoLinear", { type = "bezier", points = { { 0, 0 }, { 1, 1 } } })
hl.curve("monoSnap",   { type = "bezier", points = { { 0.15, 0 }, { 0.1, 1 } } })

hl.animation({ leaf = "global",       enabled = true,  speed = 8,    bezier = "monoInOut" })
hl.animation({ leaf = "border",       enabled = true,  speed = 6,    bezier = "monoOut" })
hl.animation({ leaf = "windows",      enabled = true,  speed = 5,    bezier = "monoOut" })
hl.animation({ leaf = "windowsIn",    enabled = true,  speed = 4.5,  bezier = "monoOut", style = "popin 94%" })
hl.animation({ leaf = "windowsOut",   enabled = true,  speed = 4,    bezier = "monoSnap", style = "popin 94%" })
hl.animation({ leaf = "fadeIn",       enabled = true,  speed = 4,    bezier = "monoOut" })
hl.animation({ leaf = "fadeOut",      enabled = true,  speed = 3.5,  bezier = "monoSnap" })
hl.animation({ leaf = "fade",         enabled = true,  speed = 4,    bezier = "monoOut" })
hl.animation({ leaf = "layers",       enabled = true,  speed = 4,    bezier = "monoOut" })
hl.animation({ leaf = "layersIn",     enabled = true,  speed = 4,    bezier = "monoOut", style = "fade" })
hl.animation({ leaf = "layersOut",    enabled = true,  speed = 3,    bezier = "monoSnap", style = "fade" })
hl.animation({ leaf = "fadeLayersIn", enabled = true,  speed = 3,    bezier = "monoOut" })
hl.animation({ leaf = "fadeLayersOut",enabled = true,  speed = 3,    bezier = "monoSnap" })
hl.animation({ leaf = "workspaces",   enabled = true,  speed = 5,    bezier = "monoOut", style = "slidefade 12%" })
hl.animation({ leaf = "specialWorkspace", enabled = true, speed = 5, bezier = "monoOut", style = "slidefadevert 12%" })
hl.animation({ leaf = "zoomFactor",   enabled = true,  speed = 6,    bezier = "monoSnap" })


-- ──────────────────────────────── Layouts ──────────────────────────────────

hl.config({
    dwindle = {
        preserve_split = true,     -- don't re-tile everything when a window closes
        smart_split    = false,
        force_split    = 0,        -- 0 = follow the mouse, 1/2 = always left/right
        pseudo_tile_size = 100,
    },
    master = {
        new_status = "master",
        mfact      = 0.55,
    },
    scrolling = {
        fullscreen_on_one_column = true,
        column_width             = 0.62,
    },

    -- Window tab groups (Super + G) — monochrome obsidian/silver pills instead
    -- of Hyprland's default yellow/orange groupbar colours.
    group = {
        col = {
            border_active          = { colors = { "rgba(ffffffe6)", "rgba(9a9a9a99)" }, angle = 45 },
            border_inactive        = "rgba(3a3a3a99)",
            border_locked_active   = "rgba(ffffffff)",
            border_locked_inactive = "rgba(55555599)",
        },
        groupbar = {
            font_family      = u.font or "JetBrainsMono Nerd Font",
            font_size        = 10,
            height           = 20,
            indicator_height = 2,
            gradients        = true,
            text_color       = "rgba(e6e6e6ff)",
            col = {
                active          = "rgba(383838e6)",
                inactive        = "rgba(141414cc)",
                locked_active   = "rgba(525252e6)",
                locked_inactive = "rgba(1e1e1ecc)",
            },
        },
    },
})


-- ──────────────────────────── Input devices ────────────────────────────────

hl.config({
    input = {
        kb_layout  = u.kb_layout or "us",
        kb_variant = u.kb_variant or "",
        kb_options = u.kb_options or "",
        numlock_by_default = u.numlock_by_default or false,

        follow_mouse = 1,
        mouse_refocus = true,
        focus_on_close = 2,
        sensitivity = 0.0,
        accel_profile = "adaptive",
        repeat_rate  = 30,
        repeat_delay = 400,

        touchpad = {
            natural_scroll       = u.touchpad_natural_scroll or false,
            tap_to_click         = u.touchpad_tap_to_click,
            disable_while_typing = u.touchpad_disable_typing,
            drag_lock            = false,
            clickfinger_behavior = false,
            scroll_factor        = 1.0,
        },
    },
})

-- Trackpad gestures (ignored on machines without a touchpad).
hl.gesture({ fingers = 3, direction = "horizontal", action = "workspace" })
hl.gesture({ fingers = 3, direction = "up",         action = "fullscreen" })
hl.gesture({ fingers = 4, direction = "down",       action = "special",
             workspace_name = (u.scratchpad_name or "scratchpad") })
hl.gesture({ fingers = 2, direction = "pinch",      action = "cursor_zoom",
             zoom_level = 1, mode = "live" })

-- Per-device override example (get the name from `hyprctl devices`):
-- hl.device({ name = "logitech-mx-master", sensitivity = -0.2 })
