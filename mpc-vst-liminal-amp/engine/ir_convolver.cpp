// SPDX-License-Identifier: MIT
// Copyright (c) 2026 the mpc-vst-liminal-amp contributors
#include "ir_convolver.h"
#include "wav_reader.h"

#include <cmath>
#include <cstring>

#include "dsp/Resample.h"
#include "dsp/wav.h"
#include "pffft.h"

namespace mpcnam {

namespace {
constexpr int N = 2 * IRConvolver::kBlock;   // FFT size: overlap-save with kBlock-sample partitions
float* falloc(size_t n) {
  float* p = (float*)pffft_aligned_malloc(n * sizeof(float));
  memset(p, 0, n * sizeof(float));
  return p;
}
}  // namespace

std::unique_ptr<IRConvolver> IRConvolver::make(const std::vector<float>& ir, double ir_rate, double host_rate,
                                               std::string* err, size_t max_len) {
  if (ir.empty() || ir_rate <= 0 || host_rate <= 0) {
    if (err) *err = "empty IR";
    return nullptr;
  }
  // As dsp::ImpulseResponse::_SetWeights: cubic resampling (zero-padded ends), length cap, -18 dB x 48000/rate
  std::vector<float> h;
  if (ir_rate == host_rate) {
    h = ir;
  } else {
    std::vector<float> padded(ir.size() + 2, 0.0f);
    memcpy(padded.data() + 1, ir.data(), ir.size() * sizeof(float));
    dsp::ResampleCubic<float>(padded, ir_rate, host_rate, 0.0, h);
  }
  if (max_len > kMaxLength) max_len = kMaxLength;
  if (h.size() > max_len) h.resize(max_len);
  const float gain = (float)(std::pow(10.0, -18 * 0.05) * 48000.0 / host_rate);
  for (float& v : h) v *= gain;

  std::unique_ptr<IRConvolver> c(new IRConvolver());
  c->length_ = h.size();
  c->parts_ = (int)((h.size() + kBlock - 1) / kBlock);
  c->setup_ = pffft_new_setup(N, PFFFT_REAL);
  if (!c->setup_) {
    if (err) *err = "FFT setup failed";
    return nullptr;
  }
  c->ir_spec_ = falloc((size_t)c->parts_ * N);
  c->fdl_ = falloc((size_t)c->parts_ * N);
  c->time_ = falloc(N);
  c->spec_ = falloc(N);
  c->acc_ = falloc(N);
  c->out_ = falloc(N);
  c->work_ = falloc(N);
  float* tmp = falloc(N);
  for (int p = 0; p < c->parts_; p++) {
    memset(tmp, 0, N * sizeof(float));
    const size_t off = (size_t)p * kBlock;
    const size_t cnt = std::min((size_t)kBlock, h.size() - off);
    memcpy(tmp, h.data() + off, cnt * sizeof(float));             // [partition | zeros]
    pffft_transform((PFFFT_Setup*)c->setup_, tmp, c->ir_spec_ + (size_t)p * N, c->work_, PFFFT_FORWARD);
  }
  pffft_aligned_free(tmp);
  return c;
}

std::unique_ptr<IRConvolver> IRConvolver::load_wav(const std::string& path, double host_rate, std::string* err,
                                                  size_t max_len) {
  // engine/wav_reader: the formats IR libraries ship (EXTENSIBLE with or without `fact`, 64-bit float, stereo),
  // bit-identical to AudioDSPTools' loader where that accepts a file. Several channels: averaged (the chain is mono).
  WavData w;
  if (!read_wav(path, &w, err)) return nullptr;
  return make(w.mono_mix(), w.rate, host_rate, err, max_len);
}

IRConvolver::~IRConvolver() {
  if (setup_) pffft_destroy_setup((PFFFT_Setup*)setup_);
  for (float* p : {ir_spec_, fdl_, time_, spec_, acc_, out_, work_})
    if (p) pffft_aligned_free(p);
}

void IRConvolver::reset() {
  memset(fdl_, 0, (size_t)parts_ * N * sizeof(float));
  memset(time_, 0, N * sizeof(float));
  fdl_pos_ = 0;
  in_n_ = out_r_ = out_n_ = 0;
  if (fifo_) {
    std::fill(out_fifo_.begin(), out_fifo_.end(), 0.0f);
    out_n_ = kBlock;   // the FIFO path runs one block behind
  }
}

void IRConvolver::block(const float* in, float* out) {
  // overlap-save: time_ = [previous block | this block]
  memmove(time_, time_ + kBlock, kBlock * sizeof(float));
  memcpy(time_ + kBlock, in, kBlock * sizeof(float));
  float* x = fdl_ + (size_t)fdl_pos_ * N;
  pffft_transform((PFFFT_Setup*)setup_, time_, x, work_, PFFFT_FORWARD);
  memset(acc_, 0, N * sizeof(float));
  const float scale = 1.0f / N;
  for (int p = 0; p < parts_; p++) {           // Y = sum_p X[t-p] * H[p]
    const int slot = (fdl_pos_ - p + parts_) % parts_;
    pffft_zconvolve_accumulate((PFFFT_Setup*)setup_, fdl_ + (size_t)slot * N, ir_spec_ + (size_t)p * N, acc_, scale);
  }
  pffft_transform((PFFFT_Setup*)setup_, acc_, out_, work_, PFFFT_BACKWARD);
  memcpy(out, out_ + kBlock, kBlock * sizeof(float));   // the valid half
  fdl_pos_ = (fdl_pos_ + 1) % parts_;
}

void IRConvolver::process(const float* in, float* out, int n) {
  if (!fifo_ && n % kBlock == 0) {
    float tmp[kBlock];
    for (int i = 0; i < n; i += kBlock) {
      memcpy(tmp, in + i, sizeof tmp);         // in may equal out
      block(tmp, out + i);
    }
    return;
  }
  if (!fifo_) {                                // first odd-sized block: switch to the FIFO path for good
    fifo_ = true;
    in_fifo_.assign(kBlock, 0.0f);
    out_fifo_.assign(4 * kBlock + 8192, 0.0f);
    in_n_ = 0;
    out_r_ = 0;
    out_n_ = kBlock;                           // one block of latency, pre-filled with silence
  }
  for (int i = 0; i < n; i++) {
    in_fifo_[in_n_++] = in[i];
    if (in_n_ == (size_t)kBlock) {
      float y[kBlock];
      block(in_fifo_.data(), y);
      in_n_ = 0;
      for (int k = 0; k < kBlock; k++) out_fifo_[(out_r_ + out_n_ + k) % out_fifo_.size()] = y[k];
      out_n_ += kBlock;
    }
    out[i] = out_n_ ? out_fifo_[out_r_] : 0.0f;
    if (out_n_) {
      out_r_ = (out_r_ + 1) % out_fifo_.size();
      out_n_--;
    }
  }
}

}  // namespace mpcnam
