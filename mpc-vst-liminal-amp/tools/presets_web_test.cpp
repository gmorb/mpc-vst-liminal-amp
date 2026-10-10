// SPDX-License-Identifier: MIT
// Copyright (c) 2026 the mpc-vst-liminal-amp contributors
// presets_web_test.cpp -- the My Presets phone page, over a real socket against engine/tone3000's web server.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>
#include "presets.h"
#include "presets_web.h"
#include "tone3000.h"
using namespace mpcnam;
namespace fs = std::filesystem;
static int fails = 0;
#define CHECK(c, ...) do { const bool ok_ = (c); printf("%s ", ok_ ? "ok  " : "FAIL"); printf(__VA_ARGS__); printf("\n"); if (!ok_) fails++; } while (0)
static int port = 0;
// one request; `parts` are sent as separate writes (a POST's body can arrive after its headers)
static std::string http(const std::vector<std::string>& parts) {
  int s = socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons((uint16_t)port); inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
  if (connect(s, (sockaddr*)&a, sizeof a) != 0) { close(s); return ""; }
  for (auto& p : parts) { send(s, p.data(), p.size(), 0); std::this_thread::sleep_for(std::chrono::milliseconds(60)); }
  std::string r; char b[4096]; ssize_t n;
  while ((n = recv(s, b, sizeof b, 0)) > 0) r.append(b, (size_t)n);
  close(s);
  return r;
}
static std::string get(const std::string& path) { return http({"GET " + path + " HTTP/1.1\r\nHost: x\r\n\r\n"}); }
static std::string post(const std::string& path, const std::string& body, bool split = false) {
  const std::string head = "POST " + path + " HTTP/1.1\r\nHost: x\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: " +
                           std::to_string(body.size()) + "\r\n\r\n";
  return split ? http({head, body.substr(0, body.size() / 2), body.substr(body.size() / 2)}) : http({head + body});
}
int main() {
  fs::path d = fs::temp_directory_path() / "mpcnam_presets_web";
  fs::remove_all(d);
  setenv("MPCNAM_PRESETS", (d / "presets").c_str(), 1);

  port = 18300 + (int)(getpid() % 500);
  setenv("MPCNAM_T3K_PORT", std::to_string(port).c_str(), 1);
  setenv("MPCNAM_T3K_HOST", "127.0.0.1", 1);
  std::string loaded, last_saved;
  PresetsWebHooks h;
  h.save = [&](const std::string& n, bool ow) { PresetData p; p.name = n; p.values = {{"bass", 6}, {"gate_on", 1}}; p.model = {"/m/models/Fender/Twin.nam", "Fender/Twin", "Twin.nam"}; std::string e; last_saved = n; return Presets::save(p, ow, &e) ? std::string() : e; };
  h.load = [&](const std::string& n) { loaded = n; return Presets::exists(n) ? std::string() : std::string("Preset not found"); };
  h.remove = [&](const std::string& n) { std::string e; return Presets::remove(n, &e) ? std::string() : e; };
  h.rename = [&](const std::string& a, const std::string& b) { std::string e; return Presets::rename(a, b, &e) ? std::string() : e; };
  h.current = [&] { return loaded; };
  int edits = 0;
  h.edited = [&] { edits++; };
  h.fields = [] {
    std::vector<PresetField> f(3);
    f[0].key = "bass"; f[0].label = "Bass"; f[0].min = 0; f[0].max = 10; f[0].step = 0.1f; f[0].def = 5;
    f[1].key = "gate_on"; f[1].label = "Gate"; f[1].toggle = true; f[1].def = 1;
    f[2].key = "ir_len"; f[2].label = "IR length"; f[2].options = {"Full", "Short"};
    return f;
  };
  h.choices = [](int which) {
    std::vector<PresetChoice> c(2);
    c[0].label = which ? "Celestion Â· V30" : "Fender Â· Twin"; c[0].ref = which ? PresetRef{"/m/irs/Celestion/V30.wav", "Celestion/V30", "V30.wav"} : PresetRef{"/m/models/Fender/Twin.nam", "Fender/Twin", "Twin.nam"};
    c[1].label = which ? "Mesa Â· 4x12" : "Marshall Â· Plexi"; c[1].ref = which ? PresetRef{"/m/irs/Mesa/4x12.wav", "Mesa/4x12", "4x12.wav"} : PresetRef{"/m/models/Marshall/Plexi.nam", "Marshall/Plexi", "Plexi.nam"};
    return c;
  };
  {
    Tone3000 t([](const std::string&, const std::string&) {});
    t.set_route([&](const std::string& m, const std::string& p, const std::map<std::string, std::string>& a, WebReply* o) { return presets_web_route(h, m, p, a, o); });
    CHECK(t.address().empty(), "closed: no address");
    t.start("", false);
    CHECK(t.address() == "127.0.0.1:" + std::to_string(port), "open without libcurl needed: %s", t.address().c_str());

    std::string r = get("/presets");
    CHECK(r.find("200 OK") != std::string::npos && r.find("My Presets") != std::string::npos && r.find("No saved presets yet") != std::string::npos, "the page, with no presets yet");
    r = post("/presets/save", "name=Big+Dark+Hall");
    CHECK(r.find("303 See Other") != std::string::npos && r.find("Location: /presets?msg=Saved%20%22Big%20Dark%20Hall%22.") != std::string::npos &&
          fs::exists(d / "presets/Big Dark Hall.na2p") && last_saved == "Big Dark Hall", "save: typed name -> file, redirect with a message");
    r = post("/presets/save", "name=Sp%C3%A4ce%20%26%20Time", true);
    CHECK(Presets::exists("Späce & Time"), "a body that arrives in two pieces, UTF-8 and & in the name");
    r = post("/presets/save", "name=Big+Dark+Hall");
    CHECK(r.find("already%20used") != std::string::npos, "an existing name is refused without 'replace'");
    r = post("/presets/save", "name=Big+Dark+Hall&overwrite=1");
    CHECK(r.find("Saved") != std::string::npos, "...and replaced with it");
    r = post("/presets/save", "name=++%2F%2F++");
    CHECK(Presets::exists("--") && Presets::list().size() == 3 && !fs::exists(d / "presets/--") && !fs::exists(d / "--.na2p"), "a name of slashes is cleaned to '--' and stays in the folder");
    r = post("/presets/save", "name=%3Cscript%3Ealert(1)%3C%2Fscript%3E");
    r = get("/presets");
    CHECK(r.find("<script>alert") == std::string::npos && r.find("-script-alert(1)--script-") != std::string::npos, "markup in a name is neutralised when saved");
    { PresetData q; q.name = "x"; q.values = {{"bass", 1}}; std::string e; Presets::save(q, false, &e); fs::rename(d / "presets/x.na2p", d / "presets/a&b\"c'd.na2p"); }
    r = get("/presets");
    CHECK(r.find("a&amp;b&quot;c&#39;d") != std::string::npos && r.find("a&b\"c'd") == std::string::npos, "...and a file renamed by hand with & \" ' is escaped on the page");
    CHECK(r.find("Big Dark Hall") != std::string::npos && r.find("Sp\xC3\xA4" "ce &amp; Time") != std::string::npos, "the list shows the presets");
    r = post("/presets/load", "name=Big+Dark+Hall");
    CHECK(loaded == "Big Dark Hall" && r.find("Loaded") != std::string::npos, "load calls the plugin");
    r = get("/presets");
    CHECK(r.find("in use") != std::string::npos && r.find("Amp: Twin") != std::string::npos, "the preset in use is marked, with its amp");
    // editing a preset: the page shows its amp, cab and settings; a POST writes them back (within range)
    r = get("/presets/edit?name=Big+Dark+Hall");
    CHECK(r.find("200 OK") != std::string::npos && r.find("Edit: Big Dark Hall") != std::string::npos &&
          r.find("<option value='0' selected>Fender") != std::string::npos && r.find("No cab</option>") != std::string::npos &&
          r.find("name=\"bass\"") != std::string::npos && r.find("value=6") != std::string::npos && r.find(" checked>Gate") != std::string::npos,
          "the edit page shows the preset's amp, its (missing) cab and its settings");
    r = post("/presets/update", "name=Big+Dark+Hall&model=1&ir=0&bass=42&gate_on=0&ir_len=1&apply=0");
    PresetData e; Presets::load("Big Dark Hall", &e);
    float eb = -1, eg = -1, el = -1;
    for (auto& kv : e.values) { if (kv.first == "bass") eb = kv.second; if (kv.first == "gate_on") eg = kv.second; if (kv.first == "ir_len") el = kv.second; }
    CHECK(r.find("303 See Other") != std::string::npos && r.find("Location: /presets/edit?name=Big%20Dark%20Hall&msg=Saved") != std::string::npos &&
          e.model.file == "Plexi.nam" && e.ir.file == "V30.wav" && eb == 10 && eg == 0 && el == 1 && edits == 1,
          "update: amp and cab from the lists, a setting clamped to its range (bass %g), switches and choices", eb);
    r = post("/presets/update", "name=Big+Dark+Hall&gate_on=0&gate_on=1&ir=none&apply=1");
    Presets::load("Big Dark Hall", &e);
    for (auto& kv : e.values) if (kv.first == "gate_on") eg = kv.second;
    CHECK(loaded == "Big Dark Hall" && r.find("loaded") != std::string::npos && e.ir.empty() && e.model.file == "Plexi.nam" && eg == 1,
          "Save and load: the cab removed, the amp kept, a ticked switch wins over its hidden 0, and the plugin loads it");
    CHECK(get("/presets/edit?name=Nope").find("Location: /presets?msg=Preset%20not%20found.") != std::string::npos, "editing a missing preset goes back to the list");
    r = post("/presets/update", "name=Big+Dark+Hall&model=7&bass=nan");
    Presets::load("Big Dark Hall", &e);
    for (auto& kv : e.values) if (kv.first == "bass") eb = kv.second;
    CHECK(e.model.file == "Plexi.nam" && eb == 10, "an out-of-range choice and a bad number change nothing");
    r = post("/presets/rename", "from=Big+Dark+Hall&to=Great+Hall");
    CHECK(r.find("Renamed") != std::string::npos && Presets::exists("Great Hall") && !Presets::exists("Big Dark Hall"), "rename");
    r = post("/presets/delete", "name=Great+Hall");
    CHECK(r.find("Deleted") != std::string::npos && !Presets::exists("Great Hall"), "delete");
    r = post("/presets/delete", "name=Great+Hall");
    CHECK(r.find("not%20found") != std::string::npos, "deleting a missing one says so");
    CHECK(get("/presets/save").find("405") != std::string::npos, "a change by GET is refused");
    CHECK(post("/presets/nope", "x=1").find("404") != std::string::npos, "unknown page under /presets: 404");
    CHECK(post("/presets/save", std::string(5000, 'a')).find("413") != std::string::npos, "an oversized form is refused");
    CHECK(get("/").find("TONE3000") != std::string::npos && get("/").find("/presets") != std::string::npos && get("/logo").find("404") != std::string::npos, "the home page offers TONE3000 and My Presets");
    CHECK(post("/", "a=1").find("405") != std::string::npos, "other POSTs are still refused");
    t.stop();
  }
  fs::remove_all(d);
  printf("%s (%d failure%s)\n", fails ? "FAILED" : "PASSED", fails, fails == 1 ? "" : "s");
  return fails ? 1 : 0;
}
