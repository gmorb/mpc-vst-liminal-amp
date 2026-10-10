// SPDX-License-Identifier: MIT
// Copyright (c) 2026 the mpc-vst-liminal-amp contributors
// chain_render.cpp -- 20 s through the whole chain (model, gate, EQ away from flat, 8192-sample IR, DC blocker),
// written as float32: built with the gate/EQ/DC filters in double and in float (DSP_SAMPLE_FLOAT), and compared.
//   chain_render <model.nam> <out.f32>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>
#include "nam_engine.h"
using namespace mpcnam;
int main(int argc, char** argv) {
  std::string err;
  Engine e(44100, 128);
  e.set_model(Model::load(argv[1], 44100, 128, &err));
  std::vector<float> raw(9000); std::mt19937 r(1); std::uniform_real_distribution<float> u(-1, 1);
  for (size_t i = 0; i < raw.size(); i++) raw[i] = 0.3f * u(r) * std::exp(-4.0f * i / raw.size());
  e.set_ir(IRConvolver::make(raw, 48000, 44100, &err));
  e.params.bass = 8; e.params.mid = 3; e.params.treble = 7; e.params.gate_db = -60; e.params.input_db = 6;
  std::vector<float> in(128), ol(128), orr(128);
  FILE* f = fopen(argv[2], "wb"); long ph = 0;
  for (int b = 0; b < 44100 * 20 / 128; b++) {
    for (int i = 0; i < 128; i++, ph++) {            // plucked notes with silent gaps (the gate works), plus a DC step
      double t = ph / 44100.0, env = std::fmod(t, 2.0) < 1.2 ? std::exp(-3 * std::fmod(t, 2.0)) : 0.0;
      in[i] = (float)(0.3 * std::sin(2 * M_PI * 82.4 * t) * env + (t > 10 ? 0.05 : 0.0));
    }
    e.process(in.data(), in.data(), ol.data(), orr.data(), 128);
    fwrite(ol.data(), sizeof(float), 128, f);
  }
  fclose(f);
  return 0;
}
