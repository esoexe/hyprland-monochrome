-- ═══════════════════════════════════════════════════════════════════════════
--  modules/binds.lua — every keybind in one place
--
--  Docs: https://wiki.hypr.land/Configuring/Basics/Binds/
--  Print the live list:  Super + /        (or `hyprctl binds` in a terminal)
--
--  Notation:  mod = your main modifier (default SUPER / Windows key)
--             mod + 1..0 = workspaces, mod + SHIFT + 1..0 = move window there
-- ═══════════════════════════════════════════════════════════════════════════

local u    = user or {}
local home = os.getenv("HOME")
local S    = home .. "/.config/hypr/scripts/"

local mod      = u.mainMod or "SUPER"
local terminal = u.terminal or "kitty"
local term_cmd = terminal
if terminal == "kitty" and u.terminal_single_instance ~= false then
    term_cmd = "kitty --single-instance"
end
local scratch  = u.scratchpad_name or "scratchpad"

-- Small helpers so the list below stays readable.
local function sh(keys, cmd, desc, flags)
    local opts = flags or {}
    opts.description = desc
    hl.bind(mod .. " + " .. keys, hl.dsp.exec_cmd(cmd), opts)
end

local function act(keys, dispatcher, desc, flags)
    local opts = flags or {}
    opts.description = desc
    hl.bind(mod .. " + " .. keys, dispatcher, opts)
end


-- ═══════════════════════════ APPLICATIONS ═════════════════════════════════

sh("RETURN",         term_cmd,                "Terminal (<3ms shared GPU glyph cache)")
sh("T",              term_cmd,                "Terminal (<3ms shared GPU glyph cache)")
sh("SHIFT + RETURN", u.terminal_float or "kitty --class kitty-float", "Floating terminal")
sh("SHIFT + T",      S .. "scratchterm.sh",      "Dropdown terminal (quake style)")
sh("SHIFT + B",      S .. "scratchpad.sh btop",  "Floating btop++ telemetry drawer")
sh("APOSTROPHE",     S .. "scratchpad.sh notes", "Scratchpad Markdown notes (~/Documents/scratch.md)")
sh("D",              S .. "menu.sh launcher",          "App launcher (Wayland, X11, Flatpak, AppImage, CLI)")
sh("SHIFT + O",      S .. "open-app.sh picker",        "Universal opener (any file, URL, AppImage, Flatpak, .exe)")
sh("SHIFT + P",      S .. "palette.sh",                "Command palette & live math calculator")
sh("SEMICOLON",      S .. "palette.sh",                "Command palette (home-row alias)")
sh("SHIFT + D",      S .. "menu.sh windows",           "Window switcher")
sh("E",              S .. "open-app.sh files",         "File manager (auto-fallback across PCs)")
sh("B",              S .. "open-app.sh browser",       "Browser (auto-detects any installed browser)")
sh("ALT + B",        S .. "browser-theme.sh open-startpage", "Open browser on OpenCode startpage")
sh("W",              S .. "wallpaper.sh pick","Wallpaper picker (120fps Bezier transition)")
sh("ALT + W",        S .. "wallpaper.sh live","Cycle animated live monochrome wallpaper")
sh("I",              S .. "settings.sh",      "Settings menu (system, net, BT, monitors, sound)")
sh("V",              S .. "clipboard.sh",     "Clipboard history")
sh("SHIFT + V",      S .. "clipboard.sh wipe","Clear clipboard history")
sh("SLASH",          S .. "menu.sh keybinds", "Keyboard shortcut cheat sheet")
sh("SHIFT + C",      "hyprpicker -a",         "Colour picker (copies hex to clipboard)")
sh("N",              "dunstctl history-pop",  "Show the last notification")
sh("X",              "hyprctl switchxkblayout current next", "Switch keyboard layout")


-- ═══════════════════════════ WINDOW MANAGEMENT ════════════════════════════

act("Q",               hl.dsp.window.close(),                                     "Close window")
act("SHIFT + Q",       hl.dsp.window.kill(),                                      "Force-close window")
act("F",               hl.dsp.window.fullscreen({ mode = "fullscreen", action = "toggle" }), "Toggle fullscreen")
act("SHIFT + F",       hl.dsp.window.fullscreen({ mode = "maximized",  action = "toggle" }), "Toggle maximize")
sh("SPACE",            S .. "float-center.sh",                                    "Toggle floating window (centred at 70%)")
act("SHIFT + SPACE",   hl.dsp.window.pin({ action = "toggle" }),                   "Pin window (visible on all workspaces)")
sh("PERIOD",           S .. "pip-corner.sh",                                       "Spring floating / PiP window to next corner")
sh("COMMA",            S .. "shader.sh cycle",                                     "Cycle GLSL screen shader (Off / Noir / 35mm / CRT / E-Ink / OLED)")
sh("ALT + M",          S .. "motion.sh cycle",                                     "Cycle motion physics profile (Fluid / Snappy / Cinematic / Minimal)")
sh("F12",              S .. "perf.sh turbo toggle",                                "Toggle Turbo / Game mode (0ms latency, direct scanout, tearing)")
act("P",               hl.dsp.window.pseudo(),                                    "Pseudo-tile (dwindle)")
act("Y",               hl.dsp.layout("togglesplit"),                              "Toggle split direction (dwindle)")
sh("O",                S .. "opacity.sh",                                         "Cycle window opacity")
sh("BACKSLASH",        S .. "layout.sh cycle",                                    "Cycle tiling layout (Dwindle / Master / Scrolling)")
sh("SHIFT + BACKSLASH", S .. "layout.sh menu",                                    "Tiling layout & gap-preset picker")
act("G",               hl.dsp.group.toggle(),                                     "Group / ungroup window (tabs)")
sh("ALT + G",          "hyprctl dispatch changegroupactive f",                    "Next tab in window group")
sh("SHIFT + G",        "hyprctl dispatch lockactivegroup toggle",                 "Lock / unlock window group")
sh("U",                "hyprctl dispatch focuswindow urgent",                     "Jump to urgent window")

-- Cycle windows like alt-tab (works for floating and grouped windows)
hl.bind("ALT + TAB",       hl.dsp.window.cycle_next(),                { description = "Next window" })
hl.bind("ALT + SHIFT + TAB", hl.dsp.window.cycle_next({ next = false }), { description = "Previous window" })


-- ═══════════════════════ FOCUS / MOVE / RESIZE ════════════════════════════

-- Focus: vim keys and arrows both work.
act("H", hl.dsp.focus({ direction = "left" }),  "Focus left")
act("J", hl.dsp.focus({ direction = "down" }),  "Focus down")
act("K", hl.dsp.focus({ direction = "up" }),    "Focus up")
act("L", hl.dsp.focus({ direction = "right" }), "Focus right")
act("LEFT",  hl.dsp.focus({ direction = "left" }),  "Focus left")
act("DOWN",  hl.dsp.focus({ direction = "down" }),  "Focus down")
act("UP",    hl.dsp.focus({ direction = "up" }),    "Focus up")
act("RIGHT", hl.dsp.focus({ direction = "right" }), "Focus right")

-- Move the active window inside the layout.
act("SHIFT + H", hl.dsp.window.move({ direction = "left" }),  "Move window left")
act("SHIFT + J", hl.dsp.window.move({ direction = "down" }),  "Move window down")
act("SHIFT + K", hl.dsp.window.move({ direction = "up" }),    "Move window up")
act("SHIFT + L", hl.dsp.window.move({ direction = "right" }), "Move window right")

-- Resize (hold to keep resizing).
act("CTRL + H", hl.dsp.window.resize({ x = -40, y = 0 }), "Shrink window width",  { repeating = true })
act("CTRL + L", hl.dsp.window.resize({ x = 40,  y = 0 }), "Grow window width",    { repeating = true })
act("CTRL + K", hl.dsp.window.resize({ x = 0, y = -40 }), "Shrink window height", { repeating = true })
act("CTRL + J", hl.dsp.window.resize({ x = 0, y = 40 }),  "Grow window height",   { repeating = true })

-- Mouse: move and resize by dragging.
hl.bind(mod .. " + mouse:272", hl.dsp.window.drag(),   { mouse = true, description = "Drag window" })
hl.bind(mod .. " + mouse:273", hl.dsp.window.resize(), { mouse = true, description = "Resize window" })


-- ═════════════════════════════ WORKSPACES ═════════════════════════════════

local count = u.workspace_count or 10

for i = 1, count do
    local key = (i == 10) and "0" or tostring(i)
    act(key,                   hl.dsp.focus({ workspace = i }),                      "Workspace " .. i)
    act("SHIFT + " .. key,     hl.dsp.window.move({ workspace = i }),                "Move window to workspace " .. i)
    act("ALT + " .. key,       hl.dsp.window.move({ workspace = i, follow = false }), "Move window to " .. i .. " silently")
end

-- Scroll through workspaces, and drag the active window with you.
hl.bind(mod .. " + mouse_down", hl.dsp.focus({ workspace = "e+1" }), { description = "Next workspace" })
hl.bind(mod .. " + mouse_up",   hl.dsp.focus({ workspace = "e-1" }), { description = "Previous workspace" })
hl.bind(mod .. " + CTRL + mouse_down", hl.dsp.window.move({ workspace = "e+1" }), { description = "Move window to next workspace" })
hl.bind(mod .. " + CTRL + mouse_up",   hl.dsp.window.move({ workspace = "e-1" }), { description = "Move window to previous workspace" })

-- Scratchpad (a floating workspace you can summon on any screen).
act("S",           hl.dsp.workspace.toggle_special(scratch),                       "Toggle scratchpad")
act("SHIFT + S",   hl.dsp.window.move({ workspace = "special:" .. scratch }),      "Move window to scratchpad")


-- ═══════════════════════ BAR / SESSION / SYSTEM ═══════════════════════════

sh("SHIFT + W", S .. "waybar-toggle.sh",  "Toggle the top bar")
sh("Z",         S .. "focus.sh zen",       "Zen mode (hide bar, frame workspace, mute notifications)")
sh("SHIFT + Z", S .. "focus.sh menu",      "Deep-work focus & Pomodoro timer")
sh("SHIFT + R", "hyprctl reload",          "Reload Hyprland config")
sh("ESCAPE",    S .. "powermenu.sh",       "Logout menu (lock, suspend, reboot, shutdown)")
sh("GRAVE",     S .. "powermenu.sh",       "Logout menu")
sh("SHIFT + E", S .. "session.sh quit",    "Quit the session")
sh("TAB",       "hyprlock",                "Lock screen")
hl.bind("CTRL + ALT + L", hl.dsp.exec_cmd("hyprlock"), { description = "Lock screen" })
sh("SHIFT + N", S .. "nightshift.sh toggle", "Night shift (blue light filter)")
sh("SHIFT + M", "wpctl set-mute @DEFAULT_AUDIO_SOURCE@ toggle", "Mute / unmute microphone")


-- ═══════════════ SCREENSHOTS, RECORDING & ZOOM ════════════════════════════
-- Stills use PRINT, video uses ALT + PRINT (then press again to stop).
-- Recording picks gpu-screen-recorder or wf-recorder automatically.

sh("PRINT",         S .. "screenshot.sh region", "Screenshot: select an area")
sh("SHIFT + PRINT", S .. "screenshot.sh full",   "Screenshot: whole screen")
sh("CTRL + PRINT",  S .. "screenshot.sh window", "Screenshot: active window")
sh("CTRL + SHIFT + PRINT", S .. "screenshot.sh ocr",  "OCR: extract text from screen region")
sh("CTRL + ALT + PRINT",   S .. "screenshot.sh edit", "Screenshot: select & annotate")
sh("ALT + PRINT",         S .. "record.sh toggle screen", "Record the screen (again = stop)")
sh("ALT + SHIFT + PRINT", S .. "record.sh toggle region", "Record a selected area")
sh("ALT + S",             S .. "record.sh replay",        "Save the last minute (instant replay)")
sh("ALT + P",             S .. "record.sh pause",         "Pause / resume recording")
sh("MINUS",         S .. "zoom.sh out",          "Zoom out")
sh("EQUAL",         S .. "zoom.sh in",           "Zoom in")


-- ═══════════════════════ MODAL SUBMAPS (CHORDS) ══════════════════════════
-- Waybar lights up an inverted white pill ([RESIZE] or [CAPTURE]) while a
-- modal submap is active. Escape always returns to normal mode.

act("R", hl.dsp.submap("RESIZE"), "Enter RESIZE mode (h/j/k/l, 1-3/0 gaps, Esc)")

hl.define_submap("RESIZE", function()
    hl.bind("H",      hl.dsp.window.resize({ x = -40, y = 0 }), { repeating = true, description = "Resize narrower" })
    hl.bind("L",      hl.dsp.window.resize({ x = 40,  y = 0 }), { repeating = true, description = "Resize wider" })
    hl.bind("K",      hl.dsp.window.resize({ x = 0, y = -40 }), { repeating = true, description = "Resize shorter" })
    hl.bind("J",      hl.dsp.window.resize({ x = 0, y = 40 }),  { repeating = true, description = "Resize taller" })
    hl.bind("LEFT",   hl.dsp.window.resize({ x = -40, y = 0 }), { repeating = true, description = "Resize narrower" })
    hl.bind("RIGHT",  hl.dsp.window.resize({ x = 40,  y = 0 }), { repeating = true, description = "Resize wider" })
    hl.bind("UP",     hl.dsp.window.resize({ x = 0, y = -40 }), { repeating = true, description = "Resize shorter" })
    hl.bind("DOWN",   hl.dsp.window.resize({ x = 0, y = 40 }),  { repeating = true, description = "Resize taller" })
    hl.bind("1",      hl.dsp.exec_cmd(S .. "layout.sh gaps default"), { description = "Gap preset: Default" })
    hl.bind("2",      hl.dsp.exec_cmd(S .. "layout.sh gaps compact"), { description = "Gap preset: Compact" })
    hl.bind("3",      hl.dsp.exec_cmd(S .. "layout.sh gaps airy"),    { description = "Gap preset: Airy gallery" })
    hl.bind("0",      hl.dsp.exec_cmd(S .. "layout.sh gaps zero"),    { description = "Gap preset: Zero monocle" })
    hl.bind("RETURN", hl.dsp.submap("reset"), { description = "Exit RESIZE mode" })
    hl.bind("ESCAPE", hl.dsp.submap("reset"), { description = "Exit RESIZE mode" })
end)

act("C", hl.dsp.submap("CAPTURE"), "Enter CAPTURE chord (s/f/w/o/e/r/p/m)")

hl.define_submap("CAPTURE", "reset", function()
    hl.bind("S",      hl.dsp.exec_cmd(S .. "screenshot.sh region"),     { description = "Region screenshot" })
    hl.bind("F",      hl.dsp.exec_cmd(S .. "screenshot.sh full"),       { description = "Full-screen screenshot" })
    hl.bind("W",      hl.dsp.exec_cmd(S .. "screenshot.sh window"),     { description = "Active-window screenshot" })
    hl.bind("O",      hl.dsp.exec_cmd(S .. "screenshot.sh ocr"),        { description = "OCR region to clipboard" })
    hl.bind("E",      hl.dsp.exec_cmd(S .. "screenshot.sh edit"),       { description = "Region screenshot & annotate" })
    hl.bind("R",      hl.dsp.exec_cmd(S .. "record.sh toggle screen"),  { description = "Start / stop screen recording" })
    hl.bind("P",      hl.dsp.exec_cmd(S .. "record.sh replay"),         { description = "Save 60s instant replay" })
    hl.bind("M",      hl.dsp.exec_cmd(S .. "record.sh menu"),           { description = "Open screen recording menu" })
    hl.bind("ESCAPE", hl.dsp.submap("reset"),                           { description = "Cancel capture chord" })
end)

act("CTRL + S", hl.dsp.submap("SCRATCH"), "Enter SCRATCH & SESSION chord (t/b/n/f/w/r)")

hl.define_submap("SCRATCH", "reset", function()
    hl.bind("T",      hl.dsp.exec_cmd(S .. "scratchpad.sh term"),          { description = "Toggle dropdown terminal drawer" })
    hl.bind("B",      hl.dsp.exec_cmd(S .. "scratchpad.sh btop"),          { description = "Toggle btop++ telemetry drawer" })
    hl.bind("N",      hl.dsp.exec_cmd(S .. "scratchpad.sh notes"),         { description = "Toggle Markdown scratch notes" })
    hl.bind("F",      hl.dsp.exec_cmd(S .. "scratchpad.sh files"),         { description = "Toggle quick file drawer" })
    hl.bind("W",      hl.dsp.exec_cmd(S .. "workspace-session.sh save"),   { description = "Save current workspace session" })
    hl.bind("R",      hl.dsp.exec_cmd(S .. "workspace-session.sh restore"),{ description = "Restore saved workspace session" })
    hl.bind("M",      hl.dsp.exec_cmd(S .. "workspace-session.sh menu"),   { description = "Open workspace session manager" })
    hl.bind("ESCAPE", hl.dsp.submap("reset"),                              { description = "Cancel scratch chord" })
end)

act("M", hl.dsp.submap("FX"), "Enter FX & VELOCITY chord (physics, shaders, glass, turbo)")

hl.define_submap("FX", "reset", function()
    hl.bind("1",      hl.dsp.exec_cmd(S .. "motion.sh set fluid"),          { description = "Motion: Fluid damped springs" })
    hl.bind("2",      hl.dsp.exec_cmd(S .. "motion.sh set snappy"),         { description = "Motion: Snappy 140ms studio" })
    hl.bind("3",      hl.dsp.exec_cmd(S .. "motion.sh set cinematic"),      { description = "Motion: Cinematic parallax + liquid border" })
    hl.bind("0",      hl.dsp.exec_cmd(S .. "motion.sh set minimal"),        { description = "Motion: Minimal zero-motion" })
    hl.bind("B",      hl.dsp.exec_cmd(S .. "motion.sh border-loop toggle"), { description = "Toggle orbiting liquid-silver border" })
    hl.bind("M",      hl.dsp.exec_cmd(S .. "motion.sh menu"),               { description = "Open motion physics menu" })
    hl.bind("T",      hl.dsp.exec_cmd(S .. "perf.sh turbo toggle"),         { description = "Toggle Low-Latency Turbo / Game mode" })
    hl.bind("X",      hl.dsp.exec_cmd(S .. "perf.sh glass cycle"),          { description = "Cycle Optical Glass (Kawase / X-Ray / Acrylic / Off)" })
    hl.bind("V",      hl.dsp.exec_cmd(S .. "perf.sh menu"),                 { description = "Open Velocity, Glass & Benchmark menu" })
    hl.bind("S",      hl.dsp.exec_cmd(S .. "shader.sh menu"),               { description = "Open GLSL screen shader studio" })
    hl.bind("N",      hl.dsp.exec_cmd(S .. "shader.sh set noir"),           { description = "GLSL shader: Noir greyscale" })
    hl.bind("G",      hl.dsp.exec_cmd(S .. "shader.sh set cinema-grain"),   { description = "GLSL shader: 35mm film grain" })
    hl.bind("C",      hl.dsp.exec_cmd(S .. "shader.sh set crt-studio"),     { description = "GLSL shader: P4 studio CRT" })
    hl.bind("E",      hl.dsp.exec_cmd(S .. "shader.sh set paper-eink"),     { description = "GLSL shader: Matte E-Ink paper" })
    hl.bind("D",      hl.dsp.exec_cmd(S .. "shader.sh set oled-pitch"),     { description = "GLSL shader: OLED true-black saver" })
    hl.bind("O",      hl.dsp.exec_cmd(S .. "shader.sh set off"),            { description = "GLSL shader: Off" })
    hl.bind("L",      hl.dsp.exec_cmd(S .. "wallpaper.sh live"),            { description = "Cycle animated live wallpaper" })
    hl.bind("ESCAPE", hl.dsp.submap("reset"),                               { description = "Cancel FX chord" })
end)

act("CTRL + G", hl.dsp.submap("GROUP"), "Enter GROUP tab-stack chord (h/j/k/l merge, u pop, n/p tabs)")

hl.define_submap("GROUP", "reset", function()
    hl.bind("G",      hl.dsp.group.toggle(),                          { description = "Toggle window tab group" })
    hl.bind("H",      hl.dsp.window.move({ into_group = "l" }),       { description = "Merge window into group on left" })
    hl.bind("J",      hl.dsp.window.move({ into_group = "d" }),       { description = "Merge window into group below" })
    hl.bind("K",      hl.dsp.window.move({ into_group = "u" }),       { description = "Merge window into group above" })
    hl.bind("L",      hl.dsp.window.move({ into_group = "r" }),       { description = "Merge window into group on right" })
    hl.bind("U",      hl.dsp.window.move({ out_of_group = true }),    { description = "Pop window out of group" })
    hl.bind("N",      hl.dsp.group.next(),                            { description = "Next tab in group" })
    hl.bind("P",      hl.dsp.group.prev(),                            { description = "Previous tab in group" })
    hl.bind("X",      hl.dsp.group.lock_active(),                     { description = "Lock / unlock active group" })
    hl.bind("1",      hl.dsp.group.active({ index = 1 }),             { description = "Switch to tab 1 in group" })
    hl.bind("2",      hl.dsp.group.active({ index = 2 }),             { description = "Switch to tab 2 in group" })
    hl.bind("3",      hl.dsp.group.active({ index = 3 }),             { description = "Switch to tab 3 in group" })
    hl.bind("4",      hl.dsp.group.active({ index = 4 }),             { description = "Switch to tab 4 in group" })
    hl.bind("ESCAPE", hl.dsp.submap("reset"),                         { description = "Cancel GROUP chord" })
end)


-- ═══════════════════════ MEDIA & HARDWARE KEYS ════════════════════════════
-- locked = still works on the lock screen, repeating = hold to keep going.

local function media(key, cmd, desc)
    hl.bind(key, hl.dsp.exec_cmd(cmd), { locked = true, description = desc })
end

local function hold(key, cmd, desc)
    hl.bind(key, hl.dsp.exec_cmd(cmd), { locked = true, repeating = true, description = desc })
end

media("XF86AudioRaiseVolume",  S .. "volume.sh up",   "Volume up")
media("XF86AudioLowerVolume",  S .. "volume.sh down", "Volume down")
media("XF86AudioMute",         S .. "volume.sh mute", "Mute the output")
media("XF86AudioMicMute",      S .. "volume.sh mic",  "Mute the microphone")
hold("XF86MonBrightnessUp",    S .. "brightness.sh up",   "Brightness up (OSD)")
hold("XF86MonBrightnessDown",  S .. "brightness.sh down", "Brightness down (OSD)")
media("XF86AudioNext",  "playerctl next",       "Next track")
media("XF86AudioPrev",  "playerctl previous",   "Previous track")
media("XF86AudioPlay",  "playerctl play-pause", "Play / pause")
media("XF86AudioPause", "playerctl play-pause", "Play / pause")
media("XF86AudioStop",  "playerctl stop",       "Stop playback")

-- ═══════════════════════ OPTIONAL / LAPTOP ════════════════════════════════

-- Lock automatically when a laptop lid closes (harmless no-op on desktop PCs).
hl.bind("switch:on:Lid Switch", hl.dsp.exec_cmd("loginctl lock-session"), { locked = true, description = "Lock session on laptop lid close" })
media("XF86PowerOff",          S .. "hardware.sh power cycle", "Cycle power profile (Performance / Balanced / Saver)")
