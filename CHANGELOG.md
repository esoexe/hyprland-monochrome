# Changelog

Notable changes to this setup. Versions are the dates they landed.

## 2026-11-06 — 2.0.0

Major visual-craft and productivity architecture upgrade.

**Visual Craft & Monochrome Purity**
- **4 new studio-grade procedural wallpapers** in `wallpapers/generate.py` (`mono-07` Solar Eclipse + 4K master, `mono-08` Alpine Ridgeline, `mono-09` Brutalist Monoliths, `mono-10` Caustic Silk Ribbon), bringing the collection to 10 resolution-independent monochrome designs (up to 16K)
- **GTK3 & GTK4 monochrome CSS overrides** (`.config/gtk-3.0/gtk.css`, `.config/gtk-4.0/gtk.css`): replaces Adwaita-dark's blue (`#3584e4`) accents and warm grey panels with pure obsidian (`#0c0c0c`) and inverted silver (`#e6e6e6` / `#0a0a0a`) selections across Thunar and file dialogs
- **Monochrome Hyprland groupbar** in `modules/appearance.lua`: replaces Hyprland's default yellow/orange tab bars with obsidian/silver pill gradients + `scrolling` column width tuning
- **Refined Waybar & Rofi UI**: elevated frosted-glass contrast, live inverted silver Waybar pills for active Pomodoro timers (`custom/focus`) and modal chords (`hyprland/submap`), new 740px `palette.rasi`, and enabled contextual `-mesg` status/hint banners across all Rofi modals

**Advanced Productivity Architecture**
- **Command Palette (`scripts/palette.sh`, `Super + Shift + P` / `Super + ;`)**: unified omnibar combining cross-workspace open-window jumping, Focus/Zen controls, layout & gap presets, capture/OCR, config quick-edit, and a safe Python-AST live math/hex/scientific calculator (`= ...`)
- **Deep-Work Focus & Zen Engine (`scripts/focus.sh`, `Super + Z` / `Super + Shift + Z`)**: zero-polling epoch Pomodoro sprints (`start`, `pause`, `stop`, Waybar JSON `RTMIN+9` signal) + one-key **Zen Mode** (saves/restores Waybar, expands outer framing to 28px, dims background windows, pauses Dunst notifications)
- **Dynamic Layout & Gap-Preset Switcher (`scripts/layout.sh`, `Super + \` / `Super + Shift + \`)**: live switching across `dwindle`, `master`, and `scrolling` layouts plus `default`, `compact`, `airy`, and `zero` monocle gap presets
- **Modal Chord Submaps (`modules/binds.lua`)**: `Super + R` enters `RESIZE` mode (`h/j/k/l`, `1/2/3/0` gap presets, `Esc`) and `Super + C` enters single-chord `CAPTURE` mode (`s/f/w/o/e/r/p/m`)
- **Screen-Region OCR & Annotation (`scripts/screenshot.sh ocr` / `edit`)**: extracts text on screen via `slurp` + `grim` + `tesseract` straight to `wl-copy`, plus `satty`/`swappy` annotation hook; `menu.sh keybinds` (`Super + /`) now executes selected shortcuts directly on `Enter`
- **Interactive Preview & Verification**: `docs/preview.html` now embeds an interactive 10-wallpaper studio switcher and live 117-keybind filter; `test/smoke-test.sh` expanded from 100 to **122 automated checks**

## 2026-09-23 — 1.0.0

First release.

**Desktop**
- Hyprland **Lua** configuration (0.55+), split into `modules/{user,monitors,appearance,autostart,binds,rules}.lua`
- Monochrome look: grey-on-black borders, subtle blur, soft animations, uncluttered tiling
- 60+ keybinds covering terminal, launcher, files, browser, settings, wallpapers, clipboard,
  lock, logout, fullscreen, floating, bar toggle, workspaces, scratchpad and a dropdown terminal
- Window rules for floating tools, per-app opacity, picture-in-picture, games, dialogs

**Tools**
- Waybar: pill groups, workspaces, now-playing, CPU/RAM/temp, updates, network, Bluetooth,
  volume, battery, tray, clock with calendar
- Rofi on one shared theme: app launcher, window switcher, clipboard history, wallpaper grid,
  settings menu, power menu, and a searchable keybind cheat sheet built from `hyprctl binds`
- Settings menu: system info, Network (nmcli), Bluetooth (bluetoothctl), Monitors (writes a real
  config file), Sound (wpctl/pactl, per-app volume), night shift, config file shortcuts
- Wallpaper picker on awww with thumbnails, random/next/prev, solid colours and restore-on-login
- Screen recording (`Super + Alt + Print`): whole screen or a selected area, audio as
  desktop/mic/both, pause, and an instant-replay buffer that saves the last minute — on
  gpu-screen-recorder, with wf-recorder as a no-special-requirements fallback
- Recording indicator in the bar (timer while recording, `▮▮` paused, `● buf` replaying)
- hyprlock lock screen, Rofi power menu with confirmations, optional wlogout grid
- hypridle (dim → lock → screen off → suspend) and hyprsunset (automatic blue-light filter)
- Kitty, Thunar (+ tumbler), Zsh with Starship, dunst, cliphist

**Wallpapers**
- `mono-06` available as a native **16K** master (15360×8640) for large or HiDPI screens
- `wallpapers/generate.py` is now resolution-independent: every design scales with the canvas,
  so larger sizes add real detail instead of upscaling
- Fixed `--only <n>` silently matching nothing unless you zero-padded the number
- New `--suffix` and `--quality` flags; 6 more smoke-test checks cover the generator

**Theming**
- Monochrome palettes for Kitty, btop, fastfetch, dunst, Waybar, Rofi, Starship and GTK
- Optional Spicetify theme for Spotify (`scripts/spotify-theme.sh`)
- Six generated monochrome wallpapers, regenerable at any resolution (`wallpapers/generate.py`)

**Install & docs**
- `install.sh` with package detection, AUR extras, dry-run, `--link`, and double backups
  (`pristine` = your originals kept forever, plus a per-run folder with `restore.sh`)
- `uninstall.sh` that removes the config and puts your files back
- Documentation: QUICKSTART, INSTALL, KEYBINDS (generated), CUSTOMIZE, TROUBLESHOOTING, plus this changelog
- Windows companion: `windows/` (GlazeWM config + Windows Terminal scheme) generated from the
  same palette by `tools/build-windows-kit.py`, plus `docs/ON-WINDOWS.md` covering Windows,
  WSL2 and per-distro Linux installs
- `docs/desktop-demo.mp4` (`tools/record-desktop.sh`): live 1920×1080 Wayland session recording
  showing Waybar, Rofi menus, 16K wallpapers, Thunar, Dunst OSD, `record.sh` (`wf-recorder`)
  and the test suite running end-to-end
- `docs/test-run.mp4`: the CLI test suite, install sandbox and screen recording caught on
  video by `tools/demo.sh` (recorded with asciinema, rendered in this project's palette)
- `docs/preview.html`: a self-contained preview page built from the config itself
  (`tools/build-preview.py`) — bar module order, keybinds, every palette, package list
- Tests: `test/smoke-test.sh` (100 checks against stubbed tools) and `test/run-all.sh`
  (9 checks, including TOML/JSON validation and drift detection for generated docs, Windows kit and preview page)
