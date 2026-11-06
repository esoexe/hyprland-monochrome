# Keybinds

> This file is generated from `~/.config/hypr/modules/binds.lua`.
> Edit the Lua file, then run `make docs` (`./bin/keybind-table`) to refresh
> this page. Every bind also carries a description, so `hyprctl binds`
> and the cheat sheet (`Super + /`) show the same words.

The **main modifier is `Super`**. Change it in one place:
`modules/user.lua` → `mainMod`.

## Contents

- [Applications](#applications)
- [Window Management](#window-management)
- [Focus / Move / Resize](#focus--move--resize)
- [Workspaces](#workspaces)
- [Bar / Session / System](#bar--session--system)
- [Screenshots, Recording & Zoom](#screenshots,-recording--zoom)
- [Modal Submaps (Chords)](#modal-submaps-(chords))
- [Media & Hardware Keys](#media--hardware-keys)
- [Optional / Laptop](#optional--laptop)

## Applications

| Keys | Action |
| --- | --- |
| `Super + Enter` | Terminal (<3ms shared GPU glyph cache) |
| `Super + T` | Terminal (<3ms shared GPU glyph cache) |
| `Super + Shift + Enter` | Floating terminal |
| `Super + Shift + T` | Dropdown terminal (quake style) |
| `Super + Shift + B` | Floating btop++ telemetry drawer |
| `Super + '` | Scratchpad Markdown notes (~/Documents/scratch.md) |
| `Super + D` | App launcher (Wayland, X11, Flatpak, AppImage, CLI) |
| `Super + Shift + O` | Universal opener (any file, URL, AppImage, Flatpak, .exe) |
| `Super + Shift + P` | Command palette & live math calculator |
| `Super + ;` | Command palette (home-row alias) |
| `Super + Shift + D` | Window switcher |
| `Super + E` | File manager (auto-fallback across PCs) |
| `Super + B` | Browser (auto-detects any installed browser) |
| `Alt + B` | Open browser on OpenCode startpage |
| `Super + W` | Wallpaper picker (120fps Bezier transition) |
| `Alt + W` | Cycle animated live monochrome wallpaper |
| `Super + I` | Settings menu (system, net, BT, monitors, sound) |
| `Super + V` | Clipboard history |
| `Super + Shift + V` | Clear clipboard history |
| `Super + /` | Keyboard shortcut cheat sheet |
| `Super + Shift + C` | Colour picker (copies hex to clipboard) |
| `Super + N` | Show the last notification |
| `Super + X` | Switch keyboard layout |

## Window Management

| Keys | Action |
| --- | --- |
| `Super + Q` | Close window |
| `Super + Shift + Q` | Force-close window |
| `Super + F` | Toggle fullscreen |
| `Super + Shift + F` | Toggle maximize |
| `Super + Space` | Toggle floating window (centred at 70%) |
| `Super + Shift + Space` | Pin window (visible on all workspaces) |
| `Super + .` | Spring floating / PiP window to next corner |
| `Super + ,` | Cycle GLSL screen shader (Off / Noir / 35mm / CRT / E-Ink / OLED) |
| `Alt + M` | Cycle motion physics profile (Fluid / Snappy / Cinematic / Minimal) |
| `Super + F12` | Toggle Turbo / Game mode (0ms latency, direct scanout, tearing) |
| `Super + P` | Pseudo-tile (dwindle) |
| `Super + Y` | Toggle split direction (dwindle) |
| `Super + O` | Cycle window opacity |
| `Super + \` | Cycle tiling layout (Dwindle / Master / Scrolling) |
| `Super + Shift + \` | Tiling layout & gap-preset picker |
| `Super + G` | Group / ungroup window (tabs) |
| `Alt + G` | Next tab in window group |
| `Super + Shift + G` | Lock / unlock window group |
| `Super + U` | Jump to urgent window |
| `Alt + Tab` | Next window |
| `Alt + Shift + Tab` | Previous window |

## Focus / Move / Resize

| Keys | Action |
| --- | --- |
| `Super + H` | Focus left |
| `Super + J` | Focus down |
| `Super + K` | Focus up |
| `Super + L` | Focus right |
| `Super + Left` | Focus left |
| `Super + Down` | Focus down |
| `Super + Up` | Focus up |
| `Super + Right` | Focus right |
| `Super + Shift + H` | Move window left |
| `Super + Shift + J` | Move window down |
| `Super + Shift + K` | Move window up |
| `Super + Shift + L` | Move window right |
| `Ctrl + H` | Shrink window width |
| `Ctrl + L` | Grow window width |
| `Ctrl + K` | Shrink window height |
| `Ctrl + J` | Grow window height |

## Workspaces

| Keys | Action |
| --- | --- |
| `Super + scroll down` | Next workspace |
| `Super + scroll up` | Previous workspace |
| `Super + Ctrl + scroll down` | Move window to next workspace |
| `Super + Ctrl + scroll up` | Move window to previous workspace |
| `Super + S` | Toggle scratchpad |
| `Super + Shift + S` | Move window to scratchpad |
| `Super + 1..0` | Switch workspace (10 is bound to the 0 key) |
| `Super + Shift + 1..0` | Move the focused window to that workspace |
| `Super + Alt + 1..0` | Move it there without following |

## Bar / Session / System

| Keys | Action |
| --- | --- |
| `Super + Shift + W` | Toggle the top bar |
| `Super + Z` | Zen mode (hide bar, frame workspace, mute notifications) |
| `Super + Shift + Z` | Deep-work focus & Pomodoro timer |
| `Super + Shift + R` | Reload Hyprland config |
| `Super + Escape` | Logout menu (lock, suspend, reboot, shutdown) |
| `` Super + ` `` | Logout menu |
| `Super + Shift + E` | Quit the session |
| `Super + Tab` | Lock screen |
| `Ctrl + Alt + L` | Lock screen |
| `Super + Shift + N` | Night shift (blue light filter) |
| `Super + Shift + M` | Mute / unmute microphone |

## Screenshots, Recording & Zoom

| Keys | Action |
| --- | --- |
| `Super + Print` | Screenshot: select an area |
| `Super + Shift + Print` | Screenshot: whole screen |
| `Ctrl + Print` | Screenshot: active window |
| `Ctrl + Shift + Print` | OCR: extract text from screen region |
| `Ctrl + Alt + Print` | Screenshot: select & annotate |
| `Alt + Print` | Record the screen (again = stop) |
| `Alt + Shift + Print` | Record a selected area |
| `Alt + S` | Save the last minute (instant replay) |
| `Alt + P` | Pause / resume recording |
| `Super + -` | Zoom out |
| `Super + =` | Zoom in |

## Modal Submaps (Chords)

| Keys | Action |
| --- | --- |
| `Super + R` | Enter RESIZE mode (h/j/k/l, 1-3/0 gaps, Esc) |
| `[RESIZE] H` | Resize narrower |
| `[RESIZE] L` | Resize wider |
| `[RESIZE] K` | Resize shorter |
| `[RESIZE] J` | Resize taller |
| `[RESIZE] Left` | Resize narrower |
| `[RESIZE] Right` | Resize wider |
| `[RESIZE] Up` | Resize shorter |
| `[RESIZE] Down` | Resize taller |
| `[RESIZE] 1` | Gap preset: Default |
| `[RESIZE] 2` | Gap preset: Compact |
| `[RESIZE] 3` | Gap preset: Airy gallery |
| `[RESIZE] 0` | Gap preset: Zero monocle |
| `[RESIZE] Enter` | Exit RESIZE mode |
| `[RESIZE] Escape` | Exit RESIZE mode |
| `Super + C` | Enter CAPTURE chord (s/f/w/o/e/r/p/m) |
| `[CAPTURE] S` | Region screenshot |
| `[CAPTURE] F` | Full-screen screenshot |
| `[CAPTURE] W` | Active-window screenshot |
| `[CAPTURE] O` | OCR region to clipboard |
| `[CAPTURE] E` | Region screenshot & annotate |
| `[CAPTURE] R` | Start / stop screen recording |
| `[CAPTURE] P` | Save 60s instant replay |
| `[CAPTURE] M` | Open screen recording menu |
| `[CAPTURE] Escape` | Cancel capture chord |
| `Ctrl + S` | Enter SCRATCH & SESSION chord (t/b/n/f/w/r) |
| `[SCRATCH] T` | Toggle dropdown terminal drawer |
| `[SCRATCH] B` | Toggle btop++ telemetry drawer |
| `[SCRATCH] N` | Toggle Markdown scratch notes |
| `[SCRATCH] F` | Toggle quick file drawer |
| `[SCRATCH] W` | Save current workspace session |
| `[SCRATCH] R` | Restore saved workspace session |
| `[SCRATCH] M` | Open workspace session manager |
| `[SCRATCH] Escape` | Cancel scratch chord |
| `Super + M` | Enter FX & VELOCITY chord (physics, shaders, glass, turbo) |
| `[FX] 1` | Motion: Fluid damped springs |
| `[FX] 2` | Motion: Snappy 140ms studio |
| `[FX] 3` | Motion: Cinematic parallax + liquid border |
| `[FX] 0` | Motion: Minimal zero-motion |
| `[FX] B` | Toggle orbiting liquid-silver border |
| `[FX] M` | Open motion physics menu |
| `[FX] T` | Toggle Low-Latency Turbo / Game mode |
| `[FX] X` | Cycle Optical Glass (Kawase / X-Ray / Acrylic / Off) |
| `[FX] V` | Open Velocity, Glass & Benchmark menu |
| `[FX] S` | Open GLSL screen shader studio |
| `[FX] N` | GLSL shader: Noir greyscale |
| `[FX] G` | GLSL shader: 35mm film grain |
| `[FX] C` | GLSL shader: P4 studio CRT |
| `[FX] E` | GLSL shader: Matte E-Ink paper |
| `[FX] D` | GLSL shader: OLED true-black saver |
| `[FX] O` | GLSL shader: Off |
| `[FX] L` | Cycle animated live wallpaper |
| `[FX] Escape` | Cancel FX chord |
| `Ctrl + G` | Enter GROUP tab-stack chord (h/j/k/l merge, u pop, n/p tabs) |
| `[GROUP] G` | Toggle window tab group |
| `[GROUP] H` | Merge window into group on left |
| `[GROUP] J` | Merge window into group below |
| `[GROUP] K` | Merge window into group above |
| `[GROUP] L` | Merge window into group on right |
| `[GROUP] U` | Pop window out of group |
| `[GROUP] N` | Next tab in group |
| `[GROUP] P` | Previous tab in group |
| `[GROUP] X` | Lock / unlock active group |
| `[GROUP] 1` | Switch to tab 1 in group |
| `[GROUP] 2` | Switch to tab 2 in group |
| `[GROUP] 3` | Switch to tab 3 in group |
| `[GROUP] 4` | Switch to tab 4 in group |
| `[GROUP] Escape` | Cancel GROUP chord |

## Media & Hardware Keys

| Keys | Action |
| --- | --- |
| `XF86AudioRaiseVolume` | Volume up |
| `XF86AudioLowerVolume` | Volume down |
| `XF86AudioMute` | Mute the output |
| `XF86AudioMicMute` | Mute the microphone |
| `XF86MonBrightnessUp` | Brightness up (OSD) |
| `XF86MonBrightnessDown` | Brightness down (OSD) |
| `XF86AudioNext` | Next track |
| `XF86AudioPrev` | Previous track |
| `XF86AudioPlay` | Play / pause |
| `XF86AudioPause` | Play / pause |
| `XF86AudioStop` | Stop playback |

## Optional / Laptop

| Keys | Action |
| --- | --- |
| `switch:on:Lid Switch` | Lock session on laptop lid close |
| `XF86PowerOff` | Cycle power profile (Performance / Balanced / Saver) |

## Mouse

| Action | Result |
| --- | --- |
| `Super + drag left button` | Move a window |
| `Super + drag right button` | Resize a window |
| `Super + scroll` | Previous / next workspace |
| `Super + Ctrl + scroll` | Drag the focused window to another workspace |
| `Super + drag on a border/gap` | Resize (also works while tiling) |

## Cheat sheet at runtime

```bash
Super + /                 # searchable list built from your live binds
hyprctl binds            # the raw list
```

## Changing the modifier

Open `~/.config/hypr/modules/user.lua` and set, for example:

```lua
mainMod = "ALT",          -- or "CTRL" / "SUPER"
```

Then reload: `hyprctl reload`.
