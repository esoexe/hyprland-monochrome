# Contributing

Small, focused changes are very welcome — a new keybind, a Waybar module, a fix for a
machine-specific quirk, a clearer comment.

## Before you open a pull request

```bash
make test                      # C++20 build + shellcheck + Lua + JSON/TOML + smoke test + docs
make docs                      # refresh docs/KEYBINDS.md, windows/* & docs/preview.html
```

Both must be clean. `test/run-all.sh` prints exactly what is wrong if something fails.

## Where things live

| Change | File |
| --- | --- |
| A keybind | `.config/hypr/modules/binds.lua` (then regenerate the docs) |
| Programs, gaps, keyboard, paths | `.config/hypr/modules/user.lua` |
| Colours, blur, animations, input | `.config/hypr/modules/appearance.lua` |
| Window rules | `.config/hypr/modules/rules.lua` |
| Autostart | `.config/hypr/modules/autostart.lua` |
| A menu / helper behaviour | `.config/hypr/scripts/…` |
| The installer | `install.sh` / `uninstall.sh` (test with `./test/install-sandbox.sh`) |
| Bar layout or styling | `.config/waybar/config.jsonc`, `style.css` |
| Menu look | `.config/rofi/*.rasi` |
| Packages | `packages.txt` (official repos only) / `aur-packages.txt` (optional) |

## House rules

1. **Arch repo packages only** in `packages.txt`. Anything from the AUR goes in
   `aur-packages.txt` and must be genuinely optional.
2. **Keep the monochrome idea.** Greys and opacity, not colour; if you add a colour, make it
   a clearly commented one-liner someone can change.
3. **Comment the why, not the what.** One short line above each keybind, window rule and
   non-obvious shell block.
4. **Never break the installer.** `install.sh` must stay idempotent and must always back up
   before replacing.
5. **No blocking code in Hyprland bind callbacks.** Use `hl.dsp.exec_cmd("…script.sh")`
   instead of `io.popen`, `wl-paste` or `sleep` inside a Lua function — it freezes the
   compositor's event loop.
6. **Test what you can.** New shell scripts get a case in `test/smoke-test.sh`; new binds get
   a description so the cheat sheet and `hyprctl binds` stay useful.

## Reporting a problem

Include the output of:

```bash
hyprctl version
hyprctl configerrors
hyprctl monitors
cat /etc/os-release | head -3
```

…and, for script problems, `bash -x ~/.config/hypr/scripts/<script>.sh`.
See `docs/TROUBLESHOOTING.md` first — most issues are covered there.
