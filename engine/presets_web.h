// SPDX-License-Identifier: MIT
// Copyright (c) 2026 the mpc-vst-liminal-amp contributors
/* presets_web.h -- the My Presets pages of the phone web server (engine/tone3000.h's server; open them from the My
 * Presets list's NAME ON PHONE button, or the home page, then http://<address>/presets on a phone or computer on the
 * same Wi-Fi).
 *
 * The MPC's screen has no keyboard, so naming and editing happen here: save the current sound under a typed name, and
 * for each saved preset Load, Edit, Rename and Delete. Edit shows the preset's amp and cab (chosen from the plugin's
 * lists) and every setting it keeps, and saves them back to its file (optionally loading it at once). The page only
 * asks; the plugin does the work through these hooks (each returns "" when it worked, else a short message for the
 * page's banner). Every change is a POST answered with a redirect, so a reload never repeats it.
 */
#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "presets.h"
#include "tone3000.h"

namespace mpcnam {

struct PresetField {                  // one setting a preset keeps, as the edit page shows it
  std::string key, label, unit, group;   // group: the plugin page's caption it sits under (input, tone, out...)
  float min = 0, max = 1, step = 0.1f, def = 0;
  bool toggle = false;                // on/off (a checkbox)
  std::vector<std::string> options;   // a choice (a list); the value is the option's index
};
struct PresetChoice {                 // a capture or an IR the edit page offers
  std::string label;                  // "Pack · name"
  PresetRef ref;
};

struct PresetsWebHooks {
  std::function<std::string(const std::string& name, bool overwrite)> save;   // the plugin's current sound
  std::function<std::string(const std::string& name)> load, remove;
  std::function<std::string(const std::string& from, const std::string& to)> rename;
  std::function<std::string()> current;                                       // the selected preset's name
  std::function<std::vector<PresetField>()> fields;                           // what a preset keeps
  std::function<std::vector<PresetChoice>(int which)> choices;               // 0: captures, 1: IRs
  std::function<void()> edited;                                               // a preset file was changed here
};

// true when `path` is one of this page's (the caller replies with *out)
bool presets_web_route(const PresetsWebHooks& hooks, const std::string& method, const std::string& path,
                       const std::map<std::string, std::string>& args, WebReply* out);

}  // namespace mpcnam
