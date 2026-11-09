# Troubleshooting

Most problems are one of five things: a missing package, a wrong path, a config syntax
error, a service that is not running, or a driver quirk. Work through these in order.

```bash
hyprctl configerrors      # config syntax errors — look here FIRST
hyprctl version           # must be 0.55+ for the Lua config
hyprctl rollinglog        # what Hyprland is complaining about right now
systemctl --user status hypridle      # (when started through uwsm)
pgrep -a waybar; pgrep -a dunst       # are the helpers alive?
```

Hyprland itself always keeps three emergency keybinds: `Super + Q` (terminal),
`Super + R` (run a command), `Super + M` (quit). If everything else fails, use those.

---

## Nothing appears on screen after login

1. You are in a TTY and ran `Hyprland` directly? Check the error output.
2. `hyprctl configerrors` — if it lists errors, the config stopped partway through.
   Fix the file it names (`~/.config/hypr/...`) and reload.
3. Try the default config to see whether the problem is the config or the system:

```bash
mv ~/.config/hypr/hyprland.lua ~/.config/hypr/hyprland.lua.bak
Hyprland            # generates a fresh example config if none exists
```

4. If a completely default config also fails, it is a driver/session problem: check you are
   starting from a TTY or a Wayland-capable session, and that your user is in the `video` and
   `input` groups (`groups`).

## No bar

```bash
waybar &                    # run it in a terminal and read the output
pkill waybar && waybar      # in case it is stuck
```

* `Failed to parse config` → a JSON syntax error in `~/.config/waybar/config.jsonc`
  (a missing comma or trailing comma). Run `make && ./bin/mono-util check-configs .` (or
  `sed -E 's|^[[:space:]]*//.*||' ~/.config/waybar/config.jsonc | jq .`) to pinpoint it.
* A module shows an error box (the `temperature` module is the usual suspect) → remove it
  from `modules-right` in the config, or point it at the right sensor (see CUSTOMIZE.md §5).
* Bar visible but empty/transparent → check the CSS: `gtk.css` errors are printed to the
  terminal you started `waybar` from.
* `Super + Shift + W` toggles the bar if it is simply hidden.

## Rofi menus look wrong or do not open

```bash
rofi -show drun -no-config -theme ~/.config/rofi/theme.rasi -theme ~/.config/rofi/launcher.rasi
```

* Errors about the theme → a syntax error in one of the `.rasi` files (usually a missing
  `;`). The base theme is `theme.rasi`; the others only override a few values.
* Nothing happens at all → the launcher script: `~/.config/hypr/scripts/menu.sh launcher`.
* Icons missing in the launcher → it is text-only by design; turn icons on in
  `~/.config/rofi/launcher.rasi`.

## Clipboard history is empty

It is filled by `wl-paste --watch cliphist store`, started in `modules/autostart.lua`:

```bash
pgrep -af 'wl-paste.*cliphist'
cliphist list | head
```

If `pgrep` shows nothing: `wl-clipboard` and `cliphist` installed? Log out/in (or run the
two `wl-paste` commands by hand) and copy something. Images also work — `imagemagick`
generates the thumbnails.

## Wallpaper is missing or black

```bash
awww query                                  # what is the daemon showing?
pgrep -a awww-daemon
~/.config/hypr/scripts/wallpaper.sh restore
```

* On first login the daemon may start slightly after the restore call — the
  `sleep 1` in `modules/autostart.lua` handles the usual case; bump it to 2 if you have a
  slow driver.
* Wallpaper vanishes after reconnecting a monitor: this is awww behaviour (it does not
  re-apply on hotplug). Either re-run `wallpaper.sh restore` or add a
  [kanshi](https://github.com/emersion/kanshi) profile with an exec line.
* Reboot and the wallpaper is the default again → `~/Pictures/Wallpapers` must exist and
  contain images, and the state file is `~/.cache/hyprland-monochrome/wallpaper`.
* Wallpaper picker shows no thumbnails → `imagemagick` installed? Delete the cache:
  `rm -rf ~/.cache/hyprland-monochrome/wallpaper-thumbs`.

## Settings menu: a section does nothing

* **Network** needs NetworkManager running: `systemctl status NetworkManager`.
  Connecting to a new Wi-Fi from the menu uses `nmcli` — if it fails, the notification shows
  nmcli's message (wrong password, weak signal, …).
* **Bluetooth** needs `bluetooth`: `sudo systemctl enable --now bluetooth`.
  If no adapter shows up: `rfkill list` (a hardware/soft block).
* **Monitors** writes to `~/.config/hypr/modules/monitors-user.lua` and reloads. If a change
  does not apply, check the file for syntax and `hyprctl configerrors`.
  "Invalid scale" warnings mean the scale does not divide your resolution cleanly — use
  1, 1.25, 1.5, 2.
* **Sound** needs PipeWire + `wireplumber`, and `pactl` (from `libpulse`) for the per-app
  volume list: `sudo pacman -S pipewire pipewire-pulse wireplumber libpulse`.

## Volume / brightness keys do nothing

```bash
wpctl status | head -20                       # is there a sink at all?
brightnessctl -l                              # is there a backlight device?
```

* Volume: the binds call `~/.config/hypr/scripts/volume.sh`, which calls `wpctl`
  (`wireplumber`). Test it directly: `~/.config/hypr/scripts/volume.sh up`.
* Brightness: on some laptops the backlight is `intel_backlight` and requires the user to be
  in the `video` group, or a udev rule. `brightnessctl --device='*' info` lists candidates.
* On a desktop there is no backlight → delete the two brightness binds, or rebind them to
  `hyprctl hyprsunset gamma +10` / `-10` (see `hyprsunset.conf`).
* Media keys still work on the lock screen because those binds use the `locked` flag; if
  yours do not, add `{ locked = true }`.

## Notifications do not appear

```bash
pgrep -a dunst
notify-send "test" "hello"
dunstctl history-pop        # bring back the last one (Super + N)
```

* Dunst must be running exactly once. If it is not started at all, check
  `modules/autostart.lua`.
* "Notification daemon already running" → another daemon (mako, swaync) is active; disable
  one of them.
* Icons are missing → install `papirus-icon-theme` (already in `packages.txt`) and check
  `icon_path` in `~/.config/dunst/dunstrc`.

## Lock screen problems

* **Screen does not lock** → hyprlock exits when it cannot find a config:
  `hyprlock -c ~/.config/hypr/hyprlock.conf` and read the error.
* **Wrong password** → hyprlock authenticates through PAM (`/etc/pam.d/hyprlock` or `login`).
  Test your password in a TTY; if it works there but not in hyprlock, check
  `journalctl -xe | grep -i pam`.
* **Locked out after 3 failed attempts** → Arch's `pam_faillock` default; wait 10 minutes
  or adjust `/etc/security/faillock.conf`.
* **Suspend does not lock** → `hypridle` must be running and
  `before_sleep_cmd = loginctl lock-session` present in `hypridle.conf`.
* The blurred background comes from a screenshot of your screen; on very low-end GPUs set
  `blur_passes = 0` in the `background` block.

## Screen sharing / portals (OBS, Firefox, Meet)

```bash
pacman -Q xdg-desktop-portal-hyprland xdg-desktop-portal-gtk
systemctl --user status xdg-desktop-portal-hyprland
xdg-desktop-portal-hyprland --replace &
```

Pick the *Hyprland* option in the share dialog (not "Screen 1" from the GTK portal). For a
specific app, `xdg-desktop-portal-hyprland` must be the one answering — if `xdg-desktop-
portal-wlr` or `-gnome` is also installed, remove it.

## Blur/animations feel slow, or the fan never stops

```lua
-- modules/user.lua
blur_enabled = false,        -- biggest single win
```

```lua
-- modules/appearance.lua: fewer passes, no shadows
shadow = { enabled = false },
blur = { enabled = true, size = 3, passes = 1, vibrancy = 0.0 },
```

Also set `misc.vfr = true` (already on) and disable `dim_inactive` if you like.

## Hyprland refuses the config ("expected .conf")

You are on Hyprland 0.54 or older — the Lua format arrived in 0.55.

```bash
sudo pacman -Syu hyprland
hyprctl version
```

If you must stay on an older version, this setup will not work as-is; the old
hyprlang version of the wiki lives at <https://wiki.hypr.land/0.54.0/>.

## NVIDIA

* Add `nvidia_drm.modeset=1` (and usually `nvidia_drm.fbdev=1`) to your kernel command line.
* Verify: `cat /sys/module/nvidia_drm/parameters/modeset` → `Y`.
* `cat /sys/module/nvidia_drm/parameters/fbdev` → `Y` if you want the console to look right.
* If blur flickers, set `blur.enabled = false` or `xray = true`.
* Make sure `nvidia-open` vs `nvidia` matches your GPU generation (Turing+ can use `nvidia-open`).

## Fonts or icons show as boxes (□)

```bash
fc-list | grep -i "JetBrainsMono Nerd"
fc-list | grep -i "Symbols Nerd"
```

Install what is missing:

```bash
sudo pacman -S ttf-jetbrains-mono-nerd ttf-nerd-fonts-symbols otf-font-awesome noto-fonts noto-fonts-emoji
fc-cache -fv
```

The bar and menus ask for *JetBrainsMono Nerd Font*; if you prefer another Nerd Font, change
the font line in `~/.config/waybar/style.css`, `~/.config/rofi/theme.rasi`,
`~/.config/kitty/kitty.conf` and `~/.config/starship.toml` (the icons come from the
`ttf-nerd-fonts-symbols` fallback, so they keep working).

## Thunar shows no thumbnails, or cannot mount phones

```bash
pacman -Q tumbler ffmpegthumbnailer gvfs gvfs-mtp
killall thunar && thunar -q          # restart it after installing
```

For a phone: `gvfs-mtp` installed, and the phone unlocked with "file transfer" as the USB
mode.

## Everything is too big / too small

* Whole system too small on a HiDPI screen: set the monitor scale
  (`Super + I → Monitors → Scale`, e.g. 1.25) — that is the correct fix.
* Only text too small: `font_size` in `kitty/kitty.conf`, `font-size` in
  `waybar/style.css`, `font:` in `rofi/theme.rasi`.
* Cursor too small: `cursor_size` in `modules/user.lua` (applies to Hyprland and GTK), then
  reload.

## XWayland apps look blurry or scaled oddly

```bash
hyprctl getoption xwayland:force_zero_scaling
```

If you use fractional scaling, set it to true so X11 windows are not upscaled:

```lua
hl.config({ xwayland = { force_zero_scaling = true } })
```

Also make sure the right toolkit env vars are set — `hyprland.lua` already sets
`QT_QPA_PLATFORM`, `MOZ_ENABLE_WAYLAND`, `ELECTRON_OZONE_PLATFORM_HINT` and
`GDK_BACKEND` for the common cases.

## A script errors out

```bash
bash -x ~/.config/hypr/scripts/settings.sh     # trace what it does
./test/smoke-test.sh                            # run the whole suite with stubs
```

## SDDM animated greeter or account settings

* **SDDM preview fails**: confirm SDDM is installed and use `./tools/install-sddm-theme.sh --preview`. The helper picks the Qt 5 or Qt 6 greeter and stages the theme assets in a temporary directory. Preview does not write SDDM configuration.
* **SDDM theme is blank or fails after applying**: switch to a TTY, run `sudo ./tools/install-sddm-theme.sh --remove`, and review `journalctl -b -u sddm`. Keep the existing display manager enabled; the theme installer does not enable or restart it.
* **The login form rejects a valid password**: check the user name, the selected last-used session, and PAM errors in `journalctl -b -u sddm`. The theme submits credentials to SDDM and does not save them.
* **Account menu cannot change the password or shell**: the account menu launches the system `passwd`, `chfn`, or `chsh` tool in a terminal. Verify that tool exists and read its terminal output. A shell must appear in `/etc/shells`; the menu never edits account files directly.
* **No terminal opens from Account settings**: install or configure Kitty, foot, Alacritty, or xterm. The account menu intentionally does not attempt privileged changes without the normal system tool.


`scripts/lib.sh` holds the shared helpers (`mono_var`, `notify`, `rofi_menu`). If you edit
`modules/user.lua` with exotic Lua (function calls, tables), the shell scripts may no longer
be able to read a value from it — keep those lines as plain `name = value,` assignments.

## Recording does not start, or the file is empty

```bash
~/.config/hypr/scripts/record.sh status      # idle / recording / paused
cat ~/.cache/hyprland-monochrome/record.log  # what the recorder itself said
```

* **"No screen recorder found"** — install one: `sudo pacman -S gpu-screen-recorder` (GPU
  accelerated, instant replay) or `sudo pacman -S wf-recorder` (works everywhere).
* **"failed" with a driver message** — GPU encoders need the right driver: `mesa` +
  `libva-mesa-driver` on AMD, `intel-media-driver` on Intel, `nvidia-utils` on NVIDIA.
  wf-recorder has no such requirement; set `record_backend = "wf-recorder"` to sidestep it.
* **Video but no sound** — check `record.sh audio show`, then `record.sh audio desktop`.
  gpu-screen-recorder reads PipeWire directly; make sure `pipewire-audio` is installed.
* **Pause/replay do nothing** — those two exist only in gpu-screen-recorder. wf-recorder has
  neither, and the script tells you so instead of failing silently.
* **A recording survived a crash** — the state file lives in
  `~/.cache/hyprland-monochrome/record.state`; delete it if the bar ever shows a phantom
  recording. The video itself is written continuously, so killing the recorder still leaves a
  playable file.
* **Recording makes games stutter** — lower `record_fps` to 30, or use
  gpu-screen-recorder (it encodes on the GPU; wf-recorder copies frames through the CPU).

## Start from a known-good state

```bash
# config: put the shipped defaults back
cd ~/hyprland-monochrome && ./install.sh --yes --no-packages

# your own old files: restore the backup
~/.config-backups/pristine/restore.sh

# remove everything this setup installed
./uninstall.sh
```

## Ask for help

When opening an issue, include:

```bash
hyprctl version
hyprctl configerrors
hyprctl monitors
cat /etc/os-release | head -3
journalctl --user -b -p 3 --no-pager | tail -30
```
