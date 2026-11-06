-- ═══════════════════════════════════════════════════════════════════════════
--  modules/autostart.lua — what launches with the session
--
--  Everything below is spawned asynchronously by hl.exec_cmd().
--  If you use uwsm/systemd for your session, you can instead enable the
--  matching user services and delete the lines here.
-- ═══════════════════════════════════════════════════════════════════════════

local home    = os.getenv("HOME")
local scripts = home .. "/.config/hypr/scripts/"

hl.on("hyprland.start", function ()

    -- ── Desktop shell ────────────────────────────────────────────────────
    hl.exec_cmd("waybar")                    -- top bar
    hl.exec_cmd("dunst")                     -- notifications

    -- ── Wallpaper (socket-ready poll — paints in ~80ms instead of 1s sleep)
    hl.exec_cmd("awww-daemon")
    hl.exec_cmd("for _ in $(seq 1 30); do awww query >/dev/null 2>&1 && break; sleep 0.04; done; " .. scripts .. "wallpaper.sh --restore")

    -- ── Idle / power ─────────────────────────────────────────────────────
    hl.exec_cmd("hypridle")                  -- dim, lock, suspend on idle
    hl.exec_cmd("hyprsunset")                -- blue-light filter daemon

    -- ── Clipboard history ────────────────────────────────────────────────
    hl.exec_cmd("wl-paste --type text  --watch cliphist store")
    hl.exec_cmd("wl-paste --type image --watch cliphist store")

    -- ── Authentication agent (graphical sudo prompts) ─────────────────────
    hl.exec_cmd(scripts .. "polkit.sh")

    -- ── Optional extras (uncomment what you use) ──────────────────────────
    -- hl.exec_cmd("nm-applet --indicator")            -- tray for NetworkManager
    -- hl.exec_cmd("blueman-applet")                   -- tray for Bluetooth
    -- hl.exec_cmd("spicetify apply >/dev/null 2>&1")  -- re-apply Spotify theme
    -- hl.exec_cmd(scripts .. "wallpaper.sh --random") -- surprise wallpaper each boot
end)

-- Run something when the session ends (e.g. clean up a VPN, unmount drives).
hl.on("hyprland.shutdown", function ()
    -- hl.exec_cmd("pkill -x waybar")
end)
