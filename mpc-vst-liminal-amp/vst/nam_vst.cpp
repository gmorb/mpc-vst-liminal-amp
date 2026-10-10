// SPDX-License-Identifier: MIT
// Copyright (c) 2026 the mpc-vst-liminal-amp contributors
/* nam_vst.cpp -- Liminal Amp (a NAM A2 head + IR cab) as a Linux VST2 effect for MPC OS's plugin host (mpc-vst-liminal-amp).
 *
 * Hand-written VST2 ABI (no Steinberg SDK), parameter list from vst/params.json (params.h, made by mpc-vst-plugins'
 * gen_vst.py; the order is the VST index and never changes).
 *
 * Threads:
 *   - the host's audio thread calls processReplacing(): only Engine::process() and flag reads; no locks, no
 *     allocation, no file access;
 *   - the host's other threads call setParameter()/dispatcher(): they update the engine's atomic parameters and
 *     the browser requests (under `mu`, never taken by the audio thread);
 *   - one worker thread per instance scans the drives (Library) and loads models and IRs, then hands them to the
 *     engine, which swaps them in at the start of an audio block; the one replaced is freed by this thread again.
 *     A new file is loaded while the old one keeps playing, so browsing never interrupts the sound.
 * State is saved as text: every parameter, and the chosen model and IR by path, relative path and file name, so a
 * project finds them again when the card or folder moved (Library::find).
 */
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "library.h"
#include "nam_engine.h"
#include "params.h"
#include "presets.h"
#include "presets_web.h"
#include "tone3000.h"

using namespace mpcnam;

// ---- VST2 ABI (hand-written; no Steinberg SDK) ------------------------------------------------------------------
struct AEffect;
typedef intptr_t (*audioMasterCallback)(AEffect*, int32_t, int32_t, intptr_t, void*, float);
struct AEffect {
  int32_t magic;
  intptr_t (*dispatcher)(AEffect*, int32_t, int32_t, intptr_t, void*, float);
  void (*process)(AEffect*, float**, float**, int32_t);
  void (*setParameter)(AEffect*, int32_t, float);
  float (*getParameter)(AEffect*, int32_t);
  int32_t numPrograms, numParams, numInputs, numOutputs, flags;
  intptr_t resvd1, resvd2;
  int32_t initialDelay, realQualities, offQualities;
  float ioRatio;
  void *object, *user;
  int32_t uniqueID, version;
  void (*processReplacing)(AEffect*, float**, float**, int32_t);
  void (*processDoubleReplacing)(AEffect*, double**, double**, int32_t);
  char future[56];
};
enum {
  effOpen = 0, effClose = 1, effGetParamLabel = 6, effGetParamDisplay = 7, effGetParamName = 8,
  effSetSampleRate = 10, effSetBlockSize = 11, effMainsChanged = 12, effGetChunk = 23, effSetChunk = 24,
  effCanBeAutomated = 26, effGetPlugCategory = 35, effGetEffectName = 45, effGetVendorString = 47,
  effGetProductString = 48, effGetVendorVersion = 49, effCanDo = 51, effGetVstVersion = 58,
};
enum { audioMasterAutomate = 0, audioMasterUpdateDisplay = 42 };
enum { effFlagsCanReplacing = 1 << 4, effFlagsProgramChunks = 1 << 5 };
enum { kPlugCategEffect = 1 };

// ---- parameters (vst/params.json order) ----------------------------------------------------------------------------
enum {
  P_INPUT, P_GATE_ON, P_GATE, P_BASS, P_MID, P_TREBLE, P_EQ_ON, P_OUTPUT, P_NORMALIZE, P_IR_ON, P_IR_LEN,
  P_MODEL, P_MODEL_PREV, P_MODEL_NEXT, P_MODEL_INFO, P_IR, P_IR_PREV, P_IR_NEXT, P_IR_INFO, P_RESCAN,
  P_GET_TONES, P_T3K_INFO,                    // 1.0.0: TONE3000 (appended: indices never move)
  P_ECO,                                      // 1.0.0: no resampling, lower CPU (off by default)
  // 1.0.0: the AMP and CAB blocks: pack (folder / TONE3000 tone) line, pack arrows, source (Local/TONE3000: shows
  // the T3K mark), Browse TONE3000 (the entry point, per TONE3000's design requirements)
  P_MODEL_PACK, P_MODEL_PACK_PREV, P_MODEL_PACK_NEXT, P_MODEL_SRC, P_MODEL_BROWSE,
  P_IR_PACK, P_IR_PACK_PREV, P_IR_PACK_NEXT, P_IR_SRC, P_IR_BROWSE,
  P_MODEL_DELETE, P_IR_DELETE,                  // 1.0.0: DELETE (two taps: deletes the file permanently)
  P_FULL,                                       // 1.0.0: an A2 capture's Full size instead of Lite (off by default)
  // 1.0.0: the AMP LIST and CAB LIST laid over their blocks (as Liminal Hz's IR List): open/closed state (read-only),
  // the open and close buttons, the page buttons, the title ("Amps 7-12 of 83"), and six rows (text = "pack . name";
  // a tap on a row loads that capture / IR; value 1 = the one in use)
  P_AMP_MENU, P_AMP_LIST, P_AMP_CLOSE, P_AMP_PREV, P_AMP_NEXT, P_AMP_TITLE, P_AMP_ROW1,
  P_AMP_ROW_LAST = P_AMP_ROW1 + 5,
  P_CAB_MENU, P_CAB_LIST, P_CAB_CLOSE, P_CAB_PREV, P_CAB_NEXT, P_CAB_TITLE, P_CAB_ROW1,
  P_CAB_ROW_LAST = P_CAB_ROW1 + 5,
  // 1.0.0: the tubes' glow (a read-only display: the amp's output level, 0 = idle .. 24 = full; the page's tube picture)
  P_TUBES,
  // 1.0.0: the amp's pluck print (read-only display): one plucked note through the loaded capture, 14 time slices
  // (Model::pluck_print: 0 silent, 1..24 = texture * 8 + level + 1); the page's waveform pictures under the amp's name
  P_WAVE1, P_WAVE_LAST = P_WAVE1 + 13,
  // 1.0.0: My Presets (as Liminal Hz): the header's preset name (read-only), MY PRESETS (opens the list over the AMP
  // block: amp_menu = 2, sharing the AMP LIST's title, page buttons and rows), SAVE, DELETE (two taps), NAME ON PHONE
  P_PRESET, P_MP_LIST, P_MP_SAVE, P_MP_DELETE, P_MP_PHONE,
  P_COUNT
};
static_assert(P_COUNT == NPARAMS, "vst/params.json and nam_vst.cpp disagree on the parameter list");
static const size_t kShortIR = 4096;        // "IR Length: Short" (about 93 ms at 44.1 kHz)
static const size_t kText = 24;             // display text the host gets (as mpc-vst-plugins' wrapper)
static const size_t kTextLong = 64;         // names, packs, details: MPC's host takes 64 (as jacob-sabella's NAM,
                                            // used on devices); still well inside JUCE's 256-byte buffer

// ---- one browser (models or IRs) -------------------------------------------------------------------------------------
struct Browser {
  const char* folder;                       // "models" / "irs"
  const char* ext;                          // ".nam" / ".wav"
  std::vector<LibEntry> lib;                // from the last scan
  int want = -1;                            // the entry asked for (host thread)
  LibEntry loaded;                          // what the engine has (worker)
  LibEntry saved;                           // restored from a project, waiting for a scan to find it
  bool loading = false;
  std::string error;                        // last load error, shown in the info line
  // DELETE: the first tap arms it for this file (kArmSeconds), a second tap on the same file deletes it
  std::string armed_path;
  std::chrono::steady_clock::time_point armed_until, notice_until;
  bool delete_req = false;
  std::string notice;                       // "Deleted", for a few seconds
  // the list laid over this block (AMP LIST / CAB LIST)
  int which = 0;                            // 0 models (AMP), 1 IRs (CAB)
  std::atomic<int> menu_open{0};
  int menu_page = 0;                        // under mu
  long marq_key = -1;                       // long names slide (see marq_*): what the slide is timing
  std::chrono::steady_clock::time_point marq_t0;
  int marq_sent = 0;
};
static const int kArmSeconds = 4;
static const int kFullArmSeconds = 6;           // A2 FULL: time to read the warning and tap again

// ---- My Presets ------------------------------------------------------------------------------------------------------
struct MyPresets {                          // all under mu
  std::vector<PresetData> list;             // the presets folder, as last read (worker)
  int want = -1;                            // the selected one in list, or -1
  std::string sel_name;                     // the selected preset's name (kept by a project)
  bool active = false;                      // the sound is that preset's (shown in the header)
  bool refresh = true, save_req = false, listed = false;
  PresetData pending;                       // SAVE: the snapshot to write (worker)
  std::string delete_name, armed_name, notice, error;
  std::chrono::steady_clock::time_point armed_until, notice_until;
};

struct Inst {
  AEffect fx;
  audioMasterCallback master = nullptr;
  std::unique_ptr<Engine> eng;
  double rate = 44100.0;
  int block = 512;
  std::atomic<float> norm[P_COUNT];         // host-visible values of the plain parameters (written by the host,
                                            // read by the audio thread and the worker: atomic)
  std::mutex mu;                            // browsers, requests; never taken by the audio thread
  std::condition_variable cv;
  bool stop = false, scan_req = true, work_req = false, scanned = false, scanning = true;
  size_t ir_len_loaded = 0;
  int full_loaded = -1;                         // the size of the capture in the engine (worker): 0 Lite, 1 Full
  std::chrono::steady_clock::time_point full_armed_until{};   // A2 FULL: the first tap warns, a second within kFullArmSeconds turns it on (mu)
  bool model_slimmable = false;                 // the loaded capture is an A2 file (it has both sizes)
  Browser model{"models", ".nam"}, ir{"irs", ".wav"};
  MyPresets mp;
  std::thread worker;
  std::atomic<int> changed[P_COUNT];        // report the value of these to the host (audio thread)
  std::atomic<int> update_display{0};
  float tube_env = 0;                       // audio thread: the output's peak, held and released (tube-like)
  int tube_sent = 0, tube_age = 0;          // audio thread: the step last reported to the host, blocks since
  std::atomic<int> tube_step{0};            // the step the page shows (read by the host's display calls)
  std::atomic<int> wave[Model::kWaveCols];  // the amp's pluck print (worker writes, host reads)
  std::string chunk;
  std::unique_ptr<Tone3000> t3k;            // made on the first GET TONES
  std::string select_after_scan;            // a file TONE3000 just delivered: select it once the rescan finds it
  std::string t3k_seen;                     // the TONE3000 line last shown (worker)
};

static float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
static bool ieq(const std::string& a, const std::string& b) {   // names compared as a FAT card does
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++) if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
  return true;
}

static float real_of(int i, float n) {
  const param_t& p = PARAMS[i];
  if (p.nopts > 1) return (float)lroundf(clamp01(n) * (p.nopts - 1));
  return p.min + (p.max - p.min) * clamp01(n);
}

static void apply_param(Inst* w, int i) {        // plain parameter -> engine (any thread; atomics)
  if (!w->eng) return;
  Params& e = w->eng->params;
  const float v = real_of(i, w->norm[i]);
  switch (i) {
    case P_INPUT: e.input_db = v; break;
    case P_GATE_ON: e.gate_on = v > 0.5f; break;
    case P_GATE: e.gate_db = v; break;
    case P_BASS: e.bass = v; break;
    case P_MID: e.mid = v; break;
    case P_TREBLE: e.treble = v; break;
    case P_EQ_ON: e.eq_on = v > 0.5f; break;
    case P_OUTPUT: e.output_db = v; break;
    case P_NORMALIZE: e.normalize = v > 0.5f; break;
    case P_IR_ON: e.ir_on = v > 0.5f; break;
    case P_ECO: e.eco = v > 0.5f; break;
    default: break;
  }
}

static Browser* browser_of(Inst* w, int i) {
  if (i == P_MODEL || i == P_MODEL_PREV || i == P_MODEL_NEXT || i == P_MODEL_INFO || i == P_MODEL_PACK ||
      i == P_MODEL_PACK_PREV || i == P_MODEL_PACK_NEXT || i == P_MODEL_SRC || i == P_MODEL_BROWSE ||
      i == P_MODEL_DELETE) return &w->model;
  if (i == P_IR || i == P_IR_PREV || i == P_IR_NEXT || i == P_IR_INFO || i == P_IR_PACK ||
      i == P_IR_PACK_PREV || i == P_IR_PACK_NEXT || i == P_IR_SRC || i == P_IR_BROWSE ||
      i == P_IR_DELETE) return &w->ir;
  return nullptr;
}

static bool is_src(int i) { return i == P_MODEL_SRC || i == P_IR_SRC; }

static float src_norm(const Browser& b) {          // caller holds mu: 1 = the current file is a TONE3000 tone
  return (b.want >= 0 && b.want < (int)b.lib.size() && b.lib[b.want].t3k) ? 1.0f : 0.0f;
}

static void browser_changed(Inst* w, const Browser* b) {   // tell the host: the selector and its T3K mark
  w->changed[b == &w->model ? P_MODEL : P_IR] = 1;
  w->changed[b == &w->model ? P_MODEL_SRC : P_IR_SRC] = 1;
}

static float browser_norm(const Browser& b) {    // caller holds mu
  const int n = (int)b.lib.size();
  int k = b.want;
  if (k < 0 || n < 2) return 0.0f;
  return (float)k / (float)(n - 1);
}

// ---- the AMP LIST / CAB LIST laid over a block (as Liminal Hz's IR List) ------------------------------------------------
// Six rows a page, "pack . name"; a tap on a row loads it like the arrows do, and the list stays open so several can be
// tried one after another. All of these hold mu. The page draws the rows as live text; a tap on a row is a parameter write.
static const int kRows = 4;                    // rows shown (44 px touch targets); params row_5/row_6 stay unused
static const int kMpRows = 3;                  // My Presets (over the AMP block: menu_open 2): rows, then SAVE / DELETE / NAME ON PHONE
struct MenuIdx { int menu, list, close, prev, next, title, row1; };
static const MenuIdx MI[2] = {{P_AMP_MENU, P_AMP_LIST, P_AMP_CLOSE, P_AMP_PREV, P_AMP_NEXT, P_AMP_TITLE, P_AMP_ROW1},
                              {P_CAB_MENU, P_CAB_LIST, P_CAB_CLOSE, P_CAB_PREV, P_CAB_NEXT, P_CAB_TITLE, P_CAB_ROW1}};
static bool is_menu(int i) { return i >= P_AMP_MENU && i < P_TUBES; }
static const int kTubeSteps = 24;
static bool is_wave(int i) { return i >= P_WAVE1 && i <= P_WAVE_LAST; }
static bool is_display(int i) { return i == P_TUBES || is_wave(i) || i == P_PRESET; }   // read-only: never saved or automated
static bool is_mp(int i) { return i >= P_MP_LIST && i <= P_MP_PHONE; }   // My Presets buttons (momentary)
static int menu_which(int i) { return i >= P_CAB_MENU ? 1 : 0; }                     // which block's list
static bool is_row(int i) { return (i >= P_AMP_ROW1 && i <= P_AMP_ROW_LAST) || (i >= P_CAB_ROW1 && i <= P_CAB_ROW_LAST); }
// the list over a block shows its files (menu_open 1) or, over the AMP block, My Presets (menu_open 2); all hold mu
static bool mp_mode(const Browser& b) { return b.menu_open.load() == 2; }
static int menu_rows(const Browser& b) { return mp_mode(b) ? kMpRows : kRows; }
static int menu_count(const Inst* w, const Browser& b) { return mp_mode(b) ? (int)w->mp.list.size() : (int)b.lib.size(); }
static int menu_selected(const Inst* w, const Browser& b) { return mp_mode(b) ? w->mp.want : b.want; }
static int menu_pages(const Inst* w, const Browser& b) { return std::max(1, (menu_count(w, b) + menu_rows(b) - 1) / menu_rows(b)); }
static void menu_clamp(const Inst* w, Browser& b) { b.menu_page = std::max(0, std::min(b.menu_page, menu_pages(w, b) - 1)); }
static void menu_goto_selected(const Inst* w, Browser& b) {     // the page that holds the item in use
  const int sel = menu_selected(w, b);
  b.menu_page = sel >= 0 ? sel / menu_rows(b) : 0;
  menu_clamp(w, b);
}

// ---- long names scroll: the page shows a window of the text, and it slides until the end shows, then stays ----------
// The live text can't be measured, so widths are estimated per character (Titillium Web at the row's size): the window
// is what fits in kMarqPx. A name that fits never moves.
static const int kMarqPx = 540;
static int glyph_px(unsigned char c) {
  if (c >= 0x80) return 13;
  if (strchr("il.,:;|!' ", c) || c == 'I' || c == 'j') return c == ' ' ? 6 : 7;
  if (strchr("ftr()[]-/", c)) return 9;
  if (c == 'm' || c == 'w') return 20;
  if (c == 'M' || c == 'W') return 25;
  if (c >= 'A' && c <= 'Z') return 16;
  return 13;
}
static void marq_cells(const std::string& s, std::vector<size_t>& off, std::vector<int>& px) {   // per UTF-8 character
  off.clear(); px.clear();
  for (size_t i = 0; i < s.size(); i++) {
    const unsigned char c = (unsigned char)s[i];
    if ((c & 0xC0) == 0x80) continue;
    off.push_back(i);
    px.push_back(c == 0xC2 && i + 1 < s.size() && (unsigned char)s[i + 1] == 0xB7 ? 7 : glyph_px(c));   // the "\xC2\xB7" dot
  }
}
static int marq_max_step(const std::string& s) {     // characters to slide so that the end shows (0: it all fits)
  std::vector<size_t> off; std::vector<int> px;
  marq_cells(s, off, px);
  int tot = 0; for (int v : px) tot += v;
  int k = 0;
  while (tot > kMarqPx && k < (int)px.size()) tot -= px[k++];
  return k;
}
static std::string marq_window(const std::string& s, int step) {
  const int mx = marq_max_step(s);
  if (mx == 0) return s;
  std::vector<size_t> off; std::vector<int> px;
  marq_cells(s, off, px);
  const size_t k = (size_t)std::min(step, mx);
  return s.substr(off[k]);                           // (at the end of the slide the rest fits; before it, the box clips)
}
// the step the scrolling is at: 0 for 1.5 s after a page appears, then one character every 250 ms (holds mu)
static int marq_step_now(const Inst* w, Browser& b) {
  const long key = (long)b.menu_open.load() * 100000000L + (long)b.menu_page * 1000L + (long)menu_count(w, b);
  const auto now = std::chrono::steady_clock::now();
  if (key != b.marq_key) { b.marq_key = key; b.marq_t0 = now; b.marq_sent = 0; }
  const long ms = (long)std::chrono::duration_cast<std::chrono::milliseconds>(now - b.marq_t0).count();
  return ms < 1500 ? 0 : (int)((ms - 1500) / 250) + 1;
}

static std::string menu_row_text(const Inst* w, const Browser& b, int idx) {   // a row's whole text, "Pack \xC2\xB7 name" (holds mu)
  if (mp_mode(b)) return w->mp.list[idx].name;
  const LibEntry& e = b.lib[idx];
  const size_t sl = e.display.find_last_of('/');
  const std::string stem = sl == std::string::npos ? e.display : e.display.substr(sl + 1);
  std::string pack = e.t3k && !e.t3k_title.empty() ? e.t3k_title : e.pack;
  const size_t ps = pack.find_last_of('/');
  if (ps != std::string::npos) pack = pack.substr(ps + 1);
  return pack.empty() ? stem : pack + " \xC2\xB7 " + stem;
}
// the worker's tick: while a page has a long name still sliding, the rows are drawn again at each new step (holds mu)
static void marq_tick(Inst* w, Browser& b) {
  if (!b.menu_open.load()) return;
  const int step = marq_step_now(w, b), n = menu_count(w, b), rows = menu_rows(b);
  int mx = 0;
  for (int r = 0; r < rows; r++) {
    const int idx = b.menu_page * rows + r;
    if (idx < n) mx = std::max(mx, marq_max_step(menu_row_text(w, b, idx)));
  }
  const int shown = std::min(step, mx);
  if (shown != b.marq_sent) { b.marq_sent = shown; w->update_display = 1; }
}
static bool menu_row_on(const Inst* w, const Browser& b, int r) {   // the row shows the item in use (holds mu)
  const int rows = menu_rows(b), idx = b.menu_page * rows + r;
  return r < rows && b.menu_open.load() && idx < menu_count(w, b) && idx == menu_selected(w, b);
}

// ---- worker thread -------------------------------------------------------------------------------------------------
static void requested(Browser& b) {             // holds mu: show "Loading..." from the moment of the request
  b.loading = b.want >= 0 && b.want < (int)b.lib.size() && b.lib[b.want].path != b.loaded.path;
}

static void resolve(Browser& b) {                // after a scan: find the saved or loaded entry again (holds mu)
  const LibEntry& s = !b.saved.path.empty() || !b.saved.file.empty() ? b.saved : b.loaded;
  int k = (s.path.empty() && s.file.empty()) ? -1 : Library::find(b.lib, s.path, s.display, s.file);
  if (k >= 0) {
    b.want = k;
    b.saved = LibEntry();
  } else if (!b.saved.file.empty()) {
    b.want = -1;                                 // a project's file isn't on this device (yet): keep asking
    b.error = "Not found: " + b.saved.file;
  } else if (b.want >= (int)b.lib.size()) {
    b.want = b.lib.empty() ? -1 : 0;
  }
  if (b.want < 0 && b.saved.file.empty() && b.loaded.path.empty() && !b.lib.empty()) b.want = 0;   // first use
}

static void set_print(Inst* w, const int* st) {   // the amp's pluck print -> the page (nullptr: no amp)
  for (int c = 0; c < Model::kWaveCols; c++) {
    const int v = st ? st[c] : 0;
    if (w->wave[c].exchange(v) != v) w->changed[P_WAVE1 + c] = 1;
  }
}

static void load_one(Inst* w, Browser& b, bool is_model) {
  std::unique_lock<std::mutex> lk(w->mu);
  const size_t ir_len = real_of(P_IR_LEN, w->norm[P_IR_LEN]) > 0.5f ? kShortIR : IRConvolver::kMaxLength;
  if (b.want < 0 || b.want >= (int)b.lib.size()) { b.loading = false; return; }
  const LibEntry e = b.lib[b.want];
  const int full = real_of(P_FULL, w->norm[P_FULL]) > 0.5f ? 1 : 0;
  const bool same = e.path == b.loaded.path && (is_model ? full == w->full_loaded : ir_len == w->ir_len_loaded);
  if (same) { b.loading = false; return; }
  b.loading = true;
  const double rate = w->rate;
  const int block = w->block;
  w->update_display = 1;
  lk.unlock();

  std::string err;
  std::unique_ptr<Model> m;
  std::unique_ptr<IRConvolver> c;
  int print[Model::kWaveCols] = {0};
  if (is_model) {
    m = Model::load(e.path, rate, block, &err, full == 1);
    if (m) m->pluck_print(print);               // before the audio thread has it (it resets the model after)
  } else
    c = IRConvolver::load_wav(e.path, rate, &err, ir_len);

  lk.lock();
  b.loading = false;
  if (!w->eng || w->rate != rate || w->block != block) return;        // the engine was rebuilt meanwhile: retry
  if (is_model ? !m : !c) {
    // engine/wav_reader's reasons are short and readable ("Unsupported bit depth", "Not a WAV file", ...)
    if (!is_model && !err.empty() && err.size() < 32) b.error = err;
    else b.error = "Can't load " + e.file;
    b.loaded = LibEntry();
    b.loaded.path = e.path;                      // don't retry the same broken file in a loop
    if (is_model) { w->eng->set_model(nullptr); set_print(w, nullptr); } else w->eng->set_ir(nullptr);
  } else {
    b.error.clear();
    b.loaded = e;
    if (is_model) {
      w->model_slimmable = m->slimmable();
      w->full_loaded = full;
      w->eng->set_model(std::move(m));
      set_print(w, print);
    }
    else { w->eng->set_ir(std::move(c)); w->ir_len_loaded = ir_len; }
  }
  w->update_display = 1;
}

static void worker_main(Inst* w) {
  for (;;) {
    bool scan;
    {
      std::unique_lock<std::mutex> lk(w->mu);
      w->cv.wait_for(lk, std::chrono::milliseconds(250), [w] { return w->stop || w->scan_req || w->work_req; });
      if (w->stop) return;
      scan = w->scan_req;
      w->scan_req = w->work_req = false;
      if (scan) { w->scanning = true; w->update_display = 1; }
    }
    if (scan) {
      // Publish a scan's result: the lists, the saved or loaded files found again, a TONE3000 download selected.
      auto publish = [&](const std::vector<LibEntry>& models, const std::vector<LibEntry>& irs) {
        std::lock_guard<std::mutex> lk(w->mu);
        w->model.lib = models;
        w->ir.lib = irs;
        resolve(w->model);
        resolve(w->ir);
        if (!w->select_after_scan.empty()) {   // a TONE3000 download: select it in whichever list has it
          for (Browser* b : {&w->model, &w->ir}) {
            int k = Library::find(b->lib, w->select_after_scan, "", "");
            if (k >= 0) { b->want = k; b->saved = LibEntry(); b->error.clear(); w->changed[b == &w->model ? P_MODEL : P_IR] = 1; }
          }
          w->select_after_scan.clear();
        }
        requested(w->model);
        requested(w->ir);
        menu_clamp(w, w->model);
        menu_clamp(w, w->ir);
        w->scanned = true;
        w->scanning = false;
        browser_changed(w, &w->model);            // positions may have moved: tell the host
        browser_changed(w, &w->ir);
        w->update_display = 1;
      };
      // 1. Quick: the models/ and irs/ folders found last time (plus the plugin's own), no walk of the cards. If it
      //    already holds what is waited for (the saved capture and IR, a fresh TONE3000 download) the page is usable
      //    and the sound loads at once; if not, "Searching..." stays until the walk answers.
      std::vector<LibEntry> qm, qi;
      bool published_quick = false;
      if (Library::scan_quick(w->model.folder, w->model.ext, &qm) && Library::scan_quick(w->ir.folder, w->ir.ext, &qi)) {
        bool complete = true;
        {
          std::lock_guard<std::mutex> lk(w->mu);
          for (Browser* b : {&w->model, &w->ir}) {
            const std::vector<LibEntry>& q = b == &w->model ? qm : qi;
            const LibEntry& sv = !b->saved.path.empty() || !b->saved.file.empty() ? b->saved : b->loaded;
            if (!(sv.path.empty() && sv.file.empty()) && Library::find(q, sv.path, sv.display, sv.file) < 0) complete = false;
          }
          if (!w->select_after_scan.empty() && Library::find(qm, w->select_after_scan, "", "") < 0 &&
              Library::find(qi, w->select_after_scan, "", "") < 0) complete = false;
        }
        if (complete) {
          publish(qm, qi);
          published_quick = true;
          load_one(w, w->model, true);            // the saved amp and cab sound before the walk of the cards ends
          load_one(w, w->ir, false);
        }
      }
      // 2. The walk of the cards (it also refreshes the folder indexes for next time). Publishes again only if it found
      //    something different from the quick pass, so a list being browsed doesn't jump for nothing.
      const std::vector<LibEntry> models = Library::scan(w->model.folder, w->model.ext);
      const std::vector<LibEntry> irs = Library::scan(w->ir.folder, w->ir.ext);
      auto same_paths = [](const std::vector<LibEntry>& a, const std::vector<LibEntry>& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); i++) if (a[i].path != b[i].path) return false;
        return true;
      };
      if (!(published_quick && same_paths(models, qm) && same_paths(irs, qi))) publish(models, irs);
    }
    {                                            // My Presets: read the folder, save the asked-for sound, delete
      bool refresh, save;
      PresetData pend;
      std::string del;
      {
        std::lock_guard<std::mutex> lk(w->mu);
        MyPresets& m = w->mp;
        refresh = m.refresh || scan;             // (RESCAN also picks up presets renamed or deleted by hand)
        save = m.save_req; del = m.delete_name; pend = m.pending;
        m.refresh = m.save_req = false;
        m.delete_name.clear();
      }
      if (refresh || save || !del.empty()) {
        std::string err, notice, select;
        if (!del.empty()) { if (Presets::remove(del, &err)) { notice = "Deleted"; err.clear(); } }
        if (save) {
          const std::string name = Presets::unique_name(pend.name);   // named from its amp, numbered if taken
          pend.name = name;
          if (Presets::save(pend, false, &err)) { notice = "Saved"; select = name; err.clear(); }
        }
        std::vector<PresetData> l = Presets::list();
        std::lock_guard<std::mutex> lk(w->mu);
        MyPresets& m = w->mp;
        m.list = std::move(l);
        m.listed = true;
        if (!select.empty()) { m.sel_name = select; m.active = true; }   // a new preset is the selected one
        if (!del.empty() && ieq(m.sel_name, del)) { m.sel_name.clear(); m.active = false; }
        m.want = -1;
        for (int k = 0; k < (int)m.list.size(); k++) if (!m.sel_name.empty() && ieq(m.list[k].name, m.sel_name)) m.want = k;
        if (!notice.empty()) { m.notice = notice; m.notice_until = std::chrono::steady_clock::now() + std::chrono::seconds(3); m.error.clear(); }
        else if (!err.empty()) { m.error = err; m.notice_until = std::chrono::steady_clock::now() + std::chrono::seconds(4); }
        if (!select.empty() && mp_mode(w->model) && m.want >= 0) w->model.menu_page = m.want / kMpRows;   // show it
        menu_clamp(w, w->model);
        w->changed[P_PRESET] = 1;
        w->update_display = 1;
      }
    }
    for (Browser* b : {&w->model, &w->ir}) {    // DELETE: delete the file (permanently), then rescan
      LibEntry victim;
      {
        std::lock_guard<std::mutex> lk(w->mu);
        if (!b->delete_req) continue;
        b->delete_req = false;
        if (b->want < 0 || b->want >= (int)b->lib.size()) continue;
        victim = b->lib[b->want];
      }
      std::string err;
      const bool ok = Library::remove(victim, &err);       // permanent
      std::lock_guard<std::mutex> lk(w->mu);
      if (ok) {
        b->notice = "Deleted";
        b->notice_until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        b->error.clear();
        b->loaded = LibEntry();                 // the next scan lands on the next file (same position)
        w->scan_req = true;
        w->work_req = true;
        w->scanning = true;
      } else {
        b->error = err;
      }
      w->update_display = 1;
    }
    {
      std::lock_guard<std::mutex> lk(w->mu);   // an armed DELETE or a notice ran out: refresh the info line
      const auto now = std::chrono::steady_clock::now();
      for (Browser* b : {&w->model, &w->ir}) {
        if (!b->armed_path.empty() && now >= b->armed_until) { b->armed_path.clear(); w->update_display = 1; }
        if (!b->notice.empty() && now >= b->notice_until) { b->notice.clear(); w->update_display = 1; }
        marq_tick(w, *b);                        // an open list with a long name still sliding: draw its rows again
      }
      if (w->full_armed_until != std::chrono::steady_clock::time_point{} && now >= w->full_armed_until) {
        w->full_armed_until = {}; w->update_display = 1;   // the A2 FULL warning ran out: the lines go back
      }
      MyPresets& m = w->mp;
      if (!m.armed_name.empty() && now >= m.armed_until) { m.armed_name.clear(); w->update_display = 1; }
      if ((!m.notice.empty() || !m.error.empty()) && now >= m.notice_until) { m.notice.clear(); m.error.clear(); w->update_display = 1; }
    }
    load_one(w, w->model, true);
    load_one(w, w->ir, false);
    std::lock_guard<std::mutex> lk(w->mu);
    if (w->eng) w->eng->collect();               // free what the audio thread has let go of
    if (w->t3k) {                                // TONE3000's line changed (its own thread): refresh the page. MPC
      std::string st = w->t3k->status();         // re-polls value text by itself on a Force (mpc-vst-plugins NOTES),
      if (st != w->t3k_seen) { w->t3k_seen = st; w->update_display = 1; }   // this makes sure anywhere
    }
  }
}

static void kick(Inst* w, bool scan) {           // caller may hold mu or not
  w->scan_req = w->scan_req || scan;
  w->work_req = true;
  w->cv.notify_one();
}

// ---- engine lifetime (never while the host is processing: from effOpen / effMainsChanged(off->on)) --------------
static void rebuild_engine(Inst* w) {
  std::lock_guard<std::mutex> lk(w->mu);
  w->eng.reset(new Engine(w->rate, w->block));
  for (int i = 0; i < P_COUNT; i++) apply_param(w, i);
  w->model.loaded = LibEntry();                  // the new engine has nothing loaded: load both again
  w->ir.loaded = LibEntry();
  w->ir_len_loaded = 0;
  w->full_loaded = -1;
  kick(w, false);
}

// ---- VST callbacks -------------------------------------------------------------------------------------------------
static std::string stem_of(const LibEntry& e);   // below

// ---- My Presets: what a preset keeps, taking it, giving it back --------------------------------------------------------
static const int kPresetKeys[] = {P_INPUT, P_GATE_ON, P_GATE, P_BASS, P_MID, P_TREBLE, P_EQ_ON, P_OUTPUT, P_NORMALIZE,
                                  P_IR_ON, P_IR_LEN, P_ECO, P_FULL};
static void set_param(Inst* w, int32_t i, float v, bool trusted);   // below (trusted: a preset, not a tap)
static void set_real(Inst* w, int i, float r) {   // a plain parameter by its real value, as the host would set it
  const param_t& p = PARAMS[i];
  float n = p.nopts > 1 ? (float)std::max(0L, std::min((long)p.nopts - 1, std::lround(r))) / (p.nopts - 1)
                        : (p.max > p.min ? clamp01((r - p.min) / (p.max - p.min)) : 0.0f);
  set_param(w, i, n, true);                      // a preset keeps what it was saved with (no warning)
  w->changed[i] = 1;                             // the host shows the new value
}
static PresetRef ref_of(const LibEntry& e) { PresetRef r; r.path = e.path; r.display = e.display; r.file = e.file; return r; }
static const LibEntry* in_use(const Browser& b) {  // the file playing, else the one asked for (holds mu)
  for (const LibEntry& e : b.lib) if (!b.loaded.path.empty() && e.path == b.loaded.path) return &e;
  return b.want >= 0 && b.want < (int)b.lib.size() ? &b.lib[b.want] : nullptr;
}
// the sound as it is now (holds mu): the settings a preset keeps, the amp and the cab; named after the amp
static PresetData mp_snapshot(Inst* w) {
  PresetData p;
  for (int k : kPresetKeys) p.values.emplace_back(PARAMS[k].key, real_of(k, w->norm[k]));
  const LibEntry* m = in_use(w->model);
  const LibEntry* c = in_use(w->ir);
  if (m) p.model = ref_of(*m);
  if (c) p.ir = ref_of(*c);
  p.name = m ? stem_of(*m) : c ? stem_of(*c) : std::string("My Preset");
  return p;
}
// a preset's settings, then its amp and cab (found again as a project finds them; asked for after the first scan)
static void mp_apply(Inst* w, const PresetData& p) {
  for (const auto& kv : p.values)
    for (int k : kPresetKeys)
      if (kv.first == PARAMS[k].key) set_real(w, k, kv.second);
  std::lock_guard<std::mutex> lk(w->mu);
  std::string missing;
  for (int which = 0; which < 2; which++) {
    const PresetRef& r = which == 0 ? p.model : p.ir;
    if (r.empty()) continue;
    Browser& b = which == 0 ? w->model : w->ir;
    const int k = Library::find(b.lib, r.path, r.display, r.file);
    if (k >= 0) { b.want = k; b.saved = LibEntry(); b.error.clear(); }
    else if (!w->scanned) { b.want = -1; b.saved.path = r.path; b.saved.display = r.display; b.saved.file = r.file; b.error.clear(); }
    else missing = (which == 0 ? "Amp not found: " : "Cab not found: ") + (r.file.empty() ? std::string("?") : r.file);
    requested(b);
    browser_changed(w, &b);
  }
  if (!missing.empty()) { w->mp.error = missing; w->mp.notice_until = std::chrono::steady_clock::now() + std::chrono::seconds(5); }
  kick(w, false);
  w->update_display = 1;
}
static void mp_choose(Inst* w, int idx) {        // select list[idx] and apply it
  PresetData p;
  {
    std::lock_guard<std::mutex> lk(w->mu);
    MyPresets& m = w->mp;
    if (idx < 0 || idx >= (int)m.list.size()) return;
    m.want = idx; m.sel_name = m.list[idx].name; m.active = true; m.error.clear(); m.notice.clear(); m.armed_name.clear();
    p = m.list[idx];
    w->changed[P_PRESET] = 1;
    w->update_display = 1;
  }
  mp_apply(w, p);
}

// what the phone pages ask of the plugin (the server's thread; each takes mu itself)
static PresetsWebHooks mp_hooks(Inst* w) {
  PresetsWebHooks h;
  h.save = [w](const std::string& name, bool overwrite) -> std::string {
    PresetData p;
    {
      std::lock_guard<std::mutex> lk(w->mu);
      p = mp_snapshot(w);
    }
    p.name = name;
    std::string err;
    if (!Presets::save(p, overwrite, &err)) return err;
    std::lock_guard<std::mutex> lk(w->mu);
    MyPresets& m = w->mp;
    m.sel_name = name; m.active = true; m.refresh = true;
    m.notice = "Saved"; m.notice_until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    w->changed[P_PRESET] = 1;
    kick(w, false);
    w->update_display = 1;
    return "";
  };
  h.load = [w](const std::string& name) -> std::string {
    PresetData p;
    if (!Presets::load(name, &p)) return "Preset not found";
    {
      std::lock_guard<std::mutex> lk(w->mu);
      MyPresets& m = w->mp;
      m.sel_name = p.name; m.active = true; m.refresh = true; m.error.clear();
      m.want = -1;
      for (int k = 0; k < (int)m.list.size(); k++) if (ieq(m.list[k].name, p.name)) m.want = k;
      w->changed[P_PRESET] = 1;
      kick(w, false);
    }
    mp_apply(w, p);
    return "";
  };
  h.remove = [w](const std::string& name) -> std::string {
    std::string err;
    if (!Presets::remove(name, &err)) return err;
    std::lock_guard<std::mutex> lk(w->mu);
    MyPresets& m = w->mp;
    if (ieq(m.sel_name, name)) { m.sel_name.clear(); m.want = -1; m.active = false; }
    m.refresh = true;
    kick(w, false);
    w->changed[P_PRESET] = 1; w->update_display = 1;
    return "";
  };
  h.rename = [w](const std::string& from, const std::string& to) -> std::string {
    std::string err;
    if (!Presets::rename(from, to, &err)) return err;
    std::lock_guard<std::mutex> lk(w->mu);
    MyPresets& m = w->mp;
    if (ieq(m.sel_name, from)) m.sel_name = to;
    m.refresh = true;
    kick(w, false);
    w->changed[P_PRESET] = 1; w->update_display = 1;
    return "";
  };
  h.current = [w]() -> std::string {
    std::lock_guard<std::mutex> lk(w->mu);
    return w->mp.active ? w->mp.sel_name : std::string();
  };
  h.fields = []() {
    std::vector<PresetField> out;
    for (int k : kPresetKeys) {
      const param_t& p = PARAMS[k];
      PresetField f;
      f.key = p.key; f.label = p.name; f.unit = p.unit ? p.unit : "";
      f.min = p.min; f.max = p.max; f.def = real_of(k, p.def);
      f.step = (p.max - p.min) > 40 ? 0.5f : 0.1f;
      f.group = (k == P_INPUT || k == P_GATE_ON || k == P_GATE) ? "input" : (k == P_BASS || k == P_MID || k == P_TREBLE || k == P_EQ_ON) ? "tone"
              : (k == P_OUTPUT || k == P_NORMALIZE) ? "out" : (k == P_ECO || k == P_FULL) ? "amp" : "cab";
      if (p.nopts == 2 && !strcmp(p.opts[0], "Off") && !strcmp(p.opts[1], "On")) f.toggle = true;
      else if (p.nopts > 1) for (int o = 0; o < p.nopts; o++) f.options.push_back(p.opts[o]);
      out.push_back(f);
    }
    return out;
  };
  h.choices = [w](int which) {
    std::vector<PresetChoice> out;
    std::lock_guard<std::mutex> lk(w->mu);
    const Browser& b = which == 0 ? w->model : w->ir;
    for (const LibEntry& e : b.lib) {
      PresetChoice c;
      std::string pack = e.t3k && !e.t3k_title.empty() ? e.t3k_title : e.pack;
      const size_t ps = pack.find_last_of('/');
      if (ps != std::string::npos) pack = pack.substr(ps + 1);
      c.label = pack.empty() ? stem_of(e) : pack + " \xC2\xB7 " + stem_of(e);
      c.ref = ref_of(e);
      out.push_back(c);
    }
    return out;
  };
  h.edited = [w]() {
    std::lock_guard<std::mutex> lk(w->mu);
    w->mp.refresh = true;
    kick(w, false);
  };
  return h;
}

// The phone pages' server (made on the first use; destroyed only at effClose, after the worker has stopped). Host
// thread, without w->mu held: w->t3k is read by the worker and the display calls under w->mu, so installed under it.
static Tone3000* ensure_t3k(Inst* w) {
  {
    std::lock_guard<std::mutex> lk(w->mu);
    if (w->t3k) return w->t3k.get();
  }
  std::unique_ptr<Tone3000> made(new Tone3000([w](const std::string& path, const std::string&) {
    std::lock_guard<std::mutex> lk(w->mu);     // (Tone3000's thread) rescan, then select the new file
    w->select_after_scan = path;
    w->scanning = true;
    kick(w, true);
    w->update_display = 1;
  }));
  made->set_route([w](const std::string& method, const std::string& path, const std::map<std::string, std::string>& args,
                      WebReply* out) { return presets_web_route(mp_hooks(w), method, path, args, out); });
  std::lock_guard<std::mutex> lk(w->mu);
  if (!w->t3k) w->t3k = std::move(made);
  return w->t3k.get();
}
// GET TONES / Browse TONE3000: the phone page (kind "model", "ir", or "" for the choice)
static void open_tones(Inst* w, const std::string& kind) {
  ensure_t3k(w)->start(kind);
  w->update_display = 1;
}
static void open_presets_page(Inst* w) {
  ensure_t3k(w)->start("", false);               // no TONE3000 sign-in is wanted: no libcurl needed
  w->update_display = 1;
}

// MY PRESETS, SAVE, DELETE (two taps), NAME ON PHONE
static void set_mp_param(Inst* w, int i, float v) {
  bool phone = false;
  {
    std::lock_guard<std::mutex> lk(w->mu);
    MyPresets& m = w->mp;
    const int n = (int)m.list.size();
    const auto now = std::chrono::steady_clock::now();
    if (i == P_MP_LIST) {
      if (v >= 0.5f) {
        w->model.menu_open = 2;
        w->model.marq_key = -1;
        w->model.armed_path.clear();
        m.refresh = true;                        // (files renamed from a computer show up)
        kick(w, false);
        menu_goto_selected(w, w->model);
        w->changed[P_AMP_MENU] = 1;
      }
    } else if (i == P_MP_SAVE) {
      if (v >= 0.5f) {
        m.pending = mp_snapshot(w);
        m.save_req = true;
        m.notice = "Saving..."; m.notice_until = now + std::chrono::seconds(3);
        kick(w, false);
      }
    } else if (i == P_MP_DELETE) {
      if (v >= 0.5f) {
        if (m.want >= 0 && m.want < n) {
          const std::string name = m.list[m.want].name;
          if (m.armed_name == name && now < m.armed_until) { m.armed_name.clear(); m.delete_name = name; kick(w, false); }
          else { m.armed_name = name; m.armed_until = now + std::chrono::seconds(kArmSeconds); }
        } else {
          m.notice = "Pick a preset first"; m.notice_until = now + std::chrono::seconds(3);
        }
      }
    } else if (i == P_MP_PHONE) {
      phone = v >= 0.5f;
    }
    w->changed[i] = 1;                           // momentary: report it back to 0
    w->update_display = 1;
  }
  if (phone) open_presets_page(w);
}

// a tap on an AMP LIST / CAB LIST button, a page button, or a row (a file, or over the AMP block a preset)
static void set_menu_param(Inst* w, int i, float v) {
  const int k = menu_which(i);
  const MenuIdx& m = MI[k];
  Browser& b = k == 0 ? w->model : w->ir;
  int pick = -1;                                 // a preset to load (mp_choose takes mu itself)
  {
    std::lock_guard<std::mutex> lk(w->mu);
    if (i == m.list) {
      if (v >= 0.5f) {
        b.menu_open = 1;
        b.marq_key = -1;                         // (long names start from their beginning)
        menu_goto_selected(w, b);
      }
    } else if (i == m.close) {
      if (v >= 0.5f) { b.menu_open = 0; b.armed_path.clear(); w->mp.armed_name.clear(); }
    } else if (i == m.prev || i == m.next) {
      if (v >= 0.5f && b.menu_open.load()) {
        const int pages = menu_pages(w, b);
        b.menu_page = ((b.menu_page + (i == m.next ? 1 : -1)) % pages + pages) % pages;
      }
    } else if (is_row(i)) {
      const int rows = menu_rows(b), r = i - m.row1, idx = b.menu_page * rows + r;
      if (b.menu_open.load() && r >= 0 && r < rows && idx < menu_count(w, b)) {   // the list stays open
        if (mp_mode(b)) pick = idx;
        else { b.want = idx; b.saved = LibEntry(); b.error.clear(); requested(b); kick(w, false); browser_changed(w, &b); }
      }
    }
    w->changed[i] = 1;                           // (a button or row the host toggled goes back to what it is)
    w->changed[m.menu] = 1;
    w->update_display = 1;
  }
  if (pick >= 0) mp_choose(w, pick);
}

static void set_param(Inst* w, int32_t i, float v, bool trusted) {
  if (i < 0 || i >= P_COUNT) return;
  v = clamp01(v);
  if (is_display(i)) return;                     // read-only: the host cannot move the tubes, the print or the preset name
  if (is_mp(i)) { set_mp_param(w, i, v); return; }
  if (is_menu(i)) { if (i != MI[menu_which(i)].menu && i != MI[menu_which(i)].title) set_menu_param(w, i, v); return; }
  Browser* b = browser_of(w, i);
  if (!b) {
    const param_t& p = PARAMS[i];
    if (p.nopts > 1) {                           // as mpc-vst-plugins: a value between options is a one-step nudge
      float pos = v * (p.nopts - 1), cur = w->norm[i] * (p.nopts - 1);
      if (std::fabs(pos - std::round(pos)) > 0.001f) {
        int k = (int)lroundf(cur) + (pos > cur ? 1 : -1);
        k = k < 0 ? 0 : k > p.nopts - 1 ? p.nopts - 1 : k;
        v = (float)k / (p.nopts - 1);
      }
    }
    if (i == P_GET_TONES) {
      if (v >= 0.5f) {
        open_tones(w, "");
        w->changed[P_GET_TONES] = 1;               // momentary: report it back to 0
      }
      return;
    }
    if (i == P_RESCAN) {
      if (v >= 0.5f) {
        std::lock_guard<std::mutex> lk(w->mu);
        w->scanning = true;
        kick(w, true);
        w->changed[P_RESCAN] = 1;
        w->update_display = 1;
      }
      return;
    }
    if (i == P_FULL) {                           // A2 FULL costs most of the CPU on a Gen 1: it takes two taps to turn on
      std::lock_guard<std::mutex> lk(w->mu);
      const auto now = std::chrono::steady_clock::now();
      if (v < 0.5f || trusted) {                                         // off is one tap
        w->full_armed_until = {};
        if (v < 0.5f && w->norm[P_FULL] < 0.5f) { w->update_display = 1; return; }   // already off: nothing to reload
      }
      else if (w->norm[P_FULL] < 0.5f) {
        if (now < w->full_armed_until) w->full_armed_until = {};         // the second tap: on
        else {                                                           // the first tap: warn, and stay off
          w->full_armed_until = now + std::chrono::seconds(kFullArmSeconds);
          w->changed[P_FULL] = 1;                                        // (the host's toggle goes back to off)
          w->update_display = 1;
          return;
        }
      }
    }
    w->norm[i] = v;
    apply_param(w, i);
    if (i == P_IR_LEN || i == P_FULL) {         // the IR / the capture again, at the new length / size
      std::lock_guard<std::mutex> lk(w->mu);
      Browser& b = i == P_FULL ? w->model : w->ir;
      if (b.want >= 0) b.loading = true;
      kick(w, false);
    }
    w->update_display = 1;
    return;
  }
  std::string browse;
  {
  std::lock_guard<std::mutex> lk(w->mu);
  const int n = (int)b->lib.size();
  if (is_src(i)) return;                         // read-only: what the current file is
  if (i == P_MODEL || i == P_IR) {
    if (n > 0) {
      b->want = n > 1 ? (int)lroundf(v * (n - 1)) : 0;
      b->saved = LibEntry();                     // an explicit pick replaces a project's missing file
      b->error.clear();
      requested(*b);
      kick(w, false);
      w->changed[b == &w->model ? P_MODEL_SRC : P_IR_SRC] = 1;
    }
  } else if (i == P_MODEL_PREV || i == P_MODEL_NEXT || i == P_IR_PREV || i == P_IR_NEXT) {
    if (v >= 0.5f && n > 0) {
      const int step = (i == P_MODEL_NEXT || i == P_IR_NEXT) ? 1 : -1;
      const int base = b->want >= 0 ? b->want : 0;
      b->want = ((base + step) % n + n) % n;     // wraps around at both ends
      b->saved = LibEntry();
      b->error.clear();
      requested(*b);
      kick(w, false);
      browser_changed(w, b);
    }
    w->changed[i] = 1;                           // momentary: report it back to 0
  } else if (i == P_MODEL_PACK_PREV || i == P_MODEL_PACK_NEXT || i == P_IR_PACK_PREV || i == P_IR_PACK_NEXT) {
    if (v >= 0.5f && n > 0) {                    // to the first file of the previous / next pack (wraps around)
      const bool next = i == P_MODEL_PACK_NEXT || i == P_IR_PACK_NEXT;
      int k = b->want >= 0 && b->want < n ? b->want : 0;
      const std::string here = b->lib[k].pack;
      if (next) {
        int j = k;
        do { j = (j + 1) % n; } while (j != k && b->lib[j].pack == here);
        k = j;
      } else {
        int start = k;                           // the first file of this pack
        while (start > 0 && b->lib[start - 1].pack == here) start--;
        int j = (start - 1 + n) % n;             // the last file of the previous pack...
        const std::string prev = b->lib[j].pack;
        if (prev == here) {
          k = start;                             // one pack only: its first file
        } else {
          while (j > 0 && b->lib[j - 1].pack == prev) j--;   // ...and that pack's first file
          k = j;
        }
      }
      b->want = k;
      b->saved = LibEntry();
      b->error.clear();
      requested(*b);
      kick(w, false);
      browser_changed(w, b);
    }
    w->changed[i] = 1;
  } else if (i == P_MODEL_DELETE || i == P_IR_DELETE) {
    if (v >= 0.5f && b->want >= 0 && b->want < n) {
      const auto now = std::chrono::steady_clock::now();
      const std::string& path = b->lib[b->want].path;
      if (b->armed_path == path && now < b->armed_until) {   // the second tap: delete it
        b->armed_path.clear();
        b->delete_req = true;
        kick(w, false);
      } else {                                                // the first tap: ask
        b->armed_path = path;
        b->armed_until = now + std::chrono::seconds(kArmSeconds);
      }
    }
    w->changed[i] = 1;
  } else if (i == P_MODEL_BROWSE || i == P_IR_BROWSE) {
    if (v >= 0.5f) browse = i == P_IR_BROWSE ? "ir" : "model";   // opened below, after mu is released
    w->changed[i] = 1;
  }
  w->update_display = 1;
  }
  if (!browse.empty()) open_tones(w, browse);
}

static void set_parameter(AEffect* e, int32_t i, float v) { set_param((Inst*)e->object, i, v, false); }

static float get_parameter(AEffect* e, int32_t i) {
  Inst* w = (Inst*)e->object;
  if (i < 0 || i >= P_COUNT) return 0;
  if (i == P_MODEL || i == P_IR) {
    std::lock_guard<std::mutex> lk(w->mu);
    return browser_norm(*browser_of(w, i));
  }
  if (is_src(i)) {
    std::lock_guard<std::mutex> lk(w->mu);
    return src_norm(*browser_of(w, i));
  }
  if (i == P_TUBES) return w->tube_step.load() / (float)kTubeSteps;
  if (is_wave(i)) return w->wave[i - P_WAVE1].load() / 24.0f;
  if (is_menu(i)) {
    Browser& mb = menu_which(i) ? w->ir : w->model;
    if (i == MI[menu_which(i)].menu) return (float)mb.menu_open.load() / (float)(PARAMS[i].nopts - 1);
    if (!is_row(i)) return 0;
    std::lock_guard<std::mutex> lk(w->mu);
    return menu_row_on(w, mb, i - MI[menu_which(i)].row1) ? 1.0f : 0.0f;
  }
  if (browser_of(w, i) || i == P_RESCAN || i == P_GET_TONES || i == P_T3K_INFO) return 0;   // triggers, info lines
  return w->norm[i];
}

// The tubes: the output's peak, held at once and released over ~0.35 s (a tube cools slowly), shown in 24 steps from
// -56 dBFS (1) to -6 dBFS (24); below -62 dBFS the tubes idle (0). Audio thread only; one peak scan per block.
static void tubes_level(Inst* w, const float* l, const float* r, int32_t n) {
  float pk = 0;
  for (int32_t j = 0; j < n; j++) { const float a = std::fabs(l[j]), b = std::fabs(r[j]); pk = std::max(pk, std::max(a, b)); }
  w->tube_env = std::max(pk, w->tube_env * std::exp(-(float)n / ((float)w->rate * 0.35f)));
  int k = 0;
  if (w->tube_env > 7.9e-4f) {                       // -62 dBFS
    const float db = 20.0f * std::log10(w->tube_env);
    k = 1 + (int)lroundf(23.0f * std::min(1.0f, std::max(0.0f, (db + 56.0f) / 50.0f)));
  }
  w->tube_step = k;
}

static void notify(Inst* w) {                    // audio thread: report values the plugin changed itself
  {                                              // the tubes: at most every 9 blocks (~26 ms at 128 frames), going dark at once
    const int k = w->tube_step.load();
    w->tube_age++;
    if (k != w->tube_sent && (w->tube_age >= 9 || k == 0)) {
      w->tube_sent = k;
      w->tube_age = 0;
      w->master(&w->fx, audioMasterAutomate, P_TUBES, 0, nullptr, k / (float)kTubeSteps);
    }
  }
  const bool disp = w->update_display.exchange(0);
  if (disp) {
    for (int k = 0; k < 2; k++) {
      if (!(k == 0 ? w->model : w->ir).menu_open.load()) continue;   // a list is open: its title and rows are read again
      w->changed[MI[k].title] = 1;
      for (int r = 0; r < kRows; r++) w->changed[MI[k].row1 + r] = 1;
    }
  }
  for (int i = 0; i < P_COUNT; i++) {
    if (!w->changed[i].exchange(0)) continue;
    float v = 0;
    if (i == P_TUBES) v = w->tube_step.load() / (float)kTubeSteps;
    if (is_wave(i)) v = w->wave[i - P_WAVE1].load() / 24.0f;
    if (i == P_MODEL || i == P_IR || is_src(i) || is_row(i)) {
      std::unique_lock<std::mutex> lk(w->mu, std::try_to_lock);   // never wait on the audio thread
      if (!lk.owns_lock()) { w->changed[i] = 1; continue; }
      if (is_row(i)) v = menu_row_on(w, menu_which(i) ? w->ir : w->model, i - MI[menu_which(i)].row1) ? 1.0f : 0.0f;
      else v = is_src(i) ? src_norm(*browser_of(w, i)) : browser_norm(*browser_of(w, i));
    }
    if (is_menu(i) && i == MI[menu_which(i)].menu) v = (float)(menu_which(i) ? w->ir : w->model).menu_open.load() / (float)(PARAMS[i].nopts - 1);
    if (i == P_PRESET) v = 0;
    w->master(&w->fx, audioMasterAutomate, i, 0, nullptr, v);
  }
  if (disp) w->master(&w->fx, audioMasterUpdateDisplay, 0, 0, nullptr, 0);
}

static void process_replacing(AEffect* e, float** in, float** out, int32_t n) {
  Inst* w = (Inst*)e->object;
  if (w->master) notify(w);
  if (n <= 0) return;
  if (!w->eng) { memset(out[0], 0, n * sizeof(float)); memset(out[1], 0, n * sizeof(float)); tubes_level(w, out[0], out[1], n); return; }
  w->eng->process(in[0], in[1], out[0], out[1], n);
  tubes_level(w, out[0], out[1], n);
}

static void process_accumulate(AEffect* e, float** in, float** out, int32_t n) {   // legacy: adds to the outputs
  Inst* w = (Inst*)e->object;
  float l[256], r[256];
  for (int32_t off = 0; off < n; off += 256) {
    int32_t k = n - off < 256 ? n - off : 256;
    float* ip[2] = {in[0] + off, in[1] + off};
    float* op[2] = {l, r};
    process_replacing(e, ip, op, k);
    for (int32_t j = 0; j < k; j++) { out[0][off + j] += l[j]; out[1][off + j] += r[j]; }
  }
  (void)w;
}

static void copy_text(void* dst, const std::string& s, size_t limit = kText) {   // < limit bytes, "..." if cut
  std::string t = s;
  if (t.size() > limit - 1) {
    size_t cut = limit - 4;
    while (cut > 0 && ((unsigned char)t[cut] & 0xC0) == 0x80) cut--;   // never split a UTF-8 character
    t = t.substr(0, cut) + "...";
  }
  memcpy(dst, t.c_str(), t.size() + 1);
}

static size_t text_limit(int i) {
  return (i == P_MODEL || i == P_IR || i == P_MODEL_INFO || i == P_IR_INFO || i == P_MODEL_PACK || i == P_IR_PACK ||
          i == P_T3K_INFO || i == P_AMP_TITLE || i == P_CAB_TITLE || is_row(i) || i == P_PRESET) ? kTextLong : kText;
}

static std::string gear_name(const std::string& g) {        // TONE3000's Gear values, as its site names them
  if (g == "amp") return "Amp";
  if (g == "amp-cab" || g == "full-rig") return "Amp + Cab";
  if (g == "cab" || g == "ir") return "Cab";
  if (g == "pedal") return "Pedal";
  if (g == "outboard") return "Outboard";
  if (g == "space") return "Space";
  if (g == "experimental") return "Experimental";
  return g;
}

static std::string stem_of(const LibEntry& e) {
  std::string d = e.display;
  size_t slash = d.find_last_of('/');
  return slash == std::string::npos ? d : d.substr(slash + 1);
}
static std::string folder_of(const LibEntry& e) {
  std::string d = e.display;
  size_t slash = d.find_last_of('/');
  return slash == std::string::npos ? std::string() : d.substr(0, slash);
}

static bool full_armed(const Inst* w) {          // holds mu: the A2 FULL warning is up
  return w->full_armed_until != std::chrono::steady_clock::time_point{} && std::chrono::steady_clock::now() < w->full_armed_until;
}
static std::string display_of(Inst* w, int i) {
  const param_t& p = PARAMS[i];
  Browser* b = browser_of(w, i);
  char buf[64];
  if (is_menu(i)) {
    const int k = menu_which(i);
    Browser& mb = k ? w->ir : w->model;
    if (i == MI[k].menu) return p.opts[std::min(p.nopts - 1, mb.menu_open.load())];
    if (i != MI[k].title && !is_row(i)) return "";
    std::lock_guard<std::mutex> lk(w->mu);
    const int n = menu_count(w, mb), rows = menu_rows(mb);
    if (!mb.menu_open.load()) return "";
    if (i == MI[k].title) {
      if (mp_mode(mb)) {                         // My Presets: what just happened, else where the page is
        const MyPresets& m = w->mp;
        const auto now = std::chrono::steady_clock::now();
        if (!m.armed_name.empty() && now < m.armed_until) return "Tap DELETE again";
        if (!m.error.empty()) return m.error;
        if (!m.notice.empty() && now < m.notice_until) return m.notice;
        if (!m.listed) return "Reading...";
        if (n == 0) return "No presets yet";
        const int first = mb.menu_page * rows + 1, last = std::min(n, first + rows - 1);
        snprintf(buf, sizeof buf, "Presets %d-%d of %d", first, last, n);
        return buf;
      }
      if (n == 0) return !w->scanned || w->scanning ? "Searching..." : k ? "No IRs here" : "No models here";
      const int first = mb.menu_page * rows + 1, last = std::min(n, first + rows - 1);
      snprintf(buf, sizeof buf, "%s %d-%d of %d", k ? "Cabs" : "Amps", first, last, n);
      return buf;
    }
    const int r = i - MI[k].row1, idx = mb.menu_page * rows + r;
    if (r >= rows || idx >= n) return "";
    return marq_window(menu_row_text(w, mb, idx), marq_step_now(w, mb));
  }
  if (i == P_PRESET) {
    std::lock_guard<std::mutex> lk(w->mu);
    return w->mp.active && !w->mp.sel_name.empty() ? "My: " + w->mp.sel_name : std::string("No preset");
  }
  if (b) {
    std::lock_guard<std::mutex> lk(w->mu);
    const bool is_model = b == &w->model;
    const int n = (int)b->lib.size();
    if (i == P_MODEL || i == P_IR) {
      if (b->want >= 0 && b->want < n) return stem_of(b->lib[b->want]);
      if (!b->saved.file.empty()) return b->saved.file;
      return "-";
    }
    const LibEntry* cur = (b->want >= 0 && b->want < n) ? &b->lib[b->want] : nullptr;
    if (i == P_MODEL_PACK && full_armed(w)) return "A2 FULL: very heavy on CPU";   // the first tap on A2 FULL
    if (i == P_MODEL_PACK || i == P_IR_PACK) {
      if (!cur) return n == 0 ? "" : "-";
      if (cur->t3k && !cur->t3k_title.empty()) return cur->t3k_title;
      if (cur->pack.empty()) return "Loose files";
      size_t slash = cur->pack.find_last_of('/');
      return slash == std::string::npos ? cur->pack : cur->pack.substr(slash + 1);
    }
    if (is_src(i)) return cur && cur->t3k ? "TONE3000" : "Local";
    if (i == P_MODEL_INFO && full_armed(w)) return "Best on a G2. Own risk.";
    if (i == P_MODEL_INFO || i == P_IR_INFO) {
      const auto now = std::chrono::steady_clock::now();
      if (cur && b->armed_path == cur->path && now < b->armed_until) return "Tap DELETE again to delete it permanently";
      if (!b->notice.empty() && now < b->notice_until) return b->notice;
      if (!w->scanned || w->scanning) return "Searching...";
      if (b->loading) return "Loading...";
      if (!b->error.empty()) return b->error;
      if (n == 0) return is_model ? "No .nam models found" : "No .wav IRs found";
      if (!cur) return "-";
      // " · Full": an A2 capture playing at its Full size (FULL switch)
      const std::string full_tag = (is_model && w->model_slimmable && w->full_loaded == 1 && cur->path == b->loaded.path)
                                       ? " \xC2\xB7 Full" : "";
      if (cur->t3k) {                            // TONE3000's attribution: gear type, format, creator
        std::string fmt = cur->t3k_format == "ir" ? "IR" : cur->t3k_format == "nam" ? "NAM" : cur->t3k_format;
        std::string s = gear_name(cur->t3k_gear);
        if (!fmt.empty()) s += (s.empty() ? "" : " \xC2\xB7 ") + fmt;
        if (!cur->t3k_creator.empty()) s += (s.empty() ? "@" : " \xC2\xB7 @") + cur->t3k_creator;
        return s + full_tag;
      }
      int first = b->want, count = 0;            // its place in its pack
      while (first > 0 && b->lib[first - 1].pack == cur->pack) first--;
      for (int j = first; j < n && b->lib[j].pack == cur->pack; j++) count++;
      snprintf(buf, sizeof buf, "%s %d of %d \xC2\xB7 Local", is_model ? "Model" : "IR", b->want - first + 1, count);
      return std::string(buf) + full_tag;
    }
    return "";
  }
  if (i == P_T3K_INFO) {
    std::lock_guard<std::mutex> lk(w->mu);
    if (full_armed(w)) return "Tap A2 FULL again to turn on";
    return w->t3k ? w->t3k->status() : "Tap BROWSE TONE3000";
  }
  if (i == P_TUBES) return p.opts[w->tube_step.load()];
  if (is_wave(i)) return p.opts[w->wave[i - P_WAVE1].load()];
  if (p.nopts > 1) return p.opts[(int)real_of(i, w->norm[i])];
  if (p.momentary) return "";
  const float v = real_of(i, w->norm[i]);
  if (!strcmp(p.unit, "dB")) snprintf(buf, sizeof buf, "%.1f dB", v);
  else snprintf(buf, sizeof buf, "%.1f", v);
  return buf;
}

// state: "nam-a2-lite=1\n", "<key>=<real value>\n" per plain parameter, then the model's and IR's path, relative
// path and file name. Newline-separated: paths may contain ';' or '=' but never a newline.
static int get_chunk(Inst* w) {
  std::string s = "nam-a2-lite=1\n";
  char buf[64];
  for (int i = 0; i < P_COUNT; i++) {
    if (browser_of(w, i) || PARAMS[i].momentary || i == P_T3K_INFO || is_display(i) || is_menu(i)) continue;
    snprintf(buf, sizeof buf, "%s=%.6g\n", PARAMS[i].key, real_of(i, w->norm[i]));
    s += buf;
  }
  std::lock_guard<std::mutex> lk(w->mu);
  for (Browser* b : {&w->model, &w->ir}) {
    const LibEntry& e = (b->want >= 0 && b->want < (int)b->lib.size()) ? b->lib[b->want] : b->saved;
    const std::string k = b == &w->model ? "model" : "ir";
    s += k + "_path=" + e.path + "\n" + k + "_display=" + e.display + "\n" + k + "_file=" + e.file + "\n";
  }
  if (!w->mp.sel_name.empty()) s += "mp_name=" + w->mp.sel_name + "\nmp_active=" + (w->mp.active ? "1" : "0") + "\n";
  w->chunk = s;
  return (int)w->chunk.size() + 1;
}

static bool set_chunk(Inst* w, const char* data, size_t len) {
  std::string s(data, strnlen(data, len));
  // the state header (internal; unchanged by the rename to "Liminal Amp", so every saved project opens)
  static const std::string kHead = "nam-a2-lite=1\n";
  if (s.compare(0, kHead.size(), kHead) != 0) return false;
  LibEntry saved[2];
  std::string mp_name;
  bool mp_active = false;
  size_t pos = kHead.size();
  while (pos < s.size()) {
    size_t nl = s.find('\n', pos);
    if (nl == std::string::npos) nl = s.size();
    std::string line = s.substr(pos, nl - pos);
    pos = nl + 1;
    size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string k = line.substr(0, eq), v = line.substr(eq + 1);
    if (k == "mp_name") { mp_name = v; continue; }
    if (k == "mp_active") { mp_active = v == "1"; continue; }
    bool done = false;
    for (int i = 0; i < P_COUNT && !done; i++) {
      if (browser_of(w, i) || PARAMS[i].momentary || is_display(i) || is_menu(i) || k != PARAMS[i].key) continue;
      const param_t& p = PARAMS[i];
      float r = (float)atof(v.c_str());
      if (p.nopts > 1) w->norm[i] = clamp01(r / (p.nopts - 1));
      else w->norm[i] = p.max > p.min ? clamp01((r - p.min) / (p.max - p.min)) : 0;
      apply_param(w, i);
      done = true;
    }
    for (int b = 0; b < 2 && !done; b++) {
      const std::string pre = b == 0 ? "model_" : "ir_";
      if (k == pre + "path") { saved[b].path = v; done = true; }
      else if (k == pre + "display") { saved[b].display = v; done = true; }
      else if (k == pre + "file") { saved[b].file = v; done = true; }
    }
  }
  std::lock_guard<std::mutex> lk(w->mu);
  Browser* bs[2] = {&w->model, &w->ir};
  for (int b = 0; b < 2; b++) {
    bs[b]->saved = saved[b];
    bs[b]->error.clear();
    if (w->scanned) resolve(*bs[b]);
  }
  w->mp.sel_name = mp_name;                     // the selected preset: shown again, not applied again (the project holds its sound)
  w->mp.active = mp_active && !mp_name.empty();
  w->mp.want = -1;
  for (int k = 0; k < (int)w->mp.list.size(); k++) if (!mp_name.empty() && ieq(w->mp.list[k].name, mp_name)) w->mp.want = k;
  kick(w, !w->scanned);
  for (int i = 0; i < P_COUNT; i++) w->changed[i] = 1;
  w->update_display = 1;
  return true;
}

static intptr_t dispatcher(AEffect* e, int32_t op, int32_t idx, intptr_t v, void* p, float o) {
  Inst* w = (Inst*)e->object;
  switch (op) {
    case effOpen: return 1;
    case effClose: {
      {
        std::lock_guard<std::mutex> lk(w->mu);
        w->stop = true;
      }
      w->cv.notify_one();
      if (w->worker.joinable()) w->worker.join();    // 1. the worker (it reads w->t3k) has stopped
      std::unique_ptr<Tone3000> t;
      {
        std::lock_guard<std::mutex> lk(w->mu);
        t = std::move(w->t3k);
      }
      t.reset();                                     // 2. TONE3000's page and any download stop (joins; its callback
                                                     //    may still run and lock w->mu: w is alive until 3.)
      delete w;                                      // 3.
    }
      return 1;
    case effGetPlugCategory: return kPlugCategEffect;
    case effGetEffectName:
    case effGetProductString: strncpy((char*)p, PLUG_NAME, 31); ((char*)p)[31] = 0; return 1;
    case effGetVendorString: strncpy((char*)p, PLUG_VENDOR, 31); ((char*)p)[31] = 0; return 1;
    case effGetVendorVersion: return PLUG_VERSION;
    case effGetVstVersion: return 2400;
    case effCanBeAutomated: return idx >= 0 && idx < P_COUNT && !is_menu(idx) && !is_display(idx) && !is_mp(idx);
    case effGetParamName:
      if (idx >= 0 && idx < P_COUNT) { strncpy((char*)p, PARAMS[idx].name, 31); ((char*)p)[31] = 0; }
      return 1;
    case effGetParamLabel:
      if (p) ((char*)p)[0] = 0;
      return 1;
    case effGetParamDisplay:
      if (idx < 0 || idx >= P_COUNT || !p) return 0;
      copy_text(p, display_of(w, idx), text_limit(idx));
      return 1;
    case effSetSampleRate:
      if (o > 1000 && std::fabs(o - w->rate) > 0.5) { w->rate = o; rebuild_engine(w); }
      return 1;
    case effSetBlockSize: return 1;              // any block size works: the engine runs long ones in pieces
    case effMainsChanged: return 1;
    case effCanDo: return -1;
    case effGetChunk: {
      int len = get_chunk(w);
      *(void**)p = (void*)w->chunk.c_str();
      return len;
    }
    case effSetChunk: return (v > 0 && p) ? (set_chunk(w, (const char*)p, (size_t)v) ? 1 : 0) : 0;
    default: return 0;
  }
}

extern "C" __attribute__((visibility("default"))) AEffect* VSTPluginMain(audioMasterCallback master) {
  Inst* w = new (std::nothrow) Inst();
  if (!w) return nullptr;
  w->master = master;
  for (int i = 0; i < P_COUNT; i++) {
    w->changed[i] = 0;
    if (is_wave(i)) w->wave[i - P_WAVE1] = 0;
    const param_t& p = PARAMS[i];
    w->norm[i] = p.def;                          // params.h: defaults already normalised
  }
  w->model.which = 0;
  w->ir.which = 1;
  AEffect* e = &w->fx;
  memset(e, 0, sizeof *e);
  e->magic = 0x56737450;                         // 'VstP'
  e->dispatcher = dispatcher;
  e->process = process_accumulate;
  e->setParameter = set_parameter;
  e->getParameter = get_parameter;
  e->processReplacing = process_replacing;
  e->numParams = P_COUNT;
  e->numInputs = 2;
  e->numOutputs = 2;
  e->flags = effFlagsCanReplacing | effFlagsProgramChunks;
  e->uniqueID = PLUG_UID;
  e->version = PLUG_VERSION;
  e->object = w;
  w->eng.reset(new Engine(w->rate, w->block));   // 44.1 kHz (MPC OS); rebuilt if the host says otherwise
  for (int i = 0; i < P_COUNT; i++) apply_param(w, i);
  w->worker = std::thread(worker_main, w);       // scans the drives, then loads the first model and IR
  return e;
}
