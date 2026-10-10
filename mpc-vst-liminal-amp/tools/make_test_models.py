#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 the mpc-vst-liminal-amp contributors
"""make_test_models.py <out dir> -- test inputs, made from scratch (no downloads, no third-party captures):
  a2_3.nam         an A2-Lite WaveNet (3 channels), random weights: costs exactly what a real A2-Lite capture costs
  a2_8.nam         the same at A2-Full size (8 channels)
  a2_slimmable.nam both, as a slimmable container (as A2 files ship: Lite below 0.5, Full above)
  ir_long.wav      9000 samples at 48 kHz, 24-bit mono (longer than the 8192-sample cap)
  ir_short.wav     512 samples at 44.1 kHz, 16-bit mono
The A2 shape (kernel sizes, dilations, activations, head) is read from NeuralAmpModelerCore's a2_fast.h, and the
file layout is exactly what the NAM trainer exports for A2 (checked against neural-amp-modeler's export code), so
these load and cost exactly as downloaded A2 captures do."""
import json, math, os, random, re, struct, sys, wave
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
out = sys.argv[1]
os.makedirs(out, exist_ok=True)
h = open(os.path.join(ROOT, "src/nam-core/NAM/wavenet/a2_fast.h")).read()
num = lambda name: [int(x) for x in re.search(name + r"\s*=\s*\{([^}]*)\}", h).group(1).replace("\n", " ").split(",") if x.strip()]
ks, dl = num("kKernelSizes"), num("kDilations")
random.seed(1)
models = {}
for ch in (3, 8):
    # exactly the layer config the NAM trainer exports for A2 (neural-amp-modeler: nam/models/wavenet/_layer_array.py
    # LayerArray.export_config; PackedWaveNet with 3- and 8-channel submodels, container_max_values "uniform")
    layer = {"input_size": 1, "condition_size": 1, "head": {"out_channels": 1, "kernel_size": 16, "bias": True},
             "channels": ch, "kernel_sizes": ks, "dilations": dl,
             "activation": [{"type": "LeakyReLU", "negative_slope": 0.01}] * len(ks), "bottleneck": ch,
             "head1x1": {"active": False, "out_channels": 1, "groups": 1}, "layer1x1": {"active": True, "groups": 1},
             "groups_input": 1, "groups_input_mixin": 1, "gating_mode": ["none"] * len(ks),
             "secondary_activation": [None] * len(ks), "slimmable": None}
    # ... and, as in files TONE3000 serves (checked against a real A2 capture, 2026-09-30), every FiLM slot present
    # but inactive
    for k in ("conv_pre_film", "conv_post_film", "input_mixin_pre_film", "input_mixin_post_film",
              "activation_pre_film", "activation_post_film", "layer1x1_post_film", "head1x1_post_film"):
        layer[k] = {"active": False, "shift": True, "groups": 1}
    n = ch + sum(k * ch * ch + ch + ch + ch * ch + ch for k in ks) + 16 * ch + 1     # a2_fast.cpp's weight order
    hs = 0.011273000426214248      # a trained head_scale, from a real A2 capture (also the last weight)
    meta = {"date": {"year": 2026, "month": 9, "day": 4, "hour": 10, "minute": 39, "second": 16},
            "loudness": -16.4, "gain": 0.63, "name": "test", "gear_type": "amp_cab"}
    m = {"version": "0.7.0", "metadata": meta, "architecture": "WaveNet",
         "config": {"layers": [layer], "head": None, "head_scale": hs},
         "weights": [random.gauss(0, 0.1) for _ in range(n)] + [hs], "sample_rate": 48000}
    json.dump(m, open(os.path.join(out, "a2_%d.nam" % ch), "w"))
    models[ch] = m
json.dump({"version": "0.7.0", "architecture": "SlimmableContainer", "weights": [], "sample_rate": 48000,
           "metadata": models[3]["metadata"],
           "config": {"submodels": [{"max_value": 0.5, "model": models[3]}, {"max_value": 1, "model": models[8]}]}},
          open(os.path.join(out, "a2_slimmable.nam"), "w"))

def write(path, n, rate, bits):
    w = wave.open(path, "wb"); w.setnchannels(1); w.setsampwidth(bits // 8); w.setframerate(rate)
    fr = bytearray()
    for i in range(n):
        v = max(-0.99, min(0.99, random.gauss(0, 0.3) * math.exp(-4.0 * i / n)))
        fr += struct.pack("<i", int(v * 8388607))[:3] if bits == 24 else struct.pack("<h", int(v * 32767))
    w.writeframes(bytes(fr)); w.close()
write(os.path.join(out, "ir_long.wav"), 9000, 48000, 24)
write(os.path.join(out, "ir_short.wav"), 512, 44100, 16)
print("test inputs in", out)
