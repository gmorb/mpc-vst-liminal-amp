// SPDX-License-Identifier: MIT
// Copyright (c) 2026 the mpc-vst-liminal-amp contributors
// The chain, its constants and the resampling wrapper follow NeuralAmpModelerPlugin v0.7.14
// (NeuralAmpModeler.cpp/.h, ToneStack.cpp), Copyright (c) 2022 Steven Atkinson, MIT.
#include "nam_engine.h"
#include <algorithm>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <filesystem>

#include "NAM/get_dsp.h"
#include "NAM/slimmable.h"

namespace mpcnam {

// Flush-to-zero (and denormals-are-zero) for one block: a decaying signal must not fall into denormal arithmetic,
// which is very slow on ARM. As NeuralAmpModeler::ProcessBlock's disable_denormals().
struct FlushDenormals {
#if defined(__arm__)
  uint32_t old;
  FlushDenormals() { __asm__ __volatile__("vmrs %0, fpscr" : "=r"(old)); uint32_t v = old | (1u << 24);
                     __asm__ __volatile__("vmsr fpscr, %0" : : "r"(v)); }
  ~FlushDenormals() { __asm__ __volatile__("vmsr fpscr, %0" : : "r"(old)); }
#elif defined(__SSE__) || defined(__x86_64__)
  unsigned old;
  FlushDenormals() { old = __builtin_ia32_stmxcsr(); __builtin_ia32_ldmxcsr(old | 0x8040); }
  ~FlushDenormals() { __builtin_ia32_ldmxcsr(old); }
#else
  FlushDenormals() {}
#endif
};

// ------------------------------------------------------------------------------------------------ Model
std::unique_ptr<Model> Model::load(const std::string& path, double host_rate, int max_block, std::string* err,
                                   bool full) {
  std::unique_ptr<Model> m(new Model());
  try {
    m->dsp_ = nam::get_dsp(std::filesystem::path(path));
  } catch (const std::exception& e) {
    if (err) *err = e.what();
    return nullptr;
  }
  if (!m->dsp_) {
    if (err) *err = "not a NAM model";
    return nullptr;
  }
  if (auto* s = dynamic_cast<nam::SlimmableModel*>(m->dsp_.get())) {
    s->SetSlimmableSize(full ? 1.0 : 0.0);        // A2: the Lite size, or Full (FULL switch)
    m->slimmable_ = true;
  }
  const double r = m->dsp_->GetExpectedSampleRate();
  m->model_rate_ = r > 0 ? r : 48000.0;           // NAM's default when a file doesn't say
  m->host_rate_ = host_rate;
  m->has_loudness_ = m->dsp_->HasLoudness();
  m->loudness_ = m->has_loudness_ ? m->dsp_->GetLoudness() : 0.0;
  m->arch_ = m->slimmable_ ? (full ? "A2 (Full)" : "A2 (Lite)") : "NAM";
  // as ResamplingNAM: a Lanczos resampler around the model when the rates differ
  if (m->model_rate_ != host_rate) {
    m->rs_.reset(new dsp::ResamplingContainer<NAM_SAMPLE, 1, 12>(m->model_rate_));
    m->rs_->Reset(host_rate, max_block);
    Model* self = m.get();
    m->func_ = [self](NAM_SAMPLE** in, NAM_SAMPLE** out, int n) { self->dsp_->process(in, out, n); };
    const double up = host_rate / m->model_rate_;
    // room for both paths: resampled (~max_block / up model samples) and eco (max_block at the host rate)
    m->prewarm_block_ = std::max(max_block, (int)std::ceil((double)max_block / up) + 8);
  } else {
    m->prewarm_block_ = max_block;
  }
  m->prewarm_rate_ = host_rate;
  m->dsp_->ResetAndPrewarm(m->prewarm_rate_, m->prewarm_block_);
  return m;
}

void Model::pluck_print(int* states) {
  FlushDenormals fd;
  const double sr = model_rate_;
  const int n = (int)(1.2 * sr), period = std::max(2, (int)std::lround(sr / 110.0));
  std::vector<NAM_SAMPLE> x(n), y(n, 0);
  std::vector<float> buf(period);
  uint32_t seed = 12345u;                                         // deterministic: the same file draws the same print
  for (auto& v : buf) { seed = seed * 1664525u + 1013904223u; v = (float)((seed >> 8) * (2.0 / 16777216.0) - 1.0); }
  float lp = 0, pk = 0;
  for (int i = 0; i < n; i++) {
    const int k = i % period, k1 = (k + 1) % period;
    const float s = buf[k];
    buf[k] = 0.5f * (buf[k] + buf[k1]) * 0.990f;                   // the string's loss
    lp += (s - lp) * 0.45f;                                        // a pick, not a click
    x[i] = lp;
    pk = std::max(pk, std::fabs(lp));
  }
  for (auto& v : x) v *= (NAM_SAMPLE)(0.316 / std::max(1e-6f, pk));
  const int blk = std::max(1, std::min(64, prewarm_block_));
  for (int i = 0; i < n; i += blk) {
    NAM_SAMPLE* ip[1] = {x.data() + i};
    NAM_SAMPLE* op[1] = {y.data() + i};
    dsp_->process(ip, op, std::min(blk, n - i));
  }
  dsp_->ResetAndPrewarm(prewarm_rate_, prewarm_block_);         // the audio thread starts from a clean state
  const int cols = kWaveCols;
  float in_db[kWaveCols], out_db[kWaveCols], omax = 0;
  for (int c = 0; c < cols; c++) {
    const int a = (int)((int64_t)n * c / cols), b = (int)((int64_t)n * (c + 1) / cols);
    float pi = 0, po = 0;
    for (int i = a; i < b; i++) { pi = std::max(pi, (float)std::fabs(x[i])); if (std::isfinite(y[i])) po = std::max(po, (float)std::fabs(y[i])); }
    in_db[c] = 20.0f * std::log10(pi + 1e-9f);
    out_db[c] = 20.0f * std::log10(po + 1e-9f);
    omax = std::max(omax, po);
  }
  if (omax < 1e-5f) { for (int c = 0; c < cols; c++) states[c] = 0; return; }
  const float omax_db = 20.0f * std::log10(omax);
  // how much of the pluck's own fall (first slice to last) survives the capture: ~1 clean, ~0 a wall of gain
  const float fall_in = std::max(1.0f, in_db[0] - in_db[cols - 1]);
  const float fall_out = std::max(0.0f, *std::max_element(out_db, out_db + 3) - out_db[cols - 1]);
  const float keep = fall_out / fall_in;
  const int tex = keep > 0.62f ? 0 : keep > 0.28f ? 1 : 2;
  for (int c = 0; c < cols; c++) {
    const int lv = (int)std::lround((out_db[c] - omax_db + 24.0f) / 24.0f * 7.0f);
    states[c] = lv < 0 ? 0 : 1 + tex * 8 + std::min(7, lv);
  }
}

int Model::latency() const { return rs_ ? rs_->GetLatency() : 0; }

void Model::process(NAM_SAMPLE* in, NAM_SAMPLE* out, int n, bool eco) {
  NAM_SAMPLE* ip[1] = {in};
  NAM_SAMPLE* op[1] = {out};
  if (rs_ && !eco)
    rs_->ProcessBlock(ip, op, n, func_);
  else
    dsp_->process(ip, op, n);
}

static const int kEcoFade = 256;      // samples (~6 ms at 44.1 kHz) of fade-in after the Eco switch

// ------------------------------------------------------------------------------------------------ Engine
Engine::Engine(double host_rate, int max_block) : rate_(host_rate), max_block_(max_block) {
  in_d_.assign(max_block, 0);
  mid_d_.assign(max_block, 0);
  in_f_.assign(max_block, 0);
  out_f_.assign(max_block, 0);
  ir_in_.assign(max_block, 0);
  ir_out_.assign(max_block, 0);
  gate_trigger_.AddListener(&gate_gain_);
  gate_trigger_.SetSampleRate(host_rate);
  dc_.SetParams(recursive_linear_filter::HighPassParams(host_rate, 5.0));   // kDCBlockerFrequency
  update_filters();
}

Engine::~Engine() = default;

void Engine::set_model(std::unique_ptr<Model> m) { model_.publish(m.release()); }
void Engine::set_ir(std::unique_ptr<IRConvolver> ir) { ir_.publish(ir.release()); }
void Engine::collect() {
  model_.collect();
  ir_.collect();
}

void Engine::update_filters() {
  // ToneStack.cpp (BasicNamToneStack::SetParam)
  const float b = params.bass.load(), m = params.mid.load(), t = params.treble.load();
  if (b != bass_v_) {
    bass_v_ = b;
    bass_.SetParams(recursive_linear_filter::BiquadParams(rate_, 150.0, 0.707, 4.0 * (b - 5.0)));
  }
  if (m != mid_v_) {
    mid_v_ = m;
    const double g = 3.0 * (m - 5.0);
    mid_.SetParams(recursive_linear_filter::BiquadParams(rate_, 425.0, g < 0.0 ? 1.5 : 0.7, g));
  }
  if (t != treble_v_) {
    treble_v_ = t;
    treble_.SetParams(recursive_linear_filter::BiquadParams(rate_, 1800.0, 0.707, 2.0 * (t - 5.0)));
  }
  const float gdb = params.gate_db.load();
  if (gdb != gate_v_) {
    gate_v_ = gdb;
    // NeuralAmpModeler::ProcessBlock: time 10 ms, ratio 0.1, open 5 ms, hold 10 ms, close 50 ms
    gate_trigger_.SetParams(dsp::noise_gate::TriggerParams(0.01, gdb, 0.1, 0.005, 0.01, 0.05));
  }
}

void Engine::process(const float* in_l, const float* in_r, float* out_l, float* out_r, int n) {
  model_.take();
  ir_.take();
  update_filters();
  // Longer host blocks are run in max_block pieces (MPC always sends 128; nothing is ever dropped).
  while (n > max_block_) {
    process_chunk(in_l, in_r, out_l, out_r, max_block_);
    in_l += max_block_; if (in_r) in_r += max_block_;
    out_l += max_block_; if (out_r) out_r += max_block_;
    n -= max_block_;
  }
  if (n > 0) process_chunk(in_l, in_r, out_l, out_r, n);
}

void Engine::process_chunk(const float* in_l, const float* in_r, float* out_l, float* out_r, int n) {
  FlushDenormals fd;   // as the official plugin: denormals off while processing
  Model* model = model_.current;

  // input: channels averaged (the plugin in a DAW), input gain
  const double in_gain = std::pow(10.0, params.input_db.load() / 20.0) * (in_r ? 0.5 : 1.0);
  DSP_SAMPLE* in = in_d_.data();
  for (int i = 0; i < n; i++) in[i] = in_gain * ((double)in_l[i] + (in_r ? (double)in_r[i] : 0.0));

  const bool gate = params.gate_on.load();
  DSP_SAMPLE* trig_in[1] = {in};
  DSP_SAMPLE** trig = gate ? gate_trigger_.Process(trig_in, 1, n) : trig_in;

  NAM_SAMPLE* mf_in = in_f_.data();
  NAM_SAMPLE* mf_out = out_f_.data();
  for (int i = 0; i < n; i++) mf_in[i] = (NAM_SAMPLE)trig[0][i];
  if (model) {
    const bool eco = params.eco;
    if (eco != eco_last_) { eco_last_ = eco; eco_fade_ = kEcoFade; }   // the model's history is at the other rate
    model->process(mf_in, mf_out, n, eco);
    for (int i = 0; i < n && eco_fade_ > 0; i++, eco_fade_--)
      mf_out[i] *= 1.0f - (float)eco_fade_ / kEcoFade;                 // fade in over ~6 ms
  }
  else
    memcpy(mf_out, mf_in, n * sizeof(NAM_SAMPLE));
  DSP_SAMPLE* md = mid_d_.data();
  for (int i = 0; i < n; i++) md[i] = (DSP_SAMPLE)mf_out[i];

  DSP_SAMPLE* x[1] = {md};
  DSP_SAMPLE** y = gate ? gate_gain_.Process(x, 1, n) : x;
  if (params.eq_on.load()) y = treble_.Process(mid_.Process(bass_.Process(y, 1, n), 1, n), 1, n);

  IRConvolver* ir = ir_.current;
  if (ir && params.ir_on.load()) {
    float* fi = ir_in_.data();
    float* fo = ir_out_.data();
    for (int i = 0; i < n; i++) fi[i] = (float)y[0][i];
    ir->process(fi, fo, n);
    for (int i = 0; i < n; i++) md[i] = fo[i];
    x[0] = md;
    y = x;
  }
  y = dc_.Process(y, 1, n);

  double out_db = params.output_db.load();
  if (model && params.normalize.load() && model->has_loudness()) out_db += -18.0 - model->loudness();
  const float g = (float)std::pow(10.0, out_db / 20.0);
  for (int i = 0; i < n; i++) {
    const float v = g * y[0][i];
    out_l[i] = v;
    if (out_r) out_r[i] = v;
  }
}

}  // namespace mpcnam
