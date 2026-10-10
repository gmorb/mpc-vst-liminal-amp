// SPDX-License-Identifier: MIT
// Copyright (c) 2026 the mpc-vst-liminal-amp contributors
// a2_shape_test.cpp <a2_slimmable.nam> -- NAM Core must take its A2 fast path for both sizes of an A2 file laid out
// exactly as TONE3000 serves them (tools/make_test_models.py). NAM Core v0.5.2 refused real captures (it required
// head_scale == 0.01) and ran them on the generic path at ~3x the CPU; this keeps that from coming back.
#include <cstdio>
#include <fstream>
#include "json.hpp"
#include "NAM/wavenet/a2_fast.h"
int main(int argc, char** argv) {
  if (argc < 2) { printf("usage: a2_shape_test <a2_slimmable.nam>\n"); return 2; }
  nlohmann::json d = nlohmann::json::parse(std::ifstream(argv[1]));
  int fails = 0, n = 0;
  for (auto& s : d["config"]["submodels"]) {
    int ch = -1;
    bool ok = nam::wavenet::a2_fast::is_a2_shape(s["model"]["config"], &ch);
    printf("%s A2 fast path for the %d-channel submodel (max_value %s)\n", ok ? "ok  " : "FAIL", ch, s["max_value"].dump().c_str());
    fails += !ok; n++;
  }
  if (n != 2) { printf("FAIL expected 2 submodels, got %d\n", n); fails++; }
  printf("%s (%d failure%s)\n", fails ? "FAILED" : "PASSED", fails, fails == 1 ? "" : "s");
  return fails ? 1 : 0;
}
