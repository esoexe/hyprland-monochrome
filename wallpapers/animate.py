#!/usr/bin/env python3
"""
Generate seamless looping animated monochrome live wallpapers for `awww`.

`awww` (successor to `swww`) natively plays animated GIFs on the Wayland
background layer, caching LZ4-compressed frames in memory for minimal CPU use.

    python3 animate.py                        # 1280x720, 36 frames, both designs
    python3 animate.py 1920 1080 --frames 48  # Full HD master loops
    python3 animate.py --only orbital
"""

from __future__ import annotations

import argparse
import math
import random
from pathlib import Path
from PIL import Image, ImageChops, ImageDraw, ImageFilter

HERE = Path(__file__).resolve().parent


def render_orbital_frame(w: int, h: int, phase: float) -> Image.Image:
    """Seamless 0..1 loop: Bauhaus celestial orrery with harmonic moons."""
    k = min(w, h) / 1080.0
    cx, cy = w * 0.5, h * 0.5
    tau = math.tau * phase

    img = Image.new("L", (w, h), 8)
    d = ImageDraw.Draw(img)

    # Subtle architectural crosshairs
    d.line([(int(w * 0.08), int(cy)), (int(w * 0.92), int(cy))], fill=18, width=1)
    d.line([(int(cx), int(h * 0.08)), (int(cx), int(h * 0.92))], fill=18, width=1)

    r_core = min(w, h) * 0.17

    # Soft breathing ambient halo behind central sphere
    halo = Image.new("L", (w, h), 0)
    hd = ImageDraw.Draw(halo)
    hr = r_core * (1.26 + 0.06 * math.sin(tau))
    hd.ellipse((cx - hr, cy - hr, cx + hr, cy + hr), fill=38)
    halo = halo.filter(ImageFilter.GaussianBlur(26 * k))
    img = ImageChops.add(img, halo)
    d = ImageDraw.Draw(img)

    # Three orbital tracks + rotating degree ticks on middle ring
    orbits = [r_core * 1.48, r_core * 1.98, r_core * 2.52]
    ring_cols = [42, 34, 26]
    for r, col in zip(orbits, ring_cols):
        d.ellipse((cx - r, cy - r, cx + r, cy + r), outline=col, width=max(1, int(1.3 * k)))

    r_tick = orbits[1]
    for idx in range(36):
        ang = tau / 36.0 * 1.0 + idx * (math.tau / 36.0)
        # Note: 36 identical ticks -> advancing by 1 tick spacing over phase 0..1 is 100% seamless!
        r_in = r_tick - (5 * k if idx % 3 == 0 else 2.5 * k)
        r_out = r_tick + (5 * k if idx % 3 == 0 else 2.5 * k)
        x0, y0 = cx + math.cos(ang) * r_in, cy + math.sin(ang) * r_in
        x1, y1 = cx + math.cos(ang) * r_out, cy + math.sin(ang) * r_out
        d.line([(x0, y0), (x1, y1)], fill=58 if idx % 3 == 0 else 32, width=1)

    # Central sphere with Venetian horizontal slices & subtly oscillating rim light
    sphere = Image.new("L", (w, h), 0)
    sd = ImageDraw.Draw(sphere)
    ri = int(r_core)
    light_shift = 0.08 * math.sin(tau)
    for dy in range(-ri, ri + 1):
        span = math.sqrt(max(0.0, r_core * r_core - dy * dy))
        t = (dy + r_core) / (2.0 * r_core)
        val = int(max(14, min(232, 224 * (max(0.0, 1.0 - t + light_shift) ** 1.35) + 14)))
        if dy > 0 and (int(dy / max(1.0, 4.2 * k)) % 2 == 0):
            val = 10
        sd.line([(cx - span, cy + dy), (cx + span, cy + dy)], fill=val, width=1)
    img = Image.composite(sphere, img, sphere.point(lambda p: 255 if p > 0 else 0))
    d = ImageDraw.Draw(img)
    d.ellipse((cx - r_core, cy - r_core, cx + r_core, cy + r_core), outline=150, width=max(1, int(1.4 * k)))

    # 3 orbiting moons in integer harmonic ratios (1x, -1x, 2x -> exact 1-period loop!)
    moons = [
        (orbits[0],  1.0 * tau - 0.6, 11.0 * k, 242),
        (orbits[1], -1.0 * tau + 1.8,  7.5 * k, 196),
        (orbits[2],  2.0 * tau + 0.4,  5.0 * k, 155),
    ]
    glow = Image.new("L", (w, h), 0)
    gd = ImageDraw.Draw(glow)
    for rad, ang, mr, col in moons:
        mx = cx + math.cos(ang) * rad
        my = cy + math.sin(ang) * rad
        # Faint wake arc behind each moon
        gd.ellipse((mx - mr * 2.4, my - mr * 2.4, mx + mr * 2.4, my + mr * 2.4), fill=int(col * 0.38))
        d.ellipse((mx - mr, my - mr, mx + mr, my + mr), fill=col)

    glow = glow.filter(ImageFilter.GaussianBlur(7 * k))
    img = ImageChops.add(img, glow)
    return img


def render_pulse_frame(w: int, h: int, phase: float) -> Image.Image:
    """Seamless 0..1 loop: Breathing relativistic singularity & orbiting plasma knots."""
    k = min(w, h) / 1080.0
    cx, cy = w * 0.5, h * 0.5
    r0 = min(w, h) * 0.165
    tau = math.tau * phase

    img = Image.new("L", (w, h), 4)
    sd = ImageDraw.Draw(img)
    rng = random.Random(1101)
    for _ in range(180):
        sx = rng.randrange(w)
        sy = rng.randrange(h)
        twinkle = 0.72 + 0.28 * math.sin(tau + rng.random() * math.tau)
        sd.point((sx, sy), fill=max(10, int(rng.randint(35, 145) * twinkle)))

    # Breathing gravitational lensing halo
    halo = Image.new("L", (w, h), 0)
    hd = ImageDraw.Draw(halo)
    breath = 1.0 + 0.055 * math.sin(tau)
    for i in range(28, 0, -1):
        t = i / 28.0
        rad = r0 * (1.0 + t * 1.45 * breath)
        lum = int(205 * ((1.0 - t) ** 2.3) * (0.92 + 0.08 * math.sin(tau)))
        hd.ellipse((cx - rad, cy - rad * 0.93, cx + rad, cy + rad * 0.93), fill=lum)
    halo = halo.filter(ImageFilter.GaussianBlur(18 * k))

    diag = int(math.hypot(w, h))
    dc = diag // 2

    def make_disc(front: bool) -> Image.Image:
        layer = Image.new("L", (diag, diag), 0)
        ld = ImageDraw.Draw(layer)
        for ring in range(26):
            f = ring / 25.0
            rx = r0 * (1.14 + f * 2.10)
            ry = rx * 0.15
            # Revolving azimuthal density wave (m=2 harmonic -> exact loop over 0..2pi!)
            base = 238 * math.exp(-f * 1.4) * (0.62 + 0.38 * math.sin(f * 12.0 - tau) ** 2)
            start_deg = -4.0 if front else 176.0
            for seg in range(16):
                a0 = start_deg + seg * 11.5
                a1 = a0 + 12.2
                mid = math.radians((a0 + a1) * 0.5)
                doppler = (0.92 - 0.24 * math.cos(mid)) * (0.86 + 0.14 * math.cos(2.0 * mid - tau))
                lum = max(0, min(252, int(base * doppler)))
                ld.arc((dc - rx, dc - ry, dc + rx, dc + ry), start=a0, end=a1,
                       fill=lum, width=max(2, int(2.6 * k)))
        return layer.filter(ImageFilter.GaussianBlur(1.5 * k)).rotate(14, resample=Image.Resampling.BICUBIC)

    ox, oy = (diag - w) // 2, (diag - h) // 2
    bg = make_disc(front=False).crop((ox, oy, ox + w, oy + h))
    fg = make_disc(front=True).crop((ox, oy, ox + w, oy + h))

    img = ImageChops.add(img, halo)
    img = ImageChops.add(img, bg)

    # Occlude event-horizon shadow + crisp photon ring
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).ellipse((cx - r0, cy - r0, cx + r0, cy + r0), fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(1.2 * k))
    img = Image.composite(Image.new("L", (w, h), 2), img, mask)

    ring_lum = int(218 + 24 * math.sin(tau))
    ImageDraw.Draw(img).ellipse(
        (cx - r0 * 1.012, cy - r0 * 1.012, cx + r0 * 1.012, cy + r0 * 1.012),
        outline=ring_lum, width=max(1, int(2.0 * k))
    )
    img = ImageChops.add(img, fg)
    return img


ANIMATIONS = {
    "orbital": ("mono-live-01-orbital.gif", "Bauhaus celestial orrery loop", render_orbital_frame),
    "pulse":   ("mono-live-02-pulse.gif",   "breathing relativistic singularity loop", render_pulse_frame),
}


def save_loop_gif(frames: list[Image.Image], out_path: Path, fps: int = 24) -> None:
    # Build a shared 256-level pure greyscale palette so GIF quantization has ZERO colour drift
    pal = []
    for i in range(256):
        pal.extend((i, i, i))
    p_frames = []
    for fr in frames:
        p = Image.new("P", fr.size)
        p.putpalette(pal)
        p.paste(fr)
        p_frames.append(p)
    duration_ms = max(20, int(round(1000 / fps)))
    p_frames[0].save(
        out_path,
        save_all=True,
        append_images=p_frames[1:],
        duration=duration_ms,
        loop=0,
        optimize=True,
    )


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("width", nargs="?", type=int, default=1280)
    ap.add_argument("height", nargs="?", type=int, default=720)
    ap.add_argument("--frames", type=int, default=36)
    ap.add_argument("--fps", type=int, default=24)
    ap.add_argument("--only", default="", help="orbital or pulse")
    args = ap.parse_args()

    keys = [args.only] if args.only else list(ANIMATIONS.keys())
    for key in keys:
        if key not in ANIMATIONS:
            raise SystemExit(f"unknown animation '{key}' (choose from: {', '.join(ANIMATIONS)})")
        filename, desc, fn = ANIMATIONS[key]
        out = HERE / filename
        print(f"Rendering {filename} ({args.width}x{args.height}, {args.frames} frames @ {args.fps}fps — {desc})...")
        frames = [fn(args.width, args.height, i / args.frames) for i in range(args.frames)]
        save_loop_gif(frames, out, fps=args.fps)
        print(f"  wrote {out.name} ({out.stat().st_size // 1024} KB)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
