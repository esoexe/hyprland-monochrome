# Install

## Requirements

* An **Arch-based distribution**: Arch, EndeavourOS, CachyOS, Manjaro, Garuda, Arco, Artix…
* **Hyprland 0.55 or newer** — this setup uses the Lua configuration format.
  Check with `hyprctl version`. If you are on 0.54 or older, update first:
  `sudo pacman -Syu hyprland`.
* A working **GPU driver**, `networkmanager` and **PipeWire** (all in `packages.txt`).
* ~2 GB of free space for the packages, and `git` + `base-devel` to build AUR extras.

Nothing from the AUR is required. The four optional extras are listed in
`aur-packages.txt`.

## Quick install

```bash
git clone https://github.com/esoexe/hyprland-monochrome.git
cd hyprland-monochrome
./install.sh
```

Read the summary it prints, log out, and pick **Hyprland** in your display manager
(or run `Hyprland` from a TTY).

### Installer options

| Option | What it does |
| --- | --- |
| `--yes` | Answer every question with yes (unattended installs) |
| `--no-packages` | Skip package installation, only install the config |
| `--no-aur` | Do not install the optional AUR extras |
| `--no-backup` | Do not back anything up (**destructive** — think first) |
| `--link` | Symlink the config instead of copying: the repo stays the source of truth |
| `--no-zsh` | Do not offer to change your default shell |
| `--dry-run` | Print what would happen, change nothing |

`--link` is the nicest option if you intend to customise: edit `~/.config/hypr/...`, and the
edit lands in the repository — then commit it and you have your own versioned dotfiles.

## What the installer touches

| Path | What happens |
| --- | --- |
| `~/.config/{hypr,waybar,rofi,kitty,dunst,wlogout,btop,fastfetch,gtk-3.0,gtk-4.0,spicetify}` | copied/overwritten (old versions backed up) |
| `~/.zshrc`, `~/.config/starship.toml` | copied (old versions backed up) |
| `~/Pictures/Wallpapers` | the bundled wallpapers are copied in; existing files are left alone |
| `~/Pictures/Screenshots` | created |
| `~/.config-backups/` | backups + restore scripts |
| shell, Bluetooth service | asked about, never changed silently |

It does **not** touch your display manager configuration, your `fstab`, `mkinitcpio`,
`pacman.conf`, or anything system-wide other than installing packages you approved.

A leftover `~/.config/hypr/hyprland.conf` (the old hyprlang format) is backed up and then
left alone — Hyprland ignores it now that `hyprland.lua` exists. Port anything you need
from it into `~/.config/hypr/modules/*.lua`.

## Running Hyprland

### Plain TTY

```bash
Hyprland
```

### With a display manager

Pick the *Hyprland* session at login. Works with GDM, SDDM, LightDM, ly and greetd.
If the session does not appear, make sure `/usr/share/wayland-sessions/hyprland.desktop`
exists (it ships with the `hyprland` package).

### With uwsm (recommended for systemd users)

`uwsm` gives you a proper user session with services you can manage with `systemctl --user`:

```bash
sudo pacman -S uwsm
```

Add or edit `/usr/share/wayland-sessions/hyprland-uwsm.desktop` according to the
[Hyprland wiki](https://wiki.hypr.land/Useful-Utilities/Systemd-start/), then log in through
that session. This setup works either way: the scripts detect `uwsm` for logout
(`scripts/session.sh quit` uses `uwsm stop` when it is running).

With uwsm, environment variables belong in `~/.config/uwsm/env` instead of
`hyprland.lua` — the `hl.env(...)` block near the top of `hyprland.lua` is then redundant
and can be deleted.

## Manual install

If you would rather do it by hand:

```bash
# 1. packages
sudo pacman -S --needed - < packages.txt

# 2. config (back up first!)
cp -a ~/.config/hypr ~/.config/hypr.bak 2>/dev/null
cp -a .config/* ~/.config/
cp .zshrc ~/.zshrc

# 3. scripts must be executable
chmod +x ~/.config/hypr/scripts/*.sh ~/.config/waybar/scripts/*.sh

# 4. wallpapers
mkdir -p ~/Pictures/Wallpapers
cp wallpapers/*.jpg ~/Pictures/Wallpapers/

# 5. reload
hyprctl reload
```

## After installing

1. **Log out and back in** — Hyprland reads its config at startup.
2. Check that the bar, wallpaper and notifications appear.
3. `Super + /` to see the keybinds, `Super + I` for settings.
4. Open `~/.config/hypr/modules/user.lua` and set your browser if the installer guessed
   wrong.
5. Optional: give the monochrome theme a full pass — Papirus-Dark icons and a dark GTK
   theme are configured in `~/.config/gtk-3.0/settings.ini`. `nwg-look` (installed) is a
   GUI for that. For grey folder icons: `papirus-folders -C grey --theme Papirus-Dark`.

## Updating

```bash
# config from the repo
cd hyprland-monochrome && git pull
./install.sh --no-packages        # copies the new files (backups are safe)

# or, with --link, the pull already updated you
git pull && hyprctl reload

# packages
sudo pacman -Syu
```

Anything you changed by hand gets backed up before being replaced, so a `git pull` +
re-install never loses your edits — but `--link` + `git` is the cleanest way to keep
your own changes.

## Uninstalling

```bash
./uninstall.sh                  # remove the config, restore your originals
./uninstall.sh --list           # what backups exist
./uninstall.sh --from <dir>     # restore one specific backup
./uninstall.sh --keep-files     # only restore the backups, keep the new config
```

Packages are never removed automatically. `packages.txt` is the list if you want to clean up
by hand, and the wallpapers in `~/Pictures/Wallpapers` are left where they are.

## Testing without breaking anything

```bash
./test/smoke-test.sh            # runs every script against stubbed tools
```

It creates a temporary `$HOME`, stubs `hyprctl`, `rofi`, `awww`, `nmcli`, `wpctl` and
friends, and checks that the menus, pickers and helpers do the right thing. Nothing outside
that temporary directory is touched.

You can also start a nested Hyprland session to try things out:

```bash
Hyprland --nested --wayland-client
```

…and point it at another config:

```bash
Hyprland --config ~/hyprland-monochrome/.config/hypr/hyprland.lua
```
