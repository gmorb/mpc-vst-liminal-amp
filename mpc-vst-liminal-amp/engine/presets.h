// SPDX-License-Identifier: MIT
// Copyright (c) 2026 the mpc-vst-liminal-amp contributors
/* presets.h -- "My Presets": the settings, the amp and the cab you save, one small text file each (as Liminal Hz).
 *
 * Where: <plugin folder>/presets/<name>.na2p (MPCNAM_PRESETS overrides the folder, for tests). The preset's name is
 * its file name, so a preset can also be renamed or deleted from a computer or the MPC's file browser, and RESCAN
 * picks that up. A file is plain "key=value" lines after the header "nam-a2-preset=1": the parameters by their real
 * values, then the capture and the IR each as path, display (relative path) and file name, found again the way a
 * project finds them, even if the card moved. Nothing here runs on the audio thread: it touches the file system.
 */
#pragma once
#include <string>
#include <utility>
#include <vector>

namespace mpcnam {

struct PresetRef {                                         // a capture or an IR, as a project keeps it
  std::string path, display, file;
  bool empty() const { return path.empty() && file.empty(); }
};

struct PresetData {
  std::string name;                                        // the file name without ".na2p"
  std::vector<std::pair<std::string, float>> values;       // parameter key -> real value
  PresetRef model, ir;                                     // empty when the preset has none
};

class Presets {
 public:
  static constexpr int kMaxPresets = 200;                  // listed (more files are ignored)
  static constexpr size_t kMaxName = 40;                   // bytes of a name (never cuts a UTF-8 character)

  static std::string dir();                                // MPCNAM_PRESETS, else <plugin folder>/presets

  // sorted by name (case-insensitive); unreadable or foreign files are skipped
  static std::vector<PresetData> list();
  static bool load(const std::string& name, PresetData* out);

  // A name safe to use as a file name: control characters and / \ : * ? " < > | become "-", leading and
  // trailing spaces and dots go, at most kMaxName bytes. "" when nothing is left.
  static std::string clean_name(const std::string& raw);
  // `base` (cleaned), or "base 2", "base 3"... the first that no preset has (case-insensitively: a card may be FAT)
  static std::string unique_name(const std::string& base);
  static bool exists(const std::string& name);

  // overwrite=false: refuses a name that exists. The file is written whole or not at all (temp file, then rename).
  static bool save(const PresetData& p, bool overwrite, std::string* err);
  static bool remove(const std::string& name, std::string* err);
  static bool rename(const std::string& from, const std::string& to, std::string* err);
};

}  // namespace mpcnam
