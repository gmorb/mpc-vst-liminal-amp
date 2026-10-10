// engine_test.cpp -- the amp chain as MPC runs it (44.1 kHz, 128-frame blocks): output sanity, each stage's effect,
// model/IR swaps from another thread while audio runs, and the cost of each stage.
//   engine_test <model.nam> <long IR.wav> <short IR.wav>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>
#include <time.h>
#include <vector>
#include "nam_engine.h"
using namespace mpcnam;
static double cpu_us() { timespec t; clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t); return t.tv_sec * 1e6 + t.tv_nsec / 1e3; }
static int fails = 0;
#define CHECK(c, ...) do { printf("%s ", (c) ? "ok  " : "FAIL"); printf(__VA_ARGS__); printf("\n"); if (!(c)) fails++; } while (0)
static const double RATE = 44100; static const int B = 128;
struct Run { double rms = 0, peak = 0; bool finite = true; double mean_pct = 0, p99_pct = 0; };
static Run run(Engine& e, int blocks, double amp = 0.3, bool time_it = false) {
  std::vector<float> l(B), r(B), ol(B), orr(B); std::vector<double> t; double acc = 0; Run R; long n = 0;
  static long phase = 0;
  for (int b = 0; b < blocks; b++) {
    for (int i = 0; i < B; i++, phase++) { double s = amp * std::sin(2 * M_PI * 110 * phase / RATE);   /* steady: level comparisons need a stationary signal */ l[i] = r[i] = (float)s; }
    double s0 = cpu_us(); e.process(l.data(), r.data(), ol.data(), orr.data(), B); if (time_it) t.push_back(cpu_us() - s0);
    for (int i = 0; i < B; i++) { if (!std::isfinite(ol[i]) || ol[i] != orr[i]) R.finite = false; acc += ol[i] * ol[i]; R.peak = std::max(R.peak, (double)std::fabs(ol[i])); n++; }
  }
  R.rms = std::sqrt(acc / n);
  if (time_it) { std::sort(t.begin(), t.end()); double m = 0; for (double x : t) m += x; m /= t.size(); double bud = 1e6 * B / RATE; R.mean_pct = 100 * m / bud; R.p99_pct = 100 * t[t.size() * 99 / 100] / bud; }
  return R;
}
int main(int argc, char** argv) {
  if (argc < 4) { printf("usage: engine_test <model.nam> <long IR.wav> <short IR.wav>\n"); return 2; }
  std::string err;
  Engine e(RATE, 512);
  auto m = Model::load(argv[1], RATE, 512, &err);
  CHECK(m != nullptr, "model loads (%s)", m ? m->architecture().c_str() : err.c_str());
  printf("     model rate %.0f Hz, resampler latency %d samples\n", m->model_rate(), m->latency());
  auto ir = IRConvolver::load_wav(argv[2], RATE, &err);
  CHECK(ir != nullptr, "long IR loads (%zu samples at 44.1 kHz)", ir ? ir->length() : 0);
  auto bad = IRConvolver::load_wav("/nonexistent.wav", RATE, &err);
  CHECK(bad == nullptr && !err.empty(), "a missing IR is refused with a message (%s)", err.c_str());
  auto badm = Model::load("/nonexistent.nam", RATE, 512, &err);
  CHECK(badm == nullptr && !err.empty(), "a missing model is refused with a message");

  Run dry = run(e, 200);
  CHECK(dry.finite && dry.rms > 0.01, "no model, no IR: the signal passes (rms %.3f)", dry.rms);
  e.set_model(std::move(m)); e.set_ir(std::move(ir));
  Run full = run(e, 400);
  CHECK(full.finite && full.peak < 50, "model + IR: finite, bounded (rms %.4f, peak %.3f), both channels equal", full.rms, full.peak);
  e.params.ir_on = false; Run noir = run(e, 200); e.params.ir_on = true;
  CHECK(noir.finite && std::fabs(noir.rms - full.rms) > 1e-6, "IR off changes the sound (rms %.4f vs %.4f)", noir.rms, full.rms);
  e.params.bass = 10; e.params.treble = 0; Run eq = run(e, 200); e.params.bass = 5; e.params.treble = 5;
  CHECK(eq.finite && std::fabs(eq.rms - full.rms) > 1e-6, "tone controls change the sound");
  { // Eco: the model at the host rate (no resampler): different (shifted) tone, finite; switching is faded
    e.params.eco = true;
    Run eco = run(e, 400);
    CHECK(eco.finite && eco.peak < 50 && std::fabs(eco.rms - full.rms) > 1e-7,
          "Eco runs the model without resampling: finite, bounded, a different tone (rms %.4f vs %.4f)", eco.rms, full.rms);
    std::vector<float> l(128), r(128), ol(128), orr(128);
    float first = 1;
    e.params.eco = false;                                 // switch back: the first samples are faded in
    for (int i = 0; i < 128; i++) l[i] = r[i] = 0.3f * (float)std::sin(2 * M_PI * 110 * i / 44100.0);
    e.process(l.data(), r.data(), ol.data(), orr.data(), 128);
    first = std::fabs(ol[0]);
    CHECK(std::isfinite(ol[127]) && first < 0.05f, "switching Eco fades in (first sample %.5f)", first);
  }
  { // gain and gate on a predictable chain: no model, no IR (the plugin's pass-through), EQ flat at 5
    Engine p(RATE, 512);
    p.params.gate_on = false;
    run(p, 200);
    Run a0 = run(p, 200); p.params.output_db = -20; Run q = run(p, 200); p.params.output_db = 20; Run l = run(p, 200);
    p.params.output_db = 0;
    CHECK(std::fabs(a0.rms / q.rms - 10) < 0.05 && std::fabs(l.rms / a0.rms - 10) < 0.05,
          "output -20 / +20 dB: exactly 10x quieter / louder (x%.3f, x%.3f)", a0.rms / q.rms, l.rms / a0.rms);
    p.params.input_db = 6; run(p, 50); Run in6 = run(p, 200); p.params.input_db = 0;
    CHECK(std::fabs(in6.rms / a0.rms - 1.995) < 0.02, "input +6 dB: x%.3f (2.0 expected)", in6.rms / a0.rms);
    run(p, 200, 0.001);                                    // settle at the quiet level first: the 5 Hz DC blocker
                                                           // is still ringing out the loud section before
    Run open_ = run(p, 200, 0.001);                        // -60 dBFS, gate off: passes
    p.params.gate_on = true; p.params.gate_db = -20; run(p, 100, 0.001); Run shut = run(p, 200, 0.001);
    p.params.gate_db = -100; run(p, 100, 0.001); Run open2 = run(p, 200, 0.001);
    CHECK(shut.rms < open_.rms * 0.01, "gate at -20 dB closes on a -60 dBFS signal (%.2e -> %.2e)", open_.rms, shut.rms);
    CHECK(std::fabs(open2.rms / open_.rms - 1) < 0.05, "gate at -100 dB lets it through (x%.3f)", open2.rms / open_.rms);
    p.params.gate_on = false; p.params.bass = 10; run(p, 50); Run bb = run(p, 200);  // 110 Hz sine under a +20 dB shelf
    CHECK(bb.rms / a0.rms > 5, "bass 10 boosts a 110 Hz tone (x%.1f; the shelf is +20 dB at 150 Hz)", bb.rms / a0.rms);
  }

  // swaps from another thread while audio runs
  std::atomic<bool> stop{false}; std::atomic<int> swaps{0};
  std::thread loader([&] {
    for (int k = 0; k < 40; k++) {
      std::string er;
      if (k % 2) e.set_model(Model::load(argv[1], RATE, 512, &er)); else e.set_model(nullptr);
      e.set_ir(IRConvolver::load_wav(k % 3 ? argv[2] : argv[3], RATE, &er));
      e.collect(); swaps++;
      std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }
  });
  Run during; bool sane = true; int blocks = 0;
  while (swaps < 40) { during = run(e, 20); blocks += 20; if (!during.finite || during.peak > 50) sane = false; }
  loader.join(); e.collect();
  CHECK(sane, "%d model/IR swaps from another thread during %d audio blocks: no crash, output sane", (int)swaps, blocks);

  // cost of each stage (final state: model + long IR)
  e.set_model(Model::load(argv[1], RATE, 512, &err)); e.set_ir(IRConvolver::load_wav(argv[2], RATE, &err));
  run(e, 50);
  e.params.ir_on = false; e.params.eq_on = false; e.params.gate_on = false; Run c1 = run(e, 1500, 0.3, true);
  e.params.gate_on = true; e.params.eq_on = true; Run c2 = run(e, 1500, 0.3, true);
  e.params.ir_on = true; Run c3 = run(e, 1500, 0.3, true);
  printf("cost (%% of one 2902 us block): model only %.2f%% (p99 %.2f) | + gate + EQ %.2f%% | full chain with 8192-sample IR %.2f%% (p99 %.2f)\n",
         c1.mean_pct, c1.p99_pct, c2.mean_pct, c3.mean_pct, c3.p99_pct);
  printf("%s (%d failures)\n", fails ? "FAILED" : "PASSED", fails);
  return fails ? 1 : 0;
}
