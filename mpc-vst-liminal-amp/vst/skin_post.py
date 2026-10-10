#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 the mpc-vst-liminal-amp contributors
"""skin_post.py <Plugin Skins dir> -- after mpc-vst-plugins' gen_vst.py: the live text's type roles. MPC draws these
labels itself (Titillium Web); only their size and colour are set here, per component definition and label name.
What you read is bright and big, what names it is quiet and smaller; the accent is never used for plain text.
The sizes are Liminal Hz's, reached over three rounds of device photos (MPC draws a label of height H with an em of
H/1.521, so it looks smaller than a mockup that uses H as the em): do not shrink them.
  capture / IR name 52, knob values 38, knob names 27, switch names 25, pack 34, detail 30, TONE3000 status 27,
  list rows 30, list title 28."""
import json, sys
IVORY, GREY = "ffe6dfd2", "ff9a958c"
ROLES = [  # (definition key prefix, label name, height, colour)
    ("shStepText_", "Value", 52.0, IVORY),
    ("shKnob", "Value", 38.0, IVORY),
    ("shKnob", "Name", 27.0, GREY),
    ("shToggle", "Name", 25.0, GREY),
    ("shReadout_588x40", "Value", 34.0, GREY),      # the pack (cab)
    ("shReadout_300x40", "Value", 34.0, GREY),      # the pack (amp: shares its row with the detail)
    ("shReadout_284x40", "Value", 30.0, GREY),      # the amp's detail, right-aligned on the pack row
    ("shReadout_588x38", "Value", 30.0, GREY),      # the detail line (cab)
    ("shReadout_300x36", "Value", 27.0, GREY),      # the TONE3000 status (header)
    ("shReadout_242x40", "Value", 30.0, IVORY),     # the preset in use (header: "My: <name>")
    ("shReadout_226x36", "Value", 28.0, GREY),      # a list's title ("Amps 7-12 of 83")
    ("shRow_", "Value", 30.0, IVORY),               # a list row
]

p = sys.argv[1].rstrip("/") + "/TUI.json"
t = json.load(open(p))
done = set()
for d in t["pageData"]["componentDefinitions"]["localComponentDefinitions"]:
    for prefix, name, h, col in ROLES:
        if not d["key"].startswith(prefix):
            continue
        for c in d["value"].get("componentsData", []):
            cd = c["componentData"]
            if cd.get("type") == "Label" and cd.get("name") == name:
                cd["data"]["textStyle"]["font"]["height"] = h
                cd["data"]["textStyle"]["colour"] = col
                done.add((prefix, name))
missing = {(p_, n) for p_, n, _, _ in ROLES} - done
if missing:
    sys.exit("skin_post: no component for %s (layout changed?)" % sorted(missing))
json.dump(t, open(p, "w"), indent=1)
print("skin_post: %d type roles set" % len(done))
