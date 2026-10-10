#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 the mpc-vst-liminal-amp contributors
"""honest_preview.py <Plugin Skins dir> <vst/params.json> <scene.json> <out.png> [--tab N]

Draws the page the way MPC draws the built skin, from the skin's own TUI.json and images (nothing is drawn by hand on
top): the background and the per-mode images that apply, switchable pictures at the state the scene gives them, knob
filmstrip frames, buttons on/off, and MPC's live text (a control's name label shows the parameter's name, as MPC
draws it, not the layout's label; the text style's case is applied, e.g. knob values in upper case) (Titillium Web) in each text box at the size and colour the skin
sets. MPC (JUCE) sizes a font by ascent+descent, which for Titillium Web is 1.521 em, so a label of height H draws
glyphs of em H/1.521: this is why MPC's text looks smaller than a mockup that uses H as the em size.

scene.json: {"options": {key: option index (or 0/1 for a toggle/button)}, "knobs": {key: 0..1}, "text": {key: string}}
Parameters not in the scene are at option 0, knob 0, empty text. Use REAL values (from the engine/library), never invented ones."""
import json, os, re, sys
from PIL import Image, ImageDraw, ImageFont

FONTS = os.path.join(os.environ.get("MPC_VST", os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "mpc-vst-plugins")),
                     "tools", "html_art", "fonts")
EM = 1.521
W, H = 1280, 628


def font(style, height):
    f = {"Regular": "TitilliumWeb-Regular.ttf", "SemiBold": "TitilliumWeb-SemiBold.ttf", "Bold": "TitilliumWeb-Bold.ttf"}.get(style, "TitilliumWeb-Regular.ttf")
    return ImageFont.truetype(os.path.join(FONTS, f), max(4, round(height / EM)))


def xywh(b):
    return [int(float(v)) for v in b["bounds"].split()]


def conds(c):
    return [tuple(int(g) for g in m.groups()) for m in
            (re.match(r"IndexedEnabling/(\d+)/(\d+)/Parameter (\d+)$", h) for h in c["bounds"].get("additionalInvalidatingHandles", [])) if m]


def render(skin, params_json, scene, out, tab_i=0):
    plist = json.load(open(params_json))["params"]
    keys = [p["key"] for p in plist]
    names = [p.get("name", p["key"]) for p in plist]
    idx = {k: i for i, k in enumerate(keys)}
    opt = {idx[k]: v for k, v in scene.get("options", {}).items()}
    knob = {idx[k]: v for k, v in scene.get("knobs", {}).items()}
    text = {idx[k]: v for k, v in scene.get("text", {}).items()}
    t = json.load(open(os.path.join(skin, "TUI.json")))["pageData"]
    defs = {d["key"]: d["value"] for d in t["componentDefinitions"]["localComponentDefinitions"]}
    tab = t["tabs"][tab_i]
    page = tab["componentDefinition"] if "componentDefinition" in tab else defs[tab["componentName"]]
    im = Image.new("RGBA", (W, H), (0, 0, 0, 255))
    dr = ImageDraw.Draw(im)
    cache = {}

    def load(name):
        if name not in cache:
            cache[name] = Image.open(os.path.join(skin, name)).convert("RGBA")
        return cache[name]

    def paste(img, x, y):
        im.alpha_composite(img, (x, y)) if 0 <= x and 0 <= y and x + img.width <= W and y + img.height <= H else im.paste(img, (x, y), img)

    for c in page["componentsData"]:
        if not all(opt.get(p, 0) == i for i, n, p in conds(c)):
            continue
        cd = c["componentData"]
        x, y, w, h = xywh(c["bounds"])
        if cd["type"] == "Image":
            paste(load(cd["data"]["image"]), x, y)
            continue
        remap = {m["key"]: int(m["value"].split()[-1]) for m in c["handle remapping"]["map"]}
        data_p = remap.get("Data")
        for s in defs[cd["type"]]["componentsData"]:
            sd = s["componentData"]
            sx, sy, sw, sh = xywh(s["bounds"])
            if sd["type"] == "Image":
                paste(load(sd["data"]["image"]), x + sx, y + sy)
            elif sd["type"] == "Knob":
                st = load(sd["data"]["filmStrip"])
                fw = st.size[0]
                n = st.size[1] // fw
                f = min(n - 1, max(0, round(knob.get(data_p, 0.0) * (n - 1))))
                paste(st.crop((0, f * fw, fw, (f + 1) * fw)), x + sx, y + sy)
            elif sd["type"] == "Button":
                d = sd["data"]
                v = opt.get(data_p, 0)
                on = (v == d.get("buttonId", 0)) if d.get("numButtonsInGroup", 1) > 1 else v >= 1
                img = d["onImage"] if on else d["offImage"]
                if img:
                    paste(load(img), x + sx, y + sy)
            elif sd["type"] == "Label":
                ts = sd["data"]["textStyle"]
                if sd["data"].get("type") == "Value":
                    p = remap.get(sd["data"].get("handleName", "Text"))
                    s_ = text.get(p, "")
                elif sd["data"].get("type") == "Name" and remap.get(sd["data"].get("handleName", "Data")) is not None:
                    s_ = names[remap[sd["data"].get("handleName", "Data")]]   # MPC draws the parameter's own name here
                else:
                    s_ = cd.get("name", "")
                if ts.get("case") == "Upper Case":
                    s_ = s_.upper()
                elif ts.get("case") == "Lower Case":
                    s_ = s_.lower()
                if not s_:
                    continue
                col = ts["colour"]
                rgb = tuple(int(col[i:i + 2], 16) for i in (2, 4, 6)) + (int(col[0:2], 16),)
                fnt = font(ts["font"].get("style", "Regular"), ts["font"]["height"])
                lay = Image.new("RGBA", (sw, sh), (0, 0, 0, 0))
                ld = ImageDraw.Draw(lay)
                bb = ld.textbbox((0, 0), s_, font=fnt)
                tw = ld.textlength(s_, font=fnt)
                just = ts.get("justification", "")
                asc, desc = fnt.getmetrics()
                tx = (sw - tw) / 2 if "horizontallyCentred" in just else (sw - tw if "right" in just.lower() else 0)
                ty = (sh - (asc + desc)) / 2 if "verticallyCentred" in just else 0
                ld.text((tx, ty), s_, font=fnt, fill=rgb)
                paste(lay, x + sx, y + sy) if False else im.alpha_composite(lay, (x + sx, y + sy)) if (x + sx >= 0 and y + sy >= 0 and x + sx + sw <= W and y + sy + sh <= H) else None
    im.convert("RGB").save(out)


if __name__ == "__main__":
    a = [x for x in sys.argv[1:] if not x.startswith("--")]
    tab = int(sys.argv[sys.argv.index("--tab") + 1]) if "--tab" in sys.argv else 0
    if "--tab" in sys.argv:
        a.remove(sys.argv[sys.argv.index("--tab") + 1])
    render(a[0], a[1], json.load(open(a[2])), a[3], tab)
    print("wrote", a[3])
