// SPDX-License-Identifier: MIT
// Copyright (c) 2026 the mpc-vst-liminal-amp contributors
// t3k_test.cpp -- engine/tone3000 end to end against tools/t3k_mock.py, a strict stand-in for TONE3000 (client id,
// PKCE S256 recomputed from the verifier, one-time codes, redirect_uri match, Bearer auth on everything). The
// "phone" is curl following redirects: plugin page -> TONE3000 authorize -> back to the plugin's /cb.
//   t3k_test <mock port> <plugin port> <dest dir> <a2 .nam> <ir .wav>     (the mock serves copies of the last two)
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <mutex>
#include <thread>
#include "tone3000.h"
static int fails = 0;
#define CHECK(c, ...) do { printf("%s ", (c) ? "ok  " : "FAIL"); printf(__VA_ARGS__); printf("\n"); if (!(c)) fails++; } while (0)
static std::string sh(const std::string& cmd) {           // run, return stdout
  std::string out; char buf[4096];
  FILE* p = popen(cmd.c_str(), "r");
  if (!p) return out;
  size_t n; while ((n = fread(buf, 1, sizeof buf, p)) > 0) out.append(buf, n);
  pclose(p);
  return out;
}
static std::string slurp(const std::string& path) { std::ifstream f(path, std::ios::binary); std::stringstream s; s << f.rdbuf(); return s.str(); }
static bool has(const std::string& s, const std::string& x) { return s.find(x) != std::string::npos; }

int main(int argc, char** argv) {
  if (argc < 6) { printf("usage: t3k_test <mock port> <plugin port> <dest> <a2.nam> <ir.wav>\n"); return 2; }
  const std::string mock = argv[1], port = argv[2], dest = argv[3];
  setenv("MPCNAM_T3K_API", ("http://127.0.0.1:" + mock + "/api/v1").c_str(), 1);
  setenv("MPCNAM_T3K_PORT", port.c_str(), 1);
  setenv("MPCNAM_T3K_HOST", "127.0.0.1", 1);
  setenv("MPCNAM_T3K_DEST", dest.c_str(), 1);
  const std::string self = "http://127.0.0.1:" + port;
  // written by TONE3000's thread, read here: under a lock (as the plugin's callback takes its own mutex)
  std::mutex got_mu;
  std::string got_, title_;
  mpcnam::Tone3000 t([&](const std::string& p, const std::string& tt) { std::lock_guard<std::mutex> lk(got_mu); got_ = p; title_ = tt; });
  auto got_now = [&] { std::lock_guard<std::mutex> lk(got_mu); return got_; };
  auto wait_got = [&] { for (int k = 0; k < 200 && got_now().empty(); k++) std::this_thread::sleep_for(std::chrono::milliseconds(50)); };

  t.start();
  CHECK(t.status() == "Open 127.0.0.1:" + port, "GET TONES opens the page: status [%s]", t.status().c_str());
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  std::string home = sh("curl -s " + self + "/");
  CHECK(has(home, "partnered with TONE3000") && has(home, "/go?kind=model") && has(home, "/go?kind=ir"),
        "the phone page: TONE3000's partnership message, captures and IRs");

  // amp captures: the phone walks the flow; the plugin exchanges the code (PKCE) and downloads the tone's A2 models
  std::string page = sh("curl -sL " + self + "/go?kind=model");
  wait_got();
  const std::string mdir = dest + "/models/TONE3000/Fender Twin- Clean-Edge";
  CHECK(has(page, "Downloading to your MPC"), "the phone is told the download started");
  CHECK(got_now() == mdir + "/Clean.nam", "captures land in models/TONE3000/<tone>/ (%s)", got_now().c_str());
  CHECK(slurp(mdir + "/Clean.nam") == slurp(argv[4]) && slurp(mdir + "/Edge - Crunch.nam") == slurp(argv[4]),
        "both of the tone's A2 models downloaded intact (names made file-safe)");
  std::string meta = slurp(mdir + "/tone3000.json");
  CHECK(has(meta, "\"creator\": \"alice\"") && has(meta, "\"license\": \"cc-by\"") && has(meta, "tones/42"),
        "tone3000.json credits the creator, licence and link");
  {   // "Got <title> (n)": the title is shortened to fit the page's 23 characters, the count always shows
    const std::string st = t.status();
    CHECK(st.rfind("Got Fender Twin", 0) == 0 && st.size() >= 4 && st.compare(st.size() - 4, 4, " (2)") == 0 && st.size() <= 23,
          "status [%s]: title, count, fits the page", st.c_str());
  }
  std::string listing = sh("ls -A '" + mdir + "'");
  CHECK(has(listing, "Clean.nam") && !has(listing, ".part"), "no partial files left (%zu-byte listing checked)", listing.size());

  // cab IRs
  { std::lock_guard<std::mutex> lk(got_mu); got_.clear(); title_.clear(); }
  sh("curl -sL " + self + "/go?kind=ir");
  wait_got();
  CHECK(got_now() == dest + "/irs/TONE3000/4x12 V30 Pack/SM57 cap edge.wav" && slurp(got_now()) == slurp(argv[5]),
        "IRs land in irs/TONE3000/<tone>/ (%s)", got_now().c_str());

  // the callback is single-use, and needs the state this page issued
  std::string loc = sh("curl -s -o /dev/null -w '%{redirect_url}' " + self + "/go?kind=model");
  std::string cb = sh("curl -s -o /dev/null -w '%{redirect_url}' '" + loc + "'");
  std::string first = sh("curl -s -w ' %{http_code}' '" + cb + "'");
  std::this_thread::sleep_for(std::chrono::milliseconds(1500));
  std::string replay = sh("curl -s -w ' %{http_code}' '" + cb + "'");
  CHECK(has(first, " 200") && has(replay, "expired") && has(replay, " 400"), "a replayed callback is refused (400)");
  std::string forged = sh("curl -s -w ' %{http_code}' '" + self + "/cb?code=x&state=forged&tone_id=42'");
  CHECK(has(forged, " 400"), "a callback with a state this page didn't issue is refused");
  std::string inject = sh("curl -s -o /dev/null -w '%{redirect_url}' " + self + "/go?kind=model");
  size_t sp = inject.find("state="); std::string st = inject.substr(sp + 6, inject.find('&', sp) - sp - 6);
  sh("curl -s '" + self + "/cb?code=x&state=" + st + "&tone_id=../../etc'");
  CHECK(t.status() == "Canceled", "a tone id that isn't a number is not used (status [%s])", t.status().c_str());
  std::string cancel = sh("curl -s -o /dev/null -w '%{redirect_url}' " + self + "/go?kind=ir");
  sp = cancel.find("state="); st = cancel.substr(sp + 6, cancel.find('&', sp) - sp - 6);
  sh("curl -s '" + self + "/cb?state=" + st + "&canceled=true'");
  CHECK(t.status() == "Canceled", "closing TONE3000 without a pick: [%s]", t.status().c_str());
  std::string other = sh("curl -s -w ' %{http_code}' " + self + "/etc/passwd");
  CHECK(has(other, " 404") && !has(other, "root:"), "nothing else is served");

  auto t0 = std::chrono::steady_clock::now();
  t.stop();
  double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  std::string after = sh("curl -s -m 2 -w '%{http_code}' " + self + "/");
  CHECK(ms < 1000 && has(after, "000"), "stop closes the page promptly (%.0f ms)", ms);
  printf("%s (%d failure%s)\n", fails ? "FAILED" : "PASSED", fails, fails == 1 ? "" : "s");
  return fails ? 1 : 0;
}
