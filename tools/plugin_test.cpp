// SPDX-License-Identifier: MIT
// Copyright (c) 2026 the mpc-vst-liminal-amp contributors
// plugin_test.cpp -- the plugin .so as MPC's host uses it, on a made-up /media (MPCNAM_MEDIA):
// scanning, browsing (arrows, wrap-around, Q-Link), loading in the background, a broken file, the cab controls,
// saving and restoring a project (also after the card's folders changed, and with a file missing), rescanning,
// browsing while audio runs on another thread, and closing.
//   plugin_test <plugin.so> <A2 model .nam> <A2-Lite model .nam> <long IR .wav> <short IR .wav>
#include <dlfcn.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <functional>
#include <string>
#include <thread>
#include <vector>
namespace fs = std::filesystem;

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
enum { P_INPUT, P_GATE_ON, P_GATE, P_BASS, P_MID, P_TREBLE, P_EQ_ON, P_OUTPUT, P_NORMALIZE, P_IR_ON, P_IR_LEN,
       P_MODEL, P_MODEL_PREV, P_MODEL_NEXT, P_MODEL_INFO, P_IR, P_IR_PREV, P_IR_NEXT, P_IR_INFO, P_RESCAN,
       P_GET_TONES, P_T3K_INFO, P_ECO,
       P_MODEL_PACK, P_MODEL_PACK_PREV, P_MODEL_PACK_NEXT, P_MODEL_SRC, P_MODEL_BROWSE,
       P_IR_PACK, P_IR_PACK_PREV, P_IR_PACK_NEXT, P_IR_SRC, P_IR_BROWSE, P_MODEL_DELETE, P_IR_DELETE, P_FULL,
       P_AMP_MENU, P_AMP_LIST, P_AMP_CLOSE, P_AMP_PREV, P_AMP_NEXT, P_AMP_TITLE, P_AMP_ROW1, P_AMP_ROW6 = P_AMP_ROW1 + 5,
       P_CAB_MENU, P_CAB_LIST, P_CAB_CLOSE, P_CAB_PREV, P_CAB_NEXT, P_CAB_TITLE, P_CAB_ROW1, P_CAB_ROW6 = P_CAB_ROW1 + 5, P_TUBES, P_WAVE1, P_WAVE14 = P_WAVE1 + 13,
       P_PRESET, P_MP_LIST, P_MP_SAVE, P_MP_DELETE, P_MP_PHONE, P_TOTAL };

static int fails = 0;
#define CHECK(c, ...) do { printf("%s ", (c) ? "ok  " : "FAIL"); printf(__VA_ARGS__); printf("\n"); if (!(c)) fails++; } while (0)
static std::atomic<int> automates{0};
static intptr_t host(AEffect*, int32_t op, int32_t, intptr_t, void*, float) {
  if (op == 0) automates++;
  return op == 1 ? 2400 : 0;
}
static AEffect* (*entry)(hostcb);

static std::string disp(AEffect* a, int i) {
  char b[256] = {0};
  a->d(a, 7, i, 0, b, 0);
  return b;
}
static bool wait_for(std::function<bool()> ok, double secs = 20) {
  for (int i = 0; i < secs * 50; i++) { if (ok()) return true; std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
  return ok();
}
static bool idle(AEffect* a) {        // scanned, and nothing loading
  auto m = disp(a, P_MODEL_INFO), r = disp(a, P_IR_INFO);
  return m != "Searching..." && m != "Loading..." && r != "Searching..." && r != "Loading...";
}
static double render(AEffect* a, int blocks, bool* finite = nullptr) {   // rms of the output for a steady tone
  static long ph = 0;
  std::vector<float> l(128), r(128), ol(128), orr(128);
  double acc = 0; long n = 0; bool fin = true;
  for (int b = 0; b < blocks; b++) {
    for (int i = 0; i < 128; i++, ph++) l[i] = r[i] = (float)(0.3 * std::sin(2 * M_PI * 110 * ph / 44100.0));
    float* in[2] = {l.data(), r.data()};
    float* out[2] = {ol.data(), orr.data()};
    a->pr(a, in, out, 128);
    for (int i = 0; i < 128; i++) {
      if (!std::isfinite(ol[i]) || ol[i] != orr[i]) fin = false;
      if (b >= blocks / 2) { acc += ol[i] * ol[i]; n++; }
    }
  }
  if (finite) *finite = fin;
  return std::sqrt(acc / std::max(1L, n));
}
static AEffect* open_plugin() {
  AEffect* a = entry(host);
  a->d(a, 0, 0, 0, nullptr, 0);
  a->d(a, 10, 0, 0, nullptr, 44100.0f);
  a->d(a, 11, 0, 128, nullptr, 0);
  a->d(a, 12, 0, 1, nullptr, 0);
  return a;
}
static void close_plugin(AEffect* a) { a->d(a, 1, 0, 0, nullptr, 0); }
static void press(AEffect* a, int i) { a->setP(a, i, 1.0f); render(a, 2); }
static void write_stereo_wav(const fs::path& p) {   // 16-bit stereo: a valid WAV the official loader refuses
  std::ofstream f(p, std::ios::binary);
  auto u32 = [&](uint32_t v) { f.write((const char*)&v, 4); };
  auto u16 = [&](uint16_t v) { f.write((const char*)&v, 2); };
  const uint32_t frames = 1000, data = frames * 4;
  f.write("RIFF", 4); u32(36 + data); f.write("WAVE", 4);
  f.write("fmt ", 4); u32(16); u16(1); u16(2); u32(48000); u32(48000 * 4); u16(4); u16(16);
  f.write("data", 4); u32(data);
  for (uint32_t i = 0; i < frames * 2; i++) u16((uint16_t)(i < 2 ? 12000 : 0));
}

int main(int argc, char** argv) {
  if (argc < 6) { printf("usage: plugin_test <plugin.so> <A2 .nam> <A2-Lite .nam> <long IR .wav> <short IR .wav>\n"); return 2; }
  // this run's own folders (a fresh name each run), cleaned up at the end; a failed clean-up is not a test failure
  const std::string tag = std::to_string((long)getpid());
  fs::path m1 = fs::temp_directory_path() / ("mpcnam_card1_" + tag), m2 = fs::temp_directory_path() / ("mpcnam_card2_" + tag);
  std::error_code ec;
  fs::remove_all(m1, ec); fs::remove_all(m2, ec);
  fs::create_directories(m1 / "662522/models/Fender"); fs::create_directories(m1 / "662522/models/Marshall");
  fs::create_directories(m1 / "662522/NAM/irs/Mesa");
  fs::copy_file(argv[3], m1 / "662522/models/Fender/Twin Lite.nam");
  fs::copy_file(argv[2], m1 / "662522/models/Marshall/JCM A2.nam");
  std::ofstream(m1 / "662522/models/zz broken.nam") << "{ this is not a model";
  fs::copy_file(argv[4], m1 / "662522/NAM/irs/Celestion long.wav");
  fs::copy_file(argv[5], m1 / "662522/NAM/irs/Mesa/4x12 short.wav");
  setenv("MPCNAM_MEDIA", m1.c_str(), 1);

  void* h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
  if (!h) { printf("FAIL dlopen: %s\n", dlerror()); return 1; }
  entry = (AEffect * (*)(hostcb)) dlsym(h, "VSTPluginMain");
  CHECK(entry != nullptr, "exports VSTPluginMain");
  AEffect* a = open_plugin();
  char name[64] = {0}, vendor[64] = {0};
  a->d(a, 45, 0, 0, name, 0); a->d(a, 47, 0, 0, vendor, 0);
  CHECK(a->magic == 0x56737450 && a->numParams == P_TOTAL && a->numInputs == 2 && a->numOutputs == 2 &&
        a->d(a, 35, 0, 0, nullptr, 0) == 1, "VST2 effect \"%s\" by \"%s\", %d parameters, stereo", name, vendor, (int)P_TOTAL);
  bool names = true;
  for (int i = 0; i < a->numParams; i++) { char n[64] = {0}; a->d(a, 8, i, 0, n, 0); if (!n[0]) names = false; }
  CHECK(names, "every parameter has a name");

  // scanning and the first model/IR
  CHECK(wait_for([&] { return idle(a) && disp(a, P_MODEL) != "-"; }), "the drives are scanned and a model is loaded (%s)", disp(a, P_MODEL_INFO).c_str());
  CHECK(disp(a, P_MODEL) == "Twin Lite" && disp(a, P_MODEL_INFO) == "Model 1 of 1 \xC2\xB7 Local" && disp(a, P_MODEL_PACK) == "Fender", "first model: [%s] [%s]", disp(a, P_MODEL).c_str(), disp(a, P_MODEL_INFO).c_str());
  CHECK(disp(a, P_IR) == "Celestion long" && disp(a, P_IR_INFO) == "IR 1 of 1 \xC2\xB7 Local" && disp(a, P_IR_PACK) == "Loose files", "first IR: [%s] [%s]", disp(a, P_IR).c_str(), disp(a, P_IR_INFO).c_str());
  // the blocks' pack line, detail line and source; pack arrows (packs: Fender, Marshall, loose files)
  CHECK(disp(a, P_MODEL_PACK) == "Fender" && disp(a, P_MODEL_INFO) == "Model 1 of 1 \xC2\xB7 Local" && a->getP(a, P_MODEL_SRC) == 0 &&
        disp(a, P_MODEL_SRC) == "Local", "AMP block: pack [%s], detail [%s], source %s", disp(a, P_MODEL_PACK).c_str(),
        disp(a, P_MODEL_INFO).c_str(), disp(a, P_MODEL_SRC).c_str());
  press(a, P_MODEL_PACK_NEXT); wait_for([&] { return idle(a); });
  CHECK(disp(a, P_MODEL) == "JCM A2" && disp(a, P_MODEL_PACK) == "Marshall", "NEXT PACK: [%s] in [%s]", disp(a, P_MODEL).c_str(), disp(a, P_MODEL_PACK).c_str());
  press(a, P_MODEL_PACK_NEXT); wait_for([&] { return idle(a); });
  CHECK(disp(a, P_MODEL) == "zz broken" && disp(a, P_MODEL_PACK) == "Loose files", "NEXT PACK again: loose files [%s] in [%s]", disp(a, P_MODEL).c_str(), disp(a, P_MODEL_PACK).c_str());
  press(a, P_MODEL_PACK_PREV); wait_for([&] { return idle(a); });
  CHECK(disp(a, P_MODEL) == "JCM A2", "PREV PACK: back to [%s]", disp(a, P_MODEL).c_str());
  press(a, P_MODEL_PACK_PREV); press(a, P_MODEL_PACK_PREV); wait_for([&] { return idle(a); });
  CHECK(disp(a, P_MODEL) == "zz broken", "PREV PACK wraps around from the first pack to the last: [%s]", disp(a, P_MODEL).c_str());
  press(a, P_MODEL_PACK_NEXT); wait_for([&] { return idle(a); });
  CHECK(disp(a, P_MODEL) == "Twin Lite" && a->getP(a, P_MODEL_PACK_NEXT) == 0, "NEXT PACK wraps around to the first: [%s]", disp(a, P_MODEL).c_str());
  bool fin;
  double full = render(a, 400, &fin);
  CHECK(fin && full > 1e-6, "audio through model + IR: finite, both channels equal (rms %.5f)", full);
  a->setP(a, P_IR_ON, 0.0f); double noir = render(a, 400); a->setP(a, P_IR_ON, 1.0f);
  CHECK(std::fabs(noir - full) > 1e-7, "Cab off changes the sound (rms %.5f vs %.5f)", noir, full);
  a->setP(a, P_IR_LEN, 1.0f); wait_for([&] { return idle(a); }); render(a, 100); double shortir = render(a, 400);
  CHECK(disp(a, P_IR_LEN) == "Short" && std::fabs(shortir - full) > 1e-9, "IR Length Short reloads the IR shorter (%s, rms %.5f)", disp(a, P_IR_LEN).c_str(), shortir);
  a->setP(a, P_IR_LEN, 0.0f); wait_for([&] { return idle(a); });

  // browsing
  press(a, P_MODEL_NEXT); wait_for([&] { return idle(a); });
  CHECK(disp(a, P_MODEL) == "JCM A2" && disp(a, P_MODEL_INFO) == "Model 1 of 1 \xC2\xB7 Local" && disp(a, P_MODEL_PACK) == "Marshall", "next: [%s] [%s]", disp(a, P_MODEL).c_str(), disp(a, P_MODEL_INFO).c_str());
  { // A2 FULL takes two taps: the first warns and stays off, the second (within a few seconds) turns it on
    const std::string pack0 = disp(a, P_MODEL_PACK), info0 = disp(a, P_MODEL_INFO), hdr0 = disp(a, P_T3K_INFO);
    a->setP(a, P_FULL, 1.0f);
    CHECK(a->getP(a, P_FULL) == 0.0f && disp(a, P_FULL) == "Off", "A2 FULL: the first tap does not turn it on");
    CHECK(disp(a, P_MODEL_PACK).find("CPU") != std::string::npos && disp(a, P_MODEL_INFO).find("G2") != std::string::npos &&
          disp(a, P_MODEL_INFO).find("risk") != std::string::npos && disp(a, P_T3K_INFO).find("again") != std::string::npos,
          "and warns: [%s] [%s] [%s]", disp(a, P_MODEL_PACK).c_str(), disp(a, P_MODEL_INFO).c_str(), disp(a, P_T3K_INFO).c_str());
    a->setP(a, P_FULL, 0.0f);                    // (turning it off is one tap and cancels the warning)
    CHECK(disp(a, P_MODEL_PACK) == pack0 && disp(a, P_MODEL_INFO) == info0 && disp(a, P_T3K_INFO) == hdr0,
          "tapping it off again clears the warning: [%s|%s] [%s|%s] [%s|%s]", pack0.c_str(), disp(a, P_MODEL_PACK).c_str(),
          info0.c_str(), disp(a, P_MODEL_INFO).c_str(), hdr0.c_str(), disp(a, P_T3K_INFO).c_str());
    a->setP(a, P_FULL, 1.0f);                    // first tap again, then let the warning run out
    std::this_thread::sleep_for(std::chrono::milliseconds(6500));
    wait_for([&] { return disp(a, P_MODEL_INFO) == info0; }, 3);
    CHECK(disp(a, P_MODEL_PACK) == pack0 && disp(a, P_MODEL_INFO) == info0 && a->getP(a, P_FULL) == 0.0f,
          "the warning runs out by itself and it stays off: [%s] [%s]", disp(a, P_MODEL_PACK).c_str(), disp(a, P_MODEL_INFO).c_str());
    a->setP(a, P_FULL, 1.0f);                    // a lone tap after that warns again (it is not left half-armed)
    CHECK(a->getP(a, P_FULL) == 0.0f, "a tap after the warning ran out warns again");
    a->setP(a, P_FULL, 1.0f);                    // the second tap
  }
  { // A2 FULL: the same A2 file at its Full size (reloaded in the background), and back
    wait_for([&] { return idle(a) && disp(a, P_MODEL_INFO).find("Full") != std::string::npos; });
    CHECK(disp(a, P_FULL) == "On" && disp(a, P_MODEL_INFO) == "Model 1 of 1 \xC2\xB7 Local \xC2\xB7 Full",
          "A2 FULL on: the capture reloads at its Full size: [%s]", disp(a, P_MODEL_INFO).c_str());
    bool ffin = false;
    render(a, 200, &ffin);
    CHECK(ffin, "and plays (output finite)");
    a->setP(a, P_FULL, 0.0f);
    wait_for([&] { return idle(a) && disp(a, P_MODEL_INFO).find("Full") == std::string::npos; });
    CHECK(disp(a, P_MODEL_INFO) == "Model 1 of 1 \xC2\xB7 Local", "A2 FULL off: back to Lite: [%s]", disp(a, P_MODEL_INFO).c_str());
  }
  press(a, P_MODEL_NEXT); wait_for([&] { return idle(a); });
  CHECK(disp(a, P_MODEL) == "zz broken" && disp(a, P_MODEL_INFO).find("Can't load") == 0, "a broken file says so: [%s]", disp(a, P_MODEL_INFO).c_str());
  render(a, 200, &fin);
  CHECK(fin, "audio keeps running without a model");
  press(a, P_MODEL_NEXT); wait_for([&] { return idle(a); });
  CHECK(disp(a, P_MODEL) == "Twin Lite", "next wraps around to the first (%s)", disp(a, P_MODEL).c_str());
  press(a, P_MODEL_PREV); wait_for([&] { return idle(a); });
  CHECK(disp(a, P_MODEL) == "zz broken", "prev wraps around to the last (%s)", disp(a, P_MODEL).c_str());
  a->setP(a, P_MODEL, 0.5f); wait_for([&] { return idle(a); });
  CHECK(disp(a, P_MODEL) == "JCM A2" && std::fabs(a->getP(a, P_MODEL) - 0.5f) < 1e-6, "a Q-Link value picks by position (0.5 -> %s)", disp(a, P_MODEL).c_str());
  press(a, P_IR_NEXT); wait_for([&] { return idle(a); });
  CHECK(disp(a, P_IR) == "4x12 short" && disp(a, P_IR_INFO) == "IR 1 of 1 \xC2\xB7 Local" && disp(a, P_IR_PACK) == "Mesa", "IR browsing: [%s] [%s]", disp(a, P_IR).c_str(), disp(a, P_IR_INFO).c_str());
  CHECK(a->getP(a, P_MODEL_NEXT) == 0 && automates > 0, "arrows report back to 0; the host is told about changes (%d)", (int)automates);

  { // the AMP LIST and CAB LIST laid over the blocks: pages of six rows, "pack \xC2\xB7 name", a tap on a row loads it
    auto row = [&](int first, int r) { return disp(a, first + r); };
    auto on = [&](int first, int r) { return a->getP(a, first + r) > 0.5f; };
    CHECK(a->getP(a, P_AMP_MENU) == 0 && a->getP(a, P_CAB_MENU) == 0 && disp(a, P_AMP_TITLE) == "" && row(P_AMP_ROW1, 0) == "" &&
          disp(a, P_AMP_MENU) == "Closed", "both lists start closed and empty");
    a->setP(a, P_AMP_ROW1, 1.0f); render(a, 2);
    CHECK(disp(a, P_MODEL) == "JCM A2" && a->getP(a, P_AMP_MENU) == 0, "a row tap does nothing while the list is closed");
    press(a, P_AMP_LIST);
    CHECK(a->getP(a, P_AMP_MENU) == 0.5f && a->getP(a, P_CAB_MENU) == 0 && disp(a, P_AMP_MENU) == "Open" && a->getP(a, P_AMP_LIST) == 0,
          "AMP LIST opens (the button reports back to 0), the CAB LIST stays closed");
    CHECK(disp(a, P_AMP_TITLE) == "Amps 1-3 of 3" && row(P_AMP_ROW1, 0) == "Fender \xC2\xB7 Twin Lite" && row(P_AMP_ROW1, 1) == "Marshall \xC2\xB7 JCM A2" &&
          row(P_AMP_ROW1, 2) == "zz broken" && row(P_AMP_ROW1, 3) == "",
          "the amps, 'pack \xC2\xB7 name': [%s] [%s] [%s] [%s]", disp(a, P_AMP_TITLE).c_str(), row(P_AMP_ROW1, 0).c_str(), row(P_AMP_ROW1, 1).c_str(), row(P_AMP_ROW1, 2).c_str());
    CHECK(!on(P_AMP_ROW1, 0) && on(P_AMP_ROW1, 1) && !on(P_AMP_ROW1, 2) && !on(P_AMP_ROW1, 3), "the amp in use is marked (row 2)");
    press(a, P_CAB_LIST);
    CHECK(a->getP(a, P_CAB_MENU) == 1 && a->getP(a, P_AMP_MENU) == 0.5f && disp(a, P_CAB_TITLE) == "Cabs 1-2 of 2" && row(P_CAB_ROW1, 0) == "Celestion long" &&
          row(P_CAB_ROW1, 1) == "Mesa \xC2\xB7 4x12 short" && on(P_CAB_ROW1, 1) && !on(P_CAB_ROW1, 0),
          "the CAB LIST opens beside it: [%s] [%s] [%s]", disp(a, P_CAB_TITLE).c_str(), row(P_CAB_ROW1, 0).c_str(), row(P_CAB_ROW1, 1).c_str());
    press(a, P_AMP_ROW1 + 0); wait_for([&] { return idle(a); });
    CHECK(disp(a, P_MODEL) == "Twin Lite" && a->getP(a, P_AMP_MENU) == 0.5f && on(P_AMP_ROW1, 0) && !on(P_AMP_ROW1, 1) && disp(a, P_MODEL_INFO) == "Model 1 of 1 \xC2\xB7 Local",
          "a row tap loads that amp and the list stays open: [%s] [%s]", disp(a, P_MODEL).c_str(), disp(a, P_MODEL_INFO).c_str());
    press(a, P_CAB_ROW1 + 0); wait_for([&] { return idle(a); });
    CHECK(disp(a, P_IR) == "Celestion long" && on(P_CAB_ROW1, 0) && a->getP(a, P_CAB_MENU) == 1 && disp(a, P_MODEL) == "Twin Lite",
          "and the same for a cab, the amp unchanged: [%s]", disp(a, P_IR).c_str());
    bool lfin = false;
    render(a, 300, &lfin);
    CHECK(lfin, "audio runs with both lists open");
    press(a, P_AMP_ROW1 + 3);
    CHECK(disp(a, P_MODEL) == "Twin Lite", "an empty row does nothing");
    press(a, P_AMP_NEXT); press(a, P_AMP_PREV);
    CHECK(disp(a, P_AMP_TITLE) == "Amps 1-3 of 3", "one page: NEXT and PREV wrap onto it");
    // the list is not saved with the project, nor offered to automation
    { void* ck = nullptr; intptr_t n = a->d(a, 23, 0, 0, &ck, 0); std::string t((const char*)ck, n);
      CHECK(t.find("amp_menu") == std::string::npos && t.find("cab_row") == std::string::npos && t.find("amp_title") == std::string::npos, "the lists are not part of the saved state");
      CHECK(a->d(a, 26, P_AMP_MENU, 0, nullptr, 0) == 0 && a->d(a, 26, P_CAB_ROW1, 0, nullptr, 0) == 0 && a->d(a, 26, P_INPUT, 0, nullptr, 0) == 1, "and not automatable"); }
    a->setP(a, P_AMP_MENU, 0.0f); a->setP(a, P_AMP_TITLE, 0.5f);
    CHECK(a->getP(a, P_AMP_MENU) == 0.5f, "the host can't set the open/closed state");
    press(a, P_AMP_CLOSE);
    CHECK(a->getP(a, P_AMP_MENU) == 0 && a->getP(a, P_CAB_MENU) == 1 && disp(a, P_AMP_TITLE) == "" && row(P_AMP_ROW1, 0) == "" && row(P_CAB_ROW1, 0) == "Celestion long",
          "CLOSE shuts the AMP LIST only");
    press(a, P_CAB_CLOSE);
    CHECK(a->getP(a, P_CAB_MENU) == 0 && disp(a, P_CAB_TITLE) == "", "and the CAB LIST");
    // back to where the following tests expect to be: JCM A2 and the Mesa IR
    press(a, P_AMP_LIST); press(a, P_AMP_ROW1 + 1); press(a, P_AMP_CLOSE); press(a, P_CAB_LIST); press(a, P_CAB_ROW1 + 1); press(a, P_CAB_CLOSE);
    wait_for([&] { return idle(a); });
    CHECK(disp(a, P_MODEL) == "JCM A2" && disp(a, P_IR) == "4x12 short", "as before: [%s] [%s]", disp(a, P_MODEL).c_str(), disp(a, P_IR).c_str());
  }

  // saving and restoring a project
  a->setP(a, P_INPUT, 0.75f); a->setP(a, P_BASS, 0.3f); a->setP(a, P_GATE_ON, 0.0f);
  void* chunk = nullptr;
  intptr_t len = a->d(a, 23, 0, 0, &chunk, 0);
  std::string saved((const char*)chunk, len);
  CHECK(len > 0 && saved.find("model_file=JCM A2.nam") != std::string::npos, "the project saves the model and IR by name (%d bytes)", (int)len);
  AEffect* b = open_plugin();
  CHECK(b->d(b, 24, 0, len, (void*)saved.c_str(), 0) == 1, "a new instance accepts it");
  wait_for([&] { return idle(b) && disp(b, P_MODEL) == "JCM A2"; });
  CHECK(disp(b, P_MODEL) == "JCM A2" && disp(b, P_IR) == "4x12 short", "and loads the same model and IR (%s, %s)", disp(b, P_MODEL).c_str(), disp(b, P_IR).c_str());
  CHECK(std::fabs(b->getP(b, P_INPUT) - 0.75f) < 1e-5 && std::fabs(b->getP(b, P_BASS) - 0.3f) < 1e-5 && b->getP(b, P_GATE_ON) == 0,
        "with the same settings (%s, %s, gate %s)", disp(b, P_INPUT).c_str(), disp(b, P_BASS).c_str(), disp(b, P_GATE_ON).c_str());
  CHECK(b->d(b, 24, 0, 6, (void*)"hello", 0) == 0, "a foreign chunk is refused");
  close_plugin(b);

  // the same project on a card whose folders differ, and with a file missing
  fs::create_directories(m2 / "sdb1/Stuff/Models/Amps"); fs::create_directories(m2 / "sdb1/IRs");
  fs::copy_file(argv[2], m2 / "sdb1/Stuff/Models/Amps/JCM A2.nam");
  setenv("MPCNAM_MEDIA", m2.c_str(), 1);
  AEffect* c = open_plugin();
  c->d(c, 24, 0, len, (void*)saved.c_str(), 0);
  wait_for([&] { return idle(c) && disp(c, P_MODEL) == "JCM A2"; });
  CHECK(disp(c, P_MODEL) == "JCM A2" && disp(c, P_MODEL_INFO) == "Model 1 of 1 \xC2\xB7 Local" && disp(c, P_MODEL_PACK) == "Amps", "moved to another card and folder: found by name ([%s] [%s])", disp(c, P_MODEL).c_str(), disp(c, P_MODEL_INFO).c_str());
  CHECK(disp(c, P_IR_INFO) == "No .wav IRs found" || disp(c, P_IR_INFO).find("Not found") == 0, "a missing IR is reported: [%s]", disp(c, P_IR_INFO).c_str());
  render(c, 100, &fin);
  CHECK(fin, "audio runs without its IR");
  // rescan picks up new files
  fs::copy_file(argv[5], m2 / "sdb1/IRs/4x12 short.wav");
  press(c, P_RESCAN);
  wait_for([&] { return idle(c) && disp(c, P_IR) == "4x12 short"; });
  CHECK(disp(c, P_IR) == "4x12 short" && disp(c, P_IR_INFO) == "IR 1 of 1 \xC2\xB7 Local", "Rescan finds the IR once it's there, and it's the project's ([%s] [%s])", disp(c, P_IR).c_str(), disp(c, P_IR_INFO).c_str());
  // a stereo IR: the official loader refuses it; engine/wav_reader mixes it to mono
  write_stereo_wav(m2 / "sdb1/IRs/zz stereo.wav");
  press(c, P_RESCAN);
  wait_for([&] { return idle(c) && disp(c, P_IR_INFO).find("of 2") != std::string::npos; });
  press(c, P_IR_NEXT);
  wait_for([&] { return idle(c) && disp(c, P_IR) == "zz stereo"; });
  CHECK(disp(c, P_IR) == "zz stereo" && disp(c, P_IR_INFO) == "IR 2 of 2 \xC2\xB7 Local", "a stereo IR loads, mixed to mono: [%s] [%s]", disp(c, P_IR).c_str(), disp(c, P_IR_INFO).c_str());
  render(c, 100, &fin);
  CHECK(fin, "the stereo IR plays");
  close_plugin(c);

  { // many files: the lists page (six a page); and the quick start (the folders found last time) vs a folder it doesn't know
    fs::path m3 = fs::temp_directory_path() / ("mpcnam_card3_" + tag);
    fs::remove_all(m3, ec);
    for (int k = 1; k <= 14; k++) {
      char nm[64]; snprintf(nm, sizeof nm, "Bulk/Amp %02d.nam", k);
      fs::create_directories(m3 / "cardA/models/Bulk"); fs::copy_file(argv[2], m3 / "cardA/models" / nm);
    }
    for (int k = 1; k <= 7; k++) {
      char nm[64]; snprintf(nm, sizeof nm, "Cab %d.wav", k);
      fs::create_directories(m3 / "cardA/irs"); fs::copy_file(argv[5], m3 / "cardA/irs" / nm);
    }
    setenv("MPCNAM_MEDIA", m3.c_str(), 1);
    AEffect* g = open_plugin();
    wait_for([&] { return idle(g) && disp(g, P_MODEL) != "-"; });
    press(g, P_AMP_LIST); press(g, P_CAB_LIST);
    CHECK(disp(g, P_AMP_TITLE) == "Amps 1-4 of 14" && disp(g, P_AMP_ROW1) == "Bulk \xC2\xB7 Amp 01" && disp(g, P_AMP_ROW1 + 3) == "Bulk \xC2\xB7 Amp 04" &&
          disp(g, P_AMP_ROW1 + 4) == "" && disp(g, P_AMP_ROW6) == "" && disp(g, P_CAB_TITLE) == "Cabs 1-4 of 7" && disp(g, P_CAB_ROW1 + 3) == "Cab 4",
          "four to a page (44 px rows; rows 5-6 unused): [%s] [%s] / [%s]", disp(g, P_AMP_TITLE).c_str(), disp(g, P_AMP_ROW1 + 3).c_str(), disp(g, P_CAB_TITLE).c_str());
    press(g, P_AMP_NEXT); press(g, P_AMP_NEXT); press(g, P_AMP_NEXT);
    CHECK(disp(g, P_AMP_TITLE) == "Amps 13-14 of 14" && disp(g, P_AMP_ROW1) == "Bulk \xC2\xB7 Amp 13" && disp(g, P_AMP_ROW1 + 2) == "" && disp(g, P_CAB_TITLE) == "Cabs 1-4 of 7",
          "NEXT three times: the last, short page, the cab list on its own page: [%s]", disp(g, P_AMP_TITLE).c_str());
    press(g, P_AMP_NEXT);
    CHECK(disp(g, P_AMP_TITLE) == "Amps 1-4 of 14", "NEXT wraps to the first page");
    press(g, P_AMP_PREV);
    CHECK(disp(g, P_AMP_TITLE) == "Amps 13-14 of 14", "PREV wraps to the last page");
    press(g, P_AMP_ROW1 + 1); wait_for([&] { return idle(g); });
    CHECK(disp(g, P_MODEL) == "Amp 14" && g->getP(g, P_AMP_ROW1 + 1) > 0.5f && g->getP(g, P_AMP_ROW1) < 0.5f, "a row on the last page loads it: [%s]", disp(g, P_MODEL).c_str());
    press(g, P_AMP_CLOSE); press(g, P_AMP_LIST);
    CHECK(disp(g, P_AMP_TITLE) == "Amps 13-14 of 14", "reopened on the page that holds the amp in use");
    press(g, P_CAB_NEXT);
    press(g, P_CAB_ROW1 + 2); wait_for([&] { return idle(g); });
    CHECK(disp(g, P_IR) == "Cab 7" && disp(g, P_CAB_TITLE) == "Cabs 5-7 of 7", "and the cab list pages by itself: [%s]", disp(g, P_IR).c_str());
    // a long name slides until its end shows, a short one never moves
    {
      fs::create_directories(m3 / "cardA/models/Pack With A Fairly Long Folder Name");
      fs::copy_file(argv[2], m3 / "cardA/models/Pack With A Fairly Long Folder Name/Bogner Ecstasy 101B Red Channel Crunch Mod.nam");
      press(g, P_RESCAN);
      wait_for([&] { return idle(g) && disp(g, P_AMP_TITLE).find("of 15") != std::string::npos; });
      auto long_row = [&]() { for (int r = 0; r < 6; r++) if (disp(g, P_AMP_ROW1 + r).find("Pack With") == 0) return r; return -1; };
      press(g, P_AMP_CLOSE); press(g, P_AMP_LIST);
      int lr = long_row();
      if (lr < 0) { press(g, P_AMP_NEXT); lr = long_row(); }
      if (lr < 0) { press(g, P_AMP_NEXT); lr = long_row(); }
      const std::string whole = lr >= 0 ? disp(g, P_AMP_ROW1 + lr) : "";
      CHECK(lr >= 0 && whole.find("Pack With") == 0, "a long name starts whole: [%s]", whole.c_str());
      std::string now = whole;
      wait_for([&] { now = lr >= 0 ? disp(g, P_AMP_ROW1 + lr) : whole; return now != whole; }, 6);
      CHECK(now != whole && now.find("Pack With") != 0, "then it slides along: [%s]", now.c_str());
      std::string a1, a2;
      wait_for([&] { std::string c1 = disp(g, P_AMP_ROW1 + lr); std::this_thread::sleep_for(std::chrono::milliseconds(700)); std::string c2 = disp(g, P_AMP_ROW1 + lr); a1 = c1; a2 = c2; return c1 == c2; }, 20);
      CHECK(a1 == a2 && a2.find("Mod") != std::string::npos && a2.find("...") == std::string::npos, "and stops with its end showing: [%s]", a2.c_str());
    }
    press(g, P_AMP_CLOSE); press(g, P_CAB_CLOSE);
    // the quick start: a project that names a file in a folder the index doesn't know yet
    fs::create_directories(m3 / "cardZ/models");
    fs::copy_file(argv[2], m3 / "cardZ/models/Late Arrival.nam");
    const std::string late = "nam-a2-lite=1\nmodel_path=" + (m3 / "cardZ/models/Late Arrival.nam").string() + "\nmodel_display=Late Arrival\nmodel_file=Late Arrival.nam\n";
    AEffect* q = open_plugin();
    q->d(q, 24, 0, (intptr_t)late.size() + 1, (void*)late.c_str(), 0);
    wait_for([&] { return idle(q) && disp(q, P_MODEL) == "Late Arrival"; }, 30);
    CHECK(disp(q, P_MODEL) == "Late Arrival" && disp(q, P_MODEL_INFO) == "Model 1 of 1 \xC2\xB7 Local",
          "a saved amp in a folder the quick start doesn't know: waits for the walk, then loads: [%s] [%s]", disp(q, P_MODEL).c_str(), disp(q, P_MODEL_INFO).c_str());
    // and one in a known folder, with the index there: it sounds and is listed
    const std::string known = "nam-a2-lite=1\nmodel_path=" + (m3 / "cardA/models/Bulk/Amp 03.nam").string() + "\nmodel_display=Bulk/Amp 03\nmodel_file=Amp 03.nam\n";
    AEffect* q2 = open_plugin();
    q2->d(q2, 24, 0, (intptr_t)known.size() + 1, (void*)known.c_str(), 0);
    wait_for([&] { return idle(q2) && disp(q2, P_MODEL) == "Amp 03"; }, 30);
    bool qfin = false;
    render(q2, 200, &qfin);
    CHECK(disp(q2, P_MODEL) == "Amp 03" && disp(q2, P_MODEL_PACK) == "Bulk" && qfin, "a saved amp in a known folder loads: [%s] [%s]", disp(q2, P_MODEL).c_str(), disp(q2, P_MODEL_INFO).c_str());
    close_plugin(q2); close_plugin(q); close_plugin(g);
    fs::remove_all(m3, ec);
  }

  // TONE3000 (only when tools/t3k_mock.py runs: MPCNAM_T3K_API set by tools/test.sh): GET TONES, the phone walks the
  // Select flow with curl, and the plugin downloads, rescans and selects the new capture, then an IR
  if (getenv("MPCNAM_T3K_API")) {
    setenv("MPCNAM_T3K_DEST", (m1 / "662522").c_str(), 1);   // downloads land on the card the plugin scans
    setenv("MPCNAM_MEDIA", m1.c_str(), 1);
    AEffect* t = open_plugin();
    wait_for([&] { return idle(t); });
    CHECK(disp(t, P_T3K_INFO) == "Tap BROWSE TONE3000", "TONE3000 line before use: [%s]", disp(t, P_T3K_INFO).c_str());
    press(t, P_GET_TONES);
    wait_for([&] { return disp(t, P_T3K_INFO).rfind("Open ", 0) == 0; }, 5);   // the worker opens the page
    const std::string open_line = disp(t, P_T3K_INFO);
    CHECK(open_line.rfind("Open 127.0.0.1:", 0) == 0 && t->getP(t, P_GET_TONES) == 0, "GET TONES opens the phone page: [%s]", open_line.c_str());
    const std::string self = "http://" + open_line.substr(5);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    if (system(("curl -sL -o /dev/null " + self + "/go?kind=model").c_str()) != 0) printf("   (curl failed)\n");
    wait_for([&] { return idle(t) && disp(t, P_MODEL) == "Clean"; });
    CHECK(disp(t, P_MODEL) == "Clean" && disp(t, P_MODEL_PACK) == "Fender Twin: Clean/Edge" &&
          disp(t, P_MODEL_INFO) == "Amp \xC2\xB7 NAM \xC2\xB7 @alice" && t->getP(t, P_MODEL_SRC) == 1,
          "the downloaded capture is selected, with TONE3000's attribution: [%s] [%s] [%s] (T3K mark %s)", disp(t, P_MODEL).c_str(),
          disp(t, P_MODEL_PACK).c_str(), disp(t, P_MODEL_INFO).c_str(), t->getP(t, P_MODEL_SRC) == 1 ? "on" : "off");
    render(t, 100, &fin);
    CHECK(fin, "and plays");
    if (system(("curl -sL -o /dev/null " + self + "/go?kind=ir").c_str()) != 0) printf("   (curl failed)\n");
    wait_for([&] { return idle(t) && disp(t, P_IR) == "SM57 cap edge"; });
    CHECK(disp(t, P_IR) == "SM57 cap edge" && disp(t, P_MODEL) == "Clean", "then an IR, the capture stays: [%s] [%s]", disp(t, P_IR).c_str(), disp(t, P_MODEL).c_str());
    CHECK(disp(t, P_IR_PACK) == "4x12 V30 Pack" && disp(t, P_IR_INFO) == "Cab \xC2\xB7 IR \xC2\xB7 @bob" && t->getP(t, P_IR_SRC) == 1,
          "the IR block: [%s] [%s]", disp(t, P_IR_PACK).c_str(), disp(t, P_IR_INFO).c_str());
    // a block's Browse TONE3000 button: the page's Continue goes to that block's catalogue
    press(t, P_IR_BROWSE);
    std::string pagefile = (m1 / "page.html").string();
    std::string html;
    wait_for([&] {                                 // the worker passes the request on: the page's Continue follows
      if (system(("curl -s -o '" + pagefile + "' " + self + "/").c_str()) != 0) return false;
      std::ifstream f(pagefile); html.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
      return html.find("Continue: cab IRs") != std::string::npos;
    }, 5);
    CHECK(html.find("Continue: cab IRs") != std::string::npos && html.find("has partnered with TONE3000") != std::string::npos &&
          t->getP(t, P_IR_BROWSE) == 0, "CAB's BROWSE TONE3000: the partnership splash, Continue to cab IRs");
    auto t0 = std::chrono::steady_clock::now();
    close_plugin(t);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    CHECK(ms < 1500, "closes with TONE3000 open (%.0f ms)", ms);
    unsetenv("MPCNAM_T3K_DEST");
  }

  // no libcurl on the system (MPCNAM_EXPECT_NO_CURL, e.g. under qemu-arm without armhf libcurl): GET TONES says so,
  // and the plugin carries on
  if (getenv("MPCNAM_EXPECT_NO_CURL")) {
    press(a, 20);
    wait_for([&] { return disp(a, P_T3K_INFO) == "Needs libcurl"; }, 5);   // the worker tries to open the page
    CHECK(disp(a, P_T3K_INFO) == "Needs libcurl", "without libcurl, GET TONES says so: [%s]", disp(a, P_T3K_INFO).c_str());
    render(a, 100, &fin);
    CHECK(fin, "and audio carries on");
  }

  // DELETE: one tap asks, a second tap within 4 s deletes the file permanently
  {
    setenv("MPCNAM_MEDIA", m1.c_str(), 1);
    AEffect* x = open_plugin();
    wait_for([&] { return idle(x); });
    for (int k = 0; k < 8 && disp(x, P_MODEL) != "zz broken"; k++) { press(x, P_MODEL_NEXT); wait_for([&] { return idle(x); }); }
    x->setP(x, P_MODEL_DELETE, 1.0f);
    CHECK(disp(x, P_MODEL) == "zz broken" && disp(x, P_MODEL_INFO) == "Tap DELETE again to delete it permanently",
          "one tap on DELETE asks: [%s] [%s]", disp(x, P_MODEL).c_str(), disp(x, P_MODEL_INFO).c_str());
    std::this_thread::sleep_for(std::chrono::milliseconds(4500));
    CHECK(disp(x, P_MODEL_INFO) != "Tap DELETE again to delete it permanently" && fs::exists(m1 / "662522/models/zz broken.nam"),
          "after 4 s it's forgotten, the file untouched: [%s]", disp(x, P_MODEL_INFO).c_str());
    x->setP(x, P_MODEL_DELETE, 1.0f);
    x->setP(x, P_MODEL_DELETE, 1.0f);
    wait_for([&] { return disp(x, P_MODEL_INFO) == "Deleted"; }, 5);
    CHECK(!fs::exists(m1 / "662522/models/zz broken.nam") && !fs::exists(m1 / "662522/Deleted"),
          "two taps: the file is deleted permanently (info [%s])", disp(x, P_MODEL_INFO).c_str());
    wait_for([&] { return idle(x) && disp(x, P_MODEL) != "zz broken"; }, 10);
    CHECK(disp(x, P_MODEL) != "zz broken" && disp(x, P_MODEL) != "-", "and the list moves on to [%s]", disp(x, P_MODEL).c_str());
    press(x, P_RESCAN); wait_for([&] { return idle(x); });
    bool gone = true;
    for (int k = 0; k < 8; k++) { if (disp(x, P_MODEL) == "zz broken") gone = false; press(x, P_MODEL_NEXT); wait_for([&] { return idle(x); }); }
    CHECK(gone, "a rescan doesn't bring it back");
    x->d(x, 1, 0, 0, nullptr, 0);
  }

  // the phone page's port: two instances each get one (the next port when it's taken); a page that closed itself
  // (idle) opens again
  if (getenv("MPCNAM_T3K_API")) {
    const int base = atoi(getenv("MPCNAM_T3K_PORT"));
    auto fetch = [&](int port) {
      std::string f = (fs::temp_directory_path() / ("port_page_" + std::to_string(port) + "_" + tag)).string();
      std::remove(f.c_str());
      if (system(("curl -s -m 5 -o '" + f + "' http://127.0.0.1:" + std::to_string(port) + "/").c_str()) != 0) return std::string();
      std::ifstream in(f); std::string h((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
      std::remove(f.c_str());
      return h;
    };
    AEffect* p1 = open_plugin(); AEffect* p2 = open_plugin();
    wait_for([&] { return idle(p1) && idle(p2); });
    press(p1, P_GET_TONES); press(p2, P_GET_TONES);
    wait_for([&] { return disp(p1, P_T3K_INFO).find("Open ") == 0 && disp(p2, P_T3K_INFO).find("Open ") == 0; }, 5);
    const std::string s1 = disp(p1, P_T3K_INFO), s2 = disp(p2, P_T3K_INFO);
    const std::string a1 = "Open 127.0.0.1:" + std::to_string(base), a2 = "Open 127.0.0.1:" + std::to_string(base + 1);
    CHECK((s1 == a1 && s2 == a2) || (s1 == a2 && s2 == a1),            // each worker opens its own: either may be first
          "two instances, two pages: [%s] [%s]", s1.c_str(), s2.c_str());
    CHECK(fetch(base).find("has partnered with TONE3000") != std::string::npos && fetch(base + 1).find("has partnered with TONE3000") != std::string::npos,
          "both pages answer");
    p2->d(p2, 1, 0, 0, nullptr, 0); p1->d(p1, 1, 0, 0, nullptr, 0);
    setenv("MPCNAM_T3K_IDLE", "1", 1);
    AEffect* p3 = open_plugin();
    wait_for([&] { return idle(p3); });
    press(p3, P_GET_TONES);
    wait_for([&] { return disp(p3, P_T3K_INFO) == "Page closed"; }, 6);
    CHECK(disp(p3, P_T3K_INFO) == "Page closed" && fetch(base).empty(), "an idle page closes itself: [%s]", disp(p3, P_T3K_INFO).c_str());
    press(p3, P_GET_TONES);
    wait_for([&] { return disp(p3, P_T3K_INFO).find("Open ") == 0; }, 5);
    CHECK(disp(p3, P_T3K_INFO).find("Open ") == 0 && fetch(base).find("has partnered with TONE3000") != std::string::npos,
          "and opens again with the next press: [%s]", disp(p3, P_T3K_INFO).c_str());
    p3->d(p3, 1, 0, 0, nullptr, 0);
    unsetenv("MPCNAM_T3K_IDLE");
  }

  // the pluck print tells a clean amp from a saturated one: a linear (clean) capture keeps the pluck's decay
  {
    fs::path m4 = fs::temp_directory_path() / ("mpcnam_card4_" + tag);
    fs::create_directories(m4 / "c/models/Clean");
    std::ofstream(m4 / "c/models/Clean/Linear.nam") << "{\"version\":\"0.5.0\",\"architecture\":\"Linear\",\"config\":"
        "{\"receptive_field\":4,\"bias\":false},\"weights\":[1,0,0,0],\"sample_rate\":48000}";
    setenv("MPCNAM_MEDIA", m4.c_str(), 1);
    AEffect* q = open_plugin();
    wait_for([&] { return idle(q) && disp(q, P_MODEL) == "Linear" && q->getP(q, P_WAVE1) > 0; });
    int st[14]; std::string pr; bool clean = true;
    for (int c = 0; c < 14; c++) { st[c] = (int)std::lround(q->getP(q, P_WAVE1 + c) * 24); pr += disp(q, P_WAVE1 + c) + " "; if (st[c] > 8) clean = false; }
    CHECK(clean && st[0] >= 7 && st[13] >= 1 && st[13] <= st[0] - 3, "a clean capture draws a clean, decaying pluck: [%s]", pr.c_str());
    close_plugin(q);
    fs::remove_all(m4, ec);
  }

  // My Presets: SAVE keeps the sound (named after its amp), a row tap brings it back, DELETE asks twice; a project
  // remembers the selected one; the phone pages edit a preset and the plugin applies it
  {
    fs::path pd = fs::temp_directory_path() / ("mpcnam_presets_" + tag);
    fs::remove_all(pd, ec);
    setenv("MPCNAM_PRESETS", pd.c_str(), 1);
    setenv("MPCNAM_MEDIA", m1.c_str(), 1);
    AEffect* q = open_plugin();
    wait_for([&] { return idle(q) && disp(q, P_MODEL) != "-"; });
    auto title = [&] { return disp(q, P_AMP_TITLE); };
    CHECK(disp(q, P_PRESET) == "No preset", "no preset at first: [%s]", disp(q, P_PRESET).c_str());
    press(q, P_MP_LIST);
    wait_for([&] { return title() == "No presets yet"; }, 5);
    CHECK(q->getP(q, P_AMP_MENU) == 1.0f && disp(q, P_AMP_MENU) == "Presets" && title() == "No presets yet",
          "MY PRESETS opens over the AMP block: [%s]", title().c_str());
    const std::string amp = disp(q, P_MODEL);
    q->setP(q, P_BASS, 0.8f);                     // bass 8
    press(q, P_MP_SAVE);
    wait_for([&] { return disp(q, P_AMP_ROW1) != ""; }, 5);
    CHECK(fs::exists(pd / (amp + ".na2p")) && disp(q, P_AMP_ROW1) == amp && q->getP(q, P_AMP_ROW1) == 1 && disp(q, P_PRESET) == "My: " + amp,
          "SAVE keeps the sound as \"%s\", selected and named in the header: [%s] [%s]", amp.c_str(), disp(q, P_AMP_ROW1).c_str(), disp(q, P_PRESET).c_str());
    press(q, P_MP_SAVE);
    wait_for([&] { return disp(q, P_AMP_ROW1 + 1) != ""; }, 5);
    CHECK(disp(q, P_AMP_ROW1 + 1) == amp + " 2" && title().find("Presets 1-2 of 2") != std::string::npos || title() == "Saved",
          "a second SAVE is numbered: [%s] [%s]", disp(q, P_AMP_ROW1 + 1).c_str(), title().c_str());
    q->setP(q, P_BASS, 0.2f);
    q->setP(q, P_MODEL_NEXT, 1.0f); render(q, 2);
    wait_for([&] { return idle(q); });
    const std::string other = disp(q, P_MODEL);
    press(q, P_AMP_ROW1);
    wait_for([&] { return idle(q) && disp(q, P_MODEL) == amp; });
    CHECK(std::fabs(q->getP(q, P_BASS) - 0.8f) < 1e-4f && disp(q, P_MODEL) == amp && other != amp && q->getP(q, P_AMP_MENU) == 1.0f,
          "a row tap brings back the settings and the amp (from %s), the list stays open", other.c_str());
    { void* ck = nullptr; intptr_t n = q->d(q, 23, 0, 0, &ck, 0); std::string c((const char*)ck, n);
      CHECK(c.find("mp_name=" + amp + "\n") != std::string::npos && c.find("mp_save") == std::string::npos && c.find("preset=") == std::string::npos,
            "a project remembers the selected preset, not the buttons");
      AEffect* q2 = open_plugin();
      q2->d(q2, 24, 0, n, (void*)c.data(), 0);
      CHECK(disp(q2, P_PRESET) == "My: " + amp, "...and shows it again when opened: [%s]", disp(q2, P_PRESET).c_str());
      close_plugin(q2); }
    CHECK(q->d(q, 26, P_MP_SAVE, 0, nullptr, 0) == 0 && q->d(q, 26, P_PRESET, 0, nullptr, 0) == 0, "the preset controls are not automatable");
    press(q, P_MP_DELETE);
    CHECK(title() == "Tap DELETE again" && fs::exists(pd / (amp + ".na2p")), "DELETE asks first: [%s]", title().c_str());
    press(q, P_MP_DELETE);
    wait_for([&] { return !fs::exists(pd / (amp + ".na2p")); }, 5);
    wait_for([&] { return disp(q, P_PRESET) == "No preset"; }, 5);
    CHECK(!fs::exists(pd / (amp + ".na2p")) && disp(q, P_AMP_ROW1) == amp + " 2" && disp(q, P_PRESET) == "No preset",
          "the second tap deletes it: [%s]", disp(q, P_AMP_ROW1).c_str());
    // the phone pages: NAME ON PHONE opens them (no TONE3000 sign-in needed); an edit there is applied by the plugin
    press(q, P_MP_PHONE);
    wait_for([&] { return disp(q, P_T3K_INFO).find("18094") != std::string::npos; }, 5);
    CHECK(disp(q, P_T3K_INFO).find("18094") != std::string::npos, "NAME ON PHONE opens the phone pages: [%s]", disp(q, P_T3K_INFO).c_str());
    auto http = [&](const std::string& req) {
      int sk = socket(AF_INET, SOCK_STREAM, 0);
      sockaddr_in sa{}; sa.sin_family = AF_INET; sa.sin_port = htons(18094); inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
      std::string r;
      if (connect(sk, (sockaddr*)&sa, sizeof sa) == 0) {
        send(sk, req.data(), req.size(), 0);
        char b[8192]; ssize_t n;
        while ((n = recv(sk, b, sizeof b, 0)) > 0) r.append(b, (size_t)n);
      }
      close(sk);
      return r;
    };
    const std::string pname = amp + " 2";
    std::string page = http("GET /presets/edit?name=" + std::string(pname == "Twin Lite 2" ? "Twin%20Lite%202" : "JCM%20A2%202") + " HTTP/1.1\r\nHost: x\r\n\r\n");
    CHECK(page.find("200 OK") != std::string::npos && page.find("name=\"bass\"") != std::string::npos && page.find("Celestion long") != std::string::npos,
          "the edit page lists the plugin's settings and its cabs");
    const std::string body = "name=" + std::string(pname == "Twin Lite 2" ? "Twin+Lite+2" : "JCM+A2+2") + "&bass=3&full=1&ir=none&apply=1";
    page = http("POST /presets/update HTTP/1.1\r\nHost: x\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: " +
                std::to_string(body.size()) + "\r\n\r\n" + body);
    wait_for([&] { return std::fabs(q->getP(q, P_BASS) - 0.3f) < 1e-4f; }, 5);
    CHECK(page.find("303") != std::string::npos && std::fabs(q->getP(q, P_BASS) - 0.3f) < 1e-4f && disp(q, P_PRESET) == "My: " + pname,
          "Save and load from the phone: the plugin plays the edited preset: [%s]", disp(q, P_PRESET).c_str());
    CHECK(q->getP(q, P_FULL) == 1.0f && disp(q, P_MODEL_INFO).find("risk") == std::string::npos && disp(q, P_T3K_INFO).find("again") == std::string::npos,
          "a preset saved with A2 FULL on loads it as saved (the two-tap warning is for taps only)");
    press(q, P_AMP_CLOSE);
    CHECK(q->getP(q, P_AMP_MENU) == 0, "CLOSE closes My Presets");
    close_plugin(q);
    unsetenv("MPCNAM_PRESETS");
    fs::remove_all(pd, ec);
  }

  // browsing while audio runs on another thread
  setenv("MPCNAM_MEDIA", m1.c_str(), 1);
  AEffect* d = open_plugin();
  wait_for([&] { return idle(d); });
  // the tubes: a read-only display of the output level (0 idle .. 24), reported to the host, not saved, not automatable
  {
    AEffect* t = d;
    {                                                                // the amp's pluck print, measured at load
      std::string pr; int lit = 0, peak = 0;
      for (int c = 0; c < 14; c++) { const int v = (int)std::lround(t->getP(t, P_WAVE1 + c) * 24); pr += disp(t, P_WAVE1 + c) + " "; lit += v > 0; peak = std::max(peak, v); }
      CHECK(lit >= 3 && peak >= 1 && peak <= 24, "the amp's pluck print is drawn at load: [%s]", pr.c_str());
      CHECK(t->d(t, 26, P_WAVE1, 0, nullptr, 0) == 0, "and not automatable");
      t->setP(t, P_WAVE1, 0.0f);
      CHECK(t->getP(t, P_WAVE1) * 24 == std::lround(t->getP(t, P_WAVE1) * 24) && disp(t, P_WAVE1) == pr.substr(0, pr.find(' ')), "the host cannot change it");
      void* ck = nullptr; intptr_t n = t->d(t, 23, 0, 0, &ck, 0); std::string c((const char*)ck, n);
      CHECK(c.find("amp_wave") == std::string::npos, "and it is not saved");
    }
    int before = automates;
    CHECK(t->getP(t, P_TUBES) == 0 && disp(t, P_TUBES) == "Off", "the tubes start idle (%s)", disp(t, P_TUBES).c_str());
    t->setP(t, P_INPUT, 1.0f); t->setP(t, P_OUTPUT, 1.0f);           // the test model is quiet: +20 dB in, +40 dB out
    const double rms = render(t, 600);                               // a steady tone: ~1.7 s
    const float lit = t->getP(t, P_TUBES);
    CHECK(lit > 0.2f && disp(t, P_TUBES) != "Off" && automates > before, "a signal lights them (%s, output rms %.3f) and the host is told", disp(t, P_TUBES).c_str(), rms);
    CHECK(t->d(t, 26, P_TUBES, 0, nullptr, 0) == 0, "not automatable");
    t->setP(t, P_TUBES, 0.0f);
    CHECK(t->getP(t, P_TUBES) == lit, "the host cannot move them");
    { void* ck = nullptr; intptr_t n = t->d(t, 23, 0, 0, &ck, 0); std::string c((const char*)ck, n);
      CHECK(c.find("tubes") == std::string::npos, "and they are not saved"); }
    std::vector<float> z(128, 0.0f), o1(128), o2(128);
    float* zi[2] = {z.data(), z.data()}; float* zo[2] = {o1.data(), o2.data()};
    float prev = lit; bool mono = true; int blocks = 0;
    for (; blocks < 1500 && t->getP(t, P_TUBES) > 0; blocks++) {      // silence: they cool, never brighten, and go dark
      t->pr(t, zi, zo, 128);
      float cur = t->getP(t, P_TUBES);
      if (cur > prev + 1e-6f) mono = false;
      prev = cur;
    }
    CHECK(mono && t->getP(t, P_TUBES) == 0 && blocks > 60, "in silence they cool and go dark (%d blocks, %.2f s)", blocks, blocks * 128 / 44100.0);
    t->setP(t, P_INPUT, 0.5f); t->setP(t, P_OUTPUT, 0.5f);
  }

  std::atomic<bool> run{true}, sane{true};
  std::atomic<long> blocks{0};
  std::thread audio([&] {
    long ph = 0;
    std::vector<float> l(128), ol(128), orr(128);
    while (run) {
      for (int i = 0; i < 128; i++, ph++) l[i] = (float)(0.3 * std::sin(2 * M_PI * 110 * ph / 44100.0));
      float* in[2] = {l.data(), l.data()};
      float* out[2] = {ol.data(), orr.data()};
      d->pr(d, in, out, 128);
      for (int i = 0; i < 128; i++) if (!std::isfinite(ol[i])) sane = false;
      blocks++;
    }
  });
  for (int k = 0; k < 30; k++) {
    d->setP(d, k % 2 ? P_MODEL_NEXT : P_IR_NEXT, 1.0f);
    d->setP(d, P_BASS, (k % 10) / 10.0f);
    if (k % 7 == 0) d->setP(d, P_RESCAN, 1.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
  }
  wait_for([&] { return idle(d); });
  run = false; audio.join();
  CHECK(sane, "30 browses, rescans and knob moves during %ld audio blocks on another thread: output always finite", (long)blocks);
  auto t0 = std::chrono::steady_clock::now();
  close_plugin(d);
  close_plugin(a);
  double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  CHECK(ms < 1000, "instances close promptly (%.0f ms for two)", ms);
  fs::remove_all(m1, ec); fs::remove_all(m2, ec);
  printf("%s (%d failure%s)\n", fails ? "FAILED" : "PASSED", fails, fails == 1 ? "" : "s");
  return fails ? 1 : 0;
}
