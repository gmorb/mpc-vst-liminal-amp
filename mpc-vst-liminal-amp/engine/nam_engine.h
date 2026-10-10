// SPDX-License-Identifier: MIT
// Copyright (c) 2026 the mpc-vst-liminal-amp contributors
/* nam_engine.h -- the amp chain of the official NAM plugin (NeuralAmpModelerPlugin v0.7.14, MIT), for MPC OS.
 *
 *   input gain (stereo averaged to mono, as the plugin does in a DAW) -> noise-gate trigger -> NAM model (A2 at
 *   its Lite size, resampled to/from the model's rate) -> noise-gate gain -> tone stack (bass 150 Hz, mid 425 Hz,
 *   treble 1800 Hz) -> cabinet IR -> 5 Hz DC blocker -> output gain ("normalized": the model's loudness to -18 dB)
 *   -> both output channels.
 *
 * Models and IRs are loaded off the audio thread (Model::load, IRConvolver::load_wav) and handed over with
 * set_model()/set_ir(); the audio thread picks them up at the start of its next block without locks, and the one it
 * replaces is freed later by the loading thread (collect()). process() never allocates, locks or does I/O.
 */
#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "NAM/dsp.h"
#include "dsp/NoiseGate.h"
#include "dsp/RecursiveLinearFilter.h"
#include "dsp/ResamplingContainer/ResamplingContainer.h"
#include "ir_convolver.h"

namespace mpcnam {

class Model {
 public:
  // Loads a .nam file for a host running at host_rate with blocks of up to max_block frames. A2 ("slimmable")
  // models are set to their Lite size. nullptr + err on failure.
  // full: an A2 file's Full size (8 channels; ~57% of a block on a Gen 1 Force) instead of its Lite size (3 channels)
  static std::unique_ptr<Model> load(const std::string& path, double host_rate, int max_block, std::string* err,
                                     bool full = false);
  // eco: run the model at the host rate, without the resampler (lower CPU; a 48 kHz capture's tone then shifts
  // down by host/model rate, ~8% at 44.1 kHz: off by default, the official plugin always resamples)
  void process(NAM_SAMPLE* in, NAM_SAMPLE* out, int n, bool eco = false);
  bool has_loudness() const { return has_loudness_; }
  double loudness() const { return loudness_; }
  double model_rate() const { return model_rate_; }
  bool slimmable() const { return slimmable_; }
  const std::string& architecture() const { return arch_; }
  int latency() const;

  // The amp's "pluck print" for the page: one plucked A string (110 Hz, a Karplus-Strong pluck peaking at -10 dBFS,
  // 1.2 s) through the capture alone, at its own rate. The output's envelope in kWaveCols time slices, each a level
  // 0..7 (its peak, from -24 dB to the loudest slice) and one texture for the whole note from how much the capture
  // holds the note up against the pluck's own decay (0 clean, 1 crunch, 2 high gain): state = 0 (silent) or
  // 1 + texture * 8 + level (1..24). Loading thread only, before set_model(); resets the model afterwards.
  static const int kWaveCols = 14;
  void pluck_print(int* states);

 private:
  Model() = default;
  double prewarm_rate_ = 48000;
  int prewarm_block_ = 512;
  std::unique_ptr<nam::DSP> dsp_;
  std::unique_ptr<dsp::ResamplingContainer<NAM_SAMPLE, 1, 12>> rs_;
  std::function<void(NAM_SAMPLE**, NAM_SAMPLE**, int)> func_;
  double model_rate_ = 48000, host_rate_ = 48000, loudness_ = 0;
  bool has_loudness_ = false, slimmable_ = false;
  std::string arch_;
};

struct Params {                       // written by the host thread, read by the audio thread (floats: atomic)
  std::atomic<float> input_db{0}, output_db{0}, gate_db{-80};
  std::atomic<float> bass{5}, mid{5}, treble{5};
  std::atomic<bool> gate_on{true}, eq_on{true}, ir_on{true}, normalize{true};
  std::atomic<bool> eco{false};        // Model::process(eco): no resampling; off by default
};

class Engine {
 public:
  Engine(double host_rate, int max_block);
  ~Engine();
  Params params;

  // loading thread
  void set_model(std::unique_ptr<Model> m);     // nullptr: no model (the signal passes through)
  void set_ir(std::unique_ptr<IRConvolver> ir);  // nullptr: no IR
  void collect();                                // free what the audio thread has let go of

  // audio thread
  void process(const float* in_l, const float* in_r, float* out_l, float* out_r, int n);

  double host_rate() const { return rate_; }
  int max_block() const { return max_block_; }

 private:
  bool eco_last_ = false;              // audio thread: the path used last block
  int eco_fade_ = 0;                   // samples of fade-in left after switching paths (masks the switch)
  // Lock-free hand-over of one object from the loading thread to the audio thread. next: nullptr = no change,
  // none() = switch to nothing, else the new object. The audio thread parks what it replaces in retired[] for the
  // loading thread to delete (it never frees memory itself).
  template <typename T> struct Slot {
    static T* none() { static char marker; return reinterpret_cast<T*>(&marker); }
    std::atomic<T*> next{nullptr};
    std::atomic<T*> retired[8] = {};
    T* current = nullptr;                        // audio thread only
    void publish(T* p) {                         // loading thread
      T* old = next.exchange(p ? p : none());
      if (old && old != none()) delete old;      // never reached the audio thread
    }
    void take() {                                // audio thread, start of a block
      T* n = next.exchange(nullptr);
      if (!n) return;
      T* old = current;
      current = (n == none()) ? nullptr : n;
      if (old)
        for (auto& r : retired) { T* e = nullptr; if (r.compare_exchange_strong(e, old)) { old = nullptr; break; } }
    }
    void collect() {                             // loading thread
      for (auto& r : retired) { T* p = r.exchange(nullptr); delete p; }
    }
    ~Slot() { collect(); T* n = next.exchange(nullptr); if (n && n != none()) delete n; delete current; }
  };
  void update_filters();
  void process_chunk(const float* in_l, const float* in_r, float* out_l, float* out_r, int n);

  double rate_;
  int max_block_;
  Slot<Model> model_;
  Slot<IRConvolver> ir_;
  dsp::noise_gate::Trigger gate_trigger_;
  dsp::noise_gate::Gain gate_gain_;
  recursive_linear_filter::LowShelf bass_;
  recursive_linear_filter::Peaking mid_;
  recursive_linear_filter::HighShelf treble_;
  recursive_linear_filter::HighPass dc_;
  float bass_v_ = -1, mid_v_ = -1, treble_v_ = -1, gate_v_ = 1e9f;
  // DSP_SAMPLE (double: the gate, tone stack and DC blocker, as the official plugin) around NAM_SAMPLE/float (the
  // model and the IR, for speed on the device)
  std::vector<DSP_SAMPLE> in_d_, mid_d_;
  std::vector<NAM_SAMPLE> in_f_, out_f_;
  std::vector<float> ir_in_, ir_out_;
};

}  // namespace mpcnam
