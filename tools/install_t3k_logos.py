#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 the mpc-vst-liminal-amp contributors
"""install_t3k_logos.py (--zip "TONE3000 Logos.zip" | --logo FILE --mark FILE) [--out vst/art] [--list]

Puts TONE3000's official logo and T3K mark into the page's art, in the sizes the page uses:
  tone3000-logo.png  300x60   the full logo (the TONE3000 strip, and the phone pages), light on transparent
  t3k-mark.png       128x52   the compact T3K mark (the amp and cab blocks)
Get the files from https://www.tone3000.com/api ("Download TONE3000 Logos", https://www.tone3000.com/TONE3000%20Logos.zip).
The page is near-black, so use the LIGHT (white / ivory) versions. --zip lists what the archive holds and picks the full logo and
the mark by file name when that is unambiguous; otherwise it stops and says so: name them with --logo and --mark. PNG, JPG, WebP
and SVG are read (an SVG is drawn by headless Chromium, as the page's art is). --list only lists a zip.
Writes <out>/.t3k-official so tools/package.sh stops warning about placeholders. Then run vst/build.sh and tools/package.sh.
Nothing is altered but scale and padding: the files are TONE3000's, so keep their usage rules (tone3000.com/api, "Design requirements")."""
import argparse, io, os, re, sys, tempfile, zipfile
from PIL import Image

EXT = (".png", ".jpg", ".jpeg", ".webp", ".svg")


def raster(data, name, scale_to=1200):
    """PIL image (RGBA) from file bytes; an SVG through headless Chromium on a transparent page"""
    if name.lower().endswith(".svg"):
        from playwright.sync_api import sync_playwright
        svg = data.decode("utf-8", "replace")
        with sync_playwright() as pw:
            b = pw.chromium.launch(); pg = b.new_page(viewport={"width": scale_to, "height": scale_to})
            pg.set_content("<html><body style='margin:0;background:transparent'><div id=x style='display:inline-block'>%s</div></body></html>" %
                           re.sub(r"<\?xml[^>]*\?>", "", svg))
            pg.evaluate("""() => { const s = document.querySelector('svg'); const vb = s.viewBox && s.viewBox.baseVal;
                let w = vb && vb.width, h = vb && vb.height;
                if (!w || !h) { const bb = s.getBBox(); w = bb.width || 100; h = bb.height || 100; s.setAttribute('viewBox', bb.x + ' ' + bb.y + ' ' + w + ' ' + h); }
                const k = %d / Math.max(w, h); s.setAttribute('width', w * k); s.setAttribute('height', h * k); }""" % scale_to)
            png = pg.locator("#x").screenshot(omit_background=True); b.close()
        return Image.open(io.BytesIO(png)).convert("RGBA")
    return Image.open(io.BytesIO(data)).convert("RGBA")


def fit(im, w, h):
    """trim transparent margins, scale to fit w x h (never up past 4x), centre on a transparent canvas"""
    bb = im.getchannel("A").point(lambda v: 255 if v > 8 else 0).getbbox()
    if bb: im = im.crop(bb)
    k = min(w / im.width, h / im.height)
    im = im.resize((max(1, round(im.width * k)), max(1, round(im.height * k))), Image.LANCZOS)
    out = Image.new("RGBA", (w, h), (0, 0, 0, 0)); out.paste(im, ((w - im.width) // 2, (h - im.height) // 2), im)
    return out


def brightness(im):
    d = im.resize((64, 64)).tobytes()
    px = [(d[i] + d[i + 1] + d[i + 2]) / 3 for i in range(0, len(d), 4) if d[i + 3] > 128]
    return sum(px) / len(px) if px else 0


def pick(names):
    """(logo, mark) from the zip's file names, or None for either when it is not unambiguous"""
    ok = [n for n in names if n.lower().endswith(EXT) and "__macosx" not in n.lower() and not os.path.basename(n).startswith(".")]
    def light(n): return bool(re.search(r"light|white|ivory|reverse|dark[-_ ]?(bg|background|mode)", n.lower()))
    def darkv(n): return bool(re.search(r"(^|[-_ /])dark([-_ .]|$)|black", n.lower())) and not light(n)
    marks = [n for n in ok if re.search(r"t3k|mark|icon|symbol|monogram", n.lower())]
    logos = [n for n in ok if n not in marks and re.search(r"logo|wordmark|tone3000|full", n.lower())]
    def best(c):
        c = [n for n in c if not darkv(n)] or c
        l = [n for n in c if light(n)] or c
        pngs = [n for n in l if n.lower().endswith(".png")]
        l = pngs or l
        return l[0] if len(l) == 1 else (None if not l else sorted(l, key=lambda n: (n.lower().endswith(".svg"), len(n)))[0] if len({os.path.splitext(n)[0] for n in l}) == 1 else None)
    return best(logos), best(marks)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--zip"); ap.add_argument("--logo"); ap.add_argument("--mark")
    ap.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "vst", "art"))
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()
    logo = mark = None
    if a.zip:
        z = zipfile.ZipFile(a.zip)
        names = [n for n in z.namelist() if not n.endswith("/")]
        print("in %s:" % a.zip); [print("  ", n) for n in names]
        if a.list: return 0
        pl, pm = pick(names)
        lg = a.logo or pl; mk = a.mark or pm
        if not lg or not mk:
            print("\nCannot tell which file is the %s: name it with %s." % (
                " and the ".join(x for x, v in (("full logo", lg), ("T3K mark", mk)) if not v),
                " and ".join(f for f, v in (("--logo NAME", lg), ("--mark NAME", mk)) if not v)))
            return 2
        read = lambda n: z.read(n) if n in names else open(n, "rb").read()
        logo, mark = (read(lg), lg), (read(mk), mk)
        print("\nfull logo: %s\nT3K mark:  %s" % (lg, mk))
    elif a.logo and a.mark:
        logo, mark = (open(a.logo, "rb").read(), a.logo), (open(a.mark, "rb").read(), a.mark)
    else:
        ap.error("give --zip, or both --logo and --mark")
    L, M = raster(*logo), raster(*mark)
    for nm, im in (("full logo", L), ("T3K mark", M)):
        b = brightness(im)
        if b < 110:
            print("warning: the %s is dark (mean %d): the page is near-black, use the light version." % (nm, b))
    os.makedirs(a.out, exist_ok=True)
    fit(L, 300, 60).save(os.path.join(a.out, "tone3000-logo.png"), optimize=True)
    fit(M, 128, 52).save(os.path.join(a.out, "t3k-mark.png"), optimize=True)
    open(os.path.join(a.out, ".t3k-official"), "w").write("tone3000-logo.png and t3k-mark.png are TONE3000's official assets (tone3000.com/api)\n")
    print("wrote tone3000-logo.png (300x60) and t3k-mark.png (128x52) in", os.path.abspath(a.out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
