#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 the mpc-vst-liminal-amp contributors
"""param_json.py <out.json> -- the plugin's parameter description in the Force VST repository's <Name>.json
format (as its DISTRHO and AirWindows plugins ship), from vst/params.json (the VST parameter order)."""
import json, os, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
params = json.load(open(os.path.join(ROOT, "vst", "params.json")))["params"]
NOTES = {"model": "the .nam file in use (text); Q-Link or the arrows browse the models/ folders",
         "ir": "the cabinet IR .wav in use (text); Q-Link or the arrows browse the irs/ folders",
         "model_info": "position and folder of the model, or a status (read-only)",
         "ir_info": "position and folder of the IR, or a status (read-only)",
         "rescan": "look for new files on the drives", "normalize": "model loudness to -18 dB (as the official plugin)",
         "ir_len": "Short: cabinet IRs cut to 4096 samples (~93 ms), for lower CPU"}
doc = {"name": "Liminal Amp", "numParams": len(params), "parameters": {}}
for i, p in enumerate(params):
    opts = p.get("options")
    kind = ("steps" if opts else "button" if p.get("momentary") else
            "text" if p.get("display") == "string" else "knob")
    doc["parameters"][str(i)] = {
        "index": i, "caseLabel": p["key"], "name": p["name"], "label": p.get("unit"), "variable": p["key"],
        "transform": "value", "range": [0, len(opts) - 1] if opts else [p["min"], p["max"]],
        "displayExpr": None, "displayRange": None if opts else [p["min"], p["max"]], "type": kind,
        "displayValues": opts, "unit": p.get("unit") or ("enum" if opts else None),
        "display": {"format": "list" if opts else "text" if kind == "text" else "%.1f", "note": NOTES.get(p["key"], "")},
        "default": p.get("default"), "confidence": 1.0,
        "evidence": ["NeuralAmpModelerPlugin v0.7.14 NeuralAmpModeler.cpp (ranges, defaults)"
                     if i <= 10 else "mpc-vst-liminal-amp wrapper: " + p["key"]],
        "override_safe": True}
json.dump(doc, open(sys.argv[1], "w"), indent=1)
