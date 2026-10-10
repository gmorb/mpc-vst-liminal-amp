// SPDX-License-Identifier: MIT
// Copyright (c) 2026 the mpc-vst-liminal-amp contributors
// resampler_test.cpp -- AudioDSPTools' ResamplingContainer<float, 1, 12> (44.1 kHz host, 48 kHz model) run over
// 20 s of signal with a nonlinear "model" inside; writes the output as float32. Built twice, against upstream's
// LanczosResampler.h and against the patched one, and the outputs compared (tools/resampler_compare.sh).
//   resampler_test <out.f32> [block]
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>        // ResamplingContainer.h uses std::unique_ptr without including it
#include <random>
#include <vector>
#include "dsp/ResamplingContainer/ResamplingContainer.h"
int main(int argc, char** argv) {
  const int block = argc > 2 ? atoi(argv[2]) : 128;
  dsp::ResamplingContainer<float, 1, 12> rs(48000.0);
  rs.Reset(44100.0, block);
  auto model = [](float** in, float** out, int n) { for (int i = 0; i < n; i++) out[0][i] = std::tanh(3.0f * in[0][i]); };
  std::mt19937 rng(7); std::normal_distribution<float> g(0.f, 0.1f);
  std::vector<float> in(block), out(block);
  FILE* f = fopen(argv[1], "wb");
  long ph = 0;
  for (int b = 0; b < 44100 * 20 / block; b++) {
    for (int i = 0; i < block; i++, ph++) {
      double t = ph / 44100.0;
      in[i] = (float)(0.3 * std::sin(2 * M_PI * 110 * t) * std::exp(-3 * std::fmod(t, 1.0))) + g(rng);
    }
    float* ip[1] = {in.data()}; float* op[1] = {out.data()};
    rs.ProcessBlock(ip, op, block, model);
    fwrite(out.data(), sizeof(float), block, f);
  }
  fclose(f);
  return 0;
}
