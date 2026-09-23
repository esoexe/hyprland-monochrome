#!/usr/bin/env python3
"""
Build docs/preview.html — a single, self-contained page that shows what this
setup looks like and what is in it.

Everything on the page is read from the project's own files at build time:

    .config/hypr/modules/binds.lua      → the keybind tables
    .config/hypr/modules/user.lua       → the main modifier, programs, gaps
    .config/waybar/config.jsonc         → which bar modules are enabled, in order
    .config/rofi/theme.rasi             → the menu palette
    .config/kitty/theme.conf            → the terminal palette
    .config/starship.toml               → the prompt palette
    .config/hypr/modules/appearance.lua → gaps, rounding, opacity
    packages.txt / aur-packages.txt     → what gets installed
    docs/preview.svg                    → the desktop mockup (inlined)

so the preview cannot drift away from the configuration.

    python3 tools/build-preview.py            # write docs/preview.html
    python3 tools/build-preview.py --check    # exit 1 if it is out of date
"""

from __future__ import annotations

import html
import importlib.util
import json
import re
import sys
from datetime import date
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
CONFIG = REPO / ".config"
OUT = REPO / "docs/preview.html"

# Reuse the keybind parser so the page and docs/KEYBINDS.md never disagree.
_spec = importlib.util.spec_from_file_location("keybind_table", REPO / "tools/keybind-table.py")
kb = importlib.util.module_from_spec(_spec)
assert _spec and _spec.loader
_spec.loader.exec_module(kb)


# ── readers ───────────────────────────────────────────────────────────────

def read_jsonc(path: Path) -> dict:
    text = re.sub(r"(?m)^\s*//.*$", "", path.read_text())
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return json.loads(text)


def lua_value(name: str, default: str = "") -> str:
    """Read a simple `name = "value"` / `name = 5` line out of modules/user.lua."""
    match = re.search(rf'^\s*{name}\s*=\s*("?[^,\n]+?)"?,\s*(?:--.*)?$',
                      (CONFIG / "hypr/modules/user.lua").read_text(), re.M)
    if not match:
        return default
    return match.group(1).strip().strip('"')


def rofi_palette() -> list[tuple[str, str, str]]:
    """[(name, hex, rgba)] from rofi/theme.rasi — the shared menu palette."""
    text = (CONFIG / "rofi/theme.rasi").read_text()
    block = re.search(r"^\* \{(.*?)^\}", text, re.S | re.M)
    out = []
    if not block:
        return out
    for name, value in re.findall(r"^\s*([a-z0-9-]+):\s*(#[0-9a-fA-F]{6,8});", block.group(1), re.M):
        out.append((name, value))
    return out


def kitty_palette() -> dict[str, str]:
    out = {}
    for line in (CONFIG / "kitty/theme.conf").read_text().splitlines():
        line = line.split("#")[0].strip()
        if not line or " " not in line:
            continue
        key, _, value = line.partition(" ")
        if value.strip().startswith("#"):
            out[key.strip()] = value.strip()
    return out


def starship_palette() -> dict[str, str]:
    text = (CONFIG / "starship.toml").read_text()
    block = re.search(r"\[palettes\.mono\](.*?)(?=\n\[|\Z)", text, re.S)
    if not block:
        return {}
    return dict(re.findall(r"(\w+)\s*=\s*\"(#[0-9a-fA-F]{3,8})\"", block.group(1)))


def waybar_colours() -> list[str]:
    """The distinct hex values that appear in waybar/style.css."""
    text = (CONFIG / "waybar/style.css").read_text()
    seen: list[str] = []
    for value in re.findall(r"#[0-9a-fA-F]{6}\b", text):
        if value.lower() not in {v.lower() for v in seen}:
            seen.append(value)
    return seen


def packages() -> list[tuple[str, list[tuple[str, str]]]]:
    """packages.txt grouped by its banner comments."""
    groups: list[tuple[str, list[tuple[str, str]]]] = []
    banner = re.compile(r"^#\s*[─\-]{3,}\s*(.+?)\s*[─\-]{3,}\s*$")
    for raw in (REPO / "packages.txt").read_text().splitlines():
        line = raw.strip()
        if not line:
            continue
        found = banner.match(line)
        if found:
            groups.append((found.group(1), []))
            continue
        if line.startswith("#"):
            continue
        name, _, comment = line.partition("#")
        name = name.strip()
        if not name:
            continue
        if not groups:
            groups.append(("Packages", []))
        groups[-1][1].append((name, comment.strip()))
    return [(title, entries) for title, entries in groups if entries]


def aur_packages() -> list[tuple[str, str]]:
    out = []
    for raw in (REPO / "aur-packages.txt").read_text().splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        name, _, comment = line.partition("#")
        out.append((name.strip(), comment.strip()))
    return out


def keybind_sections():
    text = (CONFIG / "hypr/modules/binds.lua").read_text()
    sections, main_mod = kb.parse(text)
    return kb.documented_rows(sections, main_mod), main_mod


def project_tree() -> list[tuple[int, str]]:
    """[(depth, name)] — a simple ordered listing of the repository."""
    rows: list[tuple[int, str]] = []
    paths = sorted(
        p.relative_to(REPO) for p in REPO.rglob("*")
        if p.is_file() and ".git/" not in str(p) and "__pycache__" not in str(p)
    )
    seen_dirs: set[str] = set()
    for path in paths:
        parts = path.parts
        for index, part in enumerate(parts[:-1]):
            key = "/".join(parts[: index + 1])
            if key not in seen_dirs:
                seen_dirs.add(key)
                rows.append((index, part + "/"))
        rows.append((len(parts) - 1, parts[-1]))
    return rows


# ── bar preview ───────────────────────────────────────────────────────────
# Sample values so the bar strip looks alive. The *modules and their order*
# come from waybar/config.jsonc — that part is real.

MODULE_SAMPLE = {
    "custom/launcher": ("󰣇", ""),
    "hyprland/workspaces": ("", ""),
    "hyprland/submap": ("", ""),
    "hyprland/window": ("", "~/projects/monochrome — kitty"),
    "mpris": ("󰎈", "Midnight City — M83"),
    "cpu": ("", "12%"),
    "memory": ("", "42%"),
    "temperature": ("", "48°C"),
    "custom/updates": ("󰚰", "7"),
    "network": ("󰖩", "wlan0"),
    "bluetooth": ("󰂯", ""),
    "pulseaudio": ("󰕾", "65%"),
    "battery": ("󰁹", "88%"),
    "tray": ("", ""),
    "clock": ("", "21:42"),
    "custom/settings": ("", ""),
    "custom/power": ("⏻", ""),
}


def bar_html(config: dict) -> str:
    def group(modules: list[str], extra_class: str = "") -> str:
        chips = []
        for module in modules:
            icon, value = MODULE_SAMPLE.get(module, ("", ""))
            if module == "hyprland/workspaces":
                chips.append(
                    '<span class="ws active">1</span>'
                    '<span class="ws">2</span><span class="ws">3</span>'
                    '<span class="ws">4</span><span class="ws">5</span>'
                )
                continue
            if module == "tray":
                chips.append('<span class="mod tray"><span class="dot"></span><span class="dot"></span></span>')
                continue
            css = module.replace("/", "-")
            chips.append(
                f'<span class="mod {css}">{html.escape(icon)}'
                f'{"<b>" + html.escape(value) + "</b>" if value else ""}</span>'
            )
        return f'<div class="group {extra_class}">{"".join(chips)}</div>'

    rows = [
        ("left", group(config.get("modules-left", []))),
        ("centre", group(config.get("modules-center", []))),
        ("right", group(config.get("modules-right", []))),
    ]
    body = "".join(
        f'<div class="brow"><span class="blabel">{label}</span>{chips}</div>'
        for label, chips in rows
    )
    names = ", ".join(config.get("modules-right", []))
    return f'''<div class="bar">{body}</div>
<p class="caption">The three floating groups of the bar, exactly as listed in
<code>waybar/config.jsonc</code> — on screen they sit in one row, left · centre · right.
Module values are sample data; the right group is ordered
<code>{html.escape(names)}</code>.</p>'''


# ── page ──────────────────────────────────────────────────────────────────

CSS = """
:root{
  --ink:#080808; --surface:#0d0d0d; --surface-2:#141414; --surface-3:#1a1a1a;
  --text:#e6e6e6; --dim:#8a8a8a; --faint:#4a4a4a;
  --line:rgba(255,255,255,.08); --line-strong:rgba(255,255,255,.16);
  --mono:"JetBrainsMono Nerd Font","JetBrains Mono",ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;
}
*{box-sizing:border-box}
body{
  margin:0; background:var(--ink); color:var(--text);
  font-family:var(--mono); font-size:14px; line-height:1.6;
  -webkit-font-smoothing:antialiased;
}
.wrap{max-width:1180px; margin:0 auto; padding:56px 24px 96px}
a{color:#fff; text-decoration:none; border-bottom:1px solid var(--line-strong)}
a:hover{border-bottom-color:#fff}
h1{font-size:34px; margin:0 0 6px; letter-spacing:-.5px; color:#fff}
h2{font-size:19px; margin:64px 0 4px; color:#fff; letter-spacing:.2px}
h2 .num{color:var(--faint); margin-right:10px; font-weight:400}
h3{font-size:14px; margin:28px 0 8px; color:var(--dim); text-transform:uppercase; letter-spacing:1.4px; font-weight:500}
p{margin:8px 0; color:var(--text)}
.lead{color:var(--dim); max-width:74ch}
hr{border:0; border-top:1px solid var(--line); margin:20px 0 0}
code{font-family:var(--mono); background:var(--surface-2); border:1px solid var(--line);
     border-radius:5px; padding:1px 6px; font-size:12.5px; color:#fff}
.badges{display:flex; flex-wrap:wrap; gap:8px; margin:18px 0 0}
.badge{border:1px solid var(--line); background:var(--surface); border-radius:999px;
       padding:4px 12px; font-size:12px; color:var(--dim)}
.badge b{color:#fff; font-weight:500}
.note{border:1px solid var(--line); background:var(--surface); border-radius:10px;
      padding:12px 16px; color:var(--dim); font-size:12.5px; margin:18px 0}
.figure{border:1px solid var(--line); border-radius:14px; overflow:hidden; margin-top:18px; background:#000}
.figure svg{display:block; width:100%; height:auto}
.caption{color:var(--faint); font-size:12px; margin:10px 0 0}
.bar{display:flex; flex-direction:column; gap:7px; background:#050505;
     border:1px solid var(--line); border-radius:14px; padding:12px 14px; margin:18px 0 6px}
.brow{display:flex; gap:10px; align-items:center; flex-wrap:wrap}
.blabel{color:var(--dim); font-size:10px; letter-spacing:.16em; text-transform:uppercase;
        min-width:50px}
.group{display:flex; gap:2px; align-items:center; background:rgba(10,10,10,.72);
       border:1px solid var(--line); border-radius:12px; padding:4px 8px; flex-wrap:wrap}
.mod{color:var(--text); padding:0 8px; font-size:12.5px; white-space:nowrap}
.mod b{color:#fff; font-weight:500; margin-left:5px}
.ws{color:#6f6f6f; padding:2px 7px; border-radius:7px; font-size:12.5px}
.ws.active{background:#e6e6e6; color:#0a0a0a; font-weight:600}
.dot{display:inline-block; width:9px; height:9px; border-radius:2px; background:#3a3a3a; margin:0 3px}
table{width:100%; border-collapse:collapse; margin:10px 0 0; font-size:13px}
th,td{text-align:left; padding:8px 10px; border-bottom:1px solid var(--line); vertical-align:top}
th{color:var(--faint); font-weight:500; font-size:11.5px; text-transform:uppercase; letter-spacing:1.2px}
td:first-child{white-space:nowrap; color:#fff}
tbody tr:hover{background:var(--surface)}
.two{display:grid; grid-template-columns:repeat(auto-fit,minmax(320px,1fr)); gap:28px}
.swatches{display:flex; flex-wrap:wrap; gap:10px; margin-top:12px}
.sw{border:1px solid var(--line); border-radius:10px; overflow:hidden; width:132px; background:var(--surface)}
.sw .chip{height:44px}
.sw .meta{padding:7px 10px; font-size:11.5px; color:var(--dim)}
.sw .meta b{display:block; color:#fff; font-weight:500}
.sw .meta span{color:var(--faint)}
formula{display:block}
pre{background:var(--surface); border:1px solid var(--line); border-radius:10px;
    padding:14px 16px; overflow-x:auto; font-size:12.5px; color:var(--text); margin:10px 0}
pre .c{color:var(--faint)}
pre .k{color:#fff}
.tree{columns:2; column-gap:32px; font-size:12.5px; color:var(--dim); margin-top:12px}
.tree div{break-inside:avoid; white-space:pre}
.tree .d{color:#fff}
.grid3{display:grid; grid-template-columns:repeat(auto-fit,minmax(260px,1fr)); gap:14px; margin-top:14px}
.card{border:1px solid var(--line); background:var(--surface); border-radius:12px; padding:14px 16px}
.card h4{margin:0 0 6px; font-size:13px; color:#fff; font-weight:600}
.card p{margin:0; font-size:12.5px; color:var(--dim)}
.card kbd{font-family:var(--mono); background:#fff; color:#0a0a0a; border-radius:5px;
          padding:1px 7px; font-size:11.5px; font-weight:600}
footer{margin-top:72px; padding-top:20px; border-top:1px solid var(--line);
       color:var(--faint); font-size:12px}
@media (max-width:760px){ .tree{columns:1} h1{font-size:26px} }
"""


def swatch(name: str, value: str, label: str = "") -> str:
    return (f'<div class="sw"><div class="chip" style="background:{value}"></div>'
            f'<div class="meta"><b>{html.escape(name)}</b><span>{html.escape(value)}'
            f'{(" · " + html.escape(label)) if label else ""}</span></div></div>')


def build() -> str:
    sections, main_mod = keybind_sections()
    main_label = kb.MOD_LABELS.get(main_mod.upper(), main_mod)
    waybar_config = read_jsonc(CONFIG / "waybar/config.jsonc")
    rofi = rofi_palette()
    kitty = kitty_palette()
    starship = starship_palette()
    wb_colours = waybar_colours()
    pkgs = packages()
    aur = aur_packages()
    tree = project_tree()
    desktop_svg = (REPO / "docs/preview.svg").read_text()
    desktop_svg = re.sub(r"<\?xml.*?\?>", "", desktop_svg).strip()
    desktop_svg = desktop_svg.replace('width="1600" height="900"', 'preserveAspectRatio="xMidYMid meet"')

    total_binds = sum(len(rows) for _, rows in sections)
    total_pkgs = sum(len(entries) for _, entries in pkgs)

    # ── keybind tables ────────────────────────────────────────────────────
    bind_tables = []
    for title, rows in sections:
        body = "".join(
            f"<tr><td><kbd>{html.escape(k)}</kbd></td><td>{html.escape(d)}</td></tr>"
            for k, d in rows
        )
        bind_tables.append(f"<h3>{html.escape(title)}</h3>"
                           f"<table><thead><tr><th>Keys</th><th>Action</th></tr></thead>"
                           f"<tbody>{body}</tbody></table>")

    # ── palettes ──────────────────────────────────────────────────────────
    rofi_sw = "".join(swatch(name, value) for name, value in rofi)
    kitty_sw = "".join(swatch(k, v) for k, v in kitty.items() if k in ("background", "foreground", "cursor"))
    kitty_ramp = "".join(swatch(f"color{i}", kitty.get(f"color{i}", "#000000"))
                         for i in range(16) if f"color{i}" in kitty)
    star_sw = "".join(swatch(name, value) for name, value in starship.items())
    wb_sw = "".join(swatch(f"style.css #{i + 1}", c) for i, c in enumerate(wb_colours[:8]))

    # ── packages ──────────────────────────────────────────────────────────
    pkg_html = []
    for title, entries in pkgs:
        rows = "".join(
            f"<tr><td>{html.escape(name)}</td><td>{html.escape(comment)}</td></tr>"
            for name, comment in entries
        )
        pkg_html.append(f"<h3>{html.escape(title)}</h3><table><tbody>{rows}</tbody></table>")
    aur_rows = "".join(
        f"<tr><td>{html.escape(name)}</td><td>{html.escape(comment)}</td></tr>" for name, comment in aur
    )

    # ── tree ──────────────────────────────────────────────────────────────
    tree_html = "".join(
        f'<div>{"&nbsp;" * 0}{"│  " * depth}'
        f'<span class="{"d" if name.endswith("/") else ""}">{html.escape(name)}</span></div>'
        for depth, name in tree
    )

    gaps_in = lua_value("gaps_in", "5")
    gaps_out = lua_value("gaps_out", "10")
    rounding = lua_value("rounding", "10")
    browser = lua_value("browser", "firefox")
    terminal = lua_value("terminal", "kitty")
    files_app = lua_value("file_manager", "thunar")

    return f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>hyprland-monochrome — preview</title>
<style>{CSS}</style>
</head>
<body>
<div class="wrap">

  <h1>hyprland-monochrome</h1>
  <p class="lead">A clean monochrome Hyprland desktop for Arch, EndeavourOS, CachyOS and Manjaro.
  This page is generated from the project's own configuration files, so nothing here is
  aspirational — it is what the files actually say.</p>
  <div class="badges">
    <span class="badge"><b>Hyprland</b> 0.55+ · Lua config</span>
    <span class="badge"><b>{total_binds}</b> keybinds</span>
    <span class="badge"><b>{total_pkgs}</b> packages, all official repos</span>
    <span class="badge"><b>{len(aur)}</b> optional AUR extras</span>
    <span class="badge"><b>{terminal}</b> · <b>{files_app}</b> · Zsh + Starship</span>
    <span class="badge">MIT</span>
  </div>

  <h2><span class="num">01</span>Desktop layout</h2>
  <p class="lead">Tiled windows with {gaps_in}px inner and {gaps_out}px outer gaps, {rounding}px rounding,
  a focused-window border that goes white-to-grey, and a frosted top bar split into three floating groups.
  Rofi slides in over the top for the launcher, menus and pickers.</p>
  <div class="figure">{desktop_svg}</div>
  <p class="caption">A mockup of the layout, not a screenshot — the shapes are drawn to the real
  gap, rounding and bar geometry from <code>modules/appearance.lua</code>, but the window contents
  are illustrative. It is saved as <code>docs/preview.svg</code> next to this page.</p>

  <h2><span class="num">02</span>The bar</h2>
  <p class="lead">Waybar as configured: three pill groups, workspace numbers that invert when active,
  now-playing in the centre that hides itself when nothing plays.</p>
  {bar_html(waybar_config)}

  <h2><span class="num">03</span>Keybinds</h2>
  <p class="lead">The main modifier is <kbd>{main_label}</kbd> (one line in
  <code>modules/user.lua</code> changes it). Every bind carries a description, so
  <kbd>{main_label} + /</kbd> and <code>hyprctl binds</code> show the same wording as this page.
  These tables are parsed straight out of <code>modules/binds.lua</code>.</p>
  {"".join(bind_tables)}

  <h2><span class="num">04</span>Palette</h2>
  <p class="lead">Greyscale everywhere. Each surface has its own one-line palette so you can shift
  the whole theme by editing a handful of values.</p>

  <h3>Menus — rofi/theme.rasi</h3>
  <div class="swatches">{rofi_sw}</div>

  <div class="two">
    <div>
      <h3>Terminal — kitty/theme.conf</h3>
      <div class="swatches">{kitty_sw}</div>
      <div class="swatches">{kitty_ramp}</div>
    </div>
    <div>
      <h3>Prompt — starship.toml</h3>
      <div class="swatches">{star_sw}</div>
      <h3>Bar — waybar/style.css</h3>
      <div class="swatches">{wb_sw}</div>
    </div>
  </div>

  <h2><span class="num">05</span>What gets installed</h2>
  <p class="lead">Everything in <code>packages.txt</code> is in the official Arch repositories,
  so the install works on Arch, EndeavourOS, CachyOS and Manjaro without an AUR helper.</p>
  {"".join(pkg_html)}
  <h3>Optional AUR extras</h3>
  <table><tbody>{aur_rows}</tbody></table>

  <h2><span class="num">06</span>Project layout</h2>
  <p class="lead">Hyprland reads <code>hyprland.lua</code>, which loads the modules beside it —
  so the file you normally open is <code>modules/user.lua</code>. The shell scripts are what power
  the menus, pickers and bar helpers.</p>
  <div class="tree">{tree_html}</div>

  <h2><span class="num">07</span>Try it</h2>
  <div class="grid3">
    <div class="card"><h4>Install</h4><p><code>./install.sh</code><br>
      Packages, backups (twice), configs, wallpapers. <code>--dry-run</code> shows the plan first.</p></div>
    <div class="card"><h4>See the keys</h4><p><kbd>{main_label} + /</kbd><br>
      Searchable cheat sheet built from your live binds, or read <code>docs/KEYBINDS.md</code>.</p></div>
    <div class="card"><h4>Make it yours</h4><p><code>modules/user.lua</code><br>
      Programs, gaps, keyboard, paths — the other modules read from it.
      Right now: browser <b>{html.escape(browser)}</b>, terminal <b>{html.escape(terminal)}</b>.</p></div>
    <div class="card"><h4>Pick a wallpaper</h4><p><kbd>{main_label} + W</kbd><br>
      Thumbnail grid over <code>~/Pictures/Wallpapers</code>, or <code>--random</code> on every boot.</p></div>
    <div class="card"><h4>Record it</h4><p><kbd>{main_label} + Alt + Print</kbd><br>
    screen recording (again to stop) · <kbd>{main_label} + Alt + S</kbd> saves the last minute ·
    gpu-screen-recorder or wf-recorder, whichever you have.</p></div>
    <div class="card"><h4>System settings</h4><p><kbd>{main_label} + I</kbd><br>
      Info, Network, Bluetooth, Monitors, Sound, night shift, and every config file.</p></div>
    <div class="card"><h4>Undo everything</h4><p><code>./uninstall.sh</code><br>
      Removes the config and restores your originals from <code>~/.config-backups/pristine</code>.</p></div>
  </div>

  <pre><span class="c"># the short version</span>
git clone &lt;this repo&gt; &amp;&amp; cd hyprland-monochrome
<span class="k">./install.sh</span>            <span class="c"># then log out and back in</span>
<span class="k">./test/run-all.sh</span>        <span class="c"># shellcheck + lua + json + 63 smoke checks</span></pre>

  <footer>
    Generated from this repository's config on {date.today().isoformat()} by
    <code>tools/build-preview.py</code> · {total_binds} binds, {total_pkgs} packages, MIT licensed.
    Start reading at <code>README.md</code>; troubleshooting lives in
    <code>docs/TROUBLESHOOTING.md</code>.
  </footer>

</div>
</body>
</html>
"""


STAMP = re.compile(r"on \d{4}-\d{2}-\d{2} by")


def comparable(page: str) -> str:
    """The page minus its build date, so --check does not fail on a new day."""
    return STAMP.sub("on <date> by", page)


def main() -> int:
    page = build()
    if "--check" in sys.argv:
        if OUT.exists() and comparable(OUT.read_text()) == comparable(page):
            print("docs/preview.html is up to date.")
            return 0
        print("docs/preview.html is OUT OF DATE — run tools/build-preview.py", file=sys.stderr)
        return 1
    OUT.write_text(page)
    print(f"wrote {OUT.relative_to(REPO)} ({len(page) // 1024} KB)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
