# Quickstart

From nothing to a running Hyprland desktop, in about five minutes. This page is the short
path; the details live in [INSTALL.md](INSTALL.md) and [CUSTOMIZE.md](CUSTOMIZE.md).

---

## 0. What you need

| | |
| --- | --- |
| **Distro** | Arch, EndeavourOS, CachyOS or Manjaro (anything with `pacman`) |
| **Hyprland** | **0.55 or newer** — this setup uses the Lua config format, which older versions cannot read |
| **User** | a normal account with `sudo` (the installer refuses to run as root) |
| **Graphics** | any GPU that runs Hyprland at all. Nouveau/older NVIDIA users: read the NVIDIA note in the README |

Check Hyprland's version, and the rest as you go:

```bash
pacman -Si hyprland | grep -i version        # or, once installed:  hyprctl version
```

**Not on an Arch-based distro?** The *config* is portable — Hyprland is the same everywhere.
Only `install.sh` is Arch-specific. Copy `.config/` into `~/.config/` and `.zshrc` into `$HOME`
by hand, then install the packages your distro names differently. That path is at the end of
[INSTALL.md](INSTALL.md).

---

## 1. Get the files onto your machine

Move the project folder (or `hyprland-monochrome-<version>.tar.gz`) to your Arch machine,
then:

```bash
tar xzf hyprland-monochrome-1.0.0.tar.gz     # if you have the tarball
cd hyprland-monochrome
```

Want it in git, so you can diff your own edits later? `git init && git add -A && git commit -m "my setup"`.

---

## 2. Look before you leap (30 seconds)

```bash
bash test/run-all.sh        # syntax, shellcheck, Lua, JSON, 90 script checks
./install.sh --dry-run      # print exactly what would be touched, change nothing
```

Neither command modifies your system. If `run-all.sh` fails on a brand-new checkout, tell me —
that is the point of it.

---

## 3. Install

```bash
./install.sh
```

It will:

1. check you are on an Arch-based system, not root, and on Hyprland ≥ 0.55,
2. install everything in `packages.txt` from the official repos (with one `sudo pacman` call),
3. offer the AUR extras (wlogout, Spicetify, Papirus folders, Bibata cursors) via `paru`/`yay` if you have one,
4. **back up anything it would overwrite** — twice: `~/.config-backups/pristine/` holds your
   originals forever, plus a timestamped folder per run, each with its own `restore.sh`,
5. copy the config into `~/.config/`, the wallpapers into `~/Pictures/Wallpapers/`, and `.zshrc` into `$HOME`,
6. point the `browser` setting at whichever browser you actually have installed.

Useful variants:

| Command | What it does |
| --- | --- |
| `./install.sh --dry-run` | show the plan, touch nothing |
| `./install.sh --yes` | no questions asked |
| `./install.sh --no-packages` | config only — you install the packages yourself |
| `./install.sh --no-aur` | skip the AUR extras |
| `./install.sh --link` | symlink the config back to this folder, so edits here are live |
| `./install.sh --no-zsh` | leave your shell alone |

---

## 4. Log in

Log out, then pick the **Hyprland** session at the login screen (uwsm, SDDM, GDM and ly are all
covered in [INSTALL.md](INSTALL.md)). Log in and you should see: wallpaper, the monochrome bar
at the top, and `Super + /` works.

First five minutes, in order:

| Press | What to check |
| --- | --- |
| `Super + /` | the searchable keybind cheat sheet — everything else is in there |
| `Super + W` | pick one of the six bundled wallpapers (or drop your own in `~/Pictures/Wallpapers/`) |
| `Super + I` → **Monitors** | correct resolution/scale/position if it guessed wrong (it writes a real config file) |
| `Super + I` → **Network**, **Bluetooth**, **Sound** | connect, pair, set volume |
| `Super + T` (or `Enter`) | terminal — Kitty, Zsh, Starship prompt |

---

## 5. The twelve keybinds you will actually use

| | | | |
| --- | --- | --- | --- |
| `Super + D` | app launcher | `Super + V` | clipboard history |
| `Super + T` / `Enter` | terminal | `Super + E` | file manager |
| `Super + Shift + T` | dropdown terminal | `Super + B` | browser |
| `Super + 1..0` | switch workspace | `Super + Shift + 1..0` | move window there |
| `Super + H/J/K/L` | focus window | `Super + Space` | float + centre |
| `Super + F` | fullscreen | `Super + W` | wallpaper picker |
| `Super + I` | settings menu | `Super + Escape` | logout menu |
| `Super + Print` | screenshot (area) | `Super + Alt + Print` | record the screen |
| `Super + Tab` | lock screen | `Super + Shift + W` | hide the bar |

All 82 are in [KEYBINDS.md](KEYBINDS.md), and `Super + /` shows them live from your own config.

---

## 6. Make it yours

Ninety per cent of personalisation is one file — `~/.config/hypr/modules/user.lua`:

```lua
mainMod      = "SUPER",      -- or ALT / CTRL
browser      = "brave",      -- any app name
terminal     = "kitty",
gaps_out     = 14,           -- roomier windows
blur_enabled = false,        -- lighter on old GPUs
kb_layout    = "us,ara",     -- Super + X switches
```

Reload with `Super + Shift + R` after editing. Colours, bar modules, menus, monitors and the
shell are covered section by section in [CUSTOMIZE.md](CUSTOMIZE.md). The shell scripts read
their settings from that same file, so you never configure the same thing twice.

---

## 7. Day-to-day

* **Launch things** — `Super + D` (apps), `Super + Shift + D` (switch windows), `Super + E` (files).
* **Screenshots & video** — `Super + Print` area, `Super + Alt + Print` records (press again to
  stop), `Super + Alt + S` saves the last minute, `Super + Alt + P` pauses. Files land in
  `~/Pictures/Screenshots/` and `~/Videos/Recordings/`.
* **Clipboard** — `Super + V`, type to filter, `Super + Shift + V` wipes it.
* **Settings** — `Super + I` for anything system-level, from monitors to wallpapers to sound.
* **Notifications** — top right; the volume/brightness OSD appears bottom centre.

---

## 8. Updating, and getting out

```bash
cd hyprland-monochrome
git pull                     # or unzip a newer copy over this one
./install.sh --no-packages   # re-copy the config; your backups stay intact
```

Something went wrong, or you just want your old desktop back:

```bash
./uninstall.sh               # removes this config and restores your originals
./uninstall.sh --list        # show the backups it can restore from
./uninstall.sh --keep-files  # remove the config, keep your old files untouched
```

---

## 9. When something breaks

1. `Super + Shift + R` reloads the config, `hyprctl configerrors` prints what Hyprland disliked.
2. Emergency exits, if the screen is unusable: `Super + Q` closes the focused window,
   `Super + R` reloads Hyprland — those two are built into Hyprland itself.
3. `docs/TROUBLESHOOTING.md` covers the usual suspects: NVIDIA, no bar, no audio, lock screen,
   portals, fonts showing as boxes, recording.
4. Stuck at a black screen after login? Switch to a TTY (`Ctrl + Alt + F2`), log in, and either
   `./uninstall.sh` or fix the file that broke — the config is plain text and each module is
   guarded, so a typo in one file cannot take the session down.
