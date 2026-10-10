// bench_nam.cpp -- Liminal Amp (NAM A2) on MPC's audio clock: 44.1 kHz host, 128-frame blocks. A2 models run at 48 kHz, so each
// host block is ~139.3 model samples (the plugin resamples); timed with the thread's CPU clock, as mpc-vst-plugins'
// bench (p99 <= 15% PASS, <= 35% WARN, of one 2902 us block).
//   mpcnam-bench <model.nam> [<model.nam> ...]        (a slimmable A2 file is benched at Lite and at Full)
//   mpcnam-bench --render <model.nam> <size 0..1> <out.f32>   (2 s of a test signal, for cross-build checks)
//   -v   verbose: report every stage (start, load, warm-up, progress) as it happens, with elapsed time
// Output is unbuffered, so it shows immediately even through ssh.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <time.h>
#include <vector>
#include "NAM/dsp.h"
#include "NAM/get_dsp.h"
#include "NAM/slimmable.h"

static double now_us() { timespec t; clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t); return t.tv_sec * 1e6 + t.tv_nsec / 1e3; }
static double wall_s() { timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }
static bool verbose = false;
static double t0 = 0;
#define LOG(...) do { if (verbose) { printf("[%7.2fs] ", wall_s() - t0); printf(__VA_ARGS__); printf("\n"); fflush(stdout); } } while (0)

static void bench(const char* path, double size, const char* label) {
  LOG("%s %s: loading the model", path, label);
  auto dsp = nam::get_dsp(std::filesystem::path(path));
  if (auto* s = dynamic_cast<nam::SlimmableModel*>(dsp.get())) { s->SetSlimmableSize(size); LOG("  size set to %.1f (%s)", size, label); }
  const int host_block = 128;
  const double ratio = 48000.0 / 44100.0;
  const int max_frames = 160;
  LOG("  warming up (sample rate 48000, max block %d)", max_frames);
  dsp->ResetAndPrewarm(48000.0, max_frames);
  LOG("  running 6 s of audio in 128-frame host blocks");
  std::vector<NAM_SAMPLE> in(max_frames), out(max_frames);
  const double budget = 1e6 * host_block / 44100.0;
  std::vector<double> t;
  double acc = 0;
  for (int b = 0; b < 44100 * 6 / host_block; b++) {        // 6 s of a guitar-ish signal
    acc += host_block * ratio;
    int n = (int)acc; acc -= n;                               // 139 or 140 model samples per host block
    for (int i = 0; i < n; i++) {
      double ph = (b * 139.3 + i) / 48000.0;
      in[i] = (NAM_SAMPLE)(0.3 * std::sin(2 * M_PI * 110 * ph) * std::exp(-std::fmod(ph, 1.0) * 3));
    }
    NAM_SAMPLE* ip = in.data(); NAM_SAMPLE* op = out.data();
    double s = now_us();
    dsp->process(&ip, &op, n);
    if (b > 100) t.push_back(now_us() - s);
    if (verbose && b % 689 == 0 && b > 0)
      LOG("  %.0f of 6 s processed, last block %.0f us (%.1f%% of the budget)", b * 128 / 44100.0, now_us() - s,
          100 * (now_us() - s) / (1e6 * host_block / 44100.0));
  }
  std::sort(t.begin(), t.end());
  double mean = 0; for (double x : t) mean += x; mean /= t.size();
  double p99 = 100 * t[t.size() * 99 / 100] / budget, mx = 100 * t.back() / budget;
  const char* v = (p99 <= 15 && mx <= 50) ? "PASS" : (p99 <= 35 && mx <= 80) ? "WARN" : "FAIL";
  printf("%-34s %-10s mean %6.2f%%  p99 %6.2f%%  max %6.2f%%  %s\n", path, label, 100 * mean / budget, p99, mx, v);
  fflush(stdout);
}

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);     // show output at once, even through ssh
  t0 = wall_s();
  int a = 1;
  for (; a < argc && !strcmp(argv[a], "-v"); a++) verbose = true;
  argv += a - 1; argc -= a - 1;
  if (argc < 2) {
    printf("usage: mpcnam-bench [-v] <model.nam> [<model.nam> ...]\n       -v  report every stage as it runs\n");
    return 2;
  }
  LOG("mpcnam-bench started (verbose)");
  if (argc == 5 && !strcmp(argv[1], "--render")) {
    auto dsp = nam::get_dsp(std::filesystem::path(argv[2]));
    if (auto* s = dynamic_cast<nam::SlimmableModel*>(dsp.get())) s->SetSlimmableSize(atof(argv[3]));
    dsp->ResetAndPrewarm(48000.0, 256);
    FILE* f = fopen(argv[4], "wb");
    std::vector<NAM_SAMPLE> in(256), out(256);
    for (int b = 0; b < 48000 * 2 / 256; b++) {
      for (int i = 0; i < 256; i++) { double ph = (b * 256 + i) / 48000.0; in[i] = (NAM_SAMPLE)(0.3 * std::sin(2 * M_PI * 110 * ph)); }
      NAM_SAMPLE* ip = in.data(); NAM_SAMPLE* op = out.data();
      dsp->process(&ip, &op, 256);
      for (int i = 0; i < 256; i++) { float v = (float)out[i]; fwrite(&v, 4, 1, f); }
    }
    fclose(f);
    return 0;
  }
  printf("MPC OS: 44.1 kHz, 128-frame blocks (2902 us); A2 runs at 48 kHz (~139 samples per block)\n");
  for (int i = 1; i < argc; i++) {
    LOG("reading %s", argv[i]);
    if (!std::filesystem::exists(argv[i])) { printf("error: %s not found\n", argv[i]); return 1; }
    auto probe = nam::get_dsp(std::filesystem::path(argv[i]));
    LOG("%s: %s model", argv[i], dynamic_cast<nam::SlimmableModel*>(probe.get()) ? "slimmable (A2-Lite + A2-Full)" : "single-size");
    if (dynamic_cast<nam::SlimmableModel*>(probe.get())) {
      bench(argv[i], 0.0, "A2-Lite");
      bench(argv[i], 1.0, "A2-Full");
    } else {
      bench(argv[i], 1.0, "");
    }
  }
  LOG("done");
  printf("done\n");
  return 0;
}
