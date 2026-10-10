// SPDX-License-Identifier: MIT
// Copyright (c) 2026 the mpc-vst-liminal-amp contributors
/* ir_convolver.h -- the cabinet IR, as uniformly partitioned FFT convolution (mpc-vst-liminal-amp).
 *
 * Same result as AudioDSPTools' dsp::ImpulseResponse (which the official NAM plugin uses): the IR resampled to the
 * host rate with its cubic resampler, cut to kMaxLength samples, scaled by 10^(-18/20) * 48000 / rate. AudioDSPTools
 * convolves directly (one dot product of the whole IR per sample), far too slow for an RK3288; this does it in the
 * frequency domain with PFFFT (NEON on the device): kBlock-sample partitions, one forward and one inverse FFT per
 * block plus a multiply-add per partition. Zero latency when the host gives multiples of kBlock (MPC: 128); for other
 * block sizes it switches to an input FIFO once, adding kBlock samples of latency.
 * Built off the audio thread (make()), then used only by the audio thread (process()).
 */
#pragma once
#include <memory>
#include <string>
#include <vector>

namespace mpcnam {

class IRConvolver {
 public:
  static constexpr int kBlock = 128;
  static constexpr size_t kMaxLength = 8192;   // as dsp::ImpulseResponse

  // IR samples at their own rate -> ready-to-run convolver at host_rate (nullptr + err on failure)
  // max_len: kMaxLength (as the official plugin) or less (the plugin's "Short" IR length)
  static std::unique_ptr<IRConvolver> make(const std::vector<float>& ir, double ir_rate, double host_rate,
                                           std::string* err, size_t max_len = kMaxLength);
  static std::unique_ptr<IRConvolver> load_wav(const std::string& path, double host_rate, std::string* err,
                                               size_t max_len = kMaxLength);
  ~IRConvolver();

  void process(const float* in, float* out, int n);   // mono, in may equal out
  void reset();
  size_t length() const { return length_; }

 private:
  IRConvolver() = default;
  void block(const float* in, float* out);             // exactly kBlock samples
  void* setup_ = nullptr;                              // PFFFT_Setup*
  size_t length_ = 0;
  int parts_ = 0;
  float* ir_spec_ = nullptr;      // parts_ spectra, each 2*kBlock floats (PFFFT internal order)
  float* fdl_ = nullptr;          // frequency-domain delay line: parts_ input spectra
  int fdl_pos_ = 0;
  float* time_ = nullptr;         // 2*kBlock: previous block | current block
  float* spec_ = nullptr;         // 2*kBlock scratch spectrum
  float* acc_ = nullptr;          // 2*kBlock accumulated spectrum
  float* out_ = nullptr;          // 2*kBlock inverse output
  float* work_ = nullptr;
  // FIFO path for block sizes that aren't multiples of kBlock
  bool fifo_ = false;
  std::vector<float> in_fifo_, out_fifo_;
  size_t in_n_ = 0, out_r_ = 0, out_n_ = 0;
};

}  // namespace mpcnam
