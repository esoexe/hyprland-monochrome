-- ═══════════════════════════════════════════════════════════════════════════
--  ~/.config/hypr/hyprland.lua
--  hyprland-monochrome — entry point
--
--  Hyprland 0.55+ Lua configuration.
--  Docs: https://wiki.hypr.land/Configuring/Start/
--
--  This file only loads the modules. Edit the modules instead of this file:
--
--    modules/user.lua       ← start here: programs, gaps, keyboard, paths
--    modules/monitors.lua   ← monitor layout
--    modules/appearance.lua ← borders, blur, animations, input
--    modules/autostart.lua  ← what launches with the session
--    modules/binds.lua      ← every keybind
--    modules/rules.lua      ← window + layer rules
--    modules/smart.lua      ← reactive Lua event hooks (screenshare DND, hotplug)
--
--  Reload after a change:  hyprctl reload      (saving the file also reloads)
-- ═══════════════════════════════════════════════════════════════════════════

require("modules/user")        -- must come first, other modules read `user`
require("modules/monitors")
require("modules/appearance")
require("modules/autostart")
require("modules/binds")
require("modules/rules")
require("modules/smart")


-- ─────────────────────────── Environment ───────────────────────────────────
-- Wayland-friendly defaults. Add your own with hl.env("NAME", "value").

hl.env("XCURSOR_SIZE", tostring(user.cursor_size or 24))
hl.env("HYPRCURSOR_SIZE", tostring(user.cursor_size or 24))
hl.env("QT_QPA_PLATFORM", "wayland;xcb")
hl.env("QT_QPA_PLATFORMTHEME", "qt6ct")
hl.env("QT_WAYLAND_DISABLE_WINDOWDECORATION", "1")
hl.env("MOZ_ENABLE_WAYLAND", "1")
hl.env("ELECTRON_OZONE_PLATFORM_HINT", "auto")
hl.env("GDK_SCALE", "1")
hl.env("GDK_BACKEND", "wayland,x11")
hl.env("SDL_VIDEODRIVER", "wayland")
hl.env("CLUTTER_BACKEND", "wayland")


-- ──────────────────────────── Behaviour ────────────────────────────────────

hl.config({
    misc = {
        disable_hyprland_logo   = true,   -- no random logo on an empty workspace
        disable_splash_rendering = true,
        force_default_wallpaper = -1,     -- no anime wallpapers behind awww
        vfr                     = true,   -- variable frame rate (battery friendly)
        focus_on_close          = 2,      -- return focus to the last used window
        mouse_move_enables_dpms = true,
        key_press_enables_dpms  = true,
        animate_manual_resizes  = true,
        disable_autoreload      = false,  -- live reload when you save a file
    },
    binds = {
        workspace_back_and_forth = false,
        allow_workspace_cycles   = true,
    },
    cursor = {
        enable_hyprcursor = true,
        inactive_timeout = 5,
        sync_gsettings_theme = true,
    },
})

-- ──────────────────────────── Permissions ──────────────────────────────────
-- Uncomment only if you enable `ecosystem.enforce_permissions = true`.
-- (Requires a Hyprland restart, not just a reload.)
--
-- hl.permission("/usr/(bin|local/bin)/grim", "screencopy", "allow")
-- hl.permission("/usr/(bin|local/bin)/hyprpicker", "screencopy", "allow")
-- hl.permission("/usr/(lib|libexec|lib64)/xdg-desktop-portal-hyprland", "screencopy", "allow")
