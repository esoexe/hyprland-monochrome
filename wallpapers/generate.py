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

from PIL import Image, ImageChops, ImageDraw, ImageFilter

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


def eclipse(w, h):
    """Total eclipse: obsidian lunar disc against an asymmetric silver corona."""
    k = h / 1080.0
    cx, cy = w * 0.60, h * 0.44
    r = 186.0 * k

    # Soft outer atmospheric glow built at 1/8th scale and upsampled smoothly
    gw, gh = max(64, w // 8), max(36, h // 8)
    glow = Image.new("L", (gw, gh), INK)
    gpx = glow.load()
    gcx, gcy, gr = cx / 8.0, cy / 8.0, r / 8.0
    for y in range(gh):
        for x in range(gw):
            d = math.hypot(x - gcx, y - gcy)
            ring = max(0.0, d - gr * 0.88) / (gr * 2.4)
            # Subtle asymmetric flare brighter toward top-left (angle ~ -2.3 rad)
            ang = math.atan2(y - gcy, x - gcx)
            bias = 1.0 + 0.38 * math.cos(ang + 2.35) + 0.14 * math.cos(2 * ang - 0.8)
            val = int(INK + 195 * bias * math.exp(-2.35 * ring))
            gpx[x, y] = max(INK, min(255, val))
    img = glow.resize((w, h), Image.Resampling.BICUBIC)

    # Fine coronal filaments radiating from the limb
    rays = Image.new("L", (w, h), 0)
    rd = ImageDraw.Draw(rays)
    rnd = random.Random(42)
    for i in range(180):
        ang = (2 * math.pi * i / 180.0) + rnd.uniform(-0.012, 0.012)
        reach = r * (1.16 + 0.55 * (rnd.random() ** 2.2) * (1.0 + 0.35 * math.cos(ang + 2.35)))
        x0 = cx + math.cos(ang) * (r * 0.98)
        y0 = cy + math.sin(ang) * (r * 0.98)
        x1 = cx + math.cos(ang) * reach
        y1 = cy + math.sin(ang) * reach
        intensity = rnd.randint(16, 46)
        rd.line([(x0, y0), (x1, y1)], fill=intensity, width=max(1, int(round(1.6 * k))))
    rays = rays.filter(ImageFilter.GaussianBlur(3.2 * k))
    img = ImageChops.add(img, rays)

    # Crisp bright inner diamond-ring rim + pitch-black lunar silhouette
    rim = Image.new("L", (w, h), 0)
    rim_d = ImageDraw.Draw(rim)
    rim_d.ellipse([cx - r * 1.018, cy - r * 1.018, cx + r * 1.018, cy + r * 1.018],
                  outline=230, width=max(2, int(round(4.0 * k))))
    rim = rim.filter(ImageFilter.GaussianBlur(2.2 * k))
    img = ImageChops.add(img, rim)

    disc_d = ImageDraw.Draw(img)
    disc_d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=6)
    img = img.filter(ImageFilter.GaussianBlur(0.6 * k))
    return img


def ridgeline(w, h):
    """Stacked topographic elevation scanlines with true painter occlusion."""
    k = h / 1080.0
    img = _canvas(w, h, 8)
    d = ImageDraw.Draw(img)
    rows = 38
    step = max(2, int(round(5 * k)))
    lw = max(1, int(round(1.5 * k)))

    for idx in range(rows):
        t = idx / (rows - 1)
        y_base = h * (0.28 + 0.64 * t)
        pts = []
        for x in range(0, w + step, step):
            u = x / w
            # Central mountain envelope + multi-harmonic terrain ridges
            env = math.exp(-((u - 0.50) / 0.24) ** 2) + 0.48 * math.exp(-((u - 0.24) / 0.14) ** 2)
            env += 0.42 * math.exp(-((u - 0.77) / 0.13) ** 2)
            wave = (
                0.52 * math.sin(u * 13.0 + idx * 0.55)
                + 0.28 * math.cos(u * 27.0 - idx * 0.85)
                + 0.16 * math.sin(u * 53.0 + idx * 1.35)
                + 0.08 * math.cos(u * 97.0 + idx * 2.10)
            )
            lift = (18.0 + 96.0 * env * (0.55 + 0.45 * wave)) * k * (0.35 + 0.65 * math.sin(math.pi * t))
            pts.append((x, y_base - lift))

        # Occlude background ridges behind this horizon silhouette
        poly = pts + [(w, h), (0, h)]
        d.polygon(poly, fill=8 + int(6 * t))
        line_val = int(58 + 168 * (t ** 0.85))
        d.line(pts, fill=line_val, width=lw)

    img = img.filter(ImageFilter.GaussianBlur(0.45 * k))
    return img


def monoliths(w, h):
    """Brutalist architectural slabs lit by a directional silver graze."""
    k = h / 1080.0
    img = _canvas(w, h, 9)

    # Subtle diagonal studio light falloff across the backdrop
    bg = Image.new("L", (160, 90), 9)
    bpx = bg.load()
    for y in range(90):
        for x in range(160):
            u, v = x / 160.0, y / 90.0
            beam = math.exp(-(((u - 0.36) * 0.9 + (v - 0.32) * 0.7) ** 2) / 0.18)
            bpx[x, y] = int(8 + 26 * beam)
    img = bg.resize((w, h), Image.Resampling.BICUBIC)
    d = ImageDraw.Draw(img)

    # Isometric architectural columns (cx_frac, top_frac, width_px, depth_px, shade)
    slabs = [
        (0.18, 0.58, 120, 44, 24),
        (0.29, 0.42, 150, 56, 36),
        (0.43, 0.29, 176, 66, 52),
        (0.57, 0.37, 162, 60, 44),
        (0.70, 0.48, 138, 52, 32),
        (0.81, 0.63, 112, 42, 22),
    ]
    edge_w = max(1, int(round(1.3 * k)))
    for xf, yf, sw, sd, base in slabs:
        cx = w * xf
        top_y = h * yf
        hw = (sw * 0.5) * k
        dp = sd * k
        rise = dp * 0.48

        # Top face (brightest), left lit face (mid), right shadow face (deep obsidian)
        p_top = [(cx, top_y - rise), (cx + hw, top_y), (cx, top_y + rise), (cx - hw, top_y)]
        p_left = [(cx - hw, top_y), (cx, top_y + rise), (cx, h), (cx - hw, h)]
        p_right = [(cx, top_y + rise), (cx + hw, top_y), (cx + hw, h), (cx, h)]

        d.polygon(p_left, fill=base)
        d.polygon(p_right, fill=max(8, int(base * 0.42)))
        d.polygon(p_top, fill=min(220, int(base * 1.85)))
        # Crisp specular rim along the lit crest
        d.line([(cx - hw, top_y), (cx, top_y + rise), (cx + hw, top_y)],
               fill=min(240, base * 3), width=edge_w)
        d.line([(cx, top_y + rise), (cx, h)], fill=max(14, int(base * 1.15)), width=edge_w)

    img = img.filter(ImageFilter.GaussianBlur(0.55 * k))
    return img


def silk_ribbon(w, h):
    """Flowing caustic interference ribbons — gossamer silver on obsidian."""
    k = h / 1080.0
    img = _canvas(w, h, 7)
    step = max(2, int(round(5 * k)))
    lw = max(1, int(round(1.6 * k)))

    # Primary + secondary harmonic ribbons accumulating caustic bright folds
    for strand in range(72):
        s = strand / 71.0
        layer = Image.new("L", (w, h), 0)
        ld = ImageDraw.Draw(layer)
        pts = []
        pts2 = []
        phase = s * 2.75
        for x in range(-int(40 * k), w + step + int(40 * k), step):
            u = x / w
            y = (
                h * (0.78 - 0.50 * u)
                + math.sin(u * 5.0 + phase) * (150 * k)
                + math.cos(u * 9.2 - phase * 1.35) * (66 * k)
                + (s - 0.5) * (118 * k) * math.sin(u * math.pi)
            )
            y2 = (
                h * (0.34 + 0.38 * u)
                + math.cos(u * 4.4 - phase * 0.9) * (110 * k)
                + math.sin(u * 8.1 + phase * 1.2) * (48 * k)
                + (s - 0.5) * (86 * k) * math.cos((u - 0.5) * math.pi)
            )
            pts.append((x, y))
            pts2.append((x, y2))
        val = int(9 + 17 * math.sin(math.pi * s) ** 1.5)
        ld.line(pts, fill=val, width=lw)
        ld.line(pts2, fill=max(3, val // 2), width=lw)
        img = ImageChops.add(img, layer)

    img = img.filter(ImageFilter.GaussianBlur(0.75 * k))
    return img


def singularity(w, h):
    """Relativistic black-hole accretion disc with gravitational lensing halo."""
    k = h / 1080.0
    rng = random.Random(1107)
    img = _canvas(w, h, 5)

    # Subtle distant starfield
    stars = Image.new("L", (w, h), 0)
    sd = ImageDraw.Draw(stars)
    for _ in range(220):
        sx = rng.randrange(w)
        sy = rng.randrange(h)
        lum = rng.randint(35, 135)
        rad = max(1, int((0.8 if lum < 105 else 1.5) * k))
        sd.ellipse((sx - rad, sy - rad, sx + rad, sy + rad), fill=lum)
    img = ImageChops.add(img, stars.filter(ImageFilter.GaussianBlur(0.6 * k)))

    cx, cy = int(w * 0.52), int(h * 0.50)
    r0 = h * 0.195

    # Gravitationally lensed upper & lower relativistic arches (behind shadow)
    halo = Image.new("L", (w, h), 0)
    hd = ImageDraw.Draw(halo)
    for i in range(28, 0, -1):
        t = i / 28.0
        rr = r0 * (1.0 + 0.85 * t)
        val = int(88 * (1.0 - t) ** 2.1)
        hd.ellipse((cx - rr * 1.18, cy - rr * 1.04, cx + rr * 1.18, cy + rr * 1.04), fill=val)
    # Bright lensed upper & lower Einstein arcs
    for mult, lum in ((1.06, 185), (1.13, 130), (1.22, 75)):
        rr = r0 * mult
        hd.ellipse((cx - rr * 1.08, cy - rr * 1.03, cx + rr * 1.08, cy + rr * 1.03),
                   outline=lum, width=max(2, int(5.5 * k)))
    halo = halo.filter(ImageFilter.GaussianBlur(14 * k))
    img = ImageChops.add(img, halo)

    # Back half of tilted accretion disc (occluded by event horizon shadow)
    diag = int(math.hypot(w, h))
    disc_bg = Image.new("L", (diag, diag), 0)
    dbd = ImageDraw.Draw(disc_bg)
    dc = diag // 2
    for ring in range(46):
        f = ring / 45.0
        rx = r0 * (1.14 + f * 2.10)
        ry = rx * 0.16
        lum = int(195 * math.sin(math.pi * min(1.0, f * 1.4)) * math.exp(-f * 1.45))
        if ring % 5 == 0:
            lum = max(0, lum - 34)
        dbd.arc((dc - rx, dc - ry, dc + rx, dc + ry), start=180, end=360,
                fill=lum, width=max(2, int(3.0 * k)))
    disc_bg = disc_bg.filter(ImageFilter.GaussianBlur(2.6 * k)).rotate(14, resample=Image.Resampling.BICUBIC)
    ox, oy = cx - dc, cy - dc
    img = ImageChops.add(img, disc_bg.crop((-ox, -oy, w - ox, h - oy)))

    # Pure black event horizon shadow + crisp relativistic photon ring
    eh = ImageDraw.Draw(img)
    eh.ellipse((cx - r0 * 1.02, cy - r0 * 0.99, cx + r0 * 1.02, cy + r0 * 0.99), fill=4)

    ring_layer = Image.new("L", (w, h), 0)
    rd = ImageDraw.Draw(ring_layer)
    rd.ellipse(
        (cx - r0 * 1.035, cy - r0 * 1.005, cx + r0 * 1.035, cy + r0 * 1.005),
        outline=238,
        width=max(2, int(2.4 * k)),
    )
    img = ImageChops.add(img, ring_layer.filter(ImageFilter.GaussianBlur(1.4 * k)))

    # Foreground half ONLY (smooth continuous Doppler gradient across 0..180 deg)
    fg = Image.new("L", (diag, diag), 0)
    fgd = ImageDraw.Draw(fg)
    for ring in range(44):
        f = ring / 43.0
        rx = r0 * (1.12 + f * 2.15)
        ry = rx * 0.145
        base = 246 * math.exp(-f * 1.42) * (0.58 + 0.42 * math.sin(f * 16.0) ** 2)
        if ring % 6 == 0:
            base *= 0.62
        for seg in range(18):
            a0 = -4.0 + seg * 10.4
            a1 = a0 + 11.2
            mid = math.radians((a0 + a1) * 0.5)
            # Smooth cosine Doppler boost toward left limb (180 deg)
            doppler = 0.92 - 0.24 * math.cos(mid)
            lum = max(0, min(254, int(base * doppler)))
            fgd.arc((dc - rx, dc - ry, dc + rx, dc + ry), start=a0, end=a1,
                    fill=lum, width=max(2, int(2.8 * k)))
    fg = fg.filter(ImageFilter.GaussianBlur(1.6 * k)).rotate(14, resample=Image.Resampling.BICUBIC)
    img = ImageChops.add(img, fg.crop((-ox, -oy, w - ox, h - oy)))
    return img


def liquid_mercury(w, h):
    """Domain-warped ferrofluid / brushed liquid chrome with Blinn-Phong lighting."""
    gw = min(560, max(360, w // 4))
    gh = max(202, int(gw * h / w))
    z = [0.0] * (gw * gh)

    for j in range(gh):
        v = (j / gh) * 3.4 - 1.7
        row = j * gw
        for i in range(gw):
            u = (i / gw) * 6.0 - 3.0
            qx = math.sin(1.45 * u + 1.35 * math.cos(1.30 * v + 0.5))
            qy = math.cos(1.35 * v - 1.25 * math.sin(1.40 * u - 0.3))
            rx = math.sin(2.10 * u + 1.85 * qx + 0.75 * math.sin(2.6 * v + 1.5 * qy))
            ry = math.cos(2.05 * v - 1.75 * qy + 0.65 * math.cos(2.4 * u - 1.3 * qx))
            z[row + i] = 0.62 * rx + 0.38 * ry + 0.18 * math.sin(4.4 * u + 3.2 * ry)

    buf = bytearray(gw * gh)
    # Key light from upper-left + subtle rim from lower-right
    lx, ly, lz = -0.52, -0.60, 0.61
    hx, hy, hz = -0.28, -0.34, 0.90  # half-vector with viewer (0,0,1)
    scale_n = 28.0 / (6.0 / gw) * (1.0 / gw) * 9.5

    for j in range(gh):
        jm = max(0, j - 1) * gw
        jp = min(gh - 1, j + 1) * gw
        row = j * gw
        vy = (j / gh) - 0.5
        for i in range(gw):
            im = max(0, i - 1)
            ip = min(gw - 1, i + 1)
            dzdx = (z[row + ip] - z[row + im]) * scale_n
            dzdy = (z[jp + i] - z[jm + i]) * scale_n
            inv = 1.0 / math.sqrt(dzdx * dzdx + dzdy * dzdy + 1.0)
            nx, ny, nz = -dzdx * inv, -dzdy * inv, inv

            diff = max(0.0, nx * lx + ny * ly + nz * lz)
            rim = max(0.0, nx * 0.55 + ny * 0.58 + nz * 0.25)
            ndoth = max(0.0, nx * hx + ny * hy + nz * hz)
            spec = (ndoth ** 26) * 215.0 + (ndoth ** 6) * 44.0

            ux = (i / gw) - 0.5
            vig = max(0.28, 1.0 - 0.85 * (ux * ux + vy * vy * 1.35))
            val = (7.0 + 34.0 * (diff ** 1.6) + 18.0 * (rim ** 3.0) + spec) * vig
            buf[row + i] = max(6, min(246, int(val)))

    base = Image.frombytes("L", (gw, gh), bytes(buf))
    return base.resize((w, h), resample=Image.Resampling.BICUBIC)


def rain_noir(w, h):
    """Shallow-depth-of-field silver bokeh & midnight rain streaks on dark glass."""
    k = h / 1080.0
    rng = random.Random(1309)

    # Smooth vertical dusk ramp
    col = Image.new("L", (1, h))
    for y in range(h):
        t = y / max(1, h - 1)
        v = 7 + 20 * math.exp(-((t - 0.58) / 0.28) ** 2)
        col.putpixel((0, y), int(v))
    img = col.resize((w, h), resample=Image.Resampling.BILINEAR)

    # Three depth planes of monochrome optical bokeh
    specs = [
        (16, 46, 98, 22, 46, 30.0),   # far soft orbs
        (22, 20, 52, 26, 62, 11.0),   # mid bokeh with optical rim
        (18,  8, 22, 34, 88,  4.0),   # near crisp silver discs
    ]
    for count, rmin, rmax, lmin, lmax, blur_r in specs:
        layer = Image.new("L", (w, h), 0)
        ld = ImageDraw.Draw(layer)
        for _ in range(count):
            cx = int(rng.uniform(0.04, 0.96) * w)
            cy = int(rng.gauss(0.56, 0.20) * h)
            cy = max(int(0.08 * h), min(int(0.92 * h), cy))
            rad = rng.uniform(rmin, rmax) * k
            lum = rng.randint(lmin, lmax)
            ld.ellipse((cx - rad, cy - rad, cx + rad, cy + rad), fill=int(lum * 0.58))
            ld.ellipse(
                (cx - rad, cy - rad, cx + rad, cy + rad),
                outline=min(235, int(lum * 1.28)),
                width=max(1, int(2.0 * k)),
            )
        img = ImageChops.add(img, layer.filter(ImageFilter.GaussianBlur(blur_r * k)))

    # Crisp glass rain streaks & micro-droplets
    rain = Image.new("L", (w, h), 0)
    rd = ImageDraw.Draw(rain)
    for _ in range(130):
        x0 = rng.uniform(0, w)
        y0 = rng.uniform(0, h * 0.92)
        ln = rng.uniform(28, 135) * k
        dx = ln * -0.09
        lum = rng.randint(14, 48)
        rd.line([(x0, y0), (x0 + dx, y0 + ln)], fill=lum, width=max(1, int(1.1 * k)))
        if lum > 34:
            dr = max(1, int(1.6 * k))
            rd.ellipse((x0 + dx - dr, y0 + ln - dr, x0 + dx + dr, y0 + ln + dr), fill=min(155, lum * 2))
    img = ImageChops.add(img, rain.filter(ImageFilter.GaussianBlur(0.65 * k)))
    return img


def bauhaus_geo(w, h):
    """Swiss / Dieter Rams architectural geometry: balanced circles & hairline arcs."""
    k = h / 1080.0
    img = _canvas(w, h, 10)

    # Subtle radial studio backdrop glow
    glow = Image.new("L", (w, h), 0)
    gd = ImageDraw.Draw(glow)
    cx, cy = int(w * 0.50), int(h * 0.50)
    for i in range(20, 0, -1):
        t = i / 20.0
        rr = h * 0.62 * t
        gd.ellipse((cx - rr * 1.3, cy - rr, cx + rr * 1.3, cy + rr), fill=int(18 * (1.0 - t)))
    img = ImageChops.add(img, glow.filter(ImageFilter.GaussianBlur(30 * k)))

    d = ImageDraw.Draw(img)
    hw = max(1, int(round(1.25 * k)))

    # Precision horizon & golden-ratio axes
    hy = int(h * 0.56)
    gx = int(w * 0.618)
    d.line([(int(w * 0.08), hy), (int(w * 0.92), hy)], fill=34, width=hw)
    d.line([(gx, int(h * 0.12)), (gx, int(h * 0.88))], fill=28, width=hw)

    # Tonal stepped sphere at golden-ratio intersection
    R = int(h * 0.27)
    sphere = Image.new("L", (2 * R + 4, 2 * R + 4), 0)
    sd = ImageDraw.Draw(sphere)
    for y in range(2 * R):
        t = y / max(1, 2 * R - 1)
        chord = math.sqrt(max(0.0, 1.0 - (2.0 * t - 1.0) ** 2))
        x0 = int(R * (1.0 - chord)) + 2
        x1 = int(R * (1.0 + chord)) + 2
        val = int(175 * (1.0 - t) ** 1.35 + 16)
        sd.line([(x0, y + 2), (x1, y + 2)], fill=val)
    # Mask lower half into crisp Bauhaus horizontal louvers
    for y in range(R, 2 * R, max(3, int(9 * k))):
        for k_row in range(max(1, int(4 * k))):
            if y + k_row < 2 * R + 2:
                sd.line([(0, y + k_row + 2), (2 * R + 3, y + k_row + 2)], fill=0)
    img = ImageChops.add(img, _paste_centered(w, h, sphere, int(w * 0.44), int(h * 0.48)))
    d = ImageDraw.Draw(img)

    # Outer compass construction rings & counterpoint disc
    for mult, col_val in ((1.0, 96), (1.34, 48), (1.68, 30)):
        rr = int(R * mult)
        ox, oy = int(w * 0.44), int(h * 0.48)
        d.ellipse((ox - rr, oy - rr, ox + rr, oy + rr), outline=col_val, width=hw)

    r_small = int(h * 0.048)
    sx, sy = gx, int(h * 0.48 - R * 0.72)
    d.ellipse((sx - r_small, sy - r_small, sx + r_small, sy + r_small), fill=232)
    d.ellipse((gx - int(8 * k), hy - int(8 * k), gx + int(8 * k), hy + int(8 * k)), fill=10, outline=175, width=hw)
    return img


def _paste_centered(w, h, patch, cx, cy):
    layer = Image.new("L", (w, h), 0)
    layer.paste(patch, (cx - patch.width // 2, cy - patch.height // 2))
    return layer


DESIGNS = [
    ("mono-01", gradient_mesh, "soft gradient — the default wallpaper"),
    ("mono-02", ripple, "concentric rings"),
    ("mono-03", dot_grid, "dot grid"),
    ("mono-04", hatch, "diagonal hatch"),
    ("mono-05", spotlight, "spotlight"),
    ("mono-06", waves, "layered waves"),
    ("mono-07", eclipse, "solar eclipse corona"),
    ("mono-08", ridgeline, "topographic horizon ridges"),
    ("mono-09", monoliths, "architectural monoliths"),
    ("mono-10", silk_ribbon, "caustic silver silk ribbon"),
    ("mono-11", singularity, "gravitational lensing singularity"),
    ("mono-12", liquid_mercury, "specular liquid mercury ferrofluid"),
    ("mono-13", rain_noir, "midnight glass bokeh & rain"),
    ("mono-14", bauhaus_geo, "Swiss architectural geometry"),
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
        print(f"Known designs: {names}  (also accepts 1-14)", file=sys.stderr)
        return 1
    print("Done.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
