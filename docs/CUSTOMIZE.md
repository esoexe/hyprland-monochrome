# Customize

Everything here is a small, local edit. After changing a Hyprland file the running
compositor reloads it the moment you save; `hyprctl reload` does it by hand if needed.

```bash
hyprctl reload        # re-read the config
hyprctl configerrors  # did I break something?
alias hr='hyprctl reload'   # already in ~/.zshrc
```

---

## 1. Start with `modules/user.lua`

That file holds your personal settings and every other module reads from it, so one edit
propagates everywhere (including into the shell scripts, which parse it too).

```lua
~/.config/hypr/modules/user.lua

mainMod = "SUPER",          -- try "ALT" or "CTRL"
terminal = "kitty",
browser  = "firefox",       -- the installer sets this if it can find yours
file_manager = "thunar",
editor   = "kitty -e nvim",

gaps_in = 5, gaps_out = 10,
border_size = 2, rounding = 10,
blur_enabled = true,        -- false = noticeably faster on old GPUs

kb_layout = "us",           -- "us,ara" + Super + X to switch
wallpaper_dir = home .. "/Pictures/Wallpapers",
```

## 2. Add or change a keybind

All binds live in `modules/binds.lua` next to a plain-English description. The helper
functions keep them readable:

```lua
sh("SHIFT + P", "pavucontrol", "Sound settings")                 -- run a command
act("SHIFT + B", hl.dsp.exec_cmd(browser) , "Browser")           -- same thing, clearer
act("M", hl.dsp.window.fullscreen({ mode = "maximized" }), "Maximize")
sh("SHIFT + Z", "systemctl suspend", "Suspend now")              -- any shell command
```

The `mainMod` prefix is added for you. Use `hl.bind(...)` directly when you need a different
modifier or special flags:

```lua
hl.bind("ALT + TAB", hl.dsp.window.cycle_next(), { description = "Next window" })
hl.bind("XF86AudioPlay", hl.dsp.exec_cmd("playerctl play-pause"), { locked = true })
hl.bind("SUPER + mouse:274", hl.dsp.window.close(), { description = "Middle-click to close" })
```

Handy dispatchers: `hl.dsp.window.close()`, `window.float()`, `window.fullscreen()`,
`window.pseudo()`, `window.pin()`, `window.move({ workspace = 5 })`, `focus({ direction = "left" })`,
`hl.dsp.group.toggle()`, `hl.dsp.workspace.toggle_special("name")`, `hl.dsp.exec_cmd("cmd")`.

A callback (Lua function) also works when you need logic:

```lua
hl.bind("SUPER + SHIFT + B", function()
    hl.dispatch(hl.dsp.exec_cmd(browser))
    hl.dispatch(hl.dsp.window.move({ workspace = 4, follow = false }))
end, { description = "Open the browser on workspace 4" })
```

> **Careful:** Lua callbacks run on the compositor thread. Never put `io.popen`, `wl-paste`
> or `sleep` inside one — call a script instead (`hl.dsp.exec_cmd("…/script.sh")`).
> Every script in `.config/hypr/scripts/` can be called that way.

Refresh the documentation afterwards:

```bash
make docs      # compiles & runs ./bin/keybind-table, ./bin/build-windows-kit, ./bin/build-preview
```

## 3. Colours

The whole setup is greyscale on purpose. Each file lists its palette in one place:

| Piece | File | What to edit |
| --- | --- | --- |
| Window borders, blur, opacity, animation curves | `hypr/modules/appearance.lua` | `col.active_border`, `decoration.blur`, `hl.curve(...)` |
| Bar | `waybar/style.css` | the `/* palette */` comment near the top |
| Menus (launcher, clipboard, wallpaper, power, settings) | `rofi/theme.rasi` | the `* { … }` palette block |
| Terminal | `kitty/theme.conf` | `background`, `foreground`, the `color0…15` ramp |
| Starship prompt | `starship.toml` | `[palettes.mono]` |
| Notifications | `dunst/dunstrc` | `[global]`, `[urgency_*]` |
| btop | `btop/themes/mono.theme` | the `theme[...]` lines |

Want a colour accent instead? Change one value and it flows through:

```css
/* rofi/theme.rasi */
accent: #7aa2f7ff;    /* was #ffffffff */
```

```css
/* waybar/style.css */
#workspaces button.active { background: #7aa2f7; color: #0a0a0a; }
```

```conf
# kitty/theme.conf
cursor #7aa2f7
color4 #7aa2f7
```

## 4. Wallpapers

```bash
wallpaper.sh pick          # thumbnail grid (Super + W)
wallpaper.sh random        # shuffle
wallpaper.sh next | prev   # cycle through the folder, handy on a keybind
wallpaper.sh solid 0d0d0d  # flat colour, no image at all
wallpaper.sh restore       # re-apply what you had (used at login)
```

Add your own images to `~/Pictures/Wallpapers` (or point `wallpaper_dir` in `user.lua`
somewhere else) and they show up in the picker. Thumbnails are cached in
`~/.cache/hyprland-monochrome/wallpaper-thumbs` — delete that folder to rebuild them.

### Regenerating the wallpapers

The 14 bundled static wallpapers and 3 animated live loops are drawn procedurally in
multi-threaded, zero-dependency **C++20** (`wallpapers/generate.cpp` and `wallpapers/animate.cpp`),
so you can render any resolution you like in milliseconds:

```bash
make                                                         # compile ./bin/wallpaper-gen & ./bin/wallpaper-anim
./bin/wallpaper-gen                                          # 1920x1080 (all 14 designs)
./bin/wallpaper-gen 3440 1440                                # ultrawide
./bin/wallpaper-gen 3840 2160                                # 4K UHD
./bin/wallpaper-gen 15360 8640 --only 6 --suffix -16k --quality 95   # 16K master
./bin/wallpaper-anim 1280 720 --frames 36 --fps 24           # seamless animated GIF loops
```

Every design is expressed in multiples of the 1080p reference height, so a bigger canvas adds
**real detail** rather than enlarging a smaller image: at 16K the wave in `mono-06` is a smooth
curve where the 1080p file has visible stair-steps.

* `--only 6` (or `6`, or `mono-06`) renders one design.
* `--suffix -16k` names the output `mono-06-16k.jpg` instead of overwriting the 1080p file.
* `--quality 95` raises the ITU-T T.81 Grayscale JPEG quality (default 92).
* Per-pixel shaders (`mono-01`..`mono-14`) parallelize across `std::thread::hardware_concurrency()`
  in `-O3` C++20, so 1080p and 4K renders finish almost instantaneously.

`mono-06-16k.jpg` (15360×8640) ships alongside the 1080p set; nothing else needed to change,
the wallpaper picker lists whatever is in the folder.

## 5. Waybar

Modules are listed in `~/.config/waybar/config.jsonc` — delete a name from
`modules-left/center/right` and the matching block to remove it.

Adding the Bluetooth battery or a custom script:

```jsonc
"custom/vpn": {
    "exec": "nmcli -t -f TYPE,STATE con show --active | grep -q vpn && echo ' on' || echo ''",
    "interval": 10,
    "on-click": "~/.config/hypr/scripts/settings.sh"
}
```

Then style it in `style.css` (`#custom-vpn { … }`). Restart the bar from
`Super + I → Hyprland → Restart the top bar`, or `pkill -SIGUSR2 waybar` to reload it.

The `updates` module shell script is `~/.config/waybar/scripts/updates.sh` — make it
`chmod -x` or delete the entry if you do not want the pacman check running hourly.

If the `temperature` module shows an error on your machine:

```bash
ls /sys/class/hwmon/*/temp1_input          # find the right sensor
```

…then set `"hwmon-path": "/sys/class/hwmon/hwmon3/temp1_input"` in the config, or just
remove the module.

## 6. Menus

Rofi is driven by one shared theme plus small per-menu overrides:

```text
~/.config/rofi/theme.rasi      → the look of everything
~/.config/rofi/launcher.rasi   → app launcher / window switcher (icons on/off)
~/.config/rofi/clipboard.rasi  → clipboard history
~/.config/rofi/wallpaper.rasi  → thumbnail grid (columns, thumbnail size)
~/.config/rofi/settings.rasi   → settings menu
~/.config/rofi/powermenu.rasi  → power menu and confirmation dialogs
~/.config/rofi/keybinds.rasi   → cheat sheet
```

Want launcher icons back? In `launcher.rasi` set `show-icons: true` (there is a commented
line waiting for you).

Add a new menu in three steps:

```bash
# 1. a menu file
cat > ~/.config/rofi/notes.rasi <<'EOF'
configuration { show-icons: false; }
listview { lines: 8; }
EOF

# 2. a function in ~/.config/hypr/scripts/settings.sh (or your own script)
printf 'one\ntwo\n' | rofi -no-config -theme ~/.config/rofi/theme.rasi \
    -theme ~/.config/rofi/notes.rasi -dmenu -p "notes:"

# 3. a keybind
sh("SHIFT + J", S .. "notes.sh", "My notes menu")
```

## 7. Monitors

The easy way is the settings menu: `Super + I → Monitors`. Resolution, scale, rotation,
position and on/off are all there, and whatever you pick is written to
`~/.config/hypr/modules/monitors-user.lua`, which `monitors.lua` loads. It survives
reboots and you can edit it by hand.

By hand, in `modules/monitors.lua`:

```lua
hl.monitor({ output = "eDP-1",    mode = "1920x1200@60",   position = "0x0",        scale = 1.25 })
hl.monitor({ output = "HDMI-A-1", mode = "2560x1440@144",  position = "auto-right", scale = 1 })
hl.monitor({ output = "",         mode = "preferred",      position = "auto",       scale = "auto" })
```

Workspaces on specific screens:

```lua
hl.workspace_rule({ workspace = "1", monitor = "eDP-1", default = true })
hl.workspace_rule({ workspace = "6", monitor = "HDMI-A-1", default = true })
```

Check names with `hyprctl monitors`.

## 8. Terminal, file manager, shell

* **Kitty**: `~/.config/kitty/kitty.conf` — font, padding, tab bar, keybinds. Colours are in
  `theme.conf`.
* **Thunar**: GTK theme and icons come from `~/.config/gtk-3.0/settings.ini`; thumbnails need
  `tumbler` + `ffmpegthumbnailer` (installed). `nwg-look` gives you a GUI for the GTK part.
* **Zsh**: `~/.zshrc` — history, completion, fzf, aliases (`ll`, `pac`, `gs`, `mkcd`,
  `extract`…), plus Hyprland helpers (`hr`, `hc`, `hm`, `walls`, `keys`). Add your own
  aliases at the bottom.
* **Starship**: `~/.config/starship.toml`; disable it with `starship config disable`… or just
  comment out the `eval "$(starship init zsh)"` line.
* **Switching terminal**: set `terminal = "alacritty"` (or `foot`, `wezterm`) in `user.lua`,
  change the `Super + T` bind if the launch flags differ, and add the matching opacity rule
  in `modules/rules.lua` (`terminal-opacity`). Optional colours per terminal are your call.

## 9. Spotify theming

Optional, needs the AUR package `spicetify-cli`:

```bash
yay -S spicetify-cli
~/.config/hypr/scripts/spotify-theme.sh          # apply the monochrome theme
~/.config/hypr/scripts/spotify-theme.sh restore  # back to normal
```

The theme lives in `~/.config/spicetify/Themes/monochrome/` — `color.ini` for the colour
scheme, `user.css` for the layout. **Run the script again after every Spotify update.**
To make that automatic, uncomment the `spicetify apply` line in
`modules/autostart.lua`.

## 10. Session, idle and lock

* `~/.config/hypr/hypridle.conf` — when to dim, lock, turn the screen off and suspend.
  Delete a `listener { … }` block to disable that step; the file explains each one.
* `~/.config/hypr/hyprlock.conf` — the lock screen. It uses a blurred screenshot as the
  background, a big clock and a single password field. Everything in it is a widget:
  delete the clock labels, move them around via `position = x, y`.
* `~/.config/hypr/hyprsunset.conf` — blue-light filter profiles by time of day.
* Logout behaviour is in `scripts/session.sh`: it uses `uwsm stop`, then `hyprshutdown`,
  then `hyprctl dispatch 'hl.dsp.exit()'` — whichever you have.
* Prefer the wlogout grid instead of the Rofi power menu? Install `wlogout` (AUR) and change
  the `ESCAPE`/`GRAVE` binds in `modules/binds.lua`:

```lua
sh("ESCAPE", "wlogout -b 5 -c 20 -r 20 -L 1700 -R 1700 -T 325 -B 325", "Logout menu")
```

## 11. Screenshots and recording

Screenshots are three small tools doing one job each: `grim` grabs, `slurp` selects the area,
`wl-copy` puts the result on the clipboard. The only knob most people touch is the folder:

```lua
-- modules/user.lua
screenshot_dir = home .. "/Pictures/Screenshots",
```

Recording lives in `~/.config/hypr/scripts/record.sh`. It works with either recorder:

| | [gpu-screen-recorder](https://git.dec05eba.com/gpu-screen-recorder) | wf-recorder |
| --- | --- | --- |
| Encoding | GPU (VAAPI/NVENC), ~0% CPU | software or VAAPI |
| Instant replay | yes (`Super + Alt + S`) | no |
| Pause | yes (`Super + Alt + P`) | no |
| Audio | desktop, mic, or both at once | one source |

Install whichever you prefer, or both — the script picks gpu-screen-recorder when it is
present. Force one with `record_backend = "wf-recorder"` (or `"gsr"`) in `modules/user.lua`.

```lua
-- modules/user.lua
record_backend  = "auto",     -- auto | gsr | wf-recorder
record_audio    = "desktop",  -- off | desktop | mic | both
record_fps      = 60,
replay_seconds  = 60,         -- how much the instant-replay buffer holds
recording_dir   = home .. "/Videos/Recordings",
```

Change any of these without editing the file:

```bash
~/.config/hypr/scripts/record.sh audio both     # desktop audio + microphone
~/.config/hypr/scripts/record.sh audio off      # silent video
~/.config/hypr/scripts/record.sh status         # what is happening right now
```

`audio` also has a cycle: `record.sh audio next` walks desktop → both → mic → off, and the
Settings menu (`Super + I` → *Screenshots & recording*) exposes all of it with a click.

The bar shows a white dot and a timer while recording, `▮▮` while paused and `● buf` when the
instant-replay buffer is running. Click it to stop, right-click to save a replay, middle-click
to pause. Hide it by deleting `custom/recording` from `modules-right` in `waybar/config.jsonc`.

## 12. Window rules

`modules/rules.lua` has commented examples for: floating utility windows, opacity per app,
the picture-in-picture corner, the dropdown terminal's size, games (no blur/borders) and
idle inhibition for video. Find a window's class with:

```bash
hyprctl clients | grep -E 'class|title'
```

…then add:

```lua
hl.window_rule({
    name  = "my-app",
    match = { class = "^(org.example.App)$" },
    float = true,
    size  = { 1000, 700 },
    center = true,
})
```

## 13. Keeping your changes

If you installed with `--link`, just edit the files and commit them in the repo you cloned.
Otherwise keep your local edits out of the way of updates:

```bash
# keep personal tweaks in a file the installer never touches
$EDITOR ~/.config/hypr/modules/user.lua        # safe: only created once

# or track your own fork
cd ~/hyprland-monochrome && git checkout -b mine
```
