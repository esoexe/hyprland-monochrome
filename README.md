# hyprland-monochrome

A clean, monochrome Hyprland desktop for **Arch, EndeavourOS, CachyOS, Manjaro** (and any other
Arch-based system). Simple, modern, quiet — and built so you can actually find things again
six months from now.

```
Hyprland (Lua config) · Waybar · Rofi · awww · hyprlock · hypridle · hyprsunset
Kitty · Thunar · Zsh + Starship · dunst · cliphist · PipeWire · NetworkManager
```

![hyprland-monochrome](docs/preview.png)

<p align="center">
  <em>Black surfaces, white text, greys in between — no accent colour to fight with your wallpapers.</em><br>
  <small>(<code>docs/preview.svg</code> is a mockup of the layout, not a screenshot — open
  <code>docs/preview.html</code> for a preview generated from the config itself)</small>
</p>

<p align="center">
  <a href="docs/desktop-demo.mp4">▶ <b>Watch the live 1080p desktop session</b></a> — Waybar, Rofi
  menus, 16K wallpapers, Thunar, Dunst, <code>record.sh</code> and the full test suite running live ·
  or <a href="docs/test-run.mp4">watch the terminal-only test run</a>.<br>
  <small>(real Wayland session captured with <code>wf-recorder</code> via <code>tools/record-desktop.sh</code>)</small>
</p>

---

## What you get

| | |
| --- | --- |
| **Hyprland & Reactive Lua** | Lua 0.55+/0.56 tiling (`dwindle`, `master`, `scrolling`), modal chords (`RESIZE`, `CAPTURE`, `SCRATCH`), monochrome group tab bars, and **in-compositor `hl.on` reactive hooks (`smart.lua`)** — auto-mutes notifications during screen-sharing, recovers hot-plugged monitors, and alerts on urgent background windows (`Super + U`) |
| **Command Palette** | `Super + Shift + P` (or `Super + ;`): Raycast/Linear-style omnibar for jumping to open windows across workspaces, toggling named drawers, saving/restoring workspace sessions, running the System Doctor, and evaluating live math/hex expressions (`= (15360*8640)/1e6`) |
| **Named Drawers & Sessions** | `Super + Shift + T` (dropdown terminal), `Super + Shift + B` (floating `btop++` monitor), `Super + '` (instant `~/Documents/scratch.md` in Neovim), plus `Super + Ctrl + S` (`SCRATCH` chord) and **Workspace Session Snapshots** (`workspace-session.sh save` / `restore`) |
| **Focus & Zen** | `Super + Z` enters distraction-free **Zen Mode** (hides Waybar, frames the active window with 28px obsidian margins, mutes Dunst); `Super + Shift + Z` runs Pomodoro / Deep-Work sprints with a live inverted Waybar pill (`◎ 24:58`) |
| **Layouts & Gaps** | `Super + \` cycles `Dwindle` → `Master` → `Scrolling` on the fly; `Super + Shift + \` switches gap presets (`Default`, `Compact`, `Airy` gallery, `Zero` monocle) |
| **Waybar** | Frosted glass pill groups, active-inverted workspaces, **live Unicode CPU micro-sparkline (`▁▂▄▆▃▂ 14%`)**, `RESIZE`/`CAPTURE`/`SCRATCH` submap pill, `custom/focus` Pomodoro pill, `custom/recording` dot + timer, MPRIS now-playing, RAM/temp, updates, network, Bluetooth, volume, battery, tray, calendar |
| **Full-Stack Monochrome** | **7/7 toolkits themed strictly greyscale (`R == G == B`)**: GTK3/GTK4 (`gtk.css`), **Qt5 & Qt6 (`qt5ct`/`qt6ct` 21-slot Fusion palette)**, **zero-plugin typographic Neovim (`monochrome.lua`)**, Kitty, Rofi, Waybar, Dunst progress-bar OSDs (`volume.sh` + `brightness.sh`), and a redesigned Swiss-watch `hyprlock` with live MPRIS now-playing |
| **Settings & Doctor** | `Super + I`: Command Palette, Focus/Zen, Named Drawers, Workspace Sessions, Layouts/Gaps, **System & Monochrome Theme Doctor (`doctor.sh`)**, System Info, Network, Bluetooth, Monitors, Sound, Night Shift, Capture/OCR |
| **Wallpapers** | `Super + W`: 3×2 visual gallery, random/next/prev, solid colours, login restore. **14 procedural monochrome designs** (`mono-01`..`mono-14`: **Gravitational Singularity**, **Liquid Mercury**, **Rain & Bokeh Noir**, **Bauhaus Compass**, Solar Eclipse, Caustic Silk Ribbon…), scalable to any resolution incl. 4K `mono-07`/`mono-11` & 16K `mono-06` |
| **Capture & OCR** | `Super + C` opens the single-chord `CAPTURE` mode (`s` region, `f` full, `w` window, `o` **Tesseract OCR → clipboard**, `e` **Satty/Swappy annotate**, `r` record, `p` 60s instant replay) — plus `Print` keybinds |
| **Recording** | `Super + Alt + Print` starts/stops screen recording (`gpu-screen-recorder` with 60s replay buffer, automatic `wf-recorder` fallback); live `●` timer in Waybar |
| **Idle & light** | hypridle (dim → lock → screen off → suspend) and hyprsunset (automatic blue-light filter, `Super + Shift + N` to toggle) |
| **Shell** | Zsh with Starship (monochrome prompt), autosuggestions, syntax highlighting, fzf history, `eza`/`bat`, useful aliases and functions |
| **Spotify** | Optional Spicetify theme: greyscale UI, album art is the only colour left in the app |
| **Windows** | Dual-booter? `windows/` holds a matching GlazeWM config and Windows Terminal scheme, generated from the same palette — [docs/ON-WINDOWS.md](docs/ON-WINDOWS.md) |

---

## Install

```bash
git clone https://github.com/<your-user>/hyprland-monochrome.git
cd hyprland-monochrome
./install.sh
```

That's it. The installer:

1. checks it is really talking to an Arch-based system and that Hyprland is new enough,
2. installs everything in `packages.txt` from the official repos (nothing from the AUR is required),
3. offers the optional extras in `aur-packages.txt` if you have `paru` or `yay`,
4. **backs up every file it replaces** — twice (see below),
5. copies the configuration into `~/.config` (or symlinks it with `--link`),
6. copies the wallpapers, makes the scripts executable, offers to set Zsh as your shell,
7. reloads Hyprland and reports any config errors it finds.

Then **log out and back in**, choosing the *Hyprland* session.

```text
./install.sh --help          all options
./install.sh --yes           no questions (good for a fresh VM)
./install.sh --no-packages   only install the config files
./install.sh --link          symlink instead of copy (edit in place, git-friendly)
./install.sh --dry-run       show what would happen, touch nothing
./uninstall.sh               remove the config and put your old files back
```

### Backups

Two folders are kept, and they mean different things:

```text
~/.config-backups/pristine/     your files exactly as they were before the FIRST install
                                (never overwritten, so re-installing cannot bury them)
~/.config-backups/<timestamp>/  what that particular run replaced, with restore.sh
```

Restore at any time:

```bash
~/.config-backups/pristine/restore.sh     # put your originals back
./uninstall.sh                            # remove the config, then restore the originals
./uninstall.sh --list                     # see what is available
```

Already have a desktop set up? `--link` is pleasant: the repo stays the source of truth and
`git pull` updates your desktop.

---

## Keybinds

`Super` is the main modifier. The full, always-up-to-date table lives in
**[docs/KEYBINDS.md](docs/KEYBINDS.md)** — and `Super + /` shows a searchable version of it
built from your live config.

| Keybind | Action | | Keybind | Action |
| --- | --- | --- | --- | --- |
| `Super + T` / `Super + Enter` | Terminal | | `Super + Shift + P` / `Super + ;` | **Command palette & math** |
| `Super + D` | App launcher | | `Super + Z` / `Super + Shift + Z` | **Zen mode / Pomodoro focus** |
| `Super + E` | File manager (monochrome GTK) | | `Super + R` | **`RESIZE` chord** (`h/j/k/l`, `1-3/0` gaps) |
| `Super + B` | Browser | | `Super + C` | **`CAPTURE` chord** (`s/f/w/o/e/r/p/m`) |
| `Super + I` | Settings menu | | `Super + \` / `Super + Shift + \` | **Cycle layout / gap presets** |
| `Super + W` | Wallpaper studio (14 designs) | | `Super + F` / `Super + Shift + F` | Fullscreen / Maximize |
| `Super + Shift + B` | **Floating `btop++` drawer** | | `Super + '` | **Scratch Markdown notes (`nvim`)** |
| `Super + Ctrl + S` | **`SCRATCH` & Session chord** | | `Super + U` | **Jump to urgent window** |
| `Super + V` | Clipboard history | | `Super + Space` | Floating (centred) |
| `Super + O` | Cycle window opacity | | `Super + G` / `Super + Alt + G` | Group windows / next tab |
| `Super + /` | Interactive shortcut launcher | | `Super + 1..0` | Switch workspace |
| `Super + Shift + W` | Toggle the top bar | | `Super + Shift + 1..0` | Move window to workspace |
| `Super + Shift + R` | Reload the config | | `Super + S` / `Super + Shift + T` | Scratchpad / Dropdown term |
| `Super + Print` | Screenshot (area) | | `Super + Ctrl + Shift + Print` | **OCR region to clipboard** |
| `Super + Alt + Print` | Record screen (again = stop) | | `Super + Tab` / `Super + Escape` | Lock screen / Logout menu |

Focus with `Super + H/J/K/L` (or the arrow keys), move windows with `Super + Shift + H/J/K/L`,
resize with `Super + Ctrl + H/J/K/L`, drag with `Super + left mouse`, resize with
`Super + right mouse`, switch workspaces with `Super + scroll`.

---

## Layout

```text
~/.config/hypr/
├── hyprland.lua              entry point: loads the modules below
├── hyprlock.conf             lock screen
├── hypridle.conf             idle: dim → lock → screen off → suspend
├── hyprsunset.conf           blue-light filter profiles by time of day
├── modules/                  ← everything you normally edit
│   ├── user.lua              your programs, gaps, keyboard, paths, toggles
│   ├── monitors.lua          monitor layout (+ monitors-user.lua from the settings menu)
│   ├── appearance.lua        borders, blur, animations, input devices
│   ├── autostart.lua         what starts with the session
│   ├── binds.lua             every keybind + RESIZE / CAPTURE / SCRATCH chords
│   ├── rules.lua             window + layer + named drawer rules
│   └── smart.lua             reactive hl.on() event hooks (screenshare DND, hotplug)
└── scripts/                  bash that powers the menus, drawers, sessions & doctor
```

Everything else is where you would expect it: `~/.config/waybar`, `~/.config/rofi`,
`~/.config/kitty`, `~/.config/nvim`, `~/.config/gtk-{3,4}.0`, `~/.config/qt{5,6}ct`,
`~/.config/dunst`, `~/.config/starship.toml`, `~/.config/btop`, `~/.config/fastfetch`, `~/.zshrc`.

Config files are hot-reloaded when you save them (`hyprctl reload` does it by hand).
After editing `binds.lua`, refresh the docs with `python3 tools/keybind-table.py`.

---

## Make it yours

Start with **`~/.config/hypr/modules/user.lua`** — it is the one file that holds your
browser, terminal, gaps, keyboard layout, wallpaper paths and a few toggles. The other
modules read from it, so you rarely need to touch them.

Common edits:

```lua
-- modules/user.lua
browser  = "brave",            -- Kitty, Thunar, Zsh and Starship are the defaults
gaps_out = 14,
blur_enabled = false,          -- noticeably lighter on old GPUs
kb_layout = "us,ara",          -- Super + X switches between them
```

The palette is deliberately small and repeated in a few places, one comment per file tells
you what to change:

| What | File | Key |
| --- | --- | --- |
| Window borders, blur, opacity | `hypr/modules/appearance.lua` | `col.active_border`, `decoration.blur` |
| Bar colours | `waybar/style.css` | `.modules-left`, `#workspaces button.active` |
| Menus (all of them) | `rofi/theme.rasi` | the palette block at the top |
| Terminal | `kitty/theme.conf` | 16-colour greyscale ramp |
| Prompt | `starship.toml` | `[palettes.mono]` |
| Notifications | `dunst/dunstrc` | `[global]` colours |
| btop / fastfetch | `btop/themes/mono.theme`, `fastfetch/config.jsonc` | — |

More recipes — adding keybinds, Waybar modules, a second monitor, a different terminal,
new Rofi menus, the Spotify theme — are in **[docs/CUSTOMIZE.md](docs/CUSTOMIZE.md)**.

---

## Documentation

| File | What is in it |
| --- | --- |
| [docs/QUICKSTART.md](docs/QUICKSTART.md) | **start here** — install, first login, first keybinds, updating, uninstalling |
| [docs/INSTALL.md](docs/INSTALL.md) | requirements, sessions (uwsm/SDDM/GDM/ly), manual install, updates, uninstall |
| [docs/KEYBINDS.md](docs/KEYBINDS.md) | the complete keybind table (generated from the config) |
| [docs/preview.html](docs/preview.html) | a preview page generated from the config: bar modules, keybinds, palettes, packages |
| [docs/CUSTOMIZE.md](docs/CUSTOMIZE.md) | how to change programs, colours, bar modules, menus, monitors, shell |
| [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) | the things that actually go wrong, and how to fix them |
| [docs/ON-WINDOWS.md](docs/ON-WINDOWS.md) | Windows + Linux: what runs where, per-distro install notes, WSL2, and the generated `windows/` kit |

Useful scripts, if you prefer the terminal:

```bash
~/.config/hypr/scripts/settings.sh         # the settings menu
~/.config/hypr/scripts/wallpaper.sh pick   # wallpaper picker
~/.config/hypr/scripts/menu.sh keybinds    # keybind cheat sheet
~/.config/hypr/scripts/screenshot.sh region
~/.config/hypr/scripts/record.sh toggle     # record; press again to stop
~/.config/hypr/scripts/record.sh replay     # save the last minute
~/.config/hypr/scripts/record.sh path       # where the videos live
```

---

## Tests

The scripts are tested without a running compositor, so you can hack on them safely:

```bash
./test/run-all.sh                # everything below, in one go
./test/smoke-test.sh             # 100 checks: stubs hyprctl/rofi/awww/gpu-screen-recorder/…
./test/install-sandbox.sh        # installs into a throw-away HOME: backups, --link, uninstall
python3 tools/keybind-table.py --check   # is docs/KEYBINDS.md still accurate?
python3 tools/build-preview.py --check   # is docs/preview.html still accurate?
python3 tools/build-preview.py           # rebuild docs/preview.html after editing the config
python3 tools/build-windows-kit.py --check  # is the Windows kit still in step with the palette?
```

The smoke test builds a throw-away `$HOME`, stubs every external tool and asserts on what
each script actually does — it caught real bugs (quoting, jq pipelines, backup order,
a signal-handling mistake in the recorder) while this setup was being written.

Want to watch instead of read? `docs/desktop-demo.mp4` is a real 1920×1080 Wayland session
captured by `tools/record-desktop.sh` (`wf-recorder`), and `docs/test-run.mp4` is `tools/demo.sh`
recorded with [asciinema](https://asciinema.org):

```bash
bash tools/record-desktop.sh                    # boot a headless Wayland desktop & record it
bash tools/demo.sh                              # watch the CLI walkthrough in your terminal
asciinema rec -c "bash tools/demo.sh" demo.cast # or record your own
```

---

## Requirements

Hyprland **0.55 or newer**, because the configuration uses the Lua format that replaced
hyprlang. Everything else is a normal Arch package — see
[packages.txt](packages.txt). The only AUR packages are optional
([aur-packages.txt](aur-packages.txt)): `wlogout`, `papirus-folders`, `bibata-cursor-theme`,
`spicetify-cli`.

Two notes worth knowing before you start:

* **NVIDIA**: you may need `nvidia_drm.modeset=1` on the kernel command line. See
  [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md).
* **First login**: if the screen looks empty, press `Super + T` and run `hyprctl configerrors`
  — that prints exactly what is wrong.

---

## Credits

Built on the shoulders of the Arch + Hyprland community, and inspired by setups like
[43PR/dotfiles](https://github.com/43PR/dotfiles), the [Hyprland wiki](https://wiki.hypr.land)
and [awww](https://codeberg.org/LGFae/awww). This configuration is an independent,
from-scratch implementation: its own Lua modules, scripts, Rofi theme, Waybar layout,
wallpapers and installer.

MIT licensed — see [LICENSE](LICENSE). Take it, fork it, make it yours.
