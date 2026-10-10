// SPDX-License-Identifier: MIT
// Copyright (c) 2026 the mpc-vst-liminal-amp contributors
// readme_demo.cpp -- runs the plugin .so the way MPC's host does and lets a script drive it, so the README's images
// show what the plugin itself says and does (its text, lists, valve heat, pluck prints and phone pages).
//   readme_demo <plugin.so>            commands on stdin, one per line; every command answers one line "ok" (or data, then "end")
//     p <idx>          press a button (set 1, run two blocks)
//     s <idx> <v>      set a parameter, 0..1 as the host does
//     lvl <amp>        the audio the plugin is fed: a 110 Hz sine of this amplitude (0 = silence); audio runs all the time
//     w <ms>           wait
//     dump             "idx|value|display" for every parameter (what the host and the page see), then "end"
//     quit
// MPCNAM_MEDIA, MPCNAM_PRESETS and MPCNAM_T3K_HOST / _PORT choose the card, the presets folder and the phone page's address.
#include <dlfcn.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

struct AEffect;
typedef intptr_t (*hostcb)(AEffect*, int32_t, int32_t, intptr_t, void*, float);
struct AEffect {
  int32_t magic;
  intptr_t (*d)(AEffect*, int32_t, int32_t, intptr_t, void*, float);
  void (*process)(AEffect*, float**, float**, int32_t);
  void (*setP)(AEffect*, int32_t, float);
  float (*getP)(AEffect*, int32_t);
  int32_t numPrograms, numParams, numInputs, numOutputs, flags;
  intptr_t r1, r2;
  int32_t a, b, c;
  float io;
  void *obj, *user;
  int32_t uid, ver;
  void (*pr)(AEffect*, float**, float**, int32_t);
  void* pdr;
  char f[56];
};
static intptr_t host(AEffect*, int32_t op, int32_t, intptr_t, void*, float) { return op == 1 ? 2400 : 0; }
static std::atomic<float> level{0.f};
static std::atomic<bool> running{true};

static void audio(AEffect* a) {                 // MPC's audio thread: 128-frame blocks at 44.1 kHz, in real time
  std::vector<float> l(128), r(128), ol(128), orr(128);
  long ph = 0;
  auto next = std::chrono::steady_clock::now();
  while (running) {
    const float amp = level.load();
    for (int i = 0; i < 128; i++, ph++) l[i] = r[i] = amp * (float)std::sin(2 * M_PI * 110 * ph / 44100.0);
    float* in[2] = {l.data(), r.data()};
    float* out[2] = {ol.data(), orr.data()};
    a->pr(a, in, out, 128);
    next += std::chrono::microseconds(2902);
    std::this_thread::sleep_until(next);
  }
}

int main(int argc, char** argv) {
  if (argc < 2) { fprintf(stderr, "usage: readme_demo <plugin.so>\n"); return 2; }
  void* h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
  if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
  auto entry = (AEffect * (*)(hostcb)) dlsym(h, "VSTPluginMain");
  if (!entry) { fprintf(stderr, "no VSTPluginMain\n"); return 1; }
  AEffect* a = entry(host);
  a->d(a, 0, 0, 0, nullptr, 0);
  a->d(a, 10, 0, 0, nullptr, 44100.0f);
  a->d(a, 11, 0, 128, nullptr, 0);
  a->d(a, 12, 0, 1, nullptr, 0);
  std::thread t(audio, a);
  printf("ready %d\n", a->numParams); fflush(stdout);
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream is(line);
    std::string c; is >> c;
    if (c == "p") { int i; is >> i; a->setP(a, i, 1.f); std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
    else if (c == "s") { int i; float v; is >> i >> v; a->setP(a, i, v); }
    else if (c == "lvl") { float v; is >> v; level = v; }
    else if (c == "w") { int ms; is >> ms; std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
    else if (c == "dump") {
      for (int i = 0; i < a->numParams; i++) {
        char b[256] = {0};
        a->d(a, 7, i, 0, b, 0);
        printf("%d|%.6f|%s\n", i, a->getP(a, i), b);
      }
      printf("end\n"); fflush(stdout); continue;
    }
    else if (c == "quit") break;
    printf("ok\n"); fflush(stdout);
  }
  running = false;
  t.join();
  a->d(a, 1, 0, 0, nullptr, 0);
  return 0;
}
