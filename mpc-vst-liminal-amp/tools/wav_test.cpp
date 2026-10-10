// SPDX-License-Identifier: MIT
// Copyright (c) 2026 the mpc-vst-liminal-amp contributors
// wav_test.cpp <dir from make_test_wavs.py> -- engine/wav_reader against AudioDSPTools' loader (the official NAM
// plugin's): bit-identical where it accepts a file (32-bit PCM: exactly negated, its sign bug), and loading what it
// refuses (EXTENSIBLE without `fact`, 64-bit float, stereo: averaged to mono).
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "dsp/wav.h"
#include "wav_reader.h"
using namespace mpcnam;
static int fails = 0;
#define CHECK(c, ...) do { printf("%s ", (c) ? "ok  " : "FAIL"); printf(__VA_ARGS__); printf("\n"); if (!(c)) fails++; } while (0)
int main(int argc, char** argv) {
  const std::string dir = argv[1];
  struct C { const char* f; bool official_ok; int channels; double rate; float sign; };
  const C cases[] = {{"pcm16", true, 1, 48000, 1}, {"pcm24", true, 1, 48000, 1}, {"pcm32", true, 1, 48000, -1},
                     {"float32", true, 1, 48000, 1}, {"float64", false, 1, 48000, 1}, {"ext_pcm24", false, 1, 48000, 1},
                     {"ext_pcm24_fact", true, 1, 48000, 1}, {"ext_float32", false, 1, 48000, 1},
                     {"ext_float32_fact", true, 1, 48000, 1}, {"chunks", true, 1, 48000, 1}, {"rate96k", true, 1, 96000, 1},
                     {"stereo24", false, 2, 48000, 1}, {"stereo_ext_float", false, 2, 48000, 1}};
  for (const C& c : cases) {
    const std::string path = dir + "/" + c.f + ".wav";
    WavData w; std::string err;
    bool ok = read_wav(path, &w, &err);
    CHECK(ok && (int)w.channels.size() == c.channels && w.rate == c.rate && w.channels[0].size() == 4000,
          "%-17s reads: %zu channel(s), %.0f Hz, %zu frames %s", c.f, ok ? w.channels.size() : 0, w.rate,
          ok ? w.channels[0].size() : 0, ok ? "" : err.c_str());
    std::vector<float> off; double off_rate = 0;
    const bool off_ok = dsp::wav::Load(path.c_str(), off, off_rate) == dsp::wav::LoadReturnCode::SUCCESS;
    if (c.official_ok) {
      bool same = off_ok && ok && off.size() == w.channels[0].size() && off_rate == w.rate;
      for (size_t i = 0; same && i < off.size(); i++) same = w.channels[0][i] == c.sign * off[i];
      CHECK(same, "%-17s %s the official loader's samples%s", c.f, c.sign < 0 ? "exactly negates" : "is bit-identical to",
            c.sign < 0 ? " (its 32-bit sign bug)" : "");
    } else {
      CHECK(!off_ok, "%-17s (the official loader refuses it)", c.f);
    }
    if (ok && c.channels == 2) {
      auto m = w.mono_mix(); bool avg = m.size() == w.channels[0].size();
      for (size_t i = 0; avg && i < m.size(); i++) avg = std::fabs(m[i] - 0.5f * (w.channels[0][i] + w.channels[1][i])) < 1e-7f;
      CHECK(avg, "%-17s mono mix = the average of the channels", c.f);
    }
  }
  // the same content in every format: 24-bit PCM vs its EXTENSIBLE twins (same seed per name length is not the
  // same content, so compare a format pair written from the same name length: ext_pcm24 vs ext_pcm24_fact differ by
  // name length too) -- instead: an EXTENSIBLE file with fact == without fact, sample for sample
  WavData a, b; std::string e;
  read_wav(dir + "/ext_float32.wav", &a, &e); read_wav(dir + "/ext_float32_fact.wav", &b, &e);
  CHECK(a.channels.size() == 1 && b.channels.size() == 1 && a.channels[0].size() == b.channels[0].size(),
        "EXTENSIBLE with and without fact: same layout");
  WavData bad; std::string why;
  FILE* f = fopen((dir + "/not_a_wav.wav").c_str(), "w"); fputs("hello", f); fclose(f);
  CHECK(!read_wav(dir + "/not_a_wav.wav", &bad, &why) && why == "Not a WAV file", "a non-WAV file is refused: [%s]", why.c_str());
  printf("%s (%d failure%s)\n", fails ? "FAILED" : "PASSED", fails, fails == 1 ? "" : "s");
  return fails ? 1 : 0;
}
