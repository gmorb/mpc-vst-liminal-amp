#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 the mpc-vst-liminal-amp contributors
"""make_art.py [--theme valve|grille|tolex] [--out DIR] -- the page's generated artwork (into vst/art by default).

  texture.png        1280x628: the page's bottom layer (a material, the block faceplates, the group rules)
  knob_big.png       160 px, 31 ticks, 128 frames: graphite body, engraved scale, ivory pointer, lit arc (accent)
  wordmark.png       "LIMINAL AMP  A2 · by Gm0rb"
  cap_*.png          the quiet lowercase captions (amp, cab, input, tone, out, ...)
  arrow_prev/next    the name stepper's tap zones
  menu_panel.png     616x236, what the AMP LIST / CAB LIST open on
Deterministic (fixed seeds). One colour, one meaning: the accent is only for a live value (knob arcs, the list's row in
use); everything else is ivory, grey and the material. The design system follows Liminal Hz (same family: dark,
authored, large type); the material is this plugin's own: a guitar amp (valve glow, speaker cloth, or tolex and jewel)."""
import argparse, math, os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont
from scipy.ndimage import gaussian_filter

W, H = 1280, 628
HERE = os.path.dirname(os.path.abspath(__file__))
FONTS = os.environ.get("MPC_VST_FONTS") or os.path.join(os.environ.get("MPC_VST", os.path.join(HERE, "..", "..", "mpc-vst-plugins")), "tools", "html_art", "fonts")
MONO = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"

THEMES = {   # accent: live values; glow: the material's light
    "valve":  dict(accent=(238, 152, 72), accent_glow=(255, 176, 96), light=(255, 150, 64)),
    "grille": dict(accent=(212, 176, 108), accent_glow=(236, 204, 140), light=(236, 196, 128)),
    "tolex":  dict(accent=(222, 92, 66), accent_glow=(255, 128, 92), light=(255, 80, 56)),
}

# block geometry shared with vst/layout.conf (page coordinates)
BLOCKS = [(18, 60, 616, 274), (646, 60, 616, 274)]
yy, xx = np.mgrid[0:H, 0:W].astype(float)


def fractal(scale, octaves=4, seed=0, shape=None):
    h, w = shape or (H, W)
    g = np.random.default_rng(seed); acc = np.zeros((h, w)); amp = 1.0; tot = 0
    for o in range(octaves):
        s_ = max(2, int(scale / 2 ** o))
        small = g.random((h // s_ + 3, w // s_ + 3))
        img = Image.fromarray((small * 255).astype(np.uint8)).resize(((w // s_ + 3) * s_, (h // s_ + 3) * s_), Image.BICUBIC)
        acc += amp * np.asarray(img, float)[:h, :w] / 255; tot += amp; amp *= 0.5
    return acc / tot


def glow_at(cx, cy, rx, ry, power=1.0):
    return np.exp(-(((xx - cx) / rx) ** 2 + ((yy - cy) / ry) ** 2)) ** power


def finish(rgb, grain=2.4, vignette=0.45, seed=7):
    rng = np.random.default_rng(seed)
    r = np.sqrt(((xx - W / 2) / (W / 2)) ** 2 + ((yy - H / 2) / (H / 2)) ** 2)
    rgb = rgb * (1 - vignette * np.clip(r - 0.3, 0, 1) ** 1.5)[..., None]
    rgb = rgb + rng.normal(0, grain, (H, W, 1))
    return np.clip(rgb, 0, 255)


# ---- the three materials ---------------------------------------------------------------------------------------------
# ---- the valves -------------------------------------------------------------------------------------------------------
# Three small-bottle preamp valves standing in sockets on a chassis lip in the lower right, drawn the way they look in a
# dark room: clear glass with a silvered getter flash at the crown, a dark ribbed anode with a slot, mica spacers, support
# rods, and the heater glowing orange through the slot and in the small loops above the plate. Heaters are always on (a
# dull idle glow even in silence); signal raises the filament temperature (redder -> yellower) and the bloom.
VALVES = (995, 1080, 1165)               # centres (page x); a chassis lip at y 612 hides their bases
LIP_Y = 612
SS = 4                                   # supersampling for the drawn parts


def filament_colour(t):
    """t 0..1: a heater's colour from dull red to hot orange-yellow"""
    stops = [(0.0, (150, 38, 14)), (0.35, (226, 88, 28)), (0.7, (255, 140, 52)), (1.0, (255, 182, 96))]
    for (t0, c0), (t1, c1) in zip(stops, stops[1:]):
        if t <= t1:
            f = (t - t0) / (t1 - t0)
            return np.array([c0[i] + (c1[i] - c0[i]) * f for i in range(3)]) / 255
    return np.array(stops[-1][1]) / 255


def _mask(draw_fn, w, h):
    m = Image.new("L", (w * SS, h * SS), 0)
    draw_fn(ImageDraw.Draw(m), SS)
    return np.asarray(m.resize((w, h), Image.LANCZOS), float) / 255


def valve(page, emis, occ, cx, heat, seed):
    """draws one valve into page (H x W x 3 float) and adds its light sources to emis (H x W x 3)"""
    lw, lh = 100, LIP_Y - 500 + 8                            # local canvas: x cx-50.., y 500..LIP_Y+8
    x0, y0 = cx - 50, 500
    P = page[y0:y0 + lh, x0:x0 + lw].copy()
    c = 50                                                   # local centre
    t = 0.18 + 0.82 * heat                                   # filament temperature
    fc = filament_colour(t)
    top = 18                                                 # the crown of the glass
    env = _mask(lambda d, s: (d.rounded_rectangle([(c - 24) * s, top * s, (c + 24) * s, (lh + 30) * s], radius=24 * s, fill=255),
                              d.rounded_rectangle([(c - 3) * s, (top - 5) * s, (c + 3) * s, (top + 4) * s], radius=3 * s, fill=255)), lw, lh)
    # glass body: a faint lift where the glass is, darker where it stands in front of the dark interior
    P = P * (1 - 0.06 * env[..., None]) + (env * 3.0)[..., None]
    # support rods
    rods = _mask(lambda d, s: [d.rectangle([(c + dx - 0.8) * s, 46 * s, (c + dx + 0.8) * s, lh * s], fill=255) for dx in (-17, 17)], lw, lh)
    P = P * (1 - rods[..., None]) + rods[..., None] * np.array([92, 86, 80]) * (0.7 + 0.5 * t)
    # anode: a dark grey box with folded edges, two ribs and a centre slot
    py0 = 56
    plate = _mask(lambda d, s: d.rectangle([(c - 15) * s, py0 * s, (c + 15) * s, lh * s], fill=255), lw, lh)
    slot = _mask(lambda d, s: d.rectangle([(c - 1.6) * s, (py0 + 6) * s, (c + 1.6) * s, lh * s], fill=255), lw, lh)
    ribs = _mask(lambda d, s: [d.rectangle([(c + dx - 0.7) * s, (py0 + 3) * s, (c + dx + 0.7) * s, lh * s], fill=255) for dx in (-9, 9)], lw, lh)
    edges = _mask(lambda d, s: [d.rectangle([(c + dx - 1.0) * s, py0 * s, (c + dx + 1.0) * s, lh * s], fill=255) for dx in (-14.5, 14.5)], lw, lh)
    ly = np.arange(lh)[:, None] * np.ones((1, lw)); lx = np.ones((lh, 1)) * np.arange(lw)[None, :]
    shade = 19 + 9 * np.exp(-((lx - (c - 8)) / 7) ** 2)                      # a soft sheen left of centre
    pcol = shade[..., None] * np.array([1.0, 0.97, 0.93]) + (fc * 255 * 0.03 * t)[None, None, :]
    P = P * (1 - plate[..., None]) + plate[..., None] * pcol
    P = P + (ribs * 8 + edges * 13)[..., None] * np.array([1.0, 0.95, 0.9]) * (0.7 + 0.4 * t)
    # the heater: through the slot, and two small loops above the plate
    loops = _mask(lambda d, s: [d.arc([(c + dx - 3.2) * s, (py0 - 7) * s, (c + dx + 3.2) * s, (py0 + 1) * s], 180, 360, fill=255, width=int(1.1 * s)) for dx in (-4, 4)], lw, lh)
    along = np.clip((ly - py0 - 6) / 10, 0, 1) * (0.45 + 0.55 * np.exp(-((ly - (py0 + 24)) / 22) ** 2))   # cooler at the ends
    fil = np.clip(slot * along + loops, 0, 1)
    P = P * (1 - 0.8 * slot[..., None])                                    # the slot is dark behind the glow
    hot = fil * (0.30 + 0.70 * t ** 1.4)
    P = P + (hot * 210)[..., None] * fc
    # mica spacer above the plate: a pale translucent disc seen edge-on, warmed by the heater under it
    mica = _mask(lambda d, s: d.rounded_rectangle([(c - 21) * s, 44 * s, (c + 21) * s, 47.2 * s], radius=1.5 * s, fill=255), lw, lh)
    P = P * (1 - 0.45 * mica[..., None]) + mica[..., None] * (np.array([110, 105, 98]) * 0.45 + fc * 255 * 0.22 * t)
    # getter flash: the silvered crown, darker toward the top, a little mottled
    g = np.random.default_rng(seed)
    mott = gaussian_filter(g.random((lh, lw)), 1.3)
    gm = env * np.clip((38 - ly) / 8, 0, 1)
    gcol = (16 + 26 * np.clip((ly - top) / 20, 0, 1) ** 1.5 + (mott - 0.5) * 18)[..., None] * np.array([0.96, 0.97, 1.0])
    gcol = gcol + (np.exp(-((lx - (c - 11)) / 4) ** 2) * np.exp(-((ly - 30) / 8) ** 2) * 46)[..., None] + (np.exp(-((ly - 36) / 2.5) ** 2) * gm * 30)[..., None] * fc * t    # its reflection
    P = P * (1 - 0.92 * gm[..., None]) + 0.92 * gm[..., None] * gcol
    # light inside the glass: the heater lights the envelope from within
    inner = gaussian_filter(fil, 6) * 5 * env * (1 - 0.95 * plate)               # the plate hides most of it
    P = P + (inner * 255 * (0.06 + 0.60 * t ** 1.5))[..., None] * fc
    # glass highlights: fresnel rims, a long specular streak on the left, a short one on the right, a glint on the crown
    rim = np.clip(env - gaussian_filter(env, 2.2), 0, 1)
    P = P + (rim * 60)[..., None] * (np.array([0.9, 0.88, 0.85]) * 0.5 + fc * 0.9 * t)
    streak = _mask(lambda d, s: d.rounded_rectangle([(c - 17) * s, (top + 16) * s, (c - 14.5) * s, (lh - 4) * s], radius=1.2 * s, fill=255), lw, lh)
    streak = gaussian_filter(streak, 0.7) * np.clip((ly - top - 14) / 20, 0, 1)
    P = P + (streak * 70)[..., None]
    streak2 = _mask(lambda d, s: d.rectangle([(c + 16.5) * s, (top + 26) * s, (c + 17.5) * s, (lh - 10) * s], fill=255), lw, lh)
    P = P + (gaussian_filter(streak2, 0.6) * 26)[..., None]
    glint = np.exp(-(((lx - (c - 9)) / 2.2) ** 2 + ((ly - (top + 6)) / 1.6) ** 2))
    P = P + (glint * 120)[..., None]
    page[y0:y0 + lh, x0:x0 + lw] = P
    occ[y0:y0 + lh, x0:x0 + lw] = np.maximum(occ[y0:y0 + lh, x0:x0 + lw], plate * (1 - slot) * 0.65 + gm * 0.5)
    # what lights the room: the heater, its warm interior, and a faint glass glow
    E = np.zeros((H, W)); E[y0:y0 + lh, x0:x0 + lw] = hot * 1.0 + inner * 0.4
    emis += E[..., None] * fc[None, None, :] * (0.25 + 0.75 * t ** 1.5)


def chassis(page, emis_bloom):
    """the amp's chassis lip the valves stand in: dark brushed steel, a bright top edge, sockets, warm reflections"""
    x0 = 918
    y = LIP_Y
    g = np.random.default_rng(5)
    brushed = gaussian_filter(g.normal(0, 1, (H - y, W - x0)), (0.4, 18)) * 3
    body = 13 + brushed + np.linspace(2, -3, H - y)[:, None]
    fade = smooth(x0, x0 + 40, np.arange(x0, W))[None, :]                      # its left end rounds off into the dark
    reg = page[y:, x0:]
    reg[:] = reg * (1 - fade[..., None]) + fade[..., None] * (body[..., None] * np.array([1.0, 0.97, 0.94]))
    edge = np.zeros((H - y, W - x0)); edge[0] = 1.0; edge[1] = 0.35
    reg += (edge * fade * 34)[..., None]
    for cx in VALVES:                                                            # sockets: a dark collar with a lit rim
        sx = cx - x0
        coll = np.exp(-(((np.arange(W - x0)[None, :] - sx) / 27) ** 8)) * np.exp(-((np.arange(H - y)[:, None] - 1.5) / 2.2) ** 2)
        reg *= (1 - 0.55 * coll)[..., None]
    # the lip catches the valves' light along its top edge
    refl = gaussian_filter(emis_bloom[y - 6:y + 4, x0:].mean(axis=0), (18, 0)) * 3.2
    reg[0:3] += refl[None, :, :] * fade[0][None, :, None] * 0.5



# ---- other valve designs (VALVE_STYLE) --------------------------------------------------------------------------------
# Each is a glass profile (half-width by height), its electrodes and where its getter sits. Local canvas as valve():
# 100 x 120 px, the chassis lip at local y 112.
def _profile_mask(rfun, lw, lh, c):
    S = SS
    ys = (np.arange(lh * S) + 0.5) / S; xs = (np.arange(lw * S) + 0.5) / S
    r = np.array([rfun(y) for y in ys])
    m = (np.abs(xs[None, :] - c) <= r[:, None]).astype(float)
    return np.asarray(Image.fromarray((m * 255).astype(np.uint8)).resize((lw, lh), Image.LANCZOS), float) / 255


def _sq(v):
    return math.sqrt(max(0.0, v))


def prof_el34(y):          # coke bottle: a narrow neck under a domed crown, flaring to wide shoulders
    if y < 6: return 0
    if y < 9: return 3.2                                             # exhaust tip
    if y < 22: return _sq(13 ** 2 - (22 - y) ** 2)                   # crown
    if y < 34: return 13 + (y - 22) * 0.12                           # neck
    if y < 54: return 14.4 + 12.6 * (0.5 - 0.5 * math.cos(math.pi * (y - 34) / 20))   # shoulder
    return 27


def prof_300b(y):          # ST "globe": a round bulb on a narrow waist
    if y < 8: return 0
    if y < 12: return 3.0
    if y < 82:
        g = _sq(31 ** 2 - (y - 42) ** 2)
        return max(g, 17 if y > 70 else 0)
    return 17


def prof_kt88(y):          # tall and straight with a low, flat shoulder and a small dome
    if y < 2: return 0
    if y < 5: return 3.0
    if y < 16: return _sq(15 ** 2 - (16 - y) ** 2) if y >= 1 else 0
    if y < 24: return 15 + (y - 16) * 1.5
    return 27


STYLES = {
    "el34": dict(prof=prof_el34, getter=("crown", 30), mica=(50, 46), plate=(60, 18), fil="slot", loops=True),
    "300b": dict(prof=prof_300b, getter=("side", (36, 26, 11)), mica=None, plate=None, fil="m", loops=False),
    "kt88": dict(prof=prof_kt88, getter=("crown", 22), mica=(36, 50), plate=(42, 21), fil="slot2", loops=True),
}


def valve_styled(page, emis, occ, cx, heat, seed, st):
    lw, lh = 100, LIP_Y - 500 + 8
    x0, y0 = cx - 50, 500
    P = page[y0:y0 + lh, x0:x0 + lw].copy()
    c = 50
    t = 0.18 + 0.82 * heat
    fc = filament_colour(t)
    ly = np.arange(lh)[:, None] * np.ones((1, lw)); lx = np.ones((lh, 1)) * np.arange(lw)[None, :]
    env = _profile_mask(st["prof"], lw, lh, c)
    P = P * (1 - 0.06 * env[..., None]) + (env * 3.0)[..., None]
    plate = np.zeros((lh, lw)); slot = np.zeros((lh, lw)); fil = np.zeros((lh, lw))
    if st["plate"]:
        py0, pw = st["plate"]
        rods = _mask(lambda d, s: [d.rectangle([(c + dx - 0.8) * s, (py0 - 12) * s, (c + dx + 0.8) * s, lh * s], fill=255) for dx in (-pw - 3, pw + 3)], lw, lh)
        P = P * (1 - rods[..., None]) + rods[..., None] * np.array([92, 86, 80]) * (0.7 + 0.5 * t)
        plate = _mask(lambda d, s: d.rectangle([(c - pw) * s, py0 * s, (c + pw) * s, lh * s], fill=255), lw, lh)
        slots = [0] if st["fil"] == "slot" else [-7, 7]
        slot = _mask(lambda d, s: [d.rectangle([(c + sx - 1.6) * s, (py0 + 6) * s, (c + sx + 1.6) * s, lh * s], fill=255) for sx in slots], lw, lh)
        ribs = _mask(lambda d, s: [d.rectangle([(c + dx - 0.7) * s, (py0 + 3) * s, (c + dx + 0.7) * s, lh * s], fill=255)
                                   for dx in np.linspace(-pw + 5, pw - 5, 5 if pw > 19 else 3) if all(abs(dx - sx) > 3 for sx in slots)], lw, lh)
        edges = _mask(lambda d, s: [d.rectangle([(c + dx - 1.0) * s, py0 * s, (c + dx + 1.0) * s, lh * s], fill=255) for dx in (-pw + 0.5, pw - 0.5)], lw, lh)
        shade = 19 + 9 * np.exp(-((lx - (c - pw * 0.5)) / (pw * 0.5)) ** 2)
        pcol = shade[..., None] * np.array([1.0, 0.97, 0.93]) + (fc * 255 * 0.03 * t)[None, None, :]
        P = P * (1 - plate[..., None]) + plate[..., None] * pcol
        P = P + (ribs * 8 + edges * 13)[..., None] * np.array([1.0, 0.95, 0.9]) * (0.7 + 0.4 * t)
        along = np.clip((ly - py0 - 6) / 10, 0, 1) * (0.45 + 0.55 * np.exp(-((ly - (py0 + 26)) / 24) ** 2))
        fil = slot * along
        if st["loops"]:
            fil = fil + _mask(lambda d, s: [d.arc([(c + sx + dx - 3.2) * s, (py0 - 7) * s, (c + sx + dx + 3.2) * s, (py0 + 1) * s], 180, 360, fill=255, width=int(1.1 * s))
                                            for sx in slots for dx in ((-4, 4) if len(slots) == 1 else (0,))], lw, lh)
        P = P * (1 - 0.8 * slot[..., None])
    else:
        # 300B: the M filament hangs in the bulb between two small anodes, a fine grid in front, mica above and below
        P = P + 0  # (glass already lifted)
        rods = _mask(lambda d, s: [d.rectangle([(c + dx - 0.7) * s, 22 * s, (c + dx + 0.7) * s, lh * s], fill=255) for dx in (-15, 15)], lw, lh)
        P = P * (1 - rods[..., None]) + rods[..., None] * np.array([92, 86, 80]) * (0.7 + 0.5 * t)
        plate = _mask(lambda d, s: [d.rectangle([(c + a) * s, 30 * s, (c + b) * s, 62 * s], fill=255) for a, b in ((-22, -13), (13, 22))], lw, lh)
        P = P * (1 - plate[..., None]) + plate[..., None] * np.array([24, 23, 22])
        P = P + (_mask(lambda d, s: [d.rectangle([(c + dx - 0.6) * s, 30 * s, (c + dx + 0.6) * s, 62 * s], fill=255) for dx in (-21, -14, 14, 21)], lw, lh) * 14)[..., None]
        pts = [(c - 10, 58), (c - 6, 33), (c - 2, 58), (c + 2, 33), (c + 6, 58), (c + 10, 33)]
        fil = _mask(lambda d, s: d.line([(x * s, y * s) for x, y in pts], fill=255, width=int(1.2 * s), joint="curve"), lw, lh)
        hooks = _mask(lambda d, s: [d.line([(x * s, (y - 4) * s), (x * s, y * s)], fill=255, width=int(0.6 * s)) for x, y in pts if y < 40], lw, lh)
        P = P + (hooks * 60)[..., None] * np.array([0.8, 0.78, 0.75])
        grid = _mask(lambda d, s: [d.line([((c - 12) * s, yy_ * s), ((c + 12) * s, yy_ * s)], fill=255, width=max(1, int(0.35 * s))) for yy_ in range(31, 62, 3)], lw, lh)
        P = P * (1 - 0.35 * grid[..., None])
        for my in (28, 64):
            mm = _mask(lambda d, s: d.rounded_rectangle([(c - 24) * s, my * s, (c + 24) * s, (my + 2.5) * s], radius=1 * s, fill=255), lw, lh)
            P = P * (1 - 0.45 * mm[..., None]) + mm[..., None] * (np.array([110, 105, 98]) * 0.45 + fc * 255 * 0.22 * t)
        base = _mask(lambda d, s: d.rectangle([(c - 17) * s, 82 * s, (c + 17) * s, lh * s], fill=255), lw, lh)  # the press and stem
        P = P * (1 - 0.5 * base[..., None]) + base[..., None] * np.array([30, 28, 26]) * 0.5
    hot = fil * (0.30 + 0.70 * t ** 1.4) * (0.6 if st["fil"] == "m" else 1.0)
    P = P + (hot * 215)[..., None] * fc
    if st["mica"]:
        my, mw = st["mica"]
        mica = _mask(lambda d, s: d.rounded_rectangle([(c - mw / 2) * s, my * s, (c + mw / 2) * s, (my + 3.2) * s], radius=1.5 * s, fill=255), lw, lh)
        P = P * (1 - 0.45 * mica[..., None]) + mica[..., None] * (np.array([110, 105, 98]) * 0.45 + fc * 255 * 0.22 * t)
    g = np.random.default_rng(seed)
    mott = gaussian_filter(g.random((lh, lw)), 1.3)
    kind, gp = st["getter"]
    if kind == "crown":
        gm = env * np.clip((gp - ly) / 7, 0, 1)
        gcol = (16 + 26 * np.clip(ly / gp, 0, 1) ** 1.5 + (mott - 0.5) * 18)[..., None] * np.array([0.96, 0.97, 1.0])
    else:
        gy, gx, gr = gp
        gm = env * np.clip(1.3 - np.sqrt(((lx - (c - gx)) / gr) ** 2 + ((ly - gy) / (gr * 1.3)) ** 2), 0, 1) ** 0.6
        gcol = (22 + 22 * np.exp(-((lx - (c - gx - 3)) / 4) ** 2) + (mott - 0.5) * 20)[..., None] * np.array([0.96, 0.97, 1.0])
    P = P * (1 - 0.9 * gm[..., None]) + 0.9 * gm[..., None] * gcol
    inner = gaussian_filter(fil, 6) * 5 * env * (1 - 0.95 * plate)
    P = P + (inner * 255 * (0.06 + 0.60 * t ** 1.5))[..., None] * fc
    rim = np.clip(env - gaussian_filter(env, 2.2), 0, 1)
    P = P + (rim * 60)[..., None] * (np.array([0.9, 0.88, 0.85]) * 0.5 + fc * 0.9 * t)
    # speculars follow the glass: a streak just inside its left edge, a fainter one on the right
    rr = np.array([st["prof"](y + 0.5) for y in range(lh)])
    sl = np.exp(-((lx - (c - rr[:, None] * 0.66)) / 1.1) ** 2) * (rr[:, None] > 6) * np.clip((ly - 10) / 14, 0, 1)
    sr = np.exp(-((lx - (c + rr[:, None] * 0.72)) / 0.7) ** 2) * (rr[:, None] > 6) * np.clip((ly - 20) / 14, 0, 1)
    P = P + (sl * 62 + sr * 22)[..., None] * env[..., None]
    top = next(y for y in range(lh) if rr[y] > 6)
    glint = np.exp(-(((lx - (c - rr[top + 6] * 0.4)) / 2.2) ** 2 + ((ly - (top + 5)) / 1.6) ** 2))
    P = P + (glint * 110)[..., None]
    page[y0:y0 + lh, x0:x0 + lw] = P
    occ[y0:y0 + lh, x0:x0 + lw] = np.maximum(occ[y0:y0 + lh, x0:x0 + lw], plate * (1 - slot) * 0.65 + gm * 0.5)
    E = np.zeros((H, W)); E[y0:y0 + lh, x0:x0 + lw] = hot * 1.0 + inner * 0.4
    emis += E[..., None] * fc[None, None, :] * (0.25 + 0.75 * t ** 1.5) * (0.6 if st["fil"] == "m" else 1.0)


# ---- strings into lattice (VALVE_STYLE "strings") ---------------------------------------------------------------------
# Silence: six still strings between a nut and a bridge. A touch sets them ringing; harder, points of light appear along
# each string (the signal sampled); harder still, the model's network lights through those points from the low E (its
# input) to the output node above the high E -- a WaveNet stack, dilations 1, 2, 4, 8, then the head straight up -- while
# the strings settle back to rails. Only the picture's box changes; it fades out within 18 px of the box's edges.
STR_X0, STR_X1 = 902, 1244                                    # nut and bridge (page x)
STR_Y = [552, 563.5, 575, 586.5, 598, 609.5]                  # high E (top) .. low E (bottom)
STR_G = [0.8, 0.95, 1.1, 1.5, 1.85, 2.2]
LAT_COLS = np.linspace(918, 1228, 17)
LAT_DIL = [1, 2, 4, 8]


def _pmask(draw, x0=880, y0=500, x1=1262, y1=628):
    w, h = x1 - x0, y1 - y0
    im = Image.new("L", (w * SS, h * SS), 0); d = ImageDraw.Draw(im)
    draw(d, lambda x, y: ((x - x0) * SS, (y - y0) * SS), SS)
    out = np.zeros((H, W)); out[y0:y1, x0:x1] = np.asarray(im.resize((w, h), Image.LANCZOS), float) / 255
    return out


def _band(v, a, b):
    return float(np.clip((v - a) / (b - a), 0, 1))


def strings_lattice(page, lv):
    fc = filament_colour(0.25 + 0.75 * lv)
    ring = _band(lv, 0.0, 0.35) * (1 - 0.75 * _band(lv, 0.55, 0.95))      # how hard the strings ring
    beads = _band(lv, 0.3, 0.6)                                            # the sampled points appear
    rails = 1 - 0.6 * _band(lv, 0.6, 1.0)                                  # strings settle back
    # hardware: nut and bridge saddles, steel strings
    hw = _pmask(lambda d, f, s: [d.rounded_rectangle([*f(STR_X0 - 7, 544), *f(STR_X0, 618)], radius=2 * s, fill=255),
                                 d.rounded_rectangle([*f(STR_X1, 545), *f(STR_X1 + 6, 617)], radius=2 * s, fill=255)])
    page[:] = page * (1 - hw[..., None]) + hw[..., None] * np.array([150, 140, 122]) * 0.5
    page[:] = page + (_pmask(lambda d, f, s: [d.line([f(STR_X0 - 6, 544.5), f(STR_X0 - 1, 544.5)], fill=255, width=s),
                                              d.line([f(STR_X1 + 1, 545.5), f(STR_X1 + 5, 545.5)], fill=255, width=s)]) * 40)[..., None]
    E = np.zeros((H, W))
    xs = np.linspace(STR_X0, STR_X1, 140)
    env = np.sin(np.pi * (xs - STR_X0) / (STR_X1 - STR_X0))
    for k, (y, g) in enumerate(zip(STR_Y, STR_G)):
        line = _pmask(lambda d, f, s, y=y, g=g: d.line([f(STR_X0, y), f(STR_X1, y)], fill=255, width=max(1, int(round(g * s)))))
        steel = np.array([150, 146, 140]) * (0.5 + 0.12 * k / 5)
        page[:] = page * (1 - 0.9 * line[..., None]) + 0.9 * line[..., None] * steel
        if k >= 3:                                                          # wound strings
            page[:] = page - (line * ((np.sin(xx * 2.3) > 0.35) * 0.3) * 40)[..., None]
        a = ring * (2.0 + 4.0 * k / 5) * (1.4 if lv > 0 else 0)
        if a > 0.2:
            lens = _pmask(lambda d, f, s, y=y, a=a, g=g: d.polygon([f(x, y - e * a - g / 2) for x, e in zip(xs, env)] +
                                                                    [f(x, y + e * a + g / 2) for x, e in zip(xs[::-1], env[::-1])], fill=255))
            E += np.clip(lens - line, 0, 1) * 0.09 * ring
        E += line * (0.04 + 0.5 * ring * rails + 0.18 * _band(lv, 0.5, 1.0) * rails)
    # the network on the strings: rows are the strings (low E = input row), columns are samples in time
    rows = STR_Y[::-1]                                                      # rows[0] = low E
    act = {(4, 16)}
    for r in range(4, 0, -1):
        for (rr, c) in list(act):
            if rr == r:
                act.add((r - 1, c)); act.add((r - 1, c - LAT_DIL[r - 1]))
    if beads > 0:
        dots = _pmask(lambda d, f, s: [d.ellipse([*f(LAT_COLS[c] - 1.8, rows[r] - 1.8), *f(LAT_COLS[c] + 1.8, rows[r] + 1.8)], fill=255)
                                       for r in range(5) for c in range(17)])
        E += dots * 0.35 * beads
        faint = _pmask(lambda d, f, s: [d.line([f(LAT_COLS[c], rows[r]), f(LAT_COLS[c + dl], rows[r + 1])], fill=255, width=max(1, int(0.45 * s)))
                                        for r, dl in enumerate(LAT_DIL) for c in range(17 - dl)])
        page[:] = page + (faint * 14 * beads)[..., None] * np.array([0.9, 0.85, 0.8])
    for r in range(6):
        lit = _band(lv, 0.5 + r * 0.08, 0.58 + r * 0.08)                   # input row first, the output node last
        if lit <= 0:
            continue
        def hot(d, f, s, r=r):
            if r == 5:                                                      # the head: straight up to the output
                d.line([f(LAT_COLS[16], rows[4]), f(LAT_COLS[16], rows[5])], fill=255, width=max(1, int(1.1 * s)))
                d.ellipse([*f(LAT_COLS[16] - 4, rows[5] - 4), *f(LAT_COLS[16] + 4, rows[5] + 4)], fill=255)
                return
            for (rr, c) in act:
                if rr == r:
                    d.ellipse([*f(LAT_COLS[c] - 2.8, rows[r] - 2.8), *f(LAT_COLS[c] + 2.8, rows[r] + 2.8)], fill=255)
                    if r < 4:
                        for (r2, c2) in act:
                            if r2 == r + 1 and (c2 == c or c2 - LAT_DIL[r] == c):
                                d.line([f(LAT_COLS[c], rows[r]), f(LAT_COLS[c2], rows[r2])], fill=210, width=max(1, int(1.0 * s)))
        E += _pmask(hot) * lit
    page[:] = page + (E * 210)[..., None] * fc
    bl = gaussian_filter(E, 3) * 1.0 + gaussian_filter(E, 10) * 2.0 + gaussian_filter(E, 24) * 4.0
    page[:] = page + (bl * 255 * 0.45)[..., None] * fc
    return page


VALVE_STYLE = os.environ.get("NAM_VALVE", "kt88")      # kt88 (shipped), mini (12AX7-like), el34, 300b; strings: strings into lattice; none: the room only


def tubes(light, lv=0.0, nod=None):
    """valve glow. lv 0..1 is the amp's level: 0 the idle heaters, 1 all three hot (they warm one after another).
    nod: an optional Liminal Hz layer drawn on the room, behind the valves"""
    base = np.full((H, W, 3), 9.0) * np.array([1.0, 0.96, 0.92])
    haze = fractal(240, 4, seed=3)
    base += ((haze - 0.5) * 9 + 2)[..., None] * np.array([1.0, 0.92, 0.84])
    lt = np.array(light, float) / 255
    # the room's standing light from the heaters (always on: the same at every level, so it never seams)
    amb = glow_at(1080, 640, 300, 200) * 22 + glow_at(1080, 620, 150, 90) * 16
    base += amb[..., None] * lt
    g = np.random.default_rng(11); dust = np.zeros((H, W))
    for _ in range(900):
        x, y_ = int(g.uniform(0, W)), int(g.uniform(0, H)); dust[y_, x] = g.uniform(0.3, 1.0)
    dust = gaussian_filter(dust, 0.7)
    base += (dust * (0.15 + glow_at(1080, 600, 280, 180) * 2.0) * 38)[..., None] * lt
    page = finish(base, grain=2.2, vignette=0.5)
    if nod:
        page = NODS[nod](page, light)
    if VALVE_STYLE == "none":
        return page
    if VALVE_STYLE == "strings":
        return strings_lattice(page, lv)
    emis = np.zeros((H, W, 3)); occ = np.zeros((H, W))
    for k, cx in enumerate(VALVES):
        h = min(1.0, max(0.0, (lv - 0.12 * k) / 0.6))
        if VALVE_STYLE == "mini":
            valve(page, emis, occ, cx, h, seed=50 + k)
        else:
            valve_styled(page, emis, occ, cx, h, 50 + k, STYLES[VALVE_STYLE])
    # bloom: light scattered from the heaters, a tight core and a wide soft halo (round, never boxed)
    bloom = np.stack([gaussian_filter(emis[..., i], 4) * 1.2 + gaussian_filter(emis[..., i], 14) * 2.0 + gaussian_filter(emis[..., i], 32) * 16.0
                      for i in range(3)], -1) * 255
    page = page + bloom * 0.80 * (1 - occ)[..., None]     # the anodes and getters stand in front of the haze
    chassis(page, bloom)
    return page


# the tubes' picture: the lower right of the page at 25 heats (0 idle .. 24). Pictures are opaque, so each state is the
# page's own pixels there; what the heat adds is kept inside round pools around the valves (never the picture's
# rectangle), so the picture's edge is the page texture itself (state 0) and no box shows.
TUBE_BOX = (880, 510, 382, 118)          # x, y, w, h in page coordinates (layout.conf: y + 86)
TUBE_STEPS = 24


def smooth(e0, e1, v):
    t = np.clip((v - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


def tube_pools():
    pool = np.zeros((H, W))
    for cx in VALVES:
        r = np.sqrt(((xx - cx) / 80) ** 2 + ((yy - 612) / 100) ** 2)
        pool = np.maximum(pool, 1 - smooth(0.45, 1.0, r))
    x0, y0, bw, bh = TUBE_BOX
    return pool * smooth(y0, y0 + 6, yy) * (1 - smooth(x0 + bw - 6, x0 + bw, xx)) * smooth(x0, x0 + 6, xx)


def tube_states(outdir, light, nod=None):
    x0, y0, bw, bh = TUBE_BOX
    if VALVE_STYLE == "strings":
        win = (smooth(x0, x0 + 18, xx) * (1 - smooth(x0 + bw - 18, x0 + bw, xx)) * smooth(y0, y0 + 18, yy))[..., None]
    else:
        win = tube_pools()[..., None]
    idle = tubes(light, 0.0, nod)
    for k in range(TUBE_STEPS + 1):
        lit = idle + win * (tubes(light, k / TUBE_STEPS, nod) - idle) if k else idle
        img = Image.fromarray(np.clip(lit, 0, 255).astype(np.uint8)).crop((x0, y0, x0 + bw, y0 + bh))
        img.save(os.path.join(outdir, "tube_%d.png" % k), optimize=True)


# ---- the amp's pluck print (14 pictures under the amp's name, 25 states each) ------------------------------------------
# One plucked note through the loaded capture, as a clip overview: dense hairlines mirrored about the centre, height =
# that time slice's level (8), texture = the capture's drive (clean: peaky lines; crunch: evened; high gain: a block).
# State 0 is silence (only the centre line). Pictures are opaque, so each column carries the strip's own dark panel.
WAVE_BOX = (32, 213, 588, 44)            # page coordinates (layout.conf: y + 86)
WAVE_COLS = 14
WAVE_BG = (9, 9, 10)


def wave_column(level, tex, w, h):
    S_ = 4
    im = Image.new("RGBA", (w * S_, h * S_), WAVE_BG + (255,)); d = ImageDraw.Draw(im)
    cy = h / 2 * S_
    d.line([(0, cy), (w * S_, cy)], fill=(255, 255, 255, 16), width=S_)
    if level < 0:
        return im.resize((w, h), Image.LANCZOS)
    g = np.random.default_rng(level * 7 + tex * 101)
    A = (level + 1) / 8 * (h / 2 - 2.5) * S_
    mist = Image.new("RGBA", im.size, (0, 0, 0, 0)); dm = ImageDraw.Draw(mist)
    dm.rectangle([-20 * S_, cy - A * 0.85, (w + 20) * S_, cy + A * 0.85], fill=(238, 152, 72, 64))
    mist = mist.filter(ImageFilter.GaussianBlur(4 * S_))
    im = Image.alpha_composite(im, mist)
    core = Image.new("RGBA", im.size, (0, 0, 0, 0)); dc = ImageDraw.Draw(core)
    for i in range(0, w * S_, 2 * S_):
        j = {0: g.uniform(0.22, 1.0) ** 1.6, 1: g.uniform(0.68, 1.0), 2: g.uniform(0.92, 1.0)}[tex]
        hh = A * j
        dc.line([(i + S_, cy - hh), (i + S_, cy + hh)], fill=(244, 226, 198, 220), width=int(1.2 * S_))
    im = Image.alpha_composite(im, core)
    return im.resize((w, h), Image.LANCZOS)


def wave_states(outdir):
    x, y, w, h = WAVE_BOX
    cw = w // WAVE_COLS
    wave_column(-1, 0, cw, h).convert("RGB").save(os.path.join(outdir, "wave_0.png"), optimize=True)
    for tex in range(3):
        for lv in range(8):
            wave_column(lv, tex, cw, h).convert("RGB").save(os.path.join(outdir, "wave_%d.png" % (1 + tex * 8 + lv)), optimize=True)


def cloth(light):
    """speaker cloth: a woven grille over a warm dark, a soft cone ring behind the right block, gold piping"""
    g = np.random.default_rng(21)
    base = np.full((H, W, 3), 10.0) * np.array([1.0, 0.95, 0.86])
    # weave: threads every 4 px both ways, alternating over/under, with slub and fibre noise
    px, py = 4.0, 4.0
    warp = 0.5 + 0.5 * np.cos(2 * math.pi * xx / px); weft = 0.5 + 0.5 * np.cos(2 * math.pi * yy / py)
    over = (np.floor(xx / px) + np.floor(yy / py)) % 2
    thread = np.where(over > 0, warp, weft)
    slub = gaussian_filter(g.normal(0, 1, (H, W)), 1.2)
    weave = (thread - 0.5) * 7 + slub * 3.2
    base += weave[..., None] * np.array([1.0, 0.9, 0.72])
    base += ((fractal(180, 4, seed=5) - 0.5) * 12)[..., None] * np.array([1.0, 0.9, 0.75])   # uneven lighting on the cloth
    # the cone: concentric rings of a speaker seen through the cloth, centred below the right block
    cx, cy = 1090, 620
    r = np.sqrt((xx - cx) ** 2 + (yy - cy) ** 2)
    rings = 0.5 + 0.5 * np.cos(2 * math.pi * r / 26)
    env = np.exp(-((r - 250) / 210) ** 2) * (r > 40)
    base += (np.clip(rings - 0.55, 0, 1) * env * 9)[..., None] * np.array([1.0, 0.92, 0.78])
    cap = glow_at(cx, cy, 120, 120)                                              # the dust cap, lit
    base += (cap * 30)[..., None] * np.array(light, float) / 255
    # gold piping: a thin rule along the top of the controls band
    pipe = Image.new("L", (W, H), 0); dpp = ImageDraw.Draw(pipe)
    dpp.line([(0, 342), (W, 342)], fill=255, width=1)
    pm = np.asarray(pipe.filter(ImageFilter.GaussianBlur(0.8)), float) / 255
    base += (pm * 44)[..., None] * np.array([1.0, 0.82, 0.5])
    return finish(base, grain=2.0, vignette=0.5)


def tolex(light):
    """black pebbled tolex, lit from the upper left; brass corner caps; a pilot jewel glowing in the lower right"""
    g = np.random.default_rng(31)
    n = gaussian_filter(g.normal(0, 1, (H, W)), 1.1) + 0.6 * gaussian_filter(g.normal(0, 1, (H, W)), 2.2)
    gy, gx = np.gradient(n)
    lit = np.clip(0.5 - 1.7 * (gx * 0.7 + gy * 0.7), 0, 1)                      # top-left key light on the pebbles
    base = np.full((H, W, 3), 9.0) + (lit * 11 - 4)[..., None] * np.array([1.0, 0.98, 0.95])
    base += ((fractal(300, 3, seed=9) - 0.5) * 8)[..., None]
    lt = np.array(light, float) / 255
    jx, jy = 1130, 556                                                           # the jewel
    halo = glow_at(jx, jy, 150, 120)
    base += (halo * 30)[..., None] * lt
    jew = Image.new("L", (W, H), 0); dj = ImageDraw.Draw(jew)
    dj.ellipse([jx - 30, jy - 30, jx + 30, jy + 30], fill=255)
    jm = np.asarray(jew.filter(ImageFilter.GaussianBlur(0.8)), float) / 255
    core = glow_at(jx - 6, jy - 8, 22, 22)
    base = base * (1 - jm)[..., None] + (jm * (60 + core * 170))[..., None] * lt + (jm * 8)[..., None]
    ring = Image.new("L", (W, H), 0); dr = ImageDraw.Draw(ring)
    dr.ellipse([jx - 42, jy - 42, jx + 42, jy + 42], outline=255, width=7)
    rm = np.asarray(ring.filter(ImageFilter.GaussianBlur(1.0)), float) / 255
    sheen = np.clip(0.5 - ((xx - jx) * 0.7 + (yy - jy) * 0.7) / 90, 0, 1)         # a chrome bezel, lit from the top left
    base = base * (1 - rm)[..., None] + (rm * (46 + 120 * sheen))[..., None] * np.array([1.0, 0.98, 0.94])
    for cx_, cy_, sx, sy in ((0, H, 1, -1), (W, H, -1, -1)):                    # brass corner caps, bottom corners
        cap = Image.new("L", (W, H), 0); dc = ImageDraw.Draw(cap)
        dc.polygon([(cx_, cy_), (cx_ + sx * 70, cy_), (cx_ + sx * 70, cy_ + sy * 10), (cx_ + sx * 10, cy_ + sy * 70), (cx_, cy_ + sy * 70)], fill=255)
        cm = np.asarray(cap.filter(ImageFilter.GaussianBlur(0.6)), float) / 255
        base = base * (1 - cm)[..., None] + (cm * (50 + 24 * np.sin(xx / 7 + yy / 9)))[..., None] * np.array([1.0, 0.82, 0.48])
    return finish(base, grain=1.8, vignette=0.45)



# ---- nods to Liminal Hz (optional layers on any material) -------------------------------------------------------------
def nod_air(base, light):
    """Liminal Hz's air: faint wavefront rings leaving the light source and slow flow-field wisps drifting across the page"""
    lt = np.array(light, float) / 255
    warp = (fractal(150, 3, seed=31) - 0.5) * 22
    dist = np.hypot(xx - 1100, yy - 660) + warp
    rings = np.zeros((H, W))
    for k in range(14):
        r = 130 + k * 52
        rings += np.exp(-((dist - r) / 1.5) ** 2) * 24 * math.exp(-k * 0.16)
    rings *= np.clip((dist - 330) / 120, 0, 1)                    # none around the tubes themselves: they glow on their own
    base = base + (rings * 0.45)[..., None] * lt
    ang = fractal(190, 3, seed=5) * 4 * math.pi
    wl = Image.new("L", (W, H), 0); dw = ImageDraw.Draw(wl)
    g = np.random.default_rng(41)
    for _ in range(95):
        x, y_ = g.uniform(0, W), g.uniform(0, H); pts = []
        for _s in range(170):
            if not (0 <= x < W and 0 <= y_ < H): break
            pts.append((x, y_)); a = ang[int(y_), int(x)]; x += 3.2 * math.cos(a); y_ += 3.2 * math.sin(a)
        if len(pts) > 12: dw.line(pts, fill=int(g.uniform(9, 26)), width=1)
    wm = np.asarray(wl.filter(ImageFilter.GaussianBlur(0.9)), float)
    return base + wm[..., None] * np.array([1.0, 0.93, 0.84])


def nod_door(base, light):
    """Liminal Hz's threshold: the tubes stand in a lit doorway at the far end of the room, and a hairline carries the signal to it"""
    lt = np.array(light, float) / 255
    dm = Image.new("L", (W, H), 0); dd = ImageDraw.Draw(dm)
    x0, x1, top = 962, 1238, 506
    dd.rounded_rectangle([x0, top, x1, H + 120], radius=138, fill=255)
    door = np.asarray(dm, float) / 255
    grad = np.clip((yy - top) / (H - top), 0, 1)
    base = base + (door * (6 + 26 * grad ** 1.4))[..., None] * lt
    edge = np.asarray(dm.filter(ImageFilter.GaussianBlur(1.0)), float) / 255 - np.asarray(dm.filter(ImageFilter.GaussianBlur(3.2)), float) / 255
    base = base + (np.clip(edge, 0, 1) * 110)[..., None] * np.array([1.0, 0.88, 0.72])
    base = base + (glow_at(1100, 640, 330, 90) * 26)[..., None] * lt                    # light spilling onto the floor
    ln = Image.new("L", (W, H), 0); dl = ImageDraw.Draw(ln)
    dl.line([(24, 613), (1256, 613)], fill=26, width=1)
    base = base + (np.asarray(ln, float))[..., None] * np.array([1.0, 0.95, 0.88])
    bead = glow_at(930, 613, 34, 12) * 120 + glow_at(930, 613, 7, 4) * 170              # the travelling glow, held at one place
    return base + bead[..., None] * (lt * 0.8 + 0.2)


NODS = {"air": nod_air, "door": nod_door}

MATERIALS = {"valve": tubes, "grille": cloth, "tolex": tolex}


def faceplates(img):
    """the blocks' faceplates and the switch-band rules, drawn into the texture (no component draws them)"""
    ov = Image.new("RGBA", (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(ov)
    for (x, y_, w, h) in BLOCKS:
        d.rounded_rectangle([x, y_, x + w, y_ + h], radius=10, fill=(5, 5, 6, 118), outline=(255, 255, 255, 22), width=1)
        d.line([(x + 14, y_ + 40), (x + w - 14, y_ + 40)], fill=(255, 255, 255, 16), width=1)       # under the header
        d.line([(x + 11, y_ + 1), (x + w - 11, y_ + 1)], fill=(255, 255, 255, 26), width=1)           # catch-light on the top edge
    wx, wy, ww, wh = 32, 213, 588, 44                                     # the amp's pluck print: its inset
    d.rounded_rectangle([wx - 2, wy - 2, wx + ww + 1, wy + wh + 1], radius=3, outline=(255, 255, 255, 20), width=1)
    d.line([(24, 350), (884, 350)], fill=(255, 255, 255, 18), width=1)
    d.line([(904, 350), (1256, 350)], fill=(255, 255, 255, 18), width=1)
    base = Image.fromarray(np.asarray(img, np.uint8)).convert("RGBA")
    return Image.alpha_composite(base, ov).convert("RGB")


# ---- knobs, wordmark, captions, arrows, panel -------------------------------------------------------------------------
def knob_strip(path, size, ticks, accent, glow, frames=128, ss=4):
    S = size * ss; c = S / 2

    def P(r, deg):
        a = math.radians(deg - 90); return (c + r * math.cos(a), c + r * math.sin(a))
    base = Image.new("RGBA", (S, S), (0, 0, 0, 0)); d = ImageDraw.Draw(base)
    for k in range(ticks):
        deg = -135 + 270 * k / (ticks - 1); major = k in (0, ticks // 2, ticks - 1)
        r0, r1 = (0.425 if major else 0.44) * S, 0.475 * S
        d.line([P(r0, deg), P(r1, deg)], fill=(96, 90, 82, 255) if major else (66, 62, 57, 255), width=int(S * (0.012 if major else 0.008)))
    track_r, track_w = 0.395 * S, int(S * 0.028)
    d.arc([c - track_r, c - track_r, c + track_r, c + track_r], 135, 405, fill=(24, 24, 27, 255), width=track_w)
    sh = Image.new("RGBA", (S, S), (0, 0, 0, 0)); ds = ImageDraw.Draw(sh)
    br = 0.33 * S; ds.ellipse([c - br, c - br + S * 0.03, c + br, c + br + S * 0.03], fill=(0, 0, 0, 170))
    base = Image.alpha_composite(base, sh.filter(ImageFilter.GaussianBlur(S * 0.03)))
    px = np.zeros((S, S, 4), np.uint8)
    y2, x2 = np.mgrid[0:S, 0:S]; dx, dy = (x2 - c) / br, (y2 - c) / br; rr = np.sqrt(dx * dx + dy * dy)
    light = np.clip(0.55 - 0.32 * (dx * 0.7 + dy * 0.7) / np.maximum(rr, 1e-6) * np.minimum(rr, 1), 0, 1)
    tone = 20 + 26 * light * (1 - 0.35 * rr ** 2) + 2.5 * np.sin(rr * 120)
    inside = rr <= 1.0
    for ch, k in enumerate((1.0, 1.0, 1.06)): px[..., ch] = np.clip(tone * k, 0, 255) * inside
    px[..., 3] = (inside * 255).astype(np.uint8)
    rim = (rr > 0.965) & (rr <= 1.0) & (dy < 0)
    px[rim, :3] = np.clip(px[rim, :3].astype(int) + 22, 0, 255)
    base = Image.alpha_composite(base, Image.fromarray(px, "RGBA"))
    sheet = Image.new("RGBA", (size, size * frames), (0, 0, 0, 0))
    for f in range(frames):
        v = f / (frames - 1); im = base.copy(); d = ImageDraw.Draw(im)
        end = 135 + 270 * v
        if v > 0.002:
            gl = Image.new("RGBA", (S, S), (0, 0, 0, 0)); dg = ImageDraw.Draw(gl)
            dg.arc([c - track_r, c - track_r, c + track_r, c + track_r], 135, end, fill=tuple(glow) + (120,), width=track_w * 2)
            im = Image.alpha_composite(im, gl.filter(ImageFilter.GaussianBlur(S * 0.018))); d = ImageDraw.Draw(im)
            d.arc([c - track_r, c - track_r, c + track_r, c + track_r], 135, end, fill=tuple(accent) + (255,), width=track_w)
        deg = -135 + 270 * v
        p0, p1 = P(0.12 * S, deg), P(0.30 * S, deg); w = int(S * 0.034)
        d.line([p0, p1], fill=(236, 228, 212, 255), width=w)
        for p in (p0, p1): d.ellipse([p[0] - w / 2, p[1] - w / 2, p[0] + w / 2, p[1] + w / 2], fill=(236, 228, 212, 255))
        sheet.paste(im.resize((size, size), Image.LANCZOS), (0, f * size))
    sheet.save(path, optimize=True)


def wordmark(path):
    wm = Image.new("RGBA", (600, 72), (0, 0, 0, 0)); d = ImageDraw.Draw(wm)
    f_big = ImageFont.truetype(os.path.join(FONTS, "TitilliumWeb-Regular.ttf"), 44)
    f_a2 = ImageFont.truetype(os.path.join(FONTS, "TitilliumWeb-SemiBold.ttf"), 44)
    f_by = ImageFont.truetype(os.path.join(FONTS, "TitilliumWeb-Regular.ttf"), 23)
    x = 0
    for ch in "LIMINAL":
        d.text((x, 4), ch, font=f_big, fill=(205, 197, 182, 255)); x += d.textlength(ch, font=f_big) + 7
    x += 12
    for ch in "AMP":
        d.text((x, 4), ch, font=f_a2, fill=(236, 228, 212, 255)); x += d.textlength(ch, font=f_a2) + 2
    x += 16
    d.text((x, 26), "A2 · by Gm0rb", font=f_by, fill=(140, 134, 124, 255))
    # the wordmark must fit its 600 px canvas (the layout scales the whole image to 380 px)
    assert x + d.textlength("A2 · by Gm0rb", font=f_by) <= 600, "wordmark too wide"
    wm.save(path)


def caption(path, text):
    """a quiet lowercase caption, drawn at the size it is placed at (the layout places it at its own width)"""
    f = ImageFont.truetype(os.path.join(FONTS, "TitilliumWeb-SemiBold.ttf"), 21)   # the page's one family
    w = int(f.getlength(text)) + 4
    im = Image.new("RGBA", (w, 26), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    d.text((0, -3), text, font=f, fill=(140, 132, 120, 255))
    im.save(path)
    return w


def arrows(outdir, accent):
    for name, pts in (("arrow_prev", [(40, 17), (40, 49), (21, 33)]), ("arrow_next", [(26, 17), (26, 49), (45, 33)])):
        ar = Image.new("RGBA", (66, 66), (0, 0, 0, 0)); da = ImageDraw.Draw(ar)
        da.rounded_rectangle((0, 0, 65, 65), radius=5, fill=(29, 31, 35, 255), outline=(44, 44, 49, 255), width=1)
        da.polygon(pts, fill=tuple(accent) + (255,))
        ar.save(os.path.join(outdir, name + ".png"))


def menu_panel(path):
    pn = Image.new("RGBA", (616, 236), (0, 0, 0, 0)); dp = ImageDraw.Draw(pn)
    dp.rounded_rectangle((0, -12, 615, 235), radius=10, fill=(11, 12, 14, 255), outline=(58, 55, 50, 255), width=2)
    dp.rounded_rectangle((4, -8, 611, 231), radius=7, outline=(26, 26, 29, 255), width=1)
    dp.line([(14, 36), (602, 36)], fill=(40, 38, 36, 255), width=1)          # under the title row
    pn.save(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--theme", default="valve", choices=sorted(THEMES))
    ap.add_argument("--out", default=os.path.join(HERE, "..", "vst", "art"))
    ap.add_argument("--nod", default="", choices=[""] + sorted(NODS), help="a layer that nods to Liminal Hz")
    ap.add_argument("--quick", action="store_true", help="only the texture (for previews of a material)")
    a = ap.parse_args()
    th = THEMES[a.theme]
    os.makedirs(a.out, exist_ok=True)
    if a.theme == "valve":
        mat = tubes(th["light"], 0.0, a.nod or None)
    else:
        mat = MATERIALS[a.theme](th["light"])
        if a.nod:
            mat = NODS[a.nod](mat, th["light"])
    img = faceplates(Image.fromarray(np.clip(mat, 0, 255).astype(np.uint8)))
    img.save(os.path.join(a.out, "texture.png"), optimize=True)
    if a.theme == "valve":
        tube_states(a.out, th["light"], a.nod or None)
    wave_states(a.out)
    print("texture:", a.theme)
    if a.quick:
        return
    knob_strip(os.path.join(a.out, "knob_big.png"), 160, 31, th["accent"], th["accent_glow"])
    wordmark(os.path.join(a.out, "wordmark.png"))
    for name, text in (("amp", "amp"), ("cab", "cab"), ("input", "input"), ("tone", "tone"), ("out", "out")):
        caption(os.path.join(a.out, "cap_%s.png" % name), text)
    arrows(a.out, th["accent"])
    menu_panel(os.path.join(a.out, "menu_panel.png"))
    print("art written to", os.path.abspath(a.out))


if __name__ == "__main__":
    main()
