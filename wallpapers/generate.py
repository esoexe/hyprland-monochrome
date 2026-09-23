#!/usr/bin/env python3
"""
Generate the bundled monochrome wallpapers.

Run it to (re)create the images in this folder:

    python3 generate.py                 # 1920x1080
    python3 generate.py 2560 1440       # custom size for a bigger screen
    python3 generate.py --only 3        # just one design
    python3 generate.py 15360 8640 --only 6 --suffix -16k --quality 95
                                        # 16K master, without overwriting 1080p

Every design is drawn in multiples of the 1080p reference height, so enlarging
the canvas adds real detail rather than scaling up blurry pixels. Note that
gradient_mesh and spotlight walk each pixel in Python: 4K is fine, 8K+ takes a
few minutes. The others are vector operations and scale cheaply.

Requires Pillow (python-pillow in the Arch repos).
"""

import math
import random
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

OUT = Path(__file__).parent
INK = 8  # darkest value used anywhere


def _canvas(w, h, base=INK):
    return Image.new("L", (w, h), base)


def _finish(img, name, w, h, suffix="", quality=88):
    """Turn an 8-bit greyscale image into the final JPEG."""
    img = img.convert("RGB")
    path = OUT / f"{name}{suffix}.jpg"
    img.save(path, "JPEG", quality=quality, optimize=True, progressive=True)
    print(f"  wrote {path.name}  ({path.stat().st_size // 1024} KB, {w}x{h})")


def gradient_mesh(w, h):
    """Soft vertical gradient with a gentle horizontal fall-off and grain."""
    img = _canvas(w, h)
    px = img.load()
    for y in range(h):
        g = int(6 + (y / h) ** 1.6 * 34)
        for x in range(w):
            edge = 1.0 - abs((x / w) - 0.5) * 0.6
            px[x, y] = max(0, min(255, int(g * edge)))
    rnd = random.Random(7)
    grain = Image.new("L", (w, h))
    gpx = grain.load()
    for y in range(h):
        for x in range(w):
            gpx[x, y] = rnd.randint(0, 255)
    grain = grain.filter(ImageFilter.GaussianBlur(0.6 * (h / 1080.0)))
    img = Image.blend(img, grain, 0.05)
    return img


def ripple(w, h):
    """Concentric topographic rings from a point off-centre."""
    k = h / 1080.0
    img = _canvas(w, h)
    d = ImageDraw.Draw(img)
    cx, cy = w * 0.32, h * 0.42
    r = 8.0 * k
    step = 26 * k
    while r < max(w, h) * 1.6:
        val = int(150 * math.exp(-r / (max(w, h) * 0.55)) + 12)
        width = max(1, int((3 - r / (900 * k)) * k))
        d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=val, width=width)
        r += step
    img = img.filter(ImageFilter.GaussianBlur(0.8 * k))
    return img


def dot_grid(w, h):
    """Grid of dots that fades outwards."""
    k = h / 1080.0
    img = _canvas(w, h)
    d = ImageDraw.Draw(img)
    spacing = int(28 * k)
    cx, cy = w / 2, h / 2
    max_d = math.hypot(cx, cy)
    for y in range(spacing, h, spacing):
        for x in range(spacing, w, spacing):
            dist = math.hypot(x - cx, y - cy) / max_d
            val = int(120 * (1 - dist) ** 2.2) + 6
            radius = max(1, int(3.2 * k * (1 - dist) ** 1.4))
            d.ellipse([x - radius, y - radius, x + radius, y + radius], fill=val)
    img = img.filter(ImageFilter.GaussianBlur(0.5 * k))
    return img


def hatch(w, h):
    """Diagonal hatching behind a soft vertical fade."""
    k = h / 1080.0
    img = _canvas(w, h)
    d = ImageDraw.Draw(img)
    for i in range(-h, w + h, int(14 * k)):
        d.line([(i, 0), (i + h, h)], fill=64, width=max(1, int(round(k))))
    fade = _canvas(w, h)
    fpx = fade.load()
    for y in range(h):
        v = int(255 * (y / h) ** 1.3)
        for x in range(w):
            fpx[x, y] = v
    img = Image.composite(img, _canvas(w, h), fade.point(lambda v: v))
    img = img.filter(ImageFilter.GaussianBlur(0.4 * k))
    return img


def spotlight(w, h):
    """Dark room with a single soft light in the middle."""
    img = _canvas(w, h, 6)
    px = img.load()
    cx, cy = w * 0.5, h * 0.42
    max_d = math.hypot(cx, cy)
    for y in range(h):
        for x in range(w):
            dist = math.hypot(x - cx, y - cy) / max_d
            px[x, y] = int(120 * max(0.0, 1 - dist) ** 3.0)
    img = img.filter(ImageFilter.GaussianBlur(14 * (h / 1080.0)))
    return img


def waves(w, h):
    """Layered sine waves — a soft monochrome landscape.

    Every size is expressed in multiples of the 1080p reference height, so the
    design is identical at any resolution: `generate.py 15360 8640` gives the
    same picture with 8x the detail instead of an upscaled one.
    """
    k = h / 1080.0                      # 1.0 at 1080p, 8.0 at 16K
    step = max(2, int(round(8 * k)))    # polygon sampling step

    img = _canvas(w, h, 10)
    d = ImageDraw.Draw(img)
    for layer in range(7):
        amp = (40 + layer * 26) * k
        period = (700 + layer * 240) * k
        ybase = h * 0.72 + layer * 26 * k
        val = 18 + layer * 22
        pts = [
            (x, ybase + amp * math.sin(2 * math.pi * x / period + layer))
            for x in range(0, w + step, step)
        ]
        pts += [(w, h), (0, h)]
        d.polygon(pts, fill=val)
    img = img.filter(ImageFilter.GaussianBlur(1.4 * k))
    return img


DESIGNS = [
    ("mono-01", gradient_mesh, "soft gradient — the default wallpaper"),
    ("mono-02", ripple, "concentric rings"),
    ("mono-03", dot_grid, "dot grid"),
    ("mono-04", hatch, "diagonal hatch"),
    ("mono-05", spotlight, "spotlight"),
    ("mono-06", waves, "layered waves"),
]


def _take(args, flag, default=None):
    """Pull `--flag value` out of the argument list."""
    if flag not in args:
        return default
    i = args.index(flag)
    value = args[i + 1] if i + 1 < len(args) else None
    del args[i:i + 2]
    return value


def main() -> int:
    args = [a for a in sys.argv[1:]]
    only = _take(args, "--only")
    suffix = _take(args, "--suffix", "")
    quality = int(_take(args, "--quality", 88) or 88)
    w, h = (int(args[0]), int(args[1])) if len(args) >= 2 else (1920, 1080)

    # "6", "06" and "mono-06" all select mono-06 (the old check only accepted
    # the zero-padded form, so the documented `--only 3` quietly did nothing).
    wanted = None
    if only:
        wanted = str(only).strip().lstrip("0") or "0"

    print(f"Generating monochrome wallpapers at {w}x{h}")
    wrote = 0
    for name, fn, note in DESIGNS:
        number = name.split("-")[-1].lstrip("0") or "0"
        if wanted and wanted not in (number, name.lstrip("0")):
            continue
        print(f"  · {name} — {note}")
        _finish(fn(w, h), name, w, h, suffix=suffix, quality=quality)
        wrote += 1

    if wrote == 0:
        names = ", ".join(d[0] for d in DESIGNS)
        print(f"Nothing generated: --only {only!r} matches no design.", file=sys.stderr)
        print(f"Known designs: {names}  (also accepts 1-6)", file=sys.stderr)
        return 1
    print("Done.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
