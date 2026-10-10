// SPDX-License-Identifier: MIT
// Copyright (c) 2026 the mpc-vst-liminal-amp contributors
// presets_test.cpp -- engine/presets (My Presets files) on a made-up presets folder (MPCNAM_PRESETS).
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include "presets.h"
using namespace mpcnam;
namespace fs = std::filesystem;
static int fails = 0;
#define CHECK(c, ...) do { const bool ok_ = (c); printf("%s ", ok_ ? "ok  " : "FAIL"); printf(__VA_ARGS__); printf("\n"); if (!ok_) fails++; } while (0)
static float val(const PresetData& p, const std::string& k, float dflt = -999) {
  for (auto& kv : p.values) if (kv.first == k) return kv.second;
  return dflt;
}
int main() {
  fs::path d = fs::temp_directory_path() / "mpcnam_presets";
  fs::remove_all(d);
  setenv("MPCNAM_PRESETS", (d / "presets").c_str(), 1);

  CHECK(Presets::list().empty(), "no folder yet: an empty list, no crash");
  PresetData a;
  a.name = "Big Crunch";
  a.values = {{"bass", 4.5f}, {"input", 2.5f}, {"output", -3}, {"eco", 1}};
  a.model = {"/media/card/models/Marshall/Plexi.nam", "Marshall/Plexi", "Plexi.nam"};
  a.ir = {"/media/card/irs/Celestion/V30.wav", "Celestion/V30", "V30.wav"};
  std::string err;
  CHECK(Presets::save(a, false, &err) && fs::exists(d / "presets/Big Crunch.na2p"), "save creates the folder and <name>.na2p");
  PresetData got;
  CHECK(Presets::load("Big Crunch", &got) && got.name == "Big Crunch" && val(got, "bass") == 4.5f &&
        val(got, "input") == 2.5f && val(got, "output") == -3 && val(got, "eco") == 1 && got.ir.file == "V30.wav" &&
        got.ir.display == "Celestion/V30" && got.ir.path == a.ir.path && got.model.path == a.model.path &&
        got.model.display == "Marshall/Plexi" && got.model.file == "Plexi.nam", "settings, the amp and the cab come back as saved");
  CHECK(Presets::load("big crunch", &got), "a name is found in any case (a FAT card)");
  CHECK(!Presets::save(a, false, &err) && err == "That name is already used", "a name in use is refused: [%s]", err.c_str());
  a.values[0].second = 10;
  CHECK(Presets::save(a, true, &err) && Presets::load("Big Crunch", &got) && val(got, "bass") == 10, "overwrite replaces it");
  CHECK(!fs::exists(d / "presets/.Big Crunch.tmp"), "no temp file is left");

  PresetData none; none.name = "No IR"; none.values = {{"bass", 5}};
  CHECK(Presets::save(none, false, &err) && Presets::load("No IR", &got) && got.ir.empty() && got.model.empty(), "a preset without an amp or an IR");

  // names
  CHECK(Presets::clean_name("  a/b\\c:d*e?f\"g<h>i|j ") == "a-b-c-d-e-f-g-h-i-j", "bad characters become '-', ends trimmed: [%s]", Presets::clean_name("  a/b\\c:d*e?f\"g<h>i|j ").c_str());
  CHECK(Presets::clean_name("...") == "" && Presets::clean_name("   ") == "" && Presets::clean_name("../../etc") == "-..-etc" ,
        "dots and spaces only is empty; no path escapes: [%s]", Presets::clean_name("../../etc").c_str());
  CHECK(Presets::clean_name(std::string("x\n\ty")) == "x--y", "control characters");
  std::string longname(100, 'a');
  CHECK(Presets::clean_name(longname).size() == Presets::kMaxName, "long names are cut to %zu", Presets::kMaxName);
  std::string u8 = std::string(39, 'a') + "\xC3\xA9\xC3\xA9";                      // 39 + two 2-byte letters
  const std::string cu = Presets::clean_name(u8);
  CHECK(cu.size() == 39 && (unsigned char)cu.back() == 'a', "a cut never splits a UTF-8 character (%zu bytes)", cu.size());
  CHECK(Presets::unique_name("Big Crunch") == "Big Crunch 2" && Presets::unique_name("big crunch") == "big crunch 2", "unique_name adds a number: [%s]", Presets::unique_name("Big Crunch").c_str());
  CHECK(Presets::unique_name("Fresh") == "Fresh" && Presets::unique_name("///") == "---" && Presets::unique_name("") == "My Preset", "unique_name: free names, odd ones");
  PresetData bad; bad.name = "..";
  CHECK(!Presets::save(bad, false, &err), "an empty name can't be saved: [%s]", err.c_str());

  // list, rename, delete
  PresetData z; z.name = "alpha"; z.values = {{"bass", 1}};
  Presets::save(z, false, &err);
  auto l = Presets::list();
  CHECK(l.size() == 3 && l[0].name == "alpha" && l[1].name == "Big Crunch" && l[2].name == "No IR", "listed by name, ignoring case (%zu)", l.size());
  std::ofstream(d / "presets/notes.txt") << "x";
  std::ofstream(d / "presets/junk.na2p") << "not a preset";
  std::ofstream(d / "presets/._alpha.na2p") << "nam-a2-preset=1\nbass=1\n";
  fs::create_directory(d / "presets/folder.na2p");
  CHECK(Presets::list().size() == 3, "other files, junk, hidden files and folders are skipped");
  CHECK(Presets::rename("alpha", "Beta Room", &err) && Presets::load("Beta Room", &got) && !Presets::exists("alpha"), "rename");
  CHECK(!Presets::rename("Beta Room", "No IR", &err) && err == "That name is already used", "rename onto another preset is refused: [%s]", err.c_str());
  CHECK(Presets::rename("Beta Room", "beta room", &err) && Presets::exists("Beta Room"), "rename that only changes case is fine");
  CHECK(!Presets::rename("nope", "x", &err) && err == "Preset not found", "rename of a missing preset: [%s]", err.c_str());
  CHECK(!Presets::rename("Beta Room", "///", &err) || Presets::exists("---"), "rename to an odd name is cleaned, never escapes the folder");
  CHECK(Presets::remove("No IR", &err) && !Presets::exists("No IR") && !fs::exists(d / "presets/No IR.na2p"), "delete removes the file");
  CHECK(!Presets::remove("No IR", &err) && err == "Preset not found", "deleting twice: [%s]", err.c_str());
  CHECK(fs::exists(d / "presets/notes.txt"), "only preset files are ever deleted");
  // a file edited by hand
  std::ofstream(d / "presets/By Hand.na2p") << "nam-a2-preset=1\r\nbass=7\r\nbogus line\r\ntreble=nan\r\nmodel_file=x.nam\r\n";
  CHECK(Presets::load("By Hand", &got) && val(got, "bass") == 7 && val(got, "treble") == -999 && got.model.file == "x.nam", "a hand-edited file (CRLF, junk lines, nan) loads");
  fs::remove_all(d);
  printf("%s (%d failure%s)\n", fails ? "FAILED" : "PASSED", fails, fails == 1 ? "" : "s");
  return fails ? 1 : 0;
}
