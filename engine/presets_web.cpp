// SPDX-License-Identifier: MIT
// Copyright (c) 2026 the mpc-vst-liminal-amp contributors
#include "presets_web.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "web_theme.h"

namespace mpcnam {

static std::string esc(const std::string& s) {       // for HTML text and attribute values
  std::string o;
  for (char c : s) {
    if (c == '<') o += "&lt;"; else if (c == '>') o += "&gt;"; else if (c == '&') o += "&amp;";
    else if (c == '"') o += "&quot;"; else if (c == '\'') o += "&#39;"; else o += c;
  }
  return o;
}
static std::string urlenc(const std::string& s) {
  std::string o;
  char b[4];
  for (unsigned char c : s) {
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') o += (char)c;
    else { snprintf(b, sizeof b, "%%%02X", c); o += b; }
  }
  return o;
}
static std::string arg(const std::map<std::string, std::string>& a, const char* k) {
  auto it = a.find(k);
  return it == a.end() ? std::string() : it->second;
}
static std::string page(const std::string& body) { return web_page("Liminal Amp presets", web_wordmark() + body); }

static void back(WebReply* out, const std::string& msg, const std::string& to = "/presets") {   // 303: no repeat on reload
  out->status = "303 See Other";
  out->type = "text/plain";
  out->body = "";
  const char sep = to.find('?') == std::string::npos ? '?' : '&';
  out->extra = "Location: " + to + (msg.empty() ? std::string() : std::string(1, sep) + "msg=" + urlenc(msg)) + "\r\n";
}
static std::string stem(const std::string& s) {      // "Marshall/Plexi" -> "Plexi"; "Plexi.nam" -> "Plexi"
  std::string t = s;
  const size_t sl = t.find_last_of('/');
  if (sl != std::string::npos) t = t.substr(sl + 1);
  const size_t dot = t.find_last_of('.');
  if (dot != std::string::npos && dot > 0 && t.size() - dot <= 5) t = t.substr(0, dot);
  return t;
}
static std::string ref_name(const PresetRef& r) {
  if (r.empty()) return "";
  return stem(!r.display.empty() ? r.display : !r.file.empty() ? r.file : r.path);
}
static bool same_ref(const PresetRef& a, const PresetRef& b) {
  return (!a.path.empty() && a.path == b.path) || (a.path.empty() && !a.file.empty() && a.file == b.file);
}
static std::string fmt(float v) {
  char b[32];
  snprintf(b, sizeof b, "%g", std::round(v * 100.0f) / 100.0f);
  return b;
}

static std::string list_page(const PresetsWebHooks& h, const std::map<std::string, std::string>& args) {
  const std::string cur = h.current ? h.current() : std::string();
  std::string b = "<h1>My Presets</h1><div class=sub>Your saved sounds: the amp, the cab and every setting. They are "
                  "small files in the plugin's presets folder.</div>";
  const std::string msg = arg(args, "msg");
  if (!msg.empty()) b += "<div class=msg>" + esc(msg) + "</div>";
  b += "<div class=cap>save what's playing</div><div class=card><form method=post action='/presets/save'>"
       "<label for=n>Name</label>"
       "<input type=text id=n name=name maxlength=40 placeholder='e.g. Plexi crunch, edge of breakup' autocomplete=off>"
       "<label class=sw><input type=checkbox name=overwrite value=1>Replace a preset with this name</label>"
       "<button class=go type=submit>Save current sound</button></form></div>";
  auto list = Presets::list();
  b += "<div class=cap>saved</div><div class=card>";
  if (list.empty()) b += "<small>No saved presets yet. Set the plugin the way you like it, then save it above (or tap SAVE in the MY PRESETS list on the MPC).</small>";
  for (const auto& p : list) {
    const std::string n = esc(p.name);
    std::string rig;
    if (!p.model.empty()) rig += "Amp: " + esc(ref_name(p.model));
    if (!p.ir.empty()) rig += (rig.empty() ? "" : " &middot; ") + std::string("Cab: ") + esc(ref_name(p.ir));
    b += "<div class=row><span class=name>" + n + "</span>" + (p.name == cur ? "<span class=cur>in use</span>" : "") +
         (rig.empty() ? "" : "<div class=rig>" + rig + "</div>") +
         "<form method=post action='/presets/load'><input type=hidden name=name value=\"" + n + "\"><button class=go type=submit>Load</button></form>"
         "<a class=b style='display:inline-flex' href='/presets/edit?name=" + urlenc(p.name) + "'>Edit</a>"
         "<form method=post action='/presets/delete' onsubmit=\"return confirm('Delete this preset? It can\\'t be undone.')\">"
         "<input type=hidden name=name value=\"" + n + "\"><button class=del type=submit>Delete</button></form>"
         "<form method=post action='/presets/rename' class=ren><input type=hidden name=from value=\"" + n + "\">"
         "<input type=text name=to value=\"" + n + "\" maxlength=40 autocomplete=off aria-label='New name'><button type=submit>Rename</button></form></div>";
  }
  b += "</div><p><small>Keep this phone on the same Wi-Fi as the MPC. The page closes itself after 15 idle minutes.</small></p>"
       "<div class=nav><a class=b href='/'>Home: TONE3000</a></div>";
  return page(b);
}

static std::string edit_page(const PresetsWebHooks& h, const PresetData& p, const std::map<std::string, std::string>& args) {
  const std::string n = esc(p.name);
  std::string b = "<h1>Edit: " + n + "</h1><div class=sub>Change what this preset keeps. Save writes its file; "
                  "Save and load also plays it now.</div>";
  const std::string msg = arg(args, "msg");
  if (!msg.empty()) b += "<div class=msg>" + esc(msg) + "</div>";
  b += "<form method=post action='/presets/update'><input type=hidden name=name value=\"" + n + "\">";
  for (int which = 0; which < 2; which++) {
    const PresetRef& r = which == 0 ? p.model : p.ir;
    const char* key = which == 0 ? "model" : "ir";
    b += std::string("<div class=cap>") + (which == 0 ? "amp" : "cab") + "</div><div class=card><select name=" + key +
         " aria-label='" + (which == 0 ? "Amp" : "Cab") + "'>";
    auto ch = h.choices ? h.choices(which) : std::vector<PresetChoice>();
    bool found = false;
    std::string opts;
    for (size_t k = 0; k < ch.size(); k++) {
      const bool sel = !r.empty() && same_ref(r, ch[k].ref);
      found = found || sel;
      opts += "<option value='" + std::to_string(k) + "'" + (sel ? " selected" : "") + ">" + esc(ch[k].label) + "</option>";
    }
    if (!r.empty() && !found) b += "<option value=keep selected>" + esc(ref_name(r)) + " (not on this MPC now)</option>";
    b += std::string("<option value=none") + (r.empty() ? " selected" : "") + ">" + (which == 0 ? "No amp" : "No cab") + "</option>" + opts;
    b += "</select></div>";
  }
  b += "<div class=cap>settings</div><div class=card><div class=grid>";
  auto fs = h.fields ? h.fields() : std::vector<PresetField>();
  std::vector<std::string> groups;                    // in the order the plugin page has them
  for (const auto& f : fs) {
    bool seen = false;
    for (const auto& g : groups) seen = seen || g == f.group;
    if (!seen) groups.push_back(f.group);
  }
  for (const auto& g : groups) {
    if (!g.empty()) b += "<div class=grp>" + esc(g) + "</div>";
    for (int pass = 0; pass < 2; pass++) {            // a group's switch first (as on the plugin page), then its values
      for (const auto& f : fs) {
        if (f.group != g || (pass == 0) != f.toggle) continue;
        float v = f.def;
        for (const auto& kv : p.values) if (kv.first == f.key) v = kv.second;
        const std::string id = "f_" + f.key;
        if (f.toggle) {
          b += "<input type=hidden name=\"" + esc(f.key) + "\" value=0><label class=sw><input type=checkbox name=\"" +
               esc(f.key) + "\" value=1" + (v > 0.5f ? " checked" : "") + ">" + esc(f.label) + "</label>";
        } else if (!f.options.empty()) {
          b += "<div class=f><label for=" + id + ">" + esc(f.label) + "</label><select id=" + id + " name=\"" + esc(f.key) + "\">";
          for (size_t k = 0; k < f.options.size(); k++)
            b += "<option value=" + std::to_string(k) + ((int)std::lround(v) == (int)k ? " selected" : "") + ">" + esc(f.options[k]) + "</option>";
          b += "</select></div>";
        } else {
          b += "<div class=f><label for=" + id + ">" + esc(f.label) + (f.unit.empty() ? "" : " <span class=unit>" + esc(f.unit) + "</span>") +
               "</label><input type=number inputmode=decimal id=" + id + " name=\"" + esc(f.key) + "\" min=" + fmt(f.min) +
               " max=" + fmt(f.max) + " step=" + fmt(f.step) + " value=" + fmt(v) + "></div>";
        }
      }
    }
  }
  b += "</div></div><button class=go type=submit name=apply value=1>Save and load</button>"
       "<button type=submit name=apply value=0>Save</button></form>"
       "<div class=nav><a class=b href='/presets'>Back to My Presets</a></div>";
  return page(b);
}

bool presets_web_route(const PresetsWebHooks& h, const std::string& method, const std::string& path,
                       const std::map<std::string, std::string>& args, WebReply* out) {
  if (path.compare(0, 8, "/presets") != 0) return false;
  if (path == "/presets" || path == "/presets/") {
    if (method != "GET") { out->status = "405 Method Not Allowed"; out->type = "text/plain"; out->body = "GET only"; return true; }
    out->body = list_page(h, args);
    return true;
  }
  if (path == "/presets/edit") {
    if (method != "GET") { out->status = "405 Method Not Allowed"; out->type = "text/plain"; out->body = "GET only"; return true; }
    PresetData p;
    if (!Presets::load(arg(args, "name"), &p)) { back(out, "Preset not found."); return true; }
    out->body = edit_page(h, p, args);
    return true;
  }
  if (method != "POST") { out->status = "405 Method Not Allowed"; out->type = "text/plain"; out->body = "POST only"; return true; }
  std::string err;
  if (path == "/presets/save") {
    const std::string name = Presets::clean_name(arg(args, "name"));
    if (name.empty()) { back(out, "Type a name first."); return true; }
    err = h.save ? h.save(name, arg(args, "overwrite") == "1") : "Not available";
    back(out, err.empty() ? "Saved \"" + name + "\"." : err);
  } else if (path == "/presets/load") {
    err = h.load ? h.load(arg(args, "name")) : "Not available";
    back(out, err.empty() ? "Loaded \"" + arg(args, "name") + "\"." : err);
  } else if (path == "/presets/delete") {
    err = h.remove ? h.remove(arg(args, "name")) : "Not available";
    back(out, err.empty() ? "Deleted \"" + arg(args, "name") + "\"." : err);
  } else if (path == "/presets/rename") {
    const std::string to = Presets::clean_name(arg(args, "to"));
    err = h.rename ? h.rename(arg(args, "from"), to) : "Not available";
    back(out, err.empty() ? "Renamed to \"" + to + "\"." : err);
  } else if (path == "/presets/update") {
    PresetData p;
    const std::string name = arg(args, "name");
    if (!Presets::load(name, &p)) { back(out, "Preset not found."); return true; }
    for (int which = 0; which < 2; which++) {          // the amp and the cab: one from the lists, none, or as it was
      const std::string v = arg(args, which == 0 ? "model" : "ir");
      PresetRef& r = which == 0 ? p.model : p.ir;
      if (v == "none") r = PresetRef();
      else if (!v.empty() && v != "keep") {
        auto ch = h.choices ? h.choices(which) : std::vector<PresetChoice>();
        const long k = strtol(v.c_str(), nullptr, 10);
        if (k >= 0 && k < (long)ch.size()) r = ch[(size_t)k].ref;
      }
    }
    auto fs = h.fields ? h.fields() : std::vector<PresetField>();
    for (const auto& f : fs) {                          // every setting, within its range
      auto it = args.find(f.key);
      if (it == args.end()) continue;
      float v = (float)atof(it->second.c_str());
      if (!std::isfinite(v)) continue;
      if (f.toggle) v = v > 0.5f ? 1.0f : 0.0f;
      else if (!f.options.empty()) v = (float)std::max(0L, std::min((long)f.options.size() - 1, std::lround(v)));
      else v = std::max(f.min, std::min(f.max, v));
      bool set = false;
      for (auto& kv : p.values) if (kv.first == f.key) { kv.second = v; set = true; }
      if (!set) p.values.emplace_back(f.key, v);
    }
    if (!Presets::save(p, true, &err)) { back(out, err, "/presets/edit?name=" + urlenc(name)); return true; }
    if (h.edited) h.edited();
    if (arg(args, "apply") == "1") {
      err = h.load ? h.load(name) : "Not available";
      back(out, err.empty() ? "Saved and loaded \"" + name + "\"." : err, "/presets/edit?name=" + urlenc(name));
    } else {
      const bool in_use = h.current && h.current() == name;
      back(out, std::string("Saved \"") + name + "\"." + (in_use ? " Load it to hear the changes." : ""),
           "/presets/edit?name=" + urlenc(name));
    }
  } else {
    out->status = "404 Not Found"; out->type = "text/plain"; out->body = "not found";
  }
  return true;
}

}  // namespace mpcnam
