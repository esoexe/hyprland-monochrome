-- ═══════════════════════════════════════════════════════════════════════════
--  modules/user.lua — the file you actually edit
--
--  Everything personal lives here and is used by the other modules.
--  Every value has a sane fallback, so a typo can't brick your session.
-- ═══════════════════════════════════════════════════════════════════════════

local home = os.getenv("HOME")

-- Global (no `local`) so the other modules can read it.
user = {

    -- ── Modifier key ─────────────────────────────────────────────────────
    mainMod = "SUPER",         -- SUPER (Windows key), ALT, or CTRL

    -- ── Programs ─────────────────────────────────────────────────────────
    terminal     = "kitty",
    terminal_float = "kitty --class kitty-float",   -- SUPER + SHIFT + RETURN
    dropdown_term  = "kitty --class dropdown-term", -- SUPER + SHIFT + T
    browser      = "firefox",      -- install.sh tries to fill this in for you
    file_manager = "thunar",
    editor       = "kitty -e nvim",    -- only used by the "edit config" menu entry

    -- ── Look & feel ──────────────────────────────────────────────────────
    gaps_in        = 5,
    gaps_out       = 10,
    border_size    = 2,
    rounding       = 10,
    rounding_power = 2.0,
    active_opacity   = 1.0,
    inactive_opacity = 0.94,   -- slightly dim unfocused windows
    blur_enabled     = true,   -- set to false on low-end GPUs for extra FPS

    -- ── Keyboard / mouse ─────────────────────────────────────────────────
    kb_layout = "us",          -- e.g. "us", "fr", "de", "ara", or "us,ara" to toggle
    kb_variant = "",
    kb_options = "",           -- e.g. "grp:alt_shift_toggle,caps:escape"
    cursor_size = 24,
    touchpad_natural_scroll  = false,
    touchpad_tap_to_click    = true,
    touchpad_disable_typing  = true,     -- stop palm touches while typing
    numlock_by_default       = false,

    -- ── Paths ────────────────────────────────────────────────────────────
    wallpaper_dir    = home .. "/Pictures/Wallpapers",
    screenshot_dir   = home .. "/Pictures/Screenshots",
    recording_dir    = home .. "/Videos/Recordings",
    default_wallpaper = home .. "/Pictures/Wallpapers/mono-01.jpg",

    -- ── Workflow toggles ─────────────────────────────────────────────────
    workspace_count   = 10,       -- how many numbered workspaces get keybinds
    scratchpad_name   = "scratchpad",
    dropdown_name     = "dropdown",
    terminal_opacity  = 0.92,     -- opacity of kitty / dropdown terminal
    night_shift_temp  = 4000,     -- Kelvin, higher = less orange
    screenshot_notify = true,

    -- ── Motion physics & wallpaper choreography ──────────────────────────
    motion_profile       = "fluid",  -- "fluid" | "snappy" | "cinematic" | "minimal"
    border_loop          = false,    -- true = orbiting liquid-silver active border
    wallpaper_transition = "choreo", -- "choreo" | "grow" | "wave" | "outer" | "wipe" | "fade"

    -- ── Reactive automation (modules/smart.lua) ──────────────────────────
    smart_screenshare_dnd = true, -- auto-pause notifications while screen-sharing
    smart_monitor_hotplug = true, -- restore wallpaper & bar on monitor plug/unplug
    smart_urgent_notify   = true, -- monochrome toast when a background app flags urgent
    smart_single_window   = false,-- auto-expand gallery margins when a workspace has 1 window

    -- ── Screen recording ─────────────────────────────────────────────────
    record_backend  = "auto",     -- auto | gsr | wf-recorder
    record_audio    = "desktop",  -- off | desktop | mic | both (change live: mod + Alt + PRINT twice)
    record_fps      = 60,
    replay_seconds  = 60,         -- instant replay buffer (gpu-screen-recorder only)
}
