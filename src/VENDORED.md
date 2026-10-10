# Vendored upstream source

Copied unchanged; the only build-time adaptation is engine/eigen_compat.h (force-included, see there).

| Folder | Upstream | Version | Licence |
|---|---|---|---|
| `src/nam-core/` (`NAM/`, `Dependencies/nlohmann`) | https://github.com/sdatkinson/NeuralAmpModelerCore | v0.5.4 (`1f42f88535884450104b8711d7595019afa0495b`) | MIT (Steven Atkinson); nlohmann/json MIT |
| `src/plugin/AudioDSPTools/dsp/` | https://github.com/sdatkinson/AudioDSPTools | `0827c6c2fc0deced568536142ea86f189e0b98a1` (the commit NAM Core v0.5.4 pins) | MIT (Steven Atkinson); `ResamplingContainer/` carries the iPlug2 licence (zlib-style) and WDL's (Cockos, zlib-style) |
| `src/plugin/NeuralAmpModeler/ToneStack.*` | https://github.com/sdatkinson/NeuralAmpModelerPlugin | v0.7.14 (`feb4f8c4fcf4a98be021de1d06cc816642899e50`) | MIT (Steven Atkinson) |
| `engine/pffft/` | https://github.com/marton78/pffft | as in mpc-vst-mpcilium-lite | BSD-like (FFTPACK licence) |

Eigen (MPL-2.0, header-only) is not vendored: builds use Eigen 3.4.0 headers (Ubuntu's libeigen3-dev).
The folder layout keeps upstream's relative includes working (ToneStack.h includes ../AudioDSPTools/dsp/...).

## Local changes
- `src/plugin/AudioDSPTools/dsp/ResamplingContainer/Dependencies/LanczosResampler.h` (2026-09-29; marked in the
  file), scalar `ReadSamples()`: the same Lanczos kernel (A = 12, same table and interpolation) computed in float
  only, summed in 4 lanes. Upstream mixes a double table fraction into every coefficient, which on ARMv7 means
  scalar double-precision math and no NEON. Output matches upstream within float rounding (140 dB below the
  signal on x86 and ARM; `tools/resampler_test.cpp`). Upstream's own SIMD path (`AUDIODSPTOOLS_SIMDE`) is not
  used: upstream marks it unchecked.
- `src/plugin/AudioDSPTools/dsp/wav.cpp` is no longer used for cab IRs (engine/wav_reader.cpp is). Two upstream
  issues found: EXTENSIBLE files without a `fact` chunk are rejected as invalid, and 32-bit integer PCM is scaled
  by 1 / (double)(1 << 31), negative after the int overflow (inverted polarity). tools/wav_test.cpp documents both.
