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
--    modules/hardware.lua   ← universal PC / GPU / Optimus / VM auto-tuning
--    modules/monitors.lua   ← monitor setup (optional)
--    modules/appearance.lua ← gaps, borders, blur, rounding, input
--    modules/animations.lua ← damped spring & cubic-Bezier choreography
--    modules/autostart.lua  ← what launches with the session
--    modules/binds.lua      ← every keybind + RESIZE / CAPTURE / SCRATCH / FX
--    modules/rules.lua      ← window + layer rules
--    modules/smart.lua      ← reactive Lua event hooks (screenshare DND, hotplug)
--
--  Reload after a change:  hyprctl reload      (saving the file also reloads)
-- ═══════════════════════════════════════════════════════════════════════════

local home      = os.getenv("HOME") or ""
local base_data = os.getenv("XDG_DATA_DIRS") or "/usr/local/share:/usr/share"
local base_path = os.getenv("PATH") or "/usr/local/bin:/usr/bin:/bin"

-- Expose Flatpak, Snap, Nix, and ~/.local/share `.desktop` entries so Rofi
-- (`Super + D`) discovers every installed application on any Linux PC.
hl.env("PATH", home .. "/.local/bin:" .. home .. "/Applications:" .. base_path)
hl.env("XDG_DATA_DIRS",
    home .. "/.local/share:" ..
    home .. "/.local/share/flatpak/exports/share:" ..
    "/var/lib/flatpak/exports/share:" ..
    "/var/lib/snapd/desktop:" ..
    home .. "/.nix-profile/share:" ..
    base_data)

require("modules/user")        -- must come first, other modules read `user`
require("modules/monitors")
require("modules/appearance")
require("modules/animations")
require("modules/autostart")
require("modules/binds")
require("modules/rules")
require("modules/smart")


-- ─────────────────────────── Environment ───────────────────────────────────
-- Universal Wayland + XWayland + Java + SDL2 + Electron defaults.

hl.env("XDG_CURRENT_DESKTOP", "Hyprland")
hl.env("XDG_SESSION_TYPE", "wayland")
hl.env("XDG_SESSION_DESKTOP", "Hyprland")
hl.env("XCURSOR_THEME", user.cursor_theme or "Monochrome-Obsidian")
hl.env("HYPRCURSOR_THEME", user.cursor_theme or "Monochrome-Obsidian")
hl.env("XCURSOR_SIZE", tostring(user.cursor_size or 24))
hl.env("HYPRCURSOR_SIZE", tostring(user.cursor_size or 24))
hl.env("QT_QPA_PLATFORM", "wayland;xcb")
hl.env("QT_AUTO_SCREEN_SCALE_FACTOR", "1")
hl.env("QT_QPA_PLATFORMTHEME", "qt6ct")
hl.env("QT_WAYLAND_DISABLE_WINDOWDECORATION", "1")
hl.env("MOZ_ENABLE_WAYLAND", "1")
hl.env("ELECTRON_OZONE_PLATFORM_HINT", "auto")
hl.env("GDK_SCALE", "1")
hl.env("GDK_BACKEND", "wayland,x11")
hl.env("SDL_VIDEODRIVER", "wayland,x11")
hl.env("CLUTTER_BACKEND", "wayland")
hl.env("_JAVA_AWT_WM_NONEREPARENTING", "1")
hl.env("NO_AT_BRIDGE", "1")


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

-- Auto-tune GPU / Optimus / VM / XWayland / cursor hardware flags for any PC.
require("modules/hardware")
pcall(require, "modules/cursor-user")
pcall(require, "modules/keyboard-user")

-- ──────────────────────────── Permissions ──────────────────────────────────
-- Uncomment only if you enable `ecosystem.enforce_permissions = true`.
-- (Requires a Hyprland restart, not just a reload.)
--
-- hl.permission("/usr/(bin|local/bin)/grim", "screencopy", "allow")
-- hl.permission("/usr/(bin|local/bin)/hyprpicker", "screencopy", "allow")
-- hl.permission("/usr/(lib|libexec|lib64)/xdg-desktop-portal-hyprland", "screencopy", "allow")
