# Windows, Linux — and where this setup fits

This project is a **Hyprland** desktop, and Hyprland is a Linux-only Wayland compositor. So
the honest answer splits in two:

* **On your Linux install** — this is the real thing. [QUICKSTART.md](QUICKSTART.md) gets you
  running in about five minutes, *if* your distro has a new enough Hyprland (details below).
* **On your Windows install** — Hyprland cannot run there, not even inside WSL2. What can move
  over is the *feel*: the greyscale palette, the gaps, the tiling workflow and the keybind
  layout. There is a generated kit for that in [`windows/`](../windows/), described at the
  bottom of this page.

---

## Part 1 — On Linux

### The one hard requirement

**Hyprland 0.55 or newer.** Since 0.55 the config format is **Lua**
(`~/.config/hypr/hyprland.lua`); the old hyprlang syntax still loads for a couple of releases
but is deprecated. This project is written for the Lua format, so on an older Hyprland
`hyprland.lua` is simply ignored and you would get the default desktop.

Check before you install anything:

```bash
pacman -Si hyprland | grep -i version     # Arch-based, before installing
dnf info hyprland | grep -i version       # Fedora (see the caveat below)
apt policy hyprland                       # Ubuntu / Debian
hyprland --version                        # after installing (or: hyprctl version)
```

If it prints something below **0.55**, see *Older than 0.55* further down.

### Which distro are you on?

| Your distro | How to get Hyprland | Notes |
| --- | --- | --- |
| **Arch, EndeavourOS, CachyOS, Manjaro, Garuda** | **Nothing to do** — `./install.sh` installs it | This is the supported path. Current Hyprland is always available |
| **Ubuntu 26.04 LTS** | `sudo add-apt-repository universe && sudo apt update && sudo apt install hyprland` | Upstream's own wiki warns that Ubuntu's packaged Hyprland is *"extremely outdated"* and recommends building manually. **Check the version** — if it is below 0.55, use one of the routes below |
| **Debian trixie+** | `sudo apt install -t trixie-backports hyprland` | Not available for Bookworm (packages too old) |
| **Fedora 43/44** | `sudo dnf copr enable ashbuk/Hyprland-Fedora && sudo dnf install hyprland` | Fedora **does not** ship Hyprland in its official repos at the moment; that COPR tracks latest stable |
| **openSUSE Tumbleweed** | `sudo zypper in hyprland` | Rolling release, so it keeps up |
| **NixOS** | `programs.hyprland.enable = true;` | Home Manager supports the Lua config from 26.05 |

Then install the rest of this setup **without** its package step, so your distro's names win:

```bash
./install.sh --no-packages      # config, wallpapers, .zshrc, backups
```

You will need the helper packages too — `waybar rofi kitty dunst cliphist wl-clipboard grim
slurp wf-recorder awww hyprlock hypridle hyprsunset polkit-gnome`-style equivalents. The
complete list with comments is in [`packages.txt`](../packages.txt); `install.sh --dry-run`
prints it. Names differ per distro (`polkit-gnome` → `policykit-1-gnome`, `imagemagick` →
`imagemagick`, `otf-font-awesome` → `fonts-font-awesome`, …).

### Older than 0.55?

Three options, best first:

1. **Use an Arch-based install.** CachyOS and EndeavourOS are quick installs, and
   `./install.sh` then does everything. This is what the project is tested against.
2. **Build Hyprland from source** for your distro — upstream documents it, but warns that on
   point-release distros "it's likely that it will break at some point in the future".
3. **Wait** — Ubuntu/Debian/Fedora will catch up, and this project will still be here.

### WSL2

Hyprland does **not** run in WSL2: WSLg gives you a Wayland environment for *applications*, but
there are no DRM devices, so a compositor that needs GPU access cannot start (sway gets around
it with a shim; Hyprland does not). Use WSL for what it is genuinely good at with this repo —
running the tooling and the test suite:

```bash
sudo apt install shellcheck lua5.4 jq python3
bash test/run-all.sh      # the same 9 checks, from inside Windows
```

---

## Part 2 — On Windows

Hyprland needs the Linux kernel's DRM, so it cannot run natively on Windows, and WSL2 does not
help (above). Instead, the pieces that make this desktop *this* desktop are reproduced with
Windows-native tools — all of it generated from this repo's own config, so it cannot drift.

### Get the kit

```powershell
winget install GlazeWM          # i3-style tiling for Windows; offers Zebar during setup
winget install Microsoft.WindowsTerminal
winget install Starship.Starship
winget install DEVCOM.JetBrainsMonoNerdFont
```

Then copy from `windows/` in this repo:

| File | Goes to |
| --- | --- |
| `windows/glazewm-config.yaml` | `%userprofile%\.glzr\glazewm\config.yaml` |
| `windows/windows-terminal-settings.json` | merge into Windows Terminal's `settings.json` (Ctrl+, → *Open JSON file*) |
| `.config/starship.toml` (from the repo) | `%userprofile%\.config\starship.toml` — Starship reads the same file on Windows, same palette and all |
| `wallpapers/mono-01..06.jpg` | anywhere; they are plain JPEGs |

Starship needs one line in your PowerShell profile, or it installs and does nothing:

```powershell
notepad $PROFILE                     # then add:
Invoke-Expression (&starship init powershell)
```

GlazeWM has no bar of its own — **Zebar** is its Waybar equivalent, offered as a checkbox during
the GlazeWM install. It keeps its own config in `%userprofile%\.glzr\zebar\` (a
`settings.json` plus widget packs of HTML/CSS), so this repo generates the window-manager side
and leaves Zebar's widgets to you.

The generated config deliberately leaves `startup_commands` empty. Launching Zebar on startup
is the first thing most people add — and if Zebar is not installed, GlazeWM shows an error on
every launch. Once Zebar works, uncomment these two lines in the config:

```yaml
  startup_commands: ['shell-exec zebar']
  shutdown_commands: ['shell-exec taskkill /IM zebar.exe /F']
```

### What maps to what

| On Linux | On Windows |
| --- | --- |
| Hyprland (Wayland, Lua config) | GlazeWM (YAML config) — same i3-style tiling, gaps, rules |
| Waybar | Zebar |
| Rofi launcher, menus, clipboard | PowerToys Run / Start, Win+V — no exact equivalent, see below |
| Kitty | Windows Terminal (with the scheme generated here) |
| Zsh + Starship | PowerShell + Starship (same `starship.toml`, same palette) |
| `Super` as the modifier | `Alt` — Windows reserves combos like `Win+L`, so GlazeWM's own docs advise against the Windows key |
| hyprlock / hypridle | Windows lock screen / power settings |
| grim + slurp screenshots | `Alt+Print` → Snipping Tool overlay (`ms-screenclip:`) |
| awww wallpapers | plain JPEGs — set one as your background |

### The keybinds you will actually use there

| | |
| --- | --- |
| `Alt + Enter` | terminal (Windows Terminal) |
| `Alt + E` | File Explorer |
| `Alt + B` | browser |
| `Alt + H/J/K/L` | focus window (arrows work too) |
| `Alt + Shift + H/J/K/L` | move window |
| `Alt + R` | resize mode (then HJKL, `Esc` to leave) |
| `Alt + 1..0` | workspaces (`Alt + 0` is the tenth, as on Linux) |
| `Alt + Shift + 1..0` | move window to that workspace |
| `Alt + Space` | float + centre |
| `Alt + F` | fullscreen |
| `Alt + Q` | close window |
| `Alt + Shift + R` | reload the config |
| `Alt + Print` | screenshot |

The gaps (`5px` inner, `10px` outer), the focused-window border colour (`#e6e6e6`), the inactive
opacity (`94%`) and the terminal palette all come from `modules/user.lua` and
`kitty/theme.conf` — change them there, run `python3 tools/build-windows-kit.py`, and the
Windows side follows.

### Honest limits

* **Rofi has no equal.** The launcher, the power menu, the wallpaper picker, the clipboard
  history and the settings menu are Linux tools. On Windows you get PowerToys Run (start
  menu), `Win+V` (clipboard history), Windows Settings, and Snipping Tool.
* **Zebar's widgets are its own** — this repo generates the WM config, not a Zebar layout.
* **`Alt` instead of `Super`** for the reason above; you can switch to `lwin` by editing the
  generated YAML, at the cost of clashes with Windows' own shortcuts.
* **The scripts are Linux scripts.** `record.sh`, `wallpaper.sh`, the Rofi menus — they use
  `hyprctl`, `awww`, PipeWire and friends. On Windows, use the native equivalents (Xbox Game
  Bar for recording, etc.); the *workflow* is what carries over.

---

## Which should you actually use?

If you have both, this is the pragmatic split:

* **Do your work on Linux.** This setup is built for it, and the test suite proves it runs.
* **Keep Windows for the things that need it** (games, school/work software, hardware tools) and
  make it feel familiar with the kit above — same greys, same gaps, same Alt+H/J/K/L.
* **Edit the config in one place**: `modules/user.lua` and `.config/kitty/theme.conf` are the
  source of truth for *both* halves. `tools/build-windows-kit.py --check` fails if the Windows
  kit falls behind, so the two cannot silently disagree.
