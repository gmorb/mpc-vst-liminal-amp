// SPDX-License-Identifier: MIT
// Copyright (c) 2026 the mpc-vst-liminal-amp contributors
// chain_bench.cpp -- the whole amp chain on MPC's audio clock (44.1 kHz host, 128-frame blocks), stage by stage:
// the model alone, + noise gate and tone stack, + an 8192-sample cabinet IR (the longest the plugin uses; built in
// memory, no .wav needed). Thread CPU time per block, as mpc-vst-plugins' bench (p99 <= 15% PASS, <= 35% WARN).
//   mpcnam-chain-bench [-v] <model.nam>        -v: report each stage as it runs; output is unbuffered (ssh)
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <time.h>
#include <vector>
#include "nam_engine.h"
using namespace mpcnam;
static double cpu_us() { timespec t; clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t); return t.tv_sec * 1e6 + t.tv_nsec / 1e3; }
static double wall_s() { timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }
static bool verbose = false;
static double t0;
#define LOG(...) do { if (verbose) { printf("[%7.2fs] ", wall_s() - t0); printf(__VA_ARGS__); printf("\n"); } } while (0)
static const double RATE = 44100;
static const int B = 128;
static long phase = 0;

static void stage(Engine& e, const char* name) {
  LOG("%s: running 6 s of audio", name);
  std::vector<float> l(B), ol(B), orr(B);
  std::vector<double> t;
  for (int b = 0; b < (int)(RATE * 6 / B); b++) {
    for (int i = 0; i < B; i++, phase++) {       // a plucked 110 Hz note every second
      double ts = phase / RATE;
      l[i] = (float)(0.3 * std::sin(2 * M_PI * 110 * ts) * std::exp(-3 * std::fmod(ts, 1.0)));
    }
    double s = cpu_us();
    e.process(l.data(), l.data(), ol.data(), orr.data(), B);
    if (b > 100) t.push_back(cpu_us() - s);
  }
  std::sort(t.begin(), t.end());
  double mean = 0; for (double x : t) mean += x; mean /= t.size();
  const double bud = 1e6 * B / RATE;
  double p99 = 100 * t[t.size() * 99 / 100] / bud, mx = 100 * t.back() / bud;
  const char* v = (p99 <= 15 && mx <= 50) ? "PASS" : (p99 <= 35 && mx <= 80) ? "WARN" : "FAIL";
  printf("%-40s mean %6.2f%%  p99 %6.2f%%  max %6.2f%%  %s\n", name, 100 * mean / bud, p99, mx, v);
}

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  t0 = wall_s();
  int a = 1;
  for (; a < argc && !strcmp(argv[a], "-v"); a++) verbose = true;
  if (a >= argc) { printf("usage: mpcnam-chain-bench [-v] <model.nam>\n       -v  report each stage as it runs\n"); return 2; }
  const char* path = argv[a];
  printf("MPC OS: 44.1 kHz, 128-frame blocks (2902 us budget); %% of one block on one core\n");
  LOG("loading %s", path);
  std::string err;
  auto model = Model::load(path, RATE, B, &err);
  if (!model) { printf("error: %s: %s\n", path, err.c_str()); return 1; }
  LOG("loaded: %s, model rate %.0f Hz, resampler latency %d samples", model->architecture().c_str(),
      model->model_rate(), model->latency());
  LOG("building an 8192-sample cabinet IR in memory");
  std::mt19937 rng(1);
  std::normal_distribution<float> g(0.f, 1.f);
  std::vector<float> raw(9000);
  for (size_t i = 0; i < raw.size(); i++) raw[i] = 0.3f * g(rng) * std::exp(-4.0f * i / raw.size());
  auto ir = IRConvolver::make(raw, 48000, RATE, &err);
  if (!ir) { printf("error: IR: %s\n", err.c_str()); return 1; }
  LOG("IR ready: %zu samples at 44.1 kHz", ir->length());

  Engine e(RATE, B);
  e.set_model(std::move(model));
  e.params.gate_on = false;
  e.params.eq_on = false;
  stage(e, "model (A2-Lite, resampled 44.1<->48 kHz)");
  e.params.gate_on = true;
  e.params.eq_on = true;
  e.params.bass = 7; e.params.mid = 4; e.params.treble = 6;
  stage(e, "+ noise gate + tone stack");
  e.set_ir(std::move(ir));
  stage(e, "+ 8192-sample cabinet IR (full chain)");
  e.params.eco = true;
  stage(e, "full chain, ECO (no resampling)");
  e.collect();
  LOG("done");
  printf("done\n");
  return 0;
}
