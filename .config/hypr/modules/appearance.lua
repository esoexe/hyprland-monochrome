-- ═══════════════════════════════════════════════════════════════════════════
--  modules/appearance.lua — squircle optics, GPU pacing, groups, gestures
--
--  Monochrome palette:  pure black surfaces, white text, grey hairlines.
--  Tweak the numbers in modules/user.lua first — this file reads from there.
-- ═══════════════════════════════════════════════════════════════════════════

local u = user or {}

local function num(key, fallback)
    if u[key] == nil then return fallback end
    return u[key]
end

local gaps_in        = num("gaps_in", 5)
local gaps_out       = num("gaps_out", 10)
local border_size    = num("border_size", 2)
local rounding       = num("rounding", 10)
local rounding_power = num("rounding_power", 4.0)
local blur_on        = u.blur_enabled
if blur_on == nil then blur_on = true end
local glass_mode     = u.glass_mode or "kawase"
if glass_mode == "off" then blur_on = false end

local blur_cfg = {
    enabled           = blur_on,
    new_optimizations = true,
    size              = (glass_mode == "acrylic") and 7 or 6,
    passes            = (glass_mode == "xray") and 1 or 2,
    noise             = 0.02,   -- subtle grain prevents banding on obsidian surfaces
    contrast          = 1.0,
    brightness        = 1.0,
    vibrancy          = 0.0,    -- 0 keeps blur strictly monochrome
    popups            = true,
    popups_ignorealpha = 0.30,
    xray              = (glass_mode == "xray"),
    variant           = (glass_mode == "acrylic") and "acrylic" or "kawase",
}

if glass_mode == "acrylic" then
    blur_cfg.acrylic = {
        aberration = 0.0,          -- zero RGB fringe: pure monochrome optical glass
        bulb       = 44.0,
        clarity    = 0.84,
        refraction = 18.0,
        tint       = "0x14ffffff",
    }
end


-- ─────────────────────── General, Optics & GPU Pipeline ───────────────────

hl.config({
    general = {
        gaps_in     = gaps_in,
        gaps_out    = gaps_out,
        border_size = border_size,

        -- Focused window: 3-stop liquid-silver gradient (orbits when border_loop = true).
        col = {
            active_border         = { colors = { "rgba(ffffffff)", "rgba(cfcfcfe6)", "rgba(78787899)" }, angle = 45 },
            inactive_border       = "rgba(3a3a3a99)",
            nogroup_border        = "rgba(4a4a4a99)",
            nogroup_border_active = "rgba(e6e6e6ff)",
        },

        resize_on_border        = true,  -- grab window edges with the mouse
        extend_border_grab_area = 14,
        hover_icon_on_border    = true,
        layout                  = "dwindle",
        allow_tearing           = false, -- toggled live by `perf.sh turbo` (Super + F12)

        -- Magnetic floating window edge & monitor alignment
        snap = {
            enabled        = true,
            window_gap     = 10,
            monitor_gap    = 10,
            border_overlap = false,
            respect_gaps   = true,
        },
    },

    decoration = {
        rounding           = rounding,
        rounding_power     = rounding_power, -- 4.0 = continuous G2 squircle corners
        active_opacity     = num("active_opacity", 1.0),
        inactive_opacity   = num("inactive_opacity", 0.94),
        fullscreen_opacity = 1.0,

        -- Faint dimming of unfocused windows keeps focus obvious without noise.
        dim_inactive = true,
        dim_strength = 0.06,
        dim_special  = 0.22,

        shadow = {
            enabled      = true,
            range        = 18,
            render_power = 3,
            color        = "rgba(00000055)",
            offset       = { 0, 4 },
        },

        blur = blur_cfg,
    },

    -- Hyprland 0.56 low-latency GPU scheduling, VRR & direct scanout
    render = {
        direct_scanout        = 2,    -- auto zero-copy scanout for fullscreen games
        new_render_scheduling = true, -- adaptive triple-buffering on high-Hz panels
        async_commit          = true, -- async DRM hardware cursor commits (0-frame lag)
    },

    misc = {
        vfr                        = true, -- drop idle compositor frames to save power
        vrr                        = num("vrr_mode", 2), -- Adaptive Sync in fullscreen
        animate_manual_resizes     = true, -- smooth spring glide on keyboard/border resizes
        animate_mouse_windowdragging = false,
        key_press_enables_dpms     = true,
        mouse_move_enables_dpms    = true,
        focus_on_activate          = true,
        initial_workspace_tracking = 2,
        render_unfocused_fps       = 15,
        disable_hyprland_logo      = true,
        disable_splash_rendering   = true,
        background_color           = "0x080808",
    },

    cursor = {
        enable_hyprcursor        = true, -- native vector hyprcursor (.hlc) support
        no_break_fs_vrr          = 2,    -- keep fullscreen VRR locked during mouse input
        persistent_warps         = true, -- remember last pointer position per window
        warp_on_change_workspace = 0,
        hide_on_key_press        = u.hide_cursor_typing or false,
    },

    binds = {
        workspace_back_and_forth   = 1,  -- Super+N again ping-pongs to previous workspace
        allow_workspace_cycles     = true,
        scroll_event_delay         = 80, -- snappy 80ms Super+Scroll workspace cruising
        movefocus_cycles_groupfirst = true,
    },
})


-- (Animations live in modules/animations.lua — edit `motion_profile` in user.lua
--  or press Super + M to switch physics live between fluid/snappy/cinematic/minimal.)


-- ────────────────────────── Layouts & Tab Groups ──────────────────────────

hl.config({
    dwindle = {
        preserve_split   = true, -- don't re-tile everything when a window closes
        smart_split      = false,
        force_split      = 0,    -- 0 = follow the mouse, 1/2 = always left/right
        pseudo_tile_size = 100,
    },
    master = {
        new_status = "master",
        mfact      = 0.55,
    },
    scrolling = {
        fullscreen_on_one_column = true,
        column_width             = 0.62,
        focus_fit_method         = 0,
        follow_focus             = true,
        explicit_column_widths   = "0.333, 0.5, 0.667, 1.0",
    },

    -- Window tab groups (Super + G / Super + Ctrl + G) — squircle obsidian/silver pills.
    group = {
        auto_group           = true,
        insert_after_current = true,
        drag_into_group      = 2,
        merge_groups_on_drag = true,
        merge_groups_on_groupbar = true,
        col = {
            border_active          = { colors = { "rgba(ffffffe6)", "rgba(9a9a9a99)" }, angle = 45 },
            border_inactive        = "rgba(3a3a3a99)",
            border_locked_active   = "rgba(ffffffff)",
            border_locked_inactive = "rgba(55555599)",
        },
        groupbar = {
            font_family             = u.font or "JetBrainsMono Nerd Font",
            font_size               = 10,
            height                  = 22,
            indicator_height        = 0,
            gradients               = true,
            gradient_rounding       = 8,
            gradient_rounding_power = 4.0,
            rounding                = 6,
            rounding_power          = 4.0,
            gaps_in                 = 3,
            gaps_out                = 3,
            blur                    = true,
            scrolling               = true,
            middle_click_close      = true,
            text_color              = "rgba(f2f2f2ff)",
            text_color_inactive     = "rgba(8a8a8aff)",
            col = {
                active          = "rgba(383838e6)",
                inactive        = "rgba(141414cc)",
                locked_active   = "rgba(525252e6)",
                locked_inactive = "rgba(1e1e1ecc)",
            },
        },
    },
})


-- ─────────────────────── Input & 1:1 Trackpad Gestures ────────────────────

hl.config({
    input = {
        kb_model           = u.kb_model or "pc104",
        kb_layout          = u.kb_layout or "us",
        kb_variant         = u.kb_variant or "",
        kb_options         = u.kb_options or "",
        numlock_by_default = u.numlock_by_default or false,

        follow_mouse   = 1,
        mouse_refocus  = true,
        focus_on_close = 2,
        sensitivity    = 0.0,
        accel_profile  = "adaptive",
        repeat_rate    = u.repeat_rate or 38,   -- fast 38 Hz key repeat after crisp 260ms delay
        repeat_delay   = u.repeat_delay or 260,

        touchpad = {
            natural_scroll       = u.touchpad_natural_scroll or false,
            tap_to_click         = u.touchpad_tap_to_click,
            disable_while_typing = u.touchpad_disable_typing,
            drag_lock            = false,
            clickfinger_behavior = false,
            scroll_factor        = 1.0,
        },
    },

    gestures = {
        workspace_swipe_distance           = 260,
        workspace_swipe_cancel_ratio       = 0.35,
        workspace_swipe_min_speed_to_force = 22,
        workspace_swipe_forever            = true,
    },
})

-- 1:1 finger-tracked gestures (ignored cleanly on desktop rigs without a touchpad).
hl.gesture({ fingers = 3, direction = "horizontal", action = "workspace" })
hl.gesture({ fingers = 3, direction = "up",         action = "fullscreen" })
hl.gesture({ fingers = 3, direction = "down",       action = "float" })
hl.gesture({ fingers = 4, direction = "horizontal", action = "scroll_move" })
hl.gesture({ fingers = 4, direction = "down",       action = "special",
             workspace_name = (u.scratchpad_name or "scratchpad") })
hl.gesture({ fingers = 2, direction = "pinch",      action = "cursor_zoom",
             zoom_level = 1, mode = "live" })
