# Keybinds

> This file is generated from `~/.config/hypr/modules/binds.lua`.
> Edit the Lua file, then run `python3 tools/keybind-table.py` to refresh
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
- [Media & Hardware Keys](#media--hardware-keys)

## Applications

| Keys | Action |
| --- | --- |
| `Super + Enter` | Terminal |
| `Super + T` | Terminal |
| `Super + Shift + Enter` | Floating terminal |
| `Super + Shift + T` | Dropdown terminal (quake style) |
| `Super + D` | App launcher |
| `Super + Shift + D` | Window switcher |
| `Super + E` | File manager |
| `Super + B` | Browser |
| `Super + W` | Wallpaper picker |
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
| `Super + P` | Pseudo-tile (dwindle) |
| `Super + Y` | Toggle split direction (dwindle) |
| `Super + O` | Cycle window opacity |
| `Super + G` | Group / ungroup window (tabs) |
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
| `Alt + Print` | Record the screen (again = stop) |
| `Alt + Shift + Print` | Record a selected area |
| `Alt + S` | Save the last minute (instant replay) |
| `Alt + P` | Pause / resume recording |
| `Super + -` | Zoom out |
| `Super + =` | Zoom in |

## Media & Hardware Keys

| Keys | Action |
| --- | --- |
| `XF86AudioRaiseVolume` | Volume up |
| `XF86AudioLowerVolume` | Volume down |
| `XF86AudioMute` | Mute the output |
| `XF86AudioMicMute` | Mute the microphone |
| `XF86MonBrightnessUp` | Brightness up |
| `XF86MonBrightnessDown` | Brightness down |
| `XF86AudioNext` | Next track |
| `XF86AudioPrev` | Previous track |
| `XF86AudioPlay` | Play / pause |
| `XF86AudioPause` | Play / pause |
| `XF86AudioStop` | Stop playback |

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
