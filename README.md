<div align="center">
  <a href="https://esoexe.github.io/hyprland-monochrome/">
    <img src="docs/assets/hero-banner.svg" width="100%" alt="hyprland-monochrome : Pure monochrome Wayland desktop" />
  </a>

  <br />

  <p>
    <a href="https://esoexe.github.io/hyprland-monochrome/"><img src="docs/assets/btn-live-site.svg" height="36" alt="Launch Live Website" /></a>&nbsp;&nbsp;
    <a href="https://esoexe.github.io/hyprland-monochrome/newtab.html"><img src="docs/assets/btn-startpage.svg" height="36" alt="Open Browser Startpage" /></a>&nbsp;&nbsp;
    <a href="https://github.com/esoexe/hyprland-monochrome/releases/latest"><img src="docs/assets/btn-release.svg" height="36" alt="Download Release v5.3.0" /></a>
  </p>

  <p>
    <a href="https://github.com/esoexe/hyprland-monochrome/actions/workflows/ci.yml"><img src="https://github.com/esoexe/hyprland-monochrome/actions/workflows/ci.yml/badge.svg" alt="CI" /></a>
    <a href="https://esoexe.github.io/hyprland-monochrome/"><img src="https://img.shields.io/badge/live_simulator-online-fdfcfc?style=flat&labelColor=131010&color=e6e6e6" alt="Live Website" /></a>
    <a href="https://github.com/esoexe/hyprland-monochrome/releases"><img src="https://img.shields.io/github/v/release/esoexe/hyprland-monochrome?color=201d1d&labelColor=0d0d0d" alt="Release" /></a>
    <img src="https://img.shields.io/badge/core-Pure_C11-fdfcfc?style=flat&labelColor=131010&color=e6e6e6" alt="Core: Pure C11" />
    <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-e6e6e6?labelColor=0d0d0d" alt="License: MIT" /></a>
  </p>
</div>

A minimalist monochrome **Hyprland 0.56** Wayland desktop environment engineered in **Pure C11**, **Lua 5.4**, and **POSIX Shell** (zero Python, zero C++). Built around strict luminance hierarchy, damped physical spring motion, automatic GPU and display profiling across **Arch, EndeavourOS, CachyOS, Manjaro** (plus Debian/Ubuntu, Fedora, and openSUSE), universal application routing, and a cohesive `1px` structural browser suite.

---

## Live Compositor Overview

<p align="center">
  <a href="https://esoexe.github.io/hyprland-monochrome/#simulator">
    <img src="docs/assets/compositor-live.svg" width="100%" alt="Live animated 4-workspace Hyprland 0.56 compositor simulation" />
  </a>
</p>

<p align="center">
  <img src="docs/assets/pipeline-strip.svg" width="100%" alt="Four-stage hardware, compositor, runtime, and browser architecture" />
</p>

<p align="center">
  <a href="https://esoexe.github.io/hyprland-monochrome/#input-studio">
    <img src="docs/assets/cursor-keyboard-live.svg" width="100%" alt="Live animated Bespoke Monochrome Cursor Suite and Keyboard Type Studio" />
  </a>
</p>

---

## Recorded Wayland Session &amp; Kinetic Wallpapers

<p align="center">
  <a href="docs/official-trailer.mp4">
    <img src="docs/assets/desktop-session.gif" width="100%" alt="Hyprland Monochrome Official Trailer and Recorded Wayland Session" />
  </a>
  <br />
  <sub>Click above to watch the 1080p 60 FPS Stereo Official Launch Trailer (<a href="docs/official-trailer.mp4"><code>docs/official-trailer.mp4</code></a>), view the full 2m17s compositor walkthrough (<a href="docs/desktop-demo.mp4"><code>docs/desktop-demo.mp4</code></a>), or explore the live browser simulator at <a href="https://esoexe.github.io/hyprland-monochrome/">esoexe.github.io/hyprland-monochrome</a>.</sub>
</p>

| Orbital Rings (`mono-live-01`) | Harmonic Pulse (`mono-live-02`) | Lissajous Scope (`mono-live-03`) |
| :---: | :---: | :---: |
| <img src="wallpapers/mono-live-01-orbital.gif" width="100%" alt="Orbital Rings live loop" /> | <img src="wallpapers/mono-live-02-pulse.gif" width="100%" alt="Harmonic Pulse live loop" /> | <img src="wallpapers/mono-live-03-lissajous.gif" width="100%" alt="Lissajous Scope live loop" /> |
| **Solar Eclipse (`mono-07`)** | **Liquid Mercury (`mono-12`)** | **Bauhaus Compass (`mono-14`)** |
| <img src="wallpapers/mono-07.gif" width="100%" alt="Solar Eclipse live loop" /> | <img src="wallpapers/mono-12.gif" width="100%" alt="Liquid Mercury live loop" /> | <img src="wallpapers/mono-14.gif" width="100%" alt="Bauhaus Compass live loop" /> |

> **100% Live Kinetic Wallpaper Collection (`20 / 20`)** : Every single composition in `wallpapers/` (`mono-01.gif` through `mono-14.gif`, `mono-live-01` through `03`, and the 3 widescreen `Cinema` loops) is a seamless multi-frame Pure C11 animated loop streamed at zero idle CPU cost by `awww`.

---

## Core Capabilities

| Subsystem | What It Delivers |
| --- | --- |
| **Bespoke Monochrome Cursor Suite (`mono-cursor-gen.c` + `cursor.sh`)** | Pure C11 cursor compiler producing **4 custom monochrome cursor themes** (`Monochrome-Obsidian`, `Monochrome-Ivory`, `Monochrome-Precision` CAD reticle, and `Monochrome-Dot` concentric halo) in **both** native vector `hyprcursor` (`.hlc` packages with scalable 1:1 SVGs and 8-frame orbital `watch.hlc`) and 4x4-supersampled `24/32/48px` `Xcursor` binaries, synced across Hyprland, GTK3/4, Qt, X11, and Kitty (`Super + Alt + C` cycles, `Super + Ctrl + C` opens Cursor Studio). |
| **Keyboard Type &amp; Keymap Studio (`keyboard.sh`)** | Live hardware form-factor profiles (**ANSI 104**, **ISO 105**, **60% / HHKB** `caps:escape`, **Vim** `caps:swapescape`, **Home-Row Control** `ctrl:nocaps`, **Apple** `altwin:swap_lalt_lwin`, **Laptop**), ergonomic and international XKB layouts (`QWERTY`, `Colemak`, `Colemak-DH`, `Dvorak`, `Workman`, `AZERTY`, `QWERTZ`, dual-layout toggles), 3 repeat-physics curves (`38Hz`/`55Hz`/`25Hz`), and a live Waybar layout pill (`Super + X` switches, `Super + Shift + X` opens Keyboard Studio). |
| **Adaptive Hardware Engine (`hardware.lua` + `hardware.sh`)** | Sub-millisecond `/sys` and `/proc` startup inspection that configures **AMD Radeon**, **Intel Iris/Arc**, **NVIDIA RTX/GTX** (`nvidia-drm`, `nvidia_anti_flicker`, `no_hardware_cursors = 2`), **Hybrid Optimus laptops** (`AQ_DRM_DEVICES`, `AQ_FORCE_LINEAR_BLIT=0`), and **Virtual Machines** (QEMU/KVM/VMware/VirtualBox software fallback), alongside crisp zero-blur HiDPI XWayland scaling and multi-distro installation (`pacman`, `apt-get`, `dnf`, `zypper`). |
| **Universal Application Layer (`open-app.sh`)** | Seamless launch and rendering across native Wayland, X11/XWayland, Electron (`ELECTRON_OZONE_PLATFORM_HINT=auto`), Qt5/6, GTK3/4, SDL2 (`wayland,x11`), Java Swing and JetBrains IDEs (`_JAVA_AWT_WM_NONEREPARENTING=1`), **Flatpak**, **Snap**, **Nix**, auto-indexed **`.AppImage`** binaries (with FUSE-less `--appimage-extract-and-run` fallback), Windows `.exe`/`.msi` via Wine/Bottles, and `Super + Shift + O` universal file/URL dispatch. |
| **Monochrome Browser Suite (`browser-theme.sh`)** | Cohesive `1px` structural grid browser chrome (`#131010` surface, `#201d1d` elevated prompt, `#3d3838` dividers, `4px` corners, 100% monospace chrome): automated profile setup for **Firefox / Zen / LibreWolf / Floorp** (`userChrome.css`, `userContent.css`, Wayland prefs), **Chromium / Brave / Chrome / Vivaldi** Manifest V3 theme and new-tab extension, **Qutebrowser** palette, and offline **`startpage.html`** (`Alt + B`). |
| **Spring Physics &amp; Optical Glass (`animations.lua`, `appearance.lua`)** | Three damped physical spring curves (`silkSpring`, `drawerSpring`, `snappySpring`) driving 29 animation targets across windows, workspace parallax, and scratchpad drawers. Continuous G2 squircle geometry (`rounding_power = 4.0`), magnetic edge snapping, `direct_scanout`, and switchable optical glass (`kawase`, `xray`, `acrylic`) plus `Super + F12` low-latency **Direct Scanout / Game Mode**. |
| **Real-Time GLSL Shaders (`shader.sh`)** | Five GPU post-processing shaders (`Super + ,` or `Super + M` then `S`): **Noir** (Rec.709 hardware luminance), **35mm Film Grain**, **P4 Studio CRT**, **Matte E-Ink**, and **OLED Pitch** (true-black pixel power saving and micro-contrast). |
| **Pure C11 Terminal Control Deck (`mono-tui.c` + `tui.sh`)** | `Super + Ctrl + M` opens a zero-dependency POSIX `termios` 6-tab live control center (`TELEMETRY`, `VELOCITY`, `SHADERS`, `INPUT+CURSOR`, `WALLPAPERS`, `BENCHMARK`) with per-core `/proc/stat` waveforms, `/sys/bus/pci` GPU topology, `CLOCK_MONOTONIC` microsecond profiling (`~50 µs`), and headless `--snapshot` / `--json` / `--bench` CLI modes. |
| **Pure C11 Hyprland IPC &amp; Telemetry (`mono-ipc.c`)** | Native `<0.08ms` `AF_UNIX` socket client for `$XDG_RUNTIME_DIR/hypr/$HIS/.socket.sock` and real-time `.socket2.sock` event streaming (`activelayout`, `screencast`, `monitoradded`), plus RFC-8259 JSON parsing, `/proc` Waybar sparkline telemetry (`sysgraph`), keybind cheat-sheet formatting, and workspace session serialization with **zero `jq` subprocesses**. |
| **Command Palette &amp; Native `mono-calc`** | `Super + Shift + P` (or `Super + ;`) opens an omnibar for jumping to active windows, switching motion physics or GLSL shaders, toggling named drawers, managing workspace snapshots, running system diagnostics, and evaluating math expressions via the compiled Pure C11 `mono-calc` engine. |
| **Full-Stack Greyscale (`R == G == B`)** | Unified greyscale theming across all 7 desktop toolkits: GTK3/GTK4, Qt5/Qt6 (`qt5ct`/`qt6ct` 21-slot Fusion palette), zero-plugin Neovim (`monochrome.lua`), Kitty (`1ms` GPU cursor trail), Rofi, Waybar, Dunst OSDs, `hyprlock`, and optional Spicetify monochrome chrome. |
| **Capture, Replay &amp; Screen OCR (`record.sh`, `screenshot.sh`)** | `Super + C` opens the `CAPTURE` chord (`s` region, `f` full screen, `w` active window, `o` Tesseract region OCR to clipboard, `e` Satty/Swappy vector annotation, `r` start/stop recording, `p` save 60s instant replay) powered by `gpu-screen-recorder` with automatic `wf-recorder` fallback. |
| **Procedural Pure C11 Live Wallpapers** | `Super + W` (120 FPS `awww` transitions) and `Alt + W` (cycle live loops): **all 20 procedural Pure C11 monochrome compositions** (`mono-01.gif`..`mono-14.gif`, `Orbital Rings`, `Harmonic Pulse`, `Lissajous Scope`, and 3 widescreen `Cinema` masters) ship as seamless zero-drift animated GIF89a loops. |

---

## Quick Install

```bash
git clone https://github.com/esoexe/hyprland-monochrome.git
cd hyprland-monochrome
./install.sh
```

What `./install.sh` performs:

1. Detects your Linux distribution (`Arch`, `EndeavourOS`, `CachyOS`, `Manjaro`, or `Debian`/`Ubuntu`/`Fedora`/`openSUSE`) and verifies compositor prerequisites.
2. Installs required packages from official repositories (`packages.txt`) and offers optional extras (`aur-packages.txt`) when `paru` or `yay` is present.
3. **Backs up every existing file before touching anything** (preserving a permanent `pristine/` snapshot of your original setup).
4. Installs `~/.config` modules (or symlinks them with `--link`), compiles native Pure C11 utilities, copies wallpapers, applies browser theming, and reloads Hyprland cleanly.

```text
./install.sh --help          list all installer flags
./install.sh --yes           non-interactive install
./install.sh --no-packages   deploy configuration files only
./install.sh --link          symlink configs into ~/.config for live git workflow
./install.sh --dry-run       preview every action without modifying disk
./uninstall.sh               remove installed configs and restore originals
```

### Backups &amp; Instant Rollback

```text
~/.config-backups/pristine/     exact snapshot prior to the very first install (never overwritten)
~/.config-backups/<timestamp>/  incremental backup for each subsequent run with restore.sh
```

```bash
~/.config-backups/pristine/restore.sh     # restore original files directly
./uninstall.sh                            # full uninstall + automatic restoration
./uninstall.sh --list                     # list available backup snapshots
```

---

## Essential Keybinds

`Super` is the primary modifier (configurable in `.config/hypr/modules/user.lua`). Press `Super + /` on the desktop or browse **[docs/KEYBINDS.md](docs/KEYBINDS.md)** (and the [live searchable table](https://esoexe.github.io/hyprland-monochrome/#keybinds)) for all **176** shortcuts.

| Keybind | Action | Keybind | Action |
| --- | --- | --- | --- |
| `Super + T` / `Super + Enter` | Kitty terminal | `Super + Shift + P` / `Super + ;` | Command palette &amp; `mono-calc` |
| `Super + D` | Application launcher | `Super + Shift + O` | Universal file / URL / AppImage opener |
| `Super + X` / `Super + Shift + X` | Switch layout / Keyboard Studio | `Super + Alt + C` / `Super + Ctrl + C` | Cycle cursor / Cursor Design Studio |
| `Super + E` | File manager (`Thunar`) | `Super + Z` / `Super + Shift + Z` | Zen mode / Pomodoro focus timer |
| `Super + B` / `Alt + B` | Browser / Monochrome startpage | `Super + R` | `RESIZE` chord (`h/j/k/l`, `1-3/0` gaps) |
| `Super + I` | System settings menu | `Super + C` | `CAPTURE` chord (`s/f/w/o/e/r/p/m`) |
| `Super + W` / `Alt + W` | Wallpaper picker / Live loop | `Super + M` | `FX` chord (spring physics &amp; shaders) |
| `Super + ,` / `Super + .` | Cycle GLSL shader / PiP snap | `Super + \` / `Super + Shift + \` | Cycle tiling layout / gap presets |
| `Super + Shift + T` | Quake dropdown terminal | `Super + Shift + B` | Floating `btop++` telemetry drawer |
| `Super + '` | Scratchpad Markdown notes | `Super + Ctrl + S` | `SCRATCH` &amp; workspace session chord |
| `Super + V` | Clipboard history (`cliphist`) | `Super + F` / `Super + Space` | Fullscreen / Centered floating |
| `Super + Shift + W` | Toggle top Waybar | `Super + F12` | Direct Scanout / low-latency mode |
| `Super + Print` | Region screenshot | `Super + Alt + Print` | Start / stop screen recording |
| `Super + Tab` | Lock screen (`hyprlock`) | `Super + Escape` | Session power menu |

Window focus uses `Super + H/J/K/L` (or arrow keys), moving uses `Super + Shift + H/J/K/L`, resizing uses `Super + Ctrl + H/J/K/L`, and workspaces switch with `Super + 1..0` or `Super + Scroll`.

---

## Repository &amp; Configuration Architecture

```text
hyprland-monochrome/
|-- .config/hypr/
|   |-- hyprland.lua            entry point loading modular Lua configuration
|   |-- hyprlock.conf           minimal monochrome lock screen
|   |-- hypridle.conf           dim -> lock -> DPMS off -> suspend pipeline
|   |-- hyprsunset.conf         scheduled night-light colour temperature
|   |-- modules/
|   |   |-- user.lua            single source of truth for apps, gaps, layout & preferences
|   |   |-- hardware.lua        GPU, DRM, cursor, VRR & XWayland auto-configuration
|   |   |-- monitors.lua        automatic display scaling & multi-head layout
|   |   |-- appearance.lua      G2 squircles, borders, optical glass & trackpad gestures
|   |   |-- animations.lua      3 physical spring curves + 29 animation targets
|   |   |-- binds.lua           176 shortcuts + RESIZE / CAPTURE / SCRATCH / FX / GROUP chords
|   |   |-- rules.lua           universal Wayland, X11, Electron, Java & Steam window rules
|   |   `-- smart.lua           reactive hl.on() hooks (screenshare DND, monitor hotplug)
|   |-- shaders/                5 GLSL ES 3.00 post-processing shaders
|   |-- src/                    native Pure C11 mono-tui, mono-ipc, mono-calc + mono-cursor-gen compilers
|   `-- scripts/                compositor runtime scripts (cursor, keyboard, hardware, open-app)
|-- cursors/                    4 prebuilt monochrome cursor suites (hyprcursor .hlc + Xcursor)
|-- site/                       live 60 FPS interactive website + Pure C11 HTTP server (serve-site)
|-- wallpapers/                 Pure C11 procedural kinetic engine + all 20 seamless live animated GIF loops
|-- tools/                      Pure C11 build, keybind sync, verification & static server tools
`-- windows/                    matching GlazeWM + Windows Terminal dual-boot kit
```

---

## Customization &amp; Local Development

Edit **`~/.config/hypr/modules/user.lua`** to customize default applications, outer/inner gaps, blur, keyboard layouts, or motion profiles in one place:

```lua
-- ~/.config/hypr/modules/user.lua
browser      = "zen-browser",   -- auto-falls back across installed browsers if absent
gaps_out     = 14,
blur_enabled = true,
kb_layout    = "us",            -- Super + X switches active layouts
```

Build the native Pure C11 tools, run the verification suite, or launch the interactive website locally:

```bash
make -j"$(nproc)"               # compile all Pure C11 binaries (-std=c11 -O3 -Wall -Wextra -Werror)
make docs                       # sync docs/KEYBINDS.md, site/keybinds.json, windows/* & preview
make test                       # run full 10-suite verification harness
make serve                      # serve the live animated website at http://0.0.0.0:8080
```
