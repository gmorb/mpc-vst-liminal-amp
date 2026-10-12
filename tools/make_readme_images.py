#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 the mpc-vst-liminal-amp contributors
"""make_readme_images.py [--out docs/images] [--only name,name] -- the README's images, 1280x640 (the lists: 1280x628).

  banner.png       the README's top image and the GitHub social preview
  page.png         the page, floating in the room
  chain.png        the signal chain and what the plugin costs on a Force
  controls.png     the knobs and switches, at real values
  lists.png        AMP LIST and CAB LIST open
  my-presets.png   My Presets open
  pluck.png        the amp's pluck print (clean, crunch, high gain) and the valves' heat
  a2-full.png      A2 FULL's first tap
  phone.png        the phone pages: TONE3000 and My Presets
  phone-edit.png   editing a preset on the phone

Everything the page shows is what the plugin itself says: this runs the built plugin (vst/build.sh host) under
tools/readme_demo.cpp, with a card of sample files made here (three amps of known drive, a few cab IRs: synthetic,
nothing downloaded), and draws the page with tools/honest_preview.py from the built skin, so the text, lists, valve
heat and pluck prints are the engine's own. The phone pages are screenshots of the plugin's real web server.
Needs: vst/build.sh host done (plugin and skin in vst/build), MPC_VST (the mpc-vst-plugins checkout, for its fonts),
g++, Pillow, numpy, scipy, Playwright with Chromium. Deterministic apart from the screenshots' timing."""
import argparse, json, os, shutil, subprocess, sys, tempfile, time, wave
import numpy as np
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
MPC_VST = os.environ.get("MPC_VST", os.path.join(ROOT, "..", "mpc-vst-plugins"))
os.environ["MPC_VST"] = MPC_VST                          # honest_preview and make_art read it for the fonts
import honest_preview, make_art                          # noqa: E402

FONTS = os.path.join(MPC_VST, "tools", "html_art", "fonts")
ART = os.path.join(ROOT, "vst", "art")
SKIN = os.path.join(ROOT, "vst", "build", "skin", "Gm0rb - VST - Liminal Amp", "Plugin Skins")
SO = os.path.join(ROOT, "vst", "build", "host", "liminal_amp.so")
PARAMS = json.load(open(os.path.join(ROOT, "vst", "params.json")))["params"]
IDX = {p["key"]: i for i, p in enumerate(PARAMS)}
W, H = 1280, 640
IV = (232, 226, 214); GR = (150, 145, 136); DIM = (112, 107, 99); AMB = (238, 152, 72); PLATE = (5, 5, 6)


def fR(s): return ImageFont.truetype(os.path.join(FONTS, "TitilliumWeb-Regular.ttf"), s)
def fS(s): return ImageFont.truetype(os.path.join(FONTS, "TitilliumWeb-SemiBold.ttf"), s)


# ---- the sample card --------------------------------------------------------------------------------------------------
AMPS = {   # pack/name: (drive g, level): y = level * tanh(g * x), a ConvNet "capture". Drive sets the pluck print's texture.
    "Combos/Chime Combo": (0.5, 1.2), "Combos/Clean Combo": (0.8, 1.1), "Combos/Edge Of Breakup": (3.0, 0.9),
    "Heads/Crunch Head": (12.0, 0.8), "Heads/High Gain Head": (60.0, 0.6), "Heads/Lead Head": (90.0, 0.55),
    "Heads/Rhythm Head": (45.0, 0.6),
}
CABS = {   # pack/name: (length, smoothing passes, decay)
    "TONE3000/Sample Cabs/1x12 Combo": (1500, 4, 9), "TONE3000/Sample Cabs/2x12 Open": (1800, 2, 8),
    "TONE3000/Sample Cabs/4x12 Closed": (2400, 3, 7),
    "Mics/57 On Axis": (2000, 2, 6), "Mics/Ribbon Off Axis": (2200, 5, 7),
}


def stage(stage_dir):
    """<stage>/Gm0rb - VST - Liminal Amp/: the plugin, its fonts (for the phone pages), models, irs"""
    shutil.rmtree(stage_dir, ignore_errors=True)
    p = os.path.join(stage_dir, "Gm0rb - VST - Liminal Amp")
    os.makedirs(os.path.join(p, "art", "fonts"))
    for f in os.listdir(FONTS):
        shutil.copy(os.path.join(FONTS, f), os.path.join(p, "art", "fonts", f))
    shutil.copy(SO, p)
    for name, (g, lv) in AMPS.items():
        path = os.path.join(p, "models", name + ".nam"); os.makedirs(os.path.dirname(path), exist_ok=True)
        json.dump({"version": "0.5.0", "architecture": "ConvNet", "sample_rate": 48000, "metadata": {},
                   "config": {"channels": 1, "dilations": [1], "batchnorm": False, "activation": "Tanh"},
                   "weights": [0.0, g, 0.0, lv, 0.0]}, open(path, "w"))
    rng = np.random.default_rng(3)
    for name, (n, lp, decay) in CABS.items():
        path = os.path.join(p, "irs", name + ".wav"); os.makedirs(os.path.dirname(path), exist_ok=True)
        x = rng.normal(0, 1, n)
        for _ in range(lp): x = np.convolve(x, [0.5, 0.5], "same")
        x *= np.exp(-decay * np.arange(n) / n); x /= np.abs(x).max() * 1.05
        w = wave.open(path, "wb"); w.setnchannels(1); w.setsampwidth(2); w.setframerate(48000)
        w.writeframes((x * 32767).astype("<i2").tobytes()); w.close()
    json.dump({"source": "TONE3000", "title": "Sample Cabs", "creator": "creator", "gear": "cab", "format": "ir"},
              open(os.path.join(p, "irs", "TONE3000", "Sample Cabs", "tone3000.json"), "w"))
    return p


# ---- driving the plugin -----------------------------------------------------------------------------------------------
class Demo:
    def __init__(self, plugin_dir, exe, port):
        env = dict(os.environ, MPCNAM_MEDIA="/nonexistent", MPCNAM_T3K_HOST="192.168.1.20", MPCNAM_T3K_PORT=str(port),
                   MPCNAM_T3K_IDLE="3600")
        self.p = subprocess.Popen([exe, os.path.join(plugin_dir, "liminal_amp.so")], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, text=True, env=env, bufsize=1)
        assert self.p.stdout.readline().startswith("ready")
        self.wait(1500)

    def cmd(self, c):
        self.p.stdin.write(c + "\n"); self.p.stdin.flush(); return self.p.stdout.readline().strip()
    def press(self, k, wait=60): self.cmd("p %d" % IDX[k]); self.wait(wait)
    def setn(self, k, v): self.cmd("s %d %.6f" % (IDX[k], v))
    def real(self, k, v):
        p = PARAMS[IDX[k]]; self.setn(k, (v - p["min"]) / (p["max"] - p["min"]))
    def lvl(self, a): self.cmd("lvl %g" % a)
    def wait(self, ms): self.cmd("w %d" % ms)

    def dump(self):
        self.p.stdin.write("dump\n"); self.p.stdin.flush(); out = {}
        while True:
            l = self.p.stdout.readline().rstrip("\n")
            if l == "end": return out
            i, v, d = l.split("|", 2); out[PARAMS[int(i)]["key"]] = (float(v), d)

    def text(self, k): return self.dump()[k][1]
    def pick(self, key, want):                      # step a stepper (model / ir) until its name reads `want`
        for _ in range(20):
            if self.text(key) == want: return
            self.press(key + "_next", 700)
        raise RuntimeError("%s: no %r" % (key, want))
    def quit(self):
        try: self.cmd("quit")
        except Exception: pass
        self.p.wait(timeout=15)

    def scene(self):
        """what the host and the page see right now, as honest_preview.py's scene"""
        s = self.dump(); sc = {"options": {}, "knobs": {}, "text": {}}
        for p in PARAMS:
            v, d = s[p["key"]]
            if "options" in p: sc["options"][p["key"]] = int(round(v * (len(p["options"]) - 1)))
            if "min" in p: sc["knobs"][p["key"]] = v
            if p["key"].startswith(("amp_row_", "cab_row_")) and v > .5: sc["options"][p["key"]] = 1     # the list's row in use: outlined
            sc["text"][p["key"]] = d
        return sc


def render_scene(sc, out):
    honest_preview.render(SKIN, os.path.join(ROOT, "vst", "params.json"), sc, out)
    return Image.open(out).convert("RGB")


def knobs_like_a_player(d, amp="Heads/Crunch Head".split("/")[1], cab="4x12 Closed"):
    d.pick("model", amp); d.pick("ir", cab)
    d.real("input", 3.0); d.real("gate", -70.0); d.setn("bass", .60); d.setn("mid", .45); d.setn("treble", .65); d.real("output", -2.0)


def valves_at(d, target):
    """feed the plugin until its valves read about `target` (of 24); returns the dump"""
    best = None
    for a in (0.01, 0.02, 0.04, 0.08, 0.15, 0.3):
        d.lvl(a); d.wait(1800); s = d.dump(); t = round(s["tubes"][0] * 24)
        if best is None or abs(t - target) < abs(best[0] - target): best = (t, a)
        if t >= target: break
    d.lvl(best[1]); d.wait(1800)
    return d.dump()


# ---- pictures ---------------------------------------------------------------------------------------------------------
# Drawing is done on RGB images with ImageDraw.Draw(im, "RGBA") so translucent lines and fills blend (on an RGBA image
# PIL would replace the pixels instead).
def draw(im): return ImageDraw.Draw(im, "RGBA")


def wordmark(d, x, y, size, by=None):
    """LIMINAL (spaced) + AMP (heavier) [+ A2 · by Gm0rb, quiet]; returns the right edge"""
    f, fh = fR(size), fS(size)
    for ch in "LIMINAL":
        d.text((x, y), ch, font=f, fill=(206, 198, 184), anchor="ls"); x += d.textlength(ch, font=f) + size * 0.22
    x += size * 0.22
    for ch in "AMP":
        d.text((x, y), ch, font=fh, fill=IV, anchor="ls"); x += d.textlength(ch, font=fh) + size * 0.03
    if by:
        x += size * 0.35; d.text((x, y), by, font=fR(int(size * 0.36)), fill=GR, anchor="ls"); x += d.textlength(by, font=fR(int(size * 0.36)))
    return x


def fit(d, text, path, size, max_w, floor=14):
    """the largest size <= `size` at which `text` fits max_w"""
    while size > floor and d.textlength(text, font=ImageFont.truetype(path, size)) > max_w: size -= 1
    return ImageFont.truetype(path, size)


_room = {}
def room(lv, valves=True):
    """the page's own room (make_art: the dark material, the air, the KT88 valves at amp level lv 0..1), 1280x628;
    valves=False: the room alone (behind a page render, whose own valves are in the picture)"""
    key = (lv, valves)
    if key not in _room:
        keep = make_art.VALVE_STYLE
        make_art.VALVE_STYLE = keep if valves else "none"
        try: a = make_art.tubes(make_art.THEMES["valve"]["light"], lv, "air")
        finally: make_art.VALVE_STYLE = keep
        _room[key] = Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))
    return _room[key]


def bg(lv=0.0, zoom=1.0, strength=1.0, valves=False):
    im = room(lv, valves)
    if zoom != 1.0:                                  # a crop of the lower right, so the valves are the room's light
        cw, ch = int(W / zoom), int(H / zoom)
        im = im.crop((im.width - cw, im.height - ch, im.width, im.height)).resize((W, H), Image.LANCZOS)
    else:
        c = Image.new("RGB", (W, H), (9, 9, 10)); c.paste(im, (0, (H - im.height) // 2)); im = c
    return ImageEnhance.Brightness(im).enhance(strength)


def vignette(im, k=0.55):
    a = np.asarray(im.convert("RGB"), float); yy, xx = np.mgrid[0:H, 0:W]
    r = np.sqrt(((xx - W / 2) / (W / 2)) ** 2 + ((yy - H / 2) / (H / 2)) ** 2)
    a *= (1 - k * np.clip(r - 0.45, 0, 1) ** 1.4)[..., None]
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))


def floating(im, shot, pw, x, y):
    """a render of the page over the room, with a shadow and a hairline"""
    ph = int(shot.height * pw / shot.width); shot = shot.resize((pw, ph), Image.LANCZOS)
    sh = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(sh).rounded_rectangle([x - 6, y + 10, x + pw + 6, y + ph + 22], radius=14, fill=(0, 0, 0, 200))
    im = Image.alpha_composite(im.convert("RGBA"), sh.filter(ImageFilter.GaussianBlur(18))).convert("RGB")
    im.paste(shot, (x, y))
    draw(im).rounded_rectangle([x - 1, y - 1, x + pw, y + ph], radius=3, outline=(255, 255, 255, 40), width=1)
    return im


def banner(out, **_):
    im = vignette(bg(0.85, zoom=1.32, strength=1.05, valves=True), 0.5); d = draw(im)
    wordmark(d, 84, 300, 92)
    d.text((88, 350), "A2  ·  by Gm0rb", font=fR(30), fill=GR, anchor="ls")
    d.text((88, 440), "A NAM A2 head and IR cab for MPC.", font=fR(32), fill=(196, 190, 178), anchor="ls")
    d.text((88, 484), "Akai Force & MPC  ·  TONE3000 amps and cabs from your phone", font=fR(22), fill=DIM, anchor="ls")
    im.save(out)


def page_img(out, shot, line="Neural Amp Modeler A2 amps and cab IRs for Akai Force & MPC", **_):
    im = vignette(bg(0.5, strength=0.9), 0.35)
    im = floating(im, shot, 930, (W - 930) // 2 + 40, 168); d = draw(im)
    wordmark(d, 64, 104, 54, by="A2 · by Gm0rb")
    d.text((66, 146), line, font=fR(24), fill=GR, anchor="ls")
    im.save(out)


def knob_frame(v, size):
    big = Image.open(os.path.join(ART, "knob_big.png")); fw = big.width
    f = int(round(max(0, min(1, v)) * 127))
    return big.crop((0, f * fw, fw, (f + 1) * fw)).resize((size, size), Image.LANCZOS)


def controls_img(out, s, **_):
    im = vignette(bg(0.3, strength=0.8, valves=True), 0.5); d = draw(im)
    wordmark(d, 64, 120, 54, by="A2 · by Gm0rb")
    nm = lambda k: PARAMS[IDX[k]]["name"]            # what MPC shows: the parameter's name, and knob values in upper case
    groups = [("input", [(nm("input"), "input"), (nm("gate"), "gate")]), ("tone", [(nm("bass"), "bass"), (nm("mid"), "mid"), (nm("treble"), "treble")]),
              ("out", [(nm("output"), "output")])]
    size = 136; x = 64
    for gname, ks in groups:
        gx0 = x
        for j, (label, k) in enumerate(ks):
            im.paste(knob_frame(s[k][0], size), (x, 218), knob_frame(s[k][0], size))
            cx = x + size // 2
            d.text((cx, 384), label, font=fS(24), fill=GR, anchor="ms"); d.text((cx, 424), s[k][1].upper(), font=fS(32), fill=IV, anchor="ms")
            x += size + (24 if j < len(ks) - 1 else 0)
        d.text((gx0, 196), gname, font=fS(22), fill=DIM, anchor="ls")
        d.line([(gx0 + 44, 190), (x, 190)], fill=(255, 255, 255, 24), width=1)
        x += 76
    chips = [(nm(k), s[k][0] > .5) for k in ("gate_on", "eq_on", "normalize", "eco", "full", "ir_on")]
    x = 64; y = 500
    for name, on in chips:
        w = int(d.textlength(name, font=fS(22)) + 52)
        d.rounded_rectangle([x, y, x + w, y + 38], radius=19, fill=(16, 17, 19, 255), outline=(255, 255, 255, 34))
        d.ellipse([x + 14, y + 13, x + 26, y + 25], fill=AMB if on else (52, 50, 47))
        d.text((x + 36, y + 27), name, font=fS(22), fill=IV if on else DIM, anchor="ls"); x += w + 14
    d.text((x + 8, y + 27), "IR length  " + ("FULL" if s["ir_len"][0] < .5 else "SHORT"), font=fS(22), fill=GR, anchor="ls")
    d.text((66, 596), "gate  ·  the official tone stack  ·  normalize  ·  ECO  ·  A2 FULL  ·  cab on/off  ·  IR length", font=fR(21), fill=DIM, anchor="ls")
    im.save(out)


def pluck_img(out, prints, **_):
    """prints: [(amp name, [14 states])] from the engine; the valves at four heats from the page's own tube pictures"""
    im = vignette(bg(0.3, strength=0.75), 0.5); d = draw(im)
    wordmark(d, 64, 100, 50, by="A2 · by Gm0rb")
    d.text((66, 138), "The page has a pulse: a note through the loaded amp, and valves that glow with your level.", font=fR(23), fill=GR, anchor="ls")
    tex = ["clean", "crunch", "high gain"]
    y = 168
    for name, st in prints:
        kind = tex[(max(st) - 1) // 8] if max(st) > 0 else "silent"
        strip = Image.new("RGB", (588, 44))
        for c, s in enumerate(st): strip.paste(Image.open(os.path.join(ART, "wave_%d.png" % s)), (c * 42, 0))
        strip = strip.resize((882, 66), Image.LANCZOS)
        d.rounded_rectangle([60, y - 4, 64 + 882 + 3, y + 66 + 3], radius=4, fill=(9, 9, 10, 255), outline=(255, 255, 255, 26))
        im.paste(strip, (64, y))
        d.text((978, y + 28), kind, font=fS(30), fill=IV, anchor="ls"); d.text((978, y + 58), name, font=fR(22), fill=GR, anchor="ls")
        y += 84
    d.text((66, y + 14), "the amp's pluck print: one plucked note through the capture, in 14 slices of time", font=fR(20), fill=DIM, anchor="ls")
    d.text((66, y + 54), "the valves: the page at four levels of your output", font=fR(20), fill=DIM, anchor="ls")
    x = 64; y2 = 484
    for k, lab in ((0, "idle"), (8, "playing"), (16, "driven"), (24, "hot")):
        t = Image.open(os.path.join(ART, "tube_%d.png" % k)).convert("RGB").crop((70, 0, 330, 118))
        im.paste(t, (x, y2)); d.text((x + 130, y2 + 138), lab, font=fR(20), fill=GR, anchor="ms"); x += 300
    im.save(out)


def chain_img(out, **_):
    S = 2
    im = vignette(bg(0.3, strength=0.75), 0.5).resize((W * S, H * S), Image.LANCZOS); d = draw(im)
    TP = {"R": os.path.join(FONTS, "TitilliumWeb-Regular.ttf"), "S": os.path.join(FONTS, "TitilliumWeb-SemiBold.ttf")}
    def T(x, y, s, f, size, fill, anchor="ls", max_w=None):
        font = ImageFont.truetype(TP[f], size * S)
        if max_w:
            while size > 13 and d.textlength(s, font=font) > max_w * S: size -= 1; font = ImageFont.truetype(TP[f], size * S)
        d.text((x * S, y * S), s, font=font, fill=fill, anchor=anchor)
    f, fh = fR(50 * S), fS(50 * S); xx = 64 * S
    for ch in "LIMINAL": d.text((xx, 100 * S), ch, font=f, fill=(206, 198, 184), anchor="ls"); xx += d.textlength(ch, font=f) + 50 * S * 0.22
    xx += 50 * S * 0.22
    for ch in "AMP": d.text((xx, 100 * S), ch, font=fh, fill=IV, anchor="ls"); xx += d.textlength(ch, font=fh) + 50 * S * 0.03
    d.text((xx + 18 * S, 100 * S), "A2 · by Gm0rb", font=fR(18 * S), fill=GR, anchor="ls")
    T(66, 138, "What the plugin does with your sound, in order.", "R", 23, GR)
    stages = [("INPUT", "L + R averaged", "Input \u00b120 dB"), ("GATE", "noise gate", "Threshold"),
              ("AMP", "NAM A2 capture", "resampled 48 kHz"), ("TONE", "official tone stack", "Bass, Middle, Treble"),
              ("CAB", "your IR (FFT)", "up to 8192 samples"), ("OUT", "DC blocker, level", "Output, Normalize")]
    m0 = 48; n = len(stages); bw = 174; gap = (W - 2 * m0 - bw * n) / (n - 1); y0 = 196; bh = 136; x = m0
    for i, (nm, l1, l2) in enumerate(stages):
        d.rounded_rectangle([x * S, y0 * S, (x + bw) * S, (y0 + bh) * S], radius=10 * S, fill=PLATE + (200,), outline=(255, 255, 255, 40), width=S)
        d.line([((x + 14) * S, (y0 + 44) * S), ((x + bw - 14) * S, (y0 + 44) * S)], fill=(255, 255, 255, 22), width=S)
        T(x + 16, y0 + 33, nm, "S", 26, IV)
        T(x + 16, y0 + 80, l1, "R", 19, GR, max_w=bw - 26); T(x + 16, y0 + 106, l2, "R", 18, DIM, max_w=bw - 26)
        if i < n - 1:
            ax = (x + bw + 4) * S; bx = (x + bw + gap - 4) * S; ay = (y0 + bh / 2) * S
            d.line([(ax, ay), (bx - 7 * S, ay)], fill=(150, 145, 136, 200), width=2 * S)
            d.polygon([(bx, ay), (bx - 9 * S, ay - 5 * S), (bx - 9 * S, ay + 5 * S)], fill=(150, 145, 136, 230))
        x += bw + gap
    T(m0, 402, "What it costs on an Akai Force", "S", 26, IV)
    T(m0, 430, "mean share of one audio block (2.9 ms: 128 frames at 44.1 kHz)", "R", 19, DIM)
    T(W - m0, 402, "the whole plugin: 16.1%  (p99 19.0%)", "S", 24, IV, anchor="rs")
    bx0, bx1, by = m0, W - m0, 452; bwid = bx1 - bx0
    d.rounded_rectangle([bx0 * S, by * S, bx1 * S, (by + 40) * S], radius=6 * S, fill=(12, 12, 14, 255), outline=(255, 255, 255, 34), width=S)
    parts = [(11.9, (238, 152, 72), "amp, with resampling  11.9%"), (1.2, (214, 126, 52), "gate and tone stack  1.2%"),
             (3.0, (176, 96, 40), "cab IR, 8192 samples  3.0%")]
    cx = bx0
    for v, col, _lab in parts:
        w = bwid * v / 100.0
        d.rectangle([cx * S, (by + 3) * S, (cx + w) * S, (by + 37) * S], fill=col + (240,)); cx += w
    for k, lab in ((0, "0%"), (50, "50%"), (100, "100% of the block")):
        x_ = bx0 + bwid * k / 100.0
        d.line([(x_ * S, (by + 40) * S), (x_ * S, (by + 48) * S)], fill=(255, 255, 255, 60), width=S)
        T(x_, by + 66, lab, "R", 17, DIM, anchor="ls" if k == 0 else ("ms" if k == 50 else "rs"))
    lx = m0
    for v, col, lab in parts:
        d.rectangle([lx * S, 534 * S, (lx + 16) * S, (534 + 16) * S], fill=col + (240,))
        T(lx + 26, 548, lab, "R", 20, IV); lx += 26 + int(d.textlength(lab, font=ImageFont.truetype(TP["R"], 20 * S)) / S) + 40
    T(m0, 604, "A2 FULL, the heavy size, is off by default (the capture alone measured 57% on a Force): it is best kept for a Gen 2.", "R", 19, DIM)
    im.resize((W, H), Image.LANCZOS).save(out)


def phone_frame(shot, w, h, top=0):
    crop = shot.crop((0, top, shot.width, min(shot.height, top + int(shot.width * h / w))))
    crop = crop.resize((w, int(crop.height * w / crop.width)), Image.LANCZOS).crop((0, 0, w, h))
    fr = Image.new("RGB", (w + 16, h + 16), (30, 31, 35)); fr.paste(crop, (8, 8)); return fr


def shoot(urls, w=390, h=844):
    from playwright.sync_api import sync_playwright
    out = []
    with sync_playwright() as pw:
        b = pw.chromium.launch(); c = b.new_context(viewport={"width": w, "height": h}, device_scale_factor=2)
        for u in urls:
            pg = c.new_page(); pg.goto(u, wait_until="networkidle"); pg.wait_for_timeout(300)
            f = os.path.join(tempfile.gettempdir(), "lsshot_%d.png" % len(out)); pg.screenshot(path=f, full_page=True)
            out.append(Image.open(f).convert("RGB")); pg.close()
        b.close()
    return out


def phone_img(out, shots, lines, foot, **_):
    im = vignette(bg(0.2, strength=0.7), 0.5); d = draw(im)
    x = 520; fw = 330
    for s_, top in shots:
        im.paste(phone_frame(s_, fw, 560, top), (x, 36)); x += fw + 40
    wordmark(d, 64, 120, 46)
    d.text((66, 156), "A2  ·  by Gm0rb", font=fR(22), fill=GR, anchor="ls")
    y = 236
    for t, bright in lines:
        d.text((66, y), t, font=fR(27), fill=IV if bright else GR, anchor="ls"); y += 38
    y = 560
    for t in foot:
        d.text((66, y), t, font=fR(21), fill=DIM, anchor="ls"); y += 28
    im.save(out)


# ---- the run ------------------------------------------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(ROOT, "docs", "images"))
    ap.add_argument("--only", default="")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    only = set(x for x in a.only.split(",") if x)
    want = lambda n: not only or n in only
    work = tempfile.mkdtemp(prefix="liminal_amp_readme_")
    exe = os.path.join(work, "readme_demo")
    subprocess.check_call(["g++", "-O1", "-std=c++17", "-o", exe, os.path.join(HERE, "readme_demo.cpp"), "-ldl", "-lpthread"])
    O = lambda n: os.path.join(a.out, n)
    plug = stage(os.path.join(work, "stage"))

    # 1. a first session makes the presets (the plugin's own SAVE, named after the amp, then renamed on disk)
    d = Demo(plug, exe, 18091)
    for amp, cab, vals, new in (
        ("Chime Combo", "1x12 Combo", dict(input=0.0, bass=.5, mid=.5, treble=.7), "Clean Sparkle"),
        ("Crunch Head", "4x12 Closed", dict(input=3.0, bass=.6, mid=.45, treble=.65), "Crunch Rhythm"),
        ("Lead Head", "2x12 Open", dict(input=6.0, bass=.45, mid=.6, treble=.55), "Lead Sustain"),
        ("Rhythm Head", "57 On Axis", dict(input=1.5, bass=.7, mid=.4, treble=.6), "Tight Rhythm")):
        d.pick("model", amp); d.pick("ir", cab)
        for k, v in vals.items(): d.real(k, v) if k == "input" else d.setn(k, v)
        d.press("mp_save", 500)
        src = os.path.join(plug, "presets", amp + ".na2p"); assert os.path.exists(src), src
        os.rename(src, os.path.join(plug, "presets", new + ".na2p"))
    d.quit()

    # 2. a fresh session: the page as a player finds it (no preset in use), lists, presets, A2 FULL, the phone pages
    d = Demo(plug, exe, 18091)
    knobs_like_a_player(d)
    s = valves_at(d, 15); sc = d.scene(); main_state = s
    shot = render_scene(sc, os.path.join(work, "page.png"))
    if want("banner"): banner(O("banner.png"))
    if want("page"): page_img(O("page.png"), shot)
    if want("controls"): controls_img(O("controls.png"), s)
    if want("chain"): chain_img(O("chain.png"))

    if want("pluck"):                                  # one print per texture, from the engine, through the amps' own files
        prints = []
        for amp in ("Clean Combo", "Crunch Head", "High Gain Head"):
            d.pick("model", amp); d.wait(900); st = d.dump()
            prints.append((amp, [int(round(st["amp_wave_%d" % i][0] * 24)) for i in range(1, 15)]))
        pluck_img(O("pluck.png"), prints)
        d.pick("model", "Crunch Head")

    if want("lists"):
        d.pick("ir", "2x12 Open")                       # lists open on the page of the one in use: keep it on page 1
        d.press("amp_list"); d.press("cab_list"); d.wait(400)
        render_scene(d.scene(), O("lists.png"))
        d.press("amp_close"); d.press("cab_close"); d.pick("ir", "4x12 Closed")
    if want("my-presets"):
        d.press("mp_list"); d.wait(400)
        render_scene(d.scene(), O("my-presets.png"))
        d.press("amp_close"); d.wait(200)
    if want("a2-full"):
        d.press("full", 300); sc = d.scene()
        page_img(O("a2-full.png"), render_scene(sc, os.path.join(work, "full.png")),
                 line="A2 FULL asks twice. One tap warns; a second tap within 6 seconds turns it on.")
        d.wait(6800)                                   # a second tap would turn it on: let the warning lapse instead
        assert d.text("full") != "On" and d.dump()["full"][0] < .5
    if want("phone") or want("phone-edit"):
        d.press("mp_phone", 200)
        for _ in range(50):                            # the plugin's worker opens the page
            addr = d.text("t3k_info")
            if addr.startswith("Open "): break
            d.wait(100)
        port = int(addr.rsplit(":", 1)[1])
        base = "http://127.0.0.1:%d" % port
        home, plist, edit = shoot([base + "/", base + "/presets", base + "/presets/edit?name=Crunch%20Rhythm"])
        if want("phone"):
            phone_img(O("phone.png"), [(home, 0), (plist, 0)],
                      [("On your phone,", False), ("on the same Wi-Fi:", False), ("TONE3000 amps and cabs", True), ("straight to the MPC,", True),
                       ("and My Presets named", True), ("with a keyboard.", True)],
                      ["No app and no password.", "Presets need no account", "and no internet."])
        if want("phone-edit"):
            h2 = 560 * edit.width // 330
            phone_img(O("phone-edit.png"), [(edit, 0), (edit, h2 - 330)],
                      [("Edit a preset", False), ("on your phone:", False), ("every setting, the amp", True), ("and the cab, then Save,", True),
                       ("or Save and load.", True)],
                      ["Presets are small text files", "in the plugin's presets folder."])
    d.quit()
    shutil.rmtree(work, ignore_errors=True)
    print("images written to", a.out)


if __name__ == "__main__":
    main()
