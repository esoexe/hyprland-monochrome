-- ═══════════════════════════════════════════════════════════════════════════
--  modules/rules.lua — window + layer rules
--
--  Rules are applied top to bottom, so keep the specific ones below.
--  Window class names:  hyprctl clients | grep class
--  Docs: https://wiki.hypr.land/Configuring/Basics/Window-Rules/
-- ═══════════════════════════════════════════════════════════════════════════

local u       = user or {}
local term_op = u.terminal_opacity or 0.92

local function v(x) return tostring(x) end

-- ── General polish ────────────────────────────────────────────────────────

-- Ignore "please maximize me" requests: tiling windows already fill the screen.
hl.window_rule({ name = "no-maximize-requests", match = { class = ".*" }, suppress_event = "maximize" })

-- Zero-latency direct scanout & tearing hints for games, plus VRR video hints.
hl.window_rule({
    name      = "game-low-latency",
    match     = { class = "^(cs2|osu!|steam_app_.*|gamescope|Minecraft.*|lutris)$" },
    content   = "game",
    immediate = true,
})
hl.window_rule({
    name    = "video-vrr-content",
    match   = { class = "^(mpv|vlc|io\\.github\\.celluloid_player\\.Celluloid)$" },
    content = "video",
})

-- Fix occasional drag weirdness of XWayland windows.
hl.window_rule({
    name  = "xwayland-drag-fix",
    match = { class = "^$", title = "^$", xwayland = true, float = true, fullscreen = false, pin = false },
    no_focus = true,
})

-- ── Transparency / blur ───────────────────────────────────────────────────
-- "active inactive" — add " override" to each value for absolute opacity.

hl.window_rule({
    name    = "terminal-opacity",
    match   = { class = "^(kitty|foot|alacritty|dropdown-term|kitty-float|mono-btop|mono-notes)$" },
    opacity = v(term_op) .. " override 0.85 override",
})

hl.window_rule({ name = "thunar-opacity",  match = { class = "^(thunar|Thunar)$" },
                 opacity = "0.96 override 0.92 override" })
hl.window_rule({ name = "browser-opacity", match = { class = "^(firefox|brave-browser|chromium|google-chrome|zen|librewolf)$" },
                 opacity = "0.98 override 0.94 override" })
hl.window_rule({ name = "editor-opacity",  match = { class = "^(code|code-oss|VSCodium|codium|zed)$" },
                 opacity = "0.97 override 0.94 override" })

-- The bar, menus and notifications get frosted glass + choreographed layer motion.
hl.layer_rule({ name = "rofi-blur",         match = { namespace = "^(rofi)$" },          blur = true, blur_popups = true, dim_around = true, ignore_alpha = 0.30, animation = "popin 92%" })
hl.layer_rule({ name = "waybar-blur",       match = { namespace = "^(waybar)$" },        blur = true, ignore_alpha = 0.30, animation = "slide top" })
hl.layer_rule({ name = "notify-blur",       match = { namespace = "^(notifications)$" }, blur = true, ignore_alpha = 0.30, animation = "slide right" })
hl.layer_rule({ name = "wlogout-blur",      match = { namespace = "^(logout_dialog)$" }, blur = true, ignore_alpha = 0.20, animation = "fade" })
hl.layer_rule({ name = "selection-no-anim", match = { namespace = "^(selection)$" },     no_anim = true })

-- ── Floating utility windows ──────────────────────────────────────────────

local float_classes = {
    "pavucontrol", "nm-connection-editor", "blueman-manager", "blueman-adapters",
    "gnome-calculator", "org.gnome.Calculator", "galculator",
    "file-roller", "org.gnome.FileRoller", "engrampa",
    "hyprland-share-picker", "xdg-desktop-portal-gtk",
    "nitrogen", "feh", "nwg-look", "qt5ct", "qt6ct",
    "satty", "com.gabm.satty", "swappy",
}

for _, class in ipairs(float_classes) do
    hl.window_rule({
        name   = "float-" .. class,
        match  = { class = "^(" .. class .. ")$" },
        float  = true,
        center = true,
    })
end

-- "Open File" / "Save File" dialogs and authentication prompts.
hl.window_rule({ name = "float-file-dialogs",
                 match = { title = "^(Open File|Open Folder|Save File|Choose File|Save As)(.*)$" },
                 float = true, center = true })
hl.window_rule({ name = "float-auth",
                 match = { class = "^(polkit-gnome-authentication-agent-1|hyprpolkitagent)$" },
                 float = true, center = true })

-- Picture-in-picture video: floating, pinned, top-right.
hl.window_rule({
    name  = "pip",
    match = { title = "^(Picture-in-Picture)$" },
    float = true,
    pin   = true,
    move  = { "(monitor_w*0.72)", "(monitor_h*0.05)" },
    size  = { "(monitor_w*0.26)", "(monitor_h*0.26)" },
})

-- ── Scratchpad / named floating drawers ───────────────────────────────────

hl.window_rule({
    name      = "dropdown-terminal",
    match     = { class = "^(dropdown-term)$" },
    float     = true,
    center    = true,
    size      = { "(monitor_w*0.66)", "(monitor_h*0.45)" },
    workspace = "special:" .. (u.dropdown_name or "dropdown"),
})

hl.window_rule({
    name      = "drawer-btop",
    match     = { class = "^(mono-btop)$" },
    float     = true,
    center    = true,
    size      = { "(monitor_w*0.74)", "(monitor_h*0.68)" },
    workspace = "special:btop",
})

hl.window_rule({
    name      = "drawer-notes",
    match     = { class = "^(mono-notes)$" },
    float     = true,
    center    = true,
    size      = { "(monitor_w*0.64)", "(monitor_h*0.62)" },
    workspace = "special:notes",
})

hl.window_rule({
    name      = "drawer-files",
    match     = { class = "^(mono-files)$" },
    float     = true,
    center    = true,
    size      = { "(monitor_w*0.70)", "(monitor_h*0.64)" },
    workspace = "special:files",
})

-- ── App specifics ─────────────────────────────────────────────────────────

-- Media player lives on workspace 5 and opens silently there.
hl.window_rule({ name = "spotify-workspace", match = { class = "^(Spotify|spotify)$" }, workspace = "5 silent" })

-- Games: fullscreen, no blur, no gaps.
hl.window_rule({ name = "game-mode", match = { class = "^(gamescope|steam_app_.*|wine.*|.*\\.exe)$" },
                 fullscreen = true, no_blur = true, border_size = 0, rounding = 0 })

-- Screenshot / picker tools must never steal focus or look blurred.
hl.window_rule({ name = "screenshot-ui", match = { class = "^(grim|slurp|hyprpicker)$" }, no_focus = true, no_blur = true })

-- Keep video players awake while playing fullscreen.
hl.window_rule({ name = "no-idle-video", match = { class = "^(mpv|vlc|celluloid)$" }, idle_inhibit = "fullscreen" })
