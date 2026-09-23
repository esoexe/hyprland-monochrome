#!/usr/bin/env python3
"""
Generate docs/KEYBINDS.md from the live Hyprland config.

It reads .config/hypr/modules/binds.lua (the single source of truth) and the
mainMod from modules/user.lua, so the documentation can never drift away from
the actual keybinds.

    python3 tools/keybind-table.py            # rewrite docs/KEYBINDS.md
    python3 tools/keybind-table.py --check    # exit 1 if the file is out of date
                                                    (handy in a git hook)
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
CONFIG = REPO / ".config/hypr"
BINDS = CONFIG / "modules/binds.lua"
USER = CONFIG / "modules/user.lua"
OUT = REPO / "docs/KEYBINDS.md"

MOD_LABELS = {
    "SUPER": "Super",
    "ALT": "Alt",
    "CTRL": "Ctrl",
    "SHIFT": "Shift",
}

KEY_LABELS = {
    "RETURN": "Enter",
    "SPACE": "Space",
    "TAB": "Tab",
    "ESCAPE": "Escape",
    "GRAVE": "`",
    "SLASH": "/",
    "EQUAL": "=",
    "MINUS": "-",
    "PRINT": "Print",
    "DELETE": "Delete",
    "BACKSPACE": "Backspace",
    "LEFT": "Left",
    "RIGHT": "Right",
    "UP": "Up",
    "DOWN": "Down",
    "MOUSE_DOWN": "scroll down",
    "MOUSE_UP": "scroll up",
    "MOUSE_LEFT": "scroll left",
    "MOUSE_RIGHT": "scroll right",
}


def read_main_mod() -> str:
    text = USER.read_text()
    match = re.search(r'mainMod\s*=\s*"([^"]+)"', text)
    return match.group(1) if match else "SUPER"


def prettify_keys(raw: str, main_mod: str) -> str:
    """'SHIFT + RETURN' → 'Super + Shift + Return' (mainMod implied)."""
    raw = raw.strip()
    out = []
    for part in [p.strip() for p in raw.split("+") if p.strip()]:
        upper = part.upper()
        if upper in MOD_LABELS:
            out.append(MOD_LABELS[upper])
        elif upper in KEY_LABELS:
            out.append(KEY_LABELS[upper])
        elif part.islower():
            out.append(part.capitalize())
        else:
            out.append(part)
    if any(p.upper() != main_mod.upper() for p in [raw.split("+")[0].strip()]) or True:
        # helpers in binds.lua always prepend mainMod, so mirror that
        pass
    return " + ".join(out)


def statements(text: str):
    """Yield (line_number, statement_text) for the top level calls we care about."""
    # strip comments that are not inside a string
    cleaned = []
    for line in text.splitlines():
        out, in_str, quote = [], False, ""
        i = 0
        while i < len(line):
            ch = line[i]
            if in_str:
                out.append(ch)
                if ch == quote and line[i - 1] != "\\":
                    in_str = False
            elif ch in "\"'":
                in_str, quote = True, ch
                out.append(ch)
            elif ch == "-" and line[i : i + 2] == "--":
                break
            else:
                out.append(ch)
            i += 1
        cleaned.append("".join(out))

    buf, depth, start_line = [], 0, 0
    for lineno, line in enumerate(cleaned, 1):
        if not buf and not re.search(r"\b(sh|act|hl\.bind|media|hold)\s*\(", line):
            continue
        if not buf:
            start_line = lineno
        buf.append(line)
        depth += line.count("(") - line.count(")")
        if depth <= 0:
            yield start_line, " ".join(buf).strip()
            buf, depth = [], 0


def parse(text: str):
    """Return [(section_title, [(keys, description), …]), …] in file order."""
    # The two little helper definitions at the top are not binds — drop them so
    # they cannot be mistaken for keybindings.
    text = re.sub(r"local function (?:sh|act|media|hold)\(.*?\nend\n", "", text, flags=re.S)

    header_re = re.compile(r"^--\s*[═=\-]{3,}\s*(.+?)\s*[═=\-]{0,}\s*$")

    section_of: dict[int, str] = {}
    current_title = None
    for index, line in enumerate(text.splitlines(), 1):
        header = header_re.match(line)
        if header:
            title = header.group(1).strip()
            if title and not title.startswith("modules/binds"):
                current_title = title
        section_of[index] = current_title or "Other"

    found: list[tuple[str, str, str]] = []
    for lineno, stmt in statements(text):
        strings = re.findall(r'"((?:[^"\\]|\\.)*)"', stmt)
        if len(strings) < 2:
            continue
        keys, desc = strings[0], strings[-1]
        # explicit `description = "…"` always wins over a trailing command string
        explicit = re.search(r'description\s*=\s*"((?:[^"\\]|\\.)*)"', stmt)
        if explicit:
            desc = explicit.group(1)
        if not keys or not desc or keys == desc:
            continue
        # statements inside the workspace for-loop are covered by the extra rows
        if ".. key" in stmt or re.match(r"^(?:sh|act)\(\s*key\b", stmt):
            continue
        # mouse binds are described in the Mouse table instead
        if "mouse:" in keys.lower():
            continue
        # skip anything that is clearly not a key description
        if " " in desc and desc.replace(" ", "").islower() and desc.count(" ") > 4:
            continue
        found.append((section_of.get(lineno, "Other"), keys, desc))

    sections: list[tuple[str, list[tuple[str, str]]]] = []
    for section, keys, desc in found:
        if not sections or sections[-1][0] != section:
            sections.append((section, []))
        sections[-1][1].append((keys, desc))

    # Expand the workspace loop (it is generated by a for loop in the config).
    expanded = []
    for section, entries in sections:
        if section.upper().startswith("WORKSPACES"):
            entries = list(entries) + [
                ("1..0", "Switch workspace (10 is bound to the 0 key)"),
                ("SHIFT + 1..0", "Move the focused window to that workspace"),
                ("ALT + 1..0", "Move it there without following"),
            ]
        expanded.append((section, entries))
    return expanded, read_main_mod()


def code(text: str) -> str:
    """Inline code span, safely quoting a backtick inside the text."""
    if "`" in text:
        return f"`` {text} ``"
    return f"`{text}`"


def key_label(keys: str, main_mod: str) -> str:
    """The human label for one bind, e.g. 'Super + Shift + T'."""
    label = MOD_LABELS.get(main_mod.upper(), main_mod)
    if keys in ("1..0", "SHIFT + 1..0", "ALT + 1..0"):
        return f"{label} + {prettify_keys(keys, main_mod)}"
    if keys.startswith("XF86") or keys.startswith("switch:"):
        return keys
    if keys.upper().startswith("ALT") or keys.upper().startswith("CTRL"):
        return prettify_keys(keys, main_mod)
    return f"{label} + {prettify_keys(keys, main_mod)}"


def documented_rows(sections, main_mod: str):
    """[(section, [(key label, description), …]), …] — used by the docs and the preview."""
    return [(title, [(key_label(k, main_mod), d) for k, d in entries]) for title, entries in sections]


def render(sections, main_mod: str) -> str:
    label = MOD_LABELS.get(main_mod.upper(), main_mod)
    lines = [
        "# Keybinds",
        "",
        "> This file is generated from `~/.config/hypr/modules/binds.lua`.",
        "> Edit the Lua file, then run `python3 tools/keybind-table.py` to refresh",
        "> this page. Every bind also carries a description, so `hyprctl binds`",
        f"> and the cheat sheet (`{label} + /`) show the same words.",
        "",
        f"The **main modifier is `{label}`**. Change it in one place:",
        "`modules/user.lua` → `mainMod`.",
        "",
        "## Contents",
        "",
    ]
    for title, _ in sections:
        anchor = title.lower().replace(" ", "-").replace("/", "").replace("&", "")
        lines.append(f"- [{title.title()}](#{anchor})")
    lines.append("")

    for title, entries in documented_rows(sections, main_mod):
        lines.append(f"## {title.title()}")
        lines.append("")
        lines.append("| Keys | Action |")
        lines.append("| --- | --- |")
        for keys, desc in entries:
            lines.append(f"| {code(keys)} | {desc} |")
        lines.append("")

    lines += [
        "## Mouse",
        "",
        f"| Action | Result |",
        "| --- | --- |",
        f"| {code(label + ' + drag left button')} | Move a window |",
        f"| {code(label + ' + drag right button')} | Resize a window |",
        f"| {code(label + ' + scroll')} | Previous / next workspace |",
        f"| {code(label + ' + Ctrl + scroll')} | Drag the focused window to another workspace |",
        f"| {code(label + ' + drag on a border/gap')} | Resize (also works while tiling) |",
        "",
        "## Cheat sheet at runtime",
        "",
        "```bash",
        f"{label} + /                 # searchable list built from your live binds",
        "hyprctl binds            # the raw list",
        "```",
        "",
        "## Changing the modifier",
        "",
        "Open `~/.config/hypr/modules/user.lua` and set, for example:",
        "",
        "```lua",
        'mainMod = "ALT",          -- or "CTRL" / "SUPER"',
        "```",
        "",
        "Then reload: `hyprctl reload`.",
        "",
    ]
    return "\n".join(lines)


def main() -> int:
    text = BINDS.read_text()
    sections, main_mod = parse(text)
    rendered = render(sections, main_mod)

    if "--check" in sys.argv:
        if OUT.exists() and OUT.read_text() == rendered:
            print("docs/KEYBINDS.md is up to date.")
            return 0
        print("docs/KEYBINDS.md is OUT OF DATE — run tools/keybind-table.py", file=sys.stderr)
        return 1

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(rendered)
    total = sum(len(rows) for _, rows in sections)
    print(f"wrote {OUT.relative_to(REPO)} — {total} rows in {len(sections)} sections")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
