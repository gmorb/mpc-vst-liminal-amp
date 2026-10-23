# Notices

**Liminal Amp for MPC OS** is an unofficial port of the Neural Amp Modeler engine to Akai MPC OS devices. It is
not affiliated with or endorsed by the Neural Amp Modeler project, TONE3000, or Akai Professional / inMusic.

This port's own code is MIT-licensed (`LICENSE`). The plugin binary also contains the components below, each
under its own licence (texts in `licenses/` in the release, and in the source tree as noted).

| Component | Authors | Licence | Used for |
|---|---|---|---|
| NeuralAmpModelerCore v0.5.4 (`1f42f88535884450104b8711d7595019afa0495b`) | Steven Atkinson | MIT (`src/nam-core/LICENSE`) | the amp model (A2 fast path) |
| AudioDSPTools (`0827c6c2fc0deced568536142ea86f189e0b98a1`) | Steven Atkinson | MIT (`src/plugin/AudioDSPTools/LICENSE`) | noise gate, tone filters, resampler, WAV loader |
| AudioDSPTools' resampler (from iPlug2) | the iPlug 2 developers; Oli Larkin; John Schwartz / Cockos | zlib-style (in `ResamplingContainer.h`) | 44.1 <-> 48 kHz around the model; modified (see `src/VENDORED.md`) |
| WDL headers | Cockos Incorporated | zlib-style (in the files) | the resampler's buffers |
| NeuralAmpModelerPlugin v0.7.14, tone stack | Steven Atkinson | MIT (`src/plugin/LICENSE`) | bass / middle / treble filters and the signal chain's constants |
| nlohmann/json | Niels Lohmann | MIT (in the header) | reading .nam files |
| PFFFT | Julien Pommier; marton78 and contributors | BSD-like FFTPACK licence (`engine/pffft/LICENSE.txt`) | the cabinet IR's FFT convolution |
| Eigen 3.4.0 (headers) | Eigen authors | MPL-2.0 | the model's matrix maths; source: https://eigen.tuxfamily.org (or Ubuntu's libeigen3-dev) |
| Titillium Web (baked into the page's images) | Accademia di Belle Arti di Urbino | SIL OFL 1.1 | page titles |

Built with [mpc-vst-plugins](https://github.com/sd88me/mpc-vst-plugins) (sd88me) at `3e50b052268d81e6c61a17a1390ced62f0fdff74` (page and plugin-list
generator, Q-Link maps). At that commit that repository states no licence of its own; its bundled force-shadow
tools are MIT (Copyright (c) 2026 sd88me).

## TONE3000
GET TONES uses the TONE3000 API (https://www.tone3000.com/api) under its API Terms of Service, Design Requirements and
Commercial Terms, as a free, open-source integration (free tier: the Select flow). Captures and IRs downloaded from
TONE3000 belong to their creators and carry their own licences; each download's `tone3000.json` records them.

## Trademarks
"Akai", "MPC" and "Force" are trademarks of inMusic Brands; "TONE3000" is its owner's. They are used only to say
which devices this runs on and where A2 captures come from.
