// SPDX-License-Identifier: MIT
// Copyright (c) 2026 the mpc-vst-liminal-amp contributors
// ir_test.cpp -- engine/ir_convolver (FFT) against the official plugin's dsp::ImpulseResponse (AudioDSPTools,
// direct convolution): the same raw IR and input must give the same output, for IRs shorter than, equal to and
// longer than one block, at and beyond the 8192-sample cap, with and without resampling (48k IR, 44.1k host), and
// for host blocks of MPC's 128 as well as odd sizes (which add one block of latency).
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>
#include "dsp/ImpulseResponse.h"
#include "ir_convolver.h"
using namespace mpcnam;
static int fails = 0;
#define CHECK(c, ...) do { printf("%s ", (c) ? "ok  " : "FAIL"); printf(__VA_ARGS__); printf("\n"); if (!(c)) fails++; } while (0)

static void compare(int ir_len, double ir_rate, double host_rate, int host_block) {
  std::mt19937 rng(ir_len * 7 + host_block);
  std::normal_distribution<float> g(0.f, 1.f);
  std::vector<float> raw(ir_len);
  for (int i = 0; i < ir_len; i++) raw[i] = g(rng) * std::exp(-3.0f * i / ir_len);   // decaying, like a cab IR
  dsp::ImpulseResponse::IRData data;
  data.mRawAudio = raw;
  data.mRawAudioSampleRate = ir_rate;
  dsp::ImpulseResponse official(data, host_rate);
  std::string err;
  auto ours = IRConvolver::make(raw, ir_rate, host_rate, &err);
  if (!ours) { CHECK(false, "IR %d: make failed: %s", ir_len, err.c_str()); return; }
  const int total = 128 * 120;                               // > 8192 + a few blocks: the whole tail is exercised
  std::vector<double> in_d(total);
  std::vector<float> in_f(total), ref(total), out(total);
  for (int i = 0; i < total; i++) { in_f[i] = g(rng) * 0.3f; in_d[i] = in_f[i]; }
  for (int p = 0; p < total; p += 128) {                    // the official one, in 128-sample calls
    double* ip[1] = {in_d.data() + p};
    double** op = official.Process(ip, 1, 128);
    for (int i = 0; i < 128; i++) ref[p + i] = (float)op[0][i];
  }
  for (int p = 0; p < total; p += host_block) {             // ours, at the host's block size
    int n = std::min(host_block, total - p);
    ours->process(in_f.data() + p, out.data() + p, n);
  }
  const int lat = (host_block % IRConvolver::kBlock) ? IRConvolver::kBlock : 0;
  double err_max = 0, peak = 0;
  for (int i = 0; i + lat < total; i++) {
    err_max = std::max(err_max, (double)std::fabs(out[i + lat] - ref[i]));
    peak = std::max(peak, (double)std::fabs(ref[i]));
  }
  CHECK(err_max <= 2e-5 * std::max(1.0, peak),
        "IR %5d samples @ %5.0f Hz -> host %5.0f Hz, blocks of %3d: max |ours - official| %.2e (peak %.3f, latency %d)",
        ir_len, ir_rate, host_rate, host_block, err_max, peak, lat);
}

int main() {
  for (int len : {50, 128, 129, 1000, 8192, 12000}) compare(len, 44100, 44100, 128);   // no resampling
  for (int len : {500, 4096, 9000}) compare(len, 48000, 44100, 128);                   // resampled, as most IR files
  compare(2048, 44100, 44100, 100);                                                    // odd host block: FIFO path
  compare(2048, 48000, 44100, 256);                                                    // multiple of 128
  printf("%s (%d failure%s)\n", fails ? "FAILED" : "PASSED", fails, fails == 1 ? "" : "s");
  return fails ? 1 : 0;
}
