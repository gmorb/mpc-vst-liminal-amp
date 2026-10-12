# Liminal Stack for MPC OS: handoff summary

Paste this into a new chat together with `mpc-vst-liminal-stack-1.0.0-repo.zip` (and the kit zip) to continue the project.

## RENAMED 2026-10-10 (latest): the product is now "Liminal Amp"
Everything named after "Liminal Stack" was renamed: MPC name "Liminal Amp", folder `Gm0rb - VST - Liminal Amp`, `liminal_amp.so`, catalog id
and file `packaging/catalog/liminal-amp.json` (asset_pattern `Liminal-Amp-*-mpc-armv7.zip`), repo `gmorb/mpc-vst-liminal-amp` (not yet
created), zips `Liminal-Amp-for-MPC-OS-<v>.zip` / `Liminal-Amp-<v>-mpc-armv7.zip`, the wordmark (LIMINAL AMP, plugin page, phone pages,
README images), all docs and copyright lines. Why: MPC's insert slot showed "Liminal Stack" as "Limin.l Stack", and "LiminalStack" also
got the dot. Unchanged on purpose: uid NmA2, state header "nam-a2-lite=1", preset header/extension, folder-index magic.
**Check on the device:** "Liminal Amp" is 11 characters and nearly as wide as "LiminalStack" (Titillium: 520 vs 532 units; "Liminal Hz",
which fits, is 437), so it may still be shortened. If it is, a name the size of "Liminal Hz" (e.g. "Liminal A2", 440) fits.
Sections below use the names of their time ("Liminal Stack", "NAM A2").

## START HERE: state on 2026-10-10 (read this first; older sections below are a dated log and may use old names)
**Product:** "Liminal Stack" (was "NAM A2"; renamed 2026-10-10; tagline "a NAM A2 head and IR cab for MPC"), by Gm0rb, v1.0.0, not yet
published. Internal names are deliberately unchanged so saved projects open: VST uid `NmA2`, state header `nam-a2-lite=1`, preset
header `nam-a2-preset=1` (`.na2p`), folder-index magic `nam-a2 folder index 1`. Names that did change: folder
`Gm0rb - VST - Liminal Stack`, `liminal_stack.so`, catalog `packaging/catalog/liminal-stack.json` (id `liminal-stack`), repo
`mpc-vst-liminal-stack`, zips `Liminal-Stack-for-MPC-OS-<v>.zip` / `Liminal-Stack-<v>-mpc-armv7.zip`. The working directory in the
last session was still called `mpc-vst-nam-a2`; the build does not depend on the directory name.
**User:** Gm0rb (also makes Liminal Hz, the companion plugin whose design system this page follows). Wants honest answers, short
messages, results as files. Device: Gen 1 Force / MPC (RK3288); a Gen 2 (RK3588) build is a future idea.
**What 1.0.0 has:** 80 params (append-only); page v3 in Liminal Hz's design system (near-black, ivory, amber = live values only, Titillium
Web) with its own identity: KT88 valve glow lower right that reacts to the output level (P_TUBES, 25 states), faint air; per-amp "pluck
print" (14 slices, P_WAVE1..14) next to the amp name; AMP LIST / CAB LIST tap lists; fast start (hidden `.models-index`/`.irs-index`);
My Presets (header MY PRESETS, SAVE/load/delete on the page, plus phone pages `/presets` with rename and edit); TONE3000 download over the
phone (own server, port 8091, next free up to +7); ECO; **A2 FULL, which takes two taps** (first tap warns "A2 FULL: very heavy on CPU" /
"Best on a G2. Own risk." / "Tap A2 FULL again to turn on", second within 6 s engages; presets/projects load it as saved; user measured
75-80% CPU with it on); two-tap DELETE confirm. 11 test programs, all pass, also under TSan.
**Tried and dropped:** 14 factory presets (user: "they don't sound any good", fully reverted, no factory code remains); tone-curve strips
(replaced by the pluck print); circle/arch around the tubes; squared glow (now a real radial glow).
**Build / test / package** (needs the kit `mpc-vst-plugins` at commit `3e50b052268d81e6c61a17a1390ced62f0fdff74`, included in the handoff
bundle as `mpc-vst-plugins-3e50b05.zip`; Python 3 + Pillow + Playwright Chromium, Eigen 3.4, g++, Zig via `pip install ziglang`):
```
export MPC_VST=/path/to/mpc-vst-plugins
python3 tools/make_art.py --theme valve --nod air --out vst/art     # page artwork (vst/art is committed; regenerate after art edits)
vst/build.sh host        # host plugin (tests)      ->  vst/build/host/liminal_stack.so
tools/test.sh            # 11 programs;  SANITIZE=tsan|asan tools/test.sh;  ARM=1 adds the qemu device run (slow)
vst/build.sh             # device plugin: armhf, glibc 2.34 (MPC OS 3.x only), exports only VSTPluginMain
REPO=gmorb/mpc-vst-liminal-stack tools/package.sh   # dist/*.zip + SHA256SUMS + RELEASE_NOTES.md; runs catalog_check
python3 tools/honest_preview.py "<skin>/Plugin Skins" vst/params.json <scene.json> out.png   # how MPC will really draw the page
```
Pitfalls: never reorder params (project stores by index); `pkill -f tools/test.sh` kills your own shell; paths with spaces break
`release.py --extra`; the TSan run rebuilds the host .so with sanitizers (rebuild with `vst/build.sh host` after); a test amp model is very
quiet (plugin_test sets input/output to max for the tubes test); readout text must fit its box (measure with PIL at h/1.521 em).
**Open items:** (1) TONE3000 logo and the T3K mark are text placeholders (`vst/art/README.md`: where to get the official assets);
(2) device checks on the Force: is "Liminal Stack" (13 chars) cut off in the plugin list? right-aligned detail text; tubes with transport
stopped; pluck print vs real captures; load time; re-insert the plugin (params grew to 80); the old "Tap DELETE again to delete it
permanently" message (463 px) is wider than its 284 px box; (3) nobody has searched whether another plugin is called "Liminal Stack";
(4) the repo `gmorb/mpc-vst-liminal-stack` does not exist yet: diff against live GitHub before pushing, create the repo, push, tag
v1.0.0 (draft release via `.github/workflows/build.yml`), test on the device, publish, then catalog PR (`liminal-stack.json` into
sd88me/mpc-vst-plugins `catalog/plugins/`); (5) a Gen 2 aarch64 build, if wanted.
**Handoff bundle contents:** this file, `mpc-vst-liminal-stack-1.0.0-repo.zip` (all source: engine/, vst/, tools/, packaging/, docs/, vendored
NAM Core + AudioDSPTools in src/, tests, CI), `mpc-vst-plugins-3e50b05.zip` (the build kit), the two release zips, `SHA256SUMS`,
`RELEASE_NOTES.md`, `session-raw.jsonl` (the full raw session, JSON lines), `session-transcript.txt` (earlier text export, to 2026-10-09).

## What it is
A **lightweight, official-sound Neural Amp Modeler (NAM)** insert effect for Gen 1 Akai Force / MPC (RK3288,
ARMv7 Cortex-A17, MPC OS, VST2 host). Plays NAM **A2** captures at their Lite size with the official NAM plugin's
signal chain, a cabinet IR, and a touchscreen page to browse captures and IRs on the device's drives, plus
**GET TONES**: download captures and IRs from TONE3000 via a phone. MIT-licensed. Owner's GitHub: `gmorb`;
intended repo: `gmorb/mpc-vst-liminal-stack` (not yet published). Version 1.0.0 (unreleased).

Positioning: the small, faithful option. The full-featured alternative is **NAM by jacob-sabella**
(`jacob-sabella/mpc-vst-nam`, catalog id `nam`: pitch, delay, reverb, favorites, in-plugin TONE3000 browser). Both
install side by side (different uid `NmA2` vs `NAm1`, files, pages); the README points to it generously.

## Signal chain (engine/nam_engine.*)
input (L+R average, Input dB) -> noise-gate trigger (AudioDSPTools) -> NAM model at 48 kHz inside AudioDSPTools'
Lanczos ResamplingContainer (44.1 <-> 48 kHz, 27 samples latency) -> noise-gate gain -> tone stack (official:
bass 150 Hz shelf +-20 dB, mid 425 Hz peak, treble 1800 Hz shelf +-10 dB) -> cabinet IR -> 5 Hz DC blocker ->
Output dB (+ Normalize: -18 dB minus the model's loudness) -> mono to both outputs.
- A2 files are SlimmableContainers (Lite 3 ch / Full 8 ch); the engine uses Lite (SetSlimmableSize(0)).
- NAM Core **v0.5.4**'s **A2 fast path** (`NAM_ENABLE_A2_FAST`) is on. Real TONE3000 A2 files take it ONLY from v0.5.3:
  v0.5.2 demanded head_scale == 0.01 (trained files have e.g. 0.01127) -> generic path, ~3x CPU (34% for the model
  on a Force). Fixed by vendoring v0.5.4; tools/a2_shape_test guards it with real-format test files (FiLM slots,
  trained head_scale, metadata: tools/make_test_models.py, checked against a real capture).
- **Cabinet IR** = our FFT convolution (engine/ir_convolver.*, uniformly partitioned, 128-sample blocks, PFFFT with
  NEON), matching the official direct convolution within float rounding (tools/ir_test.cpp, 11 cases, x86 + ARM).
  IR resampled to the host rate (cubic, as official), capped at 8192 samples (Short mode: 4096), -18 dB scaling.
- Models/IRs handed to the audio thread lock-free; the audio thread never allocates, locks, or touches files.

## Measured on a Force (RK3288), % of one 2902 us block (44.1 kHz, 128 frames)
| | mean | p99 |
|---|---|---|
| A2-Lite + resampling | 11.9% | 13.8% |
| + gate + tone stack | 13.1% | 15.0% |
| + 8192-sample IR (whole plugin, worst case) | 16.1% | 19.0% |
A2-Full: 57% (not used). Same-machine vs jacob-sabella's NAM: A2-Lite model ~23% less CPU, cab IR ~37x less.

## Parameters (vst/params.json; order = VST index, NEVER reorder, only append)
0 input (-20..20 dB) · 1 gate_on · 2 gate (-100..0 dB) · 3 bass · 4 mid · 5 treble (0..10) · 6 eq_on ·
7 output (-40..40 dB) · 8 normalize · 9 ir_on ("Cab") · 10 ir_len (Full/Short) · 11 model (stepper, text) ·
12 model_prev · 13 model_next · 14 model_info (readout) · 15 ir (stepper) · 16 ir_prev · 17 ir_next · 18 ir_info ·
19 rescan · 20 get_tones · 21 t3k_info (readout) · 22 eco · 23-34 pack/src/browse/delete · 35 full ·
36-47 AMP LIST (menu, list, close, prev, next, title, rows 1-6) · 48-59 CAB LIST (same) · 60 tubes (read-only display).
Display text capped at 24 chars (mpc-vst-plugins' proven limit). State chunk = text: params + model/IR by path,
relative path and file name (found again if card/folder moved).

## Files: models/ and irs/ (engine/library.*)
Searches folders named `models` (.nam) and `irs` (.wav, any case) in the plugin's own folder (via dladdr) and under
`/media` (4 levels deep), subfolders = categories, sorted; skips hidden and macOS `._` files, doesn't follow links.
Uses POSIX readdir with `_FILE_OFFSET_BITS=64` (NOT std::filesystem: 32-bit readdir/std::filesystem fail with
EOVERFLOW on 64-bit-offset filesystems; found under qemu). IRs must be MONO (official loader). Info line statuses:
"3 / 12  Fender", Searching..., Loading..., Can't load X, Not found: X, IR must be mono, Unsupported bit depth.

## TONE3000 (engine/tone3000.*, http.*, pkce.*)
- Free tier (free/open-source): Select flow only. Publishable key (public client id, safe to embed):
  `t3k_pub_cQ7aniapKwcVBG-zP37dD_i04iqZcb7Q` in `engine/tone3000.cpp` (`#define MPCNAM_T3K_KEY`, overridable).
- GET TONES -> a LAN web page on the device, port **8091** (jacob's uses 8090), shows TONE3000's partnership copy and
  two buttons (captures A2 / cab IRs) -> OAuth 2.0 + PKCE `prompt=select_tone` (`format=nam&architecture=2` or
  `format=ir`), redirect_uri `http://<lan-ip>:8091/cb` -> the user picks on TONE3000 on the phone -> token exchange ->
  GET /tones/{id}, /models?tone_id=... -> downloads (Bearer) into `<plugin folder>/models/TONE3000/<tone>/` or
  `irs/TONE3000/<tone>/` + `tone3000.json` (title, creator, licence, link) -> rescan -> selects the new file.
- libcurl is the device's (`dlopen("libcurl.so.4")`, CA `/etc/ssl/certs/ca-certificates.crt`, as jacob-sabella's
  device-proven code). Without it: "Needs libcurl"; the rest works. Transfers cancellable (closing the plugin).
- No tokens stored; one-time state; replayed/forged callbacks refused; page closes after idle.
- One Tone3000 object per plugin instance (per-instance port conflicts: second instance shows "Port 8091 in use").
- Env overrides for tests: MPCNAM_T3K_API, _PORT, _HOST, _KEY, _DEST. Mock: tools/t3k_mock.py (strict: client id,
  PKCE recomputed, one-time codes, redirect_uri, Bearer).

## Packaging (tools/package.sh; mpc-vst-plugins pinned at 3e50b05 (as Liminal Hz 1.1.0; was 081c247))
- The plugin folder `Gm0rb - VST - Liminal Stack/` is built by the kit's `release.py`: `liminal_stack.so`, `plugin-meta.xml`
  (`%payload-path%`), `version.xml`, `Plugin Skins/`, `LICENSE`, `NOTICE.md`, `NAMA2.json`, `models/`, `irs/`
  (declared `--user-data`: kept across upgrades and uninstall).
- `dist/Liminal-Stack-for-MPC-OS-<v>.zip`: that folder + README, licences (Force distribution / vstscanner route).
- `dist/LiminalStack-<v>-mpc-armv7.zip`: catalog zip with install.sh (only with REPO=owner/name or in CI), checked by
  catalog_check.py --catalog (0 warnings). Catalog entry: `packaging/catalog/liminal-stack.json` (entry check OK).
- Both routes tested on simulated devices (real vstscanner.sh; install.sh incl. upgrade keeping user files).

## Build & test
```
vst/build.sh            # device .so (Zig, armhf glibc<=2.36, cortex-a17 NEON, -fsigned-char, -D_FILE_OFFSET_BITS=64)
vst/build.sh host       # PC build for tests
tools/test.sh           # pkce, http_abi_check, t3k (mock), library, ir (vs official), engine, plugin (incl. GET TONES)
SANITIZE=asan|tsan tools/test.sh ; ARM=1 tools/test.sh   # sanitizers; device .so under qemu-arm
```
Last results: host suite all PASSED; ARM (qemu) all PASSED; ASan clean. TSan found 2 races at plugin close
(effClose reset `w->t3k` while the worker read it) -> fixed (all `w->t3k` access under `w->mu`; close order: join
worker, then destroy Tone3000, then delete) -> see the verification note at the end of this file.
CI (.github/workflows/build.yml): builds, all tests, sanitizers, ARM, packages, DRAFT release on `v*` tags.

## Lessons (avoid repeating)
- Never `pkill -f`/`pgrep -f` with a pattern that appears in your own command line: it matches (kills) itself.
  Track background jobs by PID files. Detach long jobs: `setsid nohup ... > log 2>&1 < /dev/null &`.
- Throttle parallel compiles (JOBS, default 2-4): -O3 C++ needs ~1 GB each; OOM kills leave stale objects.
- Build scripts must fail on any compile error (not link stale objects).
- ARM qemu: 32-bit readdir EOVERFLOW (fixed with LFS). Test harness dirs: unique names per run.

## Open items
1. **Device test** on the Force: load, browse, save/reopen project, CPU with real A2 captures and typical IRs.
2. **GET TONES on the device** with the real TONE3000: never run against the real service yet (only the strict
   mock). Confirm: libcurl present, LAN redirect_uri accepted by TONE3000 (their "LAN-relay" pattern), downloads.
3. TONE3000 compliance: official logo asset not yet on the phone page (text wordmark now); MPC can't show tone
   images. Suggested: email support@tone3000.com describing the integration before publishing.
4. Publish: create `gmorb/mpc-vst-liminal-stack`, push, tag v1.0.0 (draft release), test, publish; then catalog PR
   with `liminal-stack.json` into sd88me/mpc-vst-plugins `catalog/plugins/`.
5. README screenshot is the kit preview (replace with a device photo).
6. **Dragonfly Reverb** (separate project, gmorb/mpc-vst-dragonfly, catalog PR #42 open): its 1.2.0 catalog zips use
   the kit's OLD layout; re-release as 1.2.1 with the kit's new portable layout before #42 is merged.
7. Kit licence: mpc-vst-plugins states no licence (its force-shadow tools are MIT); NOTICE says so.

## Verification of this snapshot (2026-09-30)
After the effClose race fix: release suite all PASSED (pkce, http_abi_check, t3k, library, ir, engine, plugin);
ThreadSanitizer clean (plugin_test incl. GET TONES and close, t3k_test); AddressSanitizer + UBSan clean; device
build OK (exports only VSTPluginMain; needs libc, libm; glibc <= 2.34); package.sh OK. The ARM (qemu) run passed
on the build just before this fix (the fix touches only lock scope and close order in vst/nam_vst.cpp).

## 2026-10-01: CPU fix (NAM Core v0.5.4)
Device report: 57% CPU with a real TONE3000 A2 capture (Fender Super Reverb, amalgamaudio) + IR; the device bench
showed the model alone at 34% (synthetic A2: 12%). Cause: NAM Core v0.5.2's is_a2_shape() required
head_scale == 0.01. Vendored v0.5.4 (1f42f88): real layout passes, x86 7.5% -> 1.7%, fast == generic output within
134 dB. All tests (host 8 programs, TSan, ASan) pass; device build OK. **Pending: confirm on the Force** with
mpcnam-chain-bench on the same capture (expected ~12-16% for the whole chain).

## 2026-10-01: ECO switch (param 22 `eco`, off by default) -- KEPT (confirmed on the Force)
Model::process(eco) bypasses the Lanczos resampler and runs the capture at the host rate (tone shifts ~8% at
44.1 kHz); 256-sample fade-in on switching (the model's history is at the other rate). MPC baseline (user, no
plugin) 8-10%; with plugin + IR 30-34% on MPC's meter, i.e. the plugin ~20-25% there (bench: 17.3% mean,
20.9% p99, 23.9% max). x86: full chain 2.90% -> 2.50% with Eco (~14% less; on the Force ~17.3% -> ~14.9%
expected). Keep only if the device bench shows a noticeable saving; otherwise remove P_ECO before release
(it's appended last, so removing it before 1.0.0 is published changes no other index).
NEON note: NAM's A2 fast path is already NEON-vectorised (429 q-register ops); -funsafe-math-optimizations
changed nothing (bit-identical output). No compiler-flag win left.
Device result (user, MPC meter, same project, capture + IR): 30-34% -> 27-29% with ECO on (MPC alone 8-10%,
so the plugin's share ~20-25% -> ~17-20%). Kept, off by default (the official sound stays the default).

## 2026-10-01: page v2 (UI) -- TONE3000 design requirements, legibility
- Browser renderer ("art": "html": headless Chromium via Playwright; works in the sandbox and needs Playwright +
  Chromium on CI) + vst/skin_post.py (live-text sizes/colours by component definition: name 34 bright, pack 22 dim,
  detail 20 dim, status 20 dim). Mockup: docs/screenshots/page.png (sample live text drawn in).
- Params 23-32 appended: model_/ir_ pack (readout), pack_prev, pack_next (momentary), src (Local/TONE3000: a
  `picture` shows the T3K mark; read-only, reported to the host when it changes), browse (momentary: opens the
  phone page with Continue to that catalogue). Detail line: TONE3000 "Gear · Format · @creator", local
  "Model k of n · Local". Long texts (names, packs, details, status) up to 63 bytes (UTF-8 safe); others 24.
- Library: LibEntry.pack + t3k_* from tone3000.json (written by GET TONES).
- Phone page: partnership splash with Continue; shows <plugin>/art/tone3000-logo.(svg|png) if present.
- TONE3000 requirements met as far as an MPC page allows: entry point at the block (BROWSE TONE3000), partnership
  splash (phone), loaded tone with T3K mark + title + gear type + format + creator, model selector within a pack,
  full logo (strip) before marks. NOT possible: tone images on the MPC page (static images only).
- PLACEHOLDERS: vst/art/tone3000-logo.png and t3k-mark.png are text stand-ins; replace with TONE3000's official
  assets (tone3000.com/api -> Download TONE3000 Logos; see vst/art/README.md). Suggest emailing
  support@tone3000.com with the page before publishing.
- Tests: plugin_test covers packs (next/prev, wrap, loose files), detail lines, source/T3K, Browse -> splash.
  Host suite and TSan PASSED; device build OK.

## 2026-10-01: IR loading (user report: a TONE3000 "Spaces" IR: stereo -> "IR must be mono", mono -> "Can't load")
Cause: AudioDSPTools' loader rejects WAVE_FORMAT_EXTENSIBLE without a `fact` chunk (valid per the WAV spec) and
stereo; also its 32-bit int PCM path inverts polarity. Fix: engine/wav_reader (PCM 16/24/32, float 32/64, EXTENSIBLE
with/without fact, any chunks, any channels; mono_mix averages), bit-identical to the official loader where it
reads (tools/wav_test, generated by tools/make_test_wavs.py). CAB Browse: gears=cab (mock enforces it). The tone
itself (Green-Wood Cemetery Catacombs) is a reverb IR: seconds long, cut to 8192 samples by a cab section ->
the separate Liminal Hz plugin (benchmark first).
Tests: 9 programs PASSED (new wav_test), TSan clean.

## 2026-10-01: DELETE (both plugins)
Param(s) appended: model_delete (33), ir_delete (34); CAB on/off moved to the controls row as "CAB" (momentary). Two taps within 4 s (first arms: info line "Tap DELETE again to remove it");
the worker calls Library::trash: rename to <parent of the models/irs/reverbs folder>/Deleted/<folder>/<pack>/file
(same drive), " (2)" on name clashes, tone3000.json moves with a pack's last audio file, empty pack folder removed;
library skips folders named Deleted. Then rescan; the selection lands on the next file. Tests: library_test (trash),
plugin_test (one tap asks / expires, two taps move, list moves on, rescan doesn't bring it back). tools/test.sh now
rebuilds the PC plugin when any source is newer (a stale build hid one run's failures). All suites, TSan, ASan PASS.

## 2026-10-01: DELETE is permanent (user's request)
Library::remove (replaces Library::trash): unlink the listed .nam/.wav (refused unless the path is
<library folder>/<display><ext>, as scan() lists it, and the extension is .nam/.wav); a pack's last audio file also
takes its tone3000.json and the empty folder. Prompt "Tap DELETE again to delete it permanently", notice "Deleted".
Folders named Deleted are still skipped by scans (files moved there by the earlier version stay out of the list).

## 2026-10-01: brand "Gm0rb"
vst.json vendor "Gm0rb" (was "NAM"): MPC lists the plugin under manufacturer Gm0rb; page/plugin folder
"Gm0rb - VST - NAM A2" (MPC finds a page by "<manufacturer> - VST - <name>"). uid, product name, catalog id, repo name unchanged.
Projects saved with the earlier "NAM" builds (test projects only) may need the plugin re-inserted.

## 2026-10-01: MPC name "NAM A2" (was "A2 Lite")
vst.json / params.json name "NAM A2" by "Gm0rb"; folder "Gm0rb - VST - NAM A2"; catalog zip
NAM-A2-<v>-mpc-armv7.zip (asset_pattern updated; the distribution zip NAM-A2-for-MPC-OS-<v>.zip doesn't
match it). Catalog name already "NAM A2". uid NmA2, .so name and state format unchanged.

## 2026-10-01: phone page ports
NAM A2 8091, Liminal Hz 8191; start() binds the first free of base..base+7 (before the server thread;
the status shows the port in use; "Ports a-b in use" if none). serving_ flag: a page that closed itself (idle)
reopens on the next press (it used to keep showing the address with no server). MPCNAM_T3K_IDLE for tests.
plugin_test: two instances get base and base+1, both answer; idle close and reopen. Host, TSan, ASan, device PASS.

## 2026-10-01: A2 FULL switch (param 35 `full`, off by default)
Model::load(..., full): SetSlimmableSize(full ? 1 : 0), arch "A2 (Full)"/"A2 (Lite)". Wrapper: load_one reloads the
capture when full != full_loaded (as IR LENGTH does for the IR); model_slimmable recorded; detail line + " · Full"
for an A2 capture at Full. Page: "A2 FULL" toggle in the controls row (GATE/EQ/NORMALIZE/ECO re-spaced).
Full on a Gen 1 Force: ~57% for the capture alone (measured earlier); intended for simple projects and Gen 2
(RK3588, Cortex-A76: needs an aarch64 build, future). Name question pending: user suggested "NAM A2 Full";
suggested "NAM A2" (default is Lite).

## 2026-10-01: renamed NAM A2 Lite -> NAM A2
Plays both sizes (Lite default, A2 FULL switch), so the product is "NAM A2": MPC name and catalog name "NAM A2"
by Gm0rb; folder "Gm0rb - VST - NAM A2"; nam_a2.so; catalog id nam-a2 (free; jacob-sabella's is "nam"); repo
mpc-vst-nam-a2; zips NAM-A2-for-MPC-OS-<v>.zip and NAM-A2-<v>-mpc-armv7.zip (asset_pattern NAM-A2-*-mpc-armv7.zip,
which doesn't match the distribution zip); param description NAMA2.json. "A2-Lite" as a capture SIZE is unchanged.
The state header stays "nam-a2-lite=1" (internal) so every saved project opens; set_chunk now checks it by its own
length (was a hard-coded 14, the pattern that broke Echo Vault's projects at its rename).
Convolver name: user asked about "NAM Convolver"; advised against ("NAM" = the Neural Amp Modeler project; the
convolver runs no NAM models); suggested "IR Convolver" (pending).

## 2026-10-09: page v3 (Liminal Hz design system), AMP/CAB LIST, quick start, glowing tubes
User request: the UI like Liminal Hz's but with its own identity (mainly a guitar amp for TONE3000 cabs and IRs), and
Liminal Hz 1.1.0's fast loading of IRs applied to amps and cabs. User picked "design A" (valve glow) with the air layer
("A1": faint wavefront rings and wisps; no rings around the tubes). Name kept: "NAM A2". Not yet device-tested.
- Page: 1280x628, one tab. Header (wordmark, TONE3000 logo, status, RESCAN); AMP and CAB blocks (pack line, name
  stepper, detail line, LIST / PREV PACK / NEXT PACK / DELETE, BROWSE TONE3000, T3K mark); input / tone / out knob groups
  each with its switch above; amp (ECO, A2 FULL) and cab (CAB, IR length) on the right; the tubes lower right.
  Text sizes per Liminal Hz (name 52, knob values 38, knob names 27, toggles 25, pack 34, detail 30, rows 30, status 27).
  vst/layout.conf is hand-maintained; vst/skin_post.py sets the live-text roles; vst/nam.css is Liminal Hz's art css.
- Art: `python3 tools/make_art.py --theme valve --nod air --out vst/art` writes texture.png (material + faceplates),
  knob_big.png (128 frames), wordmark, captions, arrows, menu_panel and tube_0..24.png. Themes valve / grille / tolex and
  nods air / door exist; valve + air shipped. Deterministic. tools/honest_preview.py draws the built skin the way MPC does
  (em = height / 1.521) from the skin's own TUI.json: use it, not a mockup, to judge text size.
- Lists (params 36-59): menu_open is read-only; rows carry text "pack . name" (kRows = 6); a row tap calls the same
  select path as the stepper; paging by PREV/NEXT; long names marquee (1.5 s hold, then a character per 250 ms,
  63-byte display cap). Excluded from the state chunk and from automation (`is_menu`).
- Quick start: engine/library.* now are Liminal Hz's (magic "nam-a2 folder index 1"): hidden `.models-index` /
  `.irs-index` in the plugin folder, validated by magic and a `media=` line; the worker lists the known folders first
  (scan_quick), loads the saved amp/cab at once if found there, then runs the full scan and publishes it only if it
  differs. Test: tools/library_test.cpp and plugin_test (quick start, late arrival, paging, marquee).
- Tubes (param 60, appended): audio thread keeps an output peak (instant attack, release tau 0.35 s) and maps -56..-6 dBFS
  to steps 1-24, below -62 dBFS step 0 (idle). notify() reports the step to the host at most every 9 blocks (going dark
  at once), as Liminal Hz's playhead. The page's `picture key=tubes` (x=880 y=596 w=382 h=118, 25 states) shows the lower
  right of the page at that heat; pictures are opaque, so each state is the page's own pixels there, and the heat is
  faded in away from the picture's edges so no seam shows. Open question: on device, does MPC call processReplacing when
  nothing is playing (the tubes would freeze lit)? Liminal Hz's playhead works the same way; check on the Force.
- Placeholders still: vst/art/tone3000-logo.png and t3k-mark.png (see vst/art/README.md).
- Tests: tools/test.sh all PASSED on the PC (new: tubes light, cool, not saved/automatable). Device build: see below.

### 2026-10-09 (later): realistic valves, round glow, 44 px rows, one type family
- Direction: a boutique valve amp head at night. Warm black, ivory lettering, amber only for live things (knob arcs,
  filaments); Titillium Web only (captions moved off DejaVu Mono).
- Valves (tools/make_art.py `valve()`, `chassis()`): drawn at 4x and downsampled; heaters always glow (idle = dull red),
  signal raises filament temperature (red -> orange -> amber) and the bloom (sigma 4/14/32 px, occluded by anodes and
  getters). The level-dependent light is kept inside round pools around each valve (`tube_pools()`), never the picture's
  rectangle, so no box shows at any state. Room light from the heaters is static (in the page texture).
- Lists: kRows 4 at th=44 (was 6 at 30: under the 44 px touch-target floor). Params amp_row_5/6, cab_row_5/6 stay (append-
  only) and always read "".
- Known limits: the kit's text buttons are a fixed 39 px tall (Liminal Hz is the same; shadow_skin.button_rect only sizes
  buttons with an image look); MPC draws live text without OpenType features (no tabular numerals).

### 2026-10-09 (later still): strings into lattice replace the valves
User picked KT88 valves, then asked for something more "amp modeler" than "real amp"; chose "strings at idle that turn
into the lattice". tools/make_art.py `strings_lattice()` (VALVE_STYLE "strings", the default; NAM_VALVE=kt88|mini|el34|300b
still render valves). Phases by level: ring 0-35% (string blur lens + glow), beads 30-60% (17 sample points per string),
network 50-100% (rows light from the low E = input up; the head links the top node to the output above the high E;
dilations 1, 2, 4, 8), strings dim to rails by 100%. The picture fades its changes out within 18 px of its box. Same
param, same picture position as the tubes (x=880 y=596 w=382 h=118, 25 states).

### 2026-10-09 (late): back to the valves (KT88)
After trying strings, strings-into-lattice, speakers, VU and magic eyes (all still in tools/make_art.py or the session
scratch), the user chose the bulbs: VALVE_STYLE default "kt88". NAM_VALVE=strings|mini|el34|300b renders the others.

### 2026-10-09 (night): the amp's pluck print
User asked for a waveform that honestly shows the loaded sound; a tone-curve mockup was "unnecessary", a plucked-note
waveform was liked, for the amp only (a cab IR barely changes a note's envelope, so its print would copy the amp's).
- Engine: Model::pluck_print(states) (engine/nam_engine.cpp): a Karplus-Strong pluck (110 Hz, loss 0.990, one-pole pick
  filter, peak -10 dBFS, fixed seed) for 1.2 s at the capture's own rate, through the capture alone in 64-sample blocks,
  then ResetAndPrewarm with the load's own arguments. 14 slices: level 0..7 = slice peak from -24 dB to the loudest
  slice; one texture: keep = output fall / input fall (max of the first 3 slices to the last): >0.62 clean, >0.28 crunch,
  else high gain. state = 0 silent, else 1 + texture*8 + level. Called in load_one on the worker before set_model.
  Cost: ~1.2 s of the capture's audio once per load on the worker (A2 Lite on a Force: estimated ~0.15 s of one core,
  Full ~0.7 s; not measured on the device yet).
- Plugin: params 61-74 amp_wave_1..14 (25 options), is_display(): not saved, not automatable, setParameter ignored,
  reported when a load changes them (set_print). No model / failed load: all 0.
- Page: 14 pictures (42 x 44, 25 states: art/wave_0..24.png from make_art.wave_states) at x=32.., layout y=299, placed
  before the AMP LIST panel art so the open list covers them; an inset frame drawn in texture.png. The amp's pack readout
  is now w=300 and its detail readout w=284 talign=right on the same row (skin_post roles shReadout_300x40 / _284x40).
  Right-aligned live text is unverified on the device.
- Tests: plugin_test draws a print at load, a hand-written Linear identity capture gives a clean decaying print
  ([8 7 6 6 5 5 5 4 4 4 3 3 3 3]); the random A2 test nets saturate (all 24).

### 2026-10-10: My Presets and the phone pages' look
User asked for presets "similar to how it works in Liminal Hz", the web server page in the NAM A2 look, and preset
editing on the phone. Ported from Liminal Hz 1.1.0 (engine/presets.*, presets_web.*, web_theme.h, the server's POST,
fonts and route hook), then extended:
- engine/presets.*: `presets/<name>.na2p`, header `nam-a2-preset=1`, real values by key + model_/ir_ path, display,
  file. MPCNAM_PRESETS overrides the folder. tools/presets_test.cpp.
- engine/presets_web.*: /presets (save typed name, Load, Edit, Rename, Delete), /presets/edit?name= (amp and cab selects
  from hooks.choices, settings from hooks.fields grouped by `group`: input, tone, out, cab, amp; numbers clamped,
  checkboxes via hidden 0 + checkbox 1), POST /presets/update (Save / Save and load via hooks.load; hooks.edited makes
  the plugin re-read). tools/presets_web_test.cpp (real socket). No JS except the Delete confirm.
- engine/web_theme.h: NAM A2 palette (#0b0a09, amber #ee9848 only for live/primary, ivory #e6dfd2), wordmark, Titillium
  served from <plugin>/art/fonts (package.sh ships Regular, SemiBold and OFL.txt), 48 px buttons and inputs.
- engine/tone3000.*: start(kind, need_https), set_route(), address(), POST bodies up to 4 KB, /font/*.ttf; home page
  (TONE3000 card + My Presets card). The status line always shows the address (also for NAME ON PHONE).
- Plugin: params 75 preset (readout, header), 76 mp_list, 77 mp_save, 78 mp_delete (two taps), 79 mp_phone.
  amp_menu now has 3 options (Closed, Open, Presets): the AMP block's list area shows My Presets at 2 (kMpRows 3, the
  AMP LIST's title/PREV/NEXT/CLOSE/rows shared, as Liminal Hz shares its menu). Menu helpers take Inst* (menu_count,
  menu_rows, menu_selected...). Snapshot keeps kPresetKeys (input, gate_on, gate, bass, mid, treble, eq_on, output,
  normalize, ir_on, ir_len, eco, full) + the amp and cab in use; named after the amp. Apply: set_real (as the host),
  then Library::find per block (saved for after the first scan; "Amp not found: x" when gone). Chunk: mp_name,
  mp_active (shown again, not re-applied). Worker: reads the folder on MY PRESETS / RESCAN / save / delete / edits.
- Page: header = wordmark, preset readout (242 x 40, ivory 30), MY PRESETS, TONE3000 logo (x 652), status (360 wide),
  RESCAN. Presets panel over the AMP block: menu_panel art, title, PREV/NEXT/CLOSE, 3 rows of 44, SAVE / DELETE /
  NAME ON PHONE at cy 394.
- Tests: plugin_test covers SAVE (named, numbered), recall (settings + amp), project memory, not automatable, DELETE
  twice, NAME ON PHONE, and an edit made over HTTP being applied by the plugin.

### 2026-10-10: factory presets tried and dropped
The user asked for 14 factory presets; with no real captures shippable, the plugin's own rigs were built (8 A2 Lite
amps trained in PyTorch on original amp designs, 5 designed cab IRs). The user listened to a rendered demo and found
they didn't sound good: all of it was removed and the release is the one before (My Presets + phone pages). If this
comes back, the better route is presets that point to real TONE3000 tones (downloaded through the phone page), not
synthetic rigs.

## 2026-10-10: renamed NAM A2 -> Liminal Stack
Chosen over NAM A2, Liminal Amp, Liminal A2 and Liminal dB (dB reads as a level tool): a "stack" is a head on a cab, which is
what the plugin loads (a NAM A2 capture + an IR). "A2" stays as a small badge in the wordmark ("LIMINAL STACK  A2 . by Gm0rb")
and the tagline is "a NAM A2 head and IR cab for MPC" ("NAM" is Steve Atkinson's project, so it is credited, not used as the title).
Changed: vst.json / params.json / param_json.py name "Liminal Stack"; folder "Gm0rb - VST - Liminal Stack"; liminal_stack.so;
catalog id and file `packaging/catalog/liminal-stack.json`; repo name mpc-vst-liminal-stack (not yet published); zips
Liminal-Stack-for-MPC-OS-<v>.zip and Liminal-Stack-<v>-mpc-armv7.zip (asset_pattern Liminal-Stack-*-mpc-armv7.zip);
wordmark (tools/make_art.py, engine/web_theme.h); phone page titles and text; READMEs, CHANGELOG, workflow, copyright lines.
Unchanged on purpose (internal, so saved projects and presets open): uid NmA2, state header "nam-a2-lite=1", preset header
"nam-a2-preset=1" and the .na2p extension, the folder-index magic "nam-a2 folder index 1".
Not yet checked: that "Liminal Stack" (13 characters) is not cut off in the MPC's plugin list on a device (the kit gives no
limit), and that no existing plugin already uses the name. Older entries above keep the names they had at the time.

## 2026-10-10: A2 FULL needs two taps
User measured 75-80% CPU with A2 FULL on (Gen 1), so it is only sensible on a G2. set_parameter -> set_param(w, i, v, trusted): for P_FULL
a tap to on while off does not engage: it arms for kFullArmSeconds (6) and reports the toggle back to 0 (changed[]); display_of shows
"A2 FULL: very heavy on CPU" (model_pack), "Best on a G2. Own risk." (model_info) and "Tap A2 FULL again to turn on" (t3k_info);
a second tap within the window engages; off is one tap (already off: no reload). The worker clears the warning on expiry.
Presets (mp_apply -> set_real, trusted) and projects (set_chunk) apply Full as saved, with no warning. Tests: plugin_test (first tap,
warning text, cancel, expiry, re-warn, preset via the phone edit page). Text widths checked in honest_preview (all fit).

## 2026-10-10 (later): README images
User asked for the repo README to carry images like Liminal Hz's (banner/header, functions and features, web server).
- `docs/images/` (1280x640, the lists 1280x628): banner, page, chain, controls, lists, my-presets, pluck, a2-full, phone, phone-edit.
  `docs/screenshots/` (the old kit preview and one phone shot) is gone. README rewritten to Hz's structure (banner, What it
  looks like, ..., Install, Build); kit commit in its build section fixed to 3e50b05 (it said 081c247; CI already used 3e50b05).
- `tools/make_readme_images.py [--out DIR] [--only a,b]` (needs `vst/build.sh host` done, MPC_VST, numpy, scipy, Pillow, Playwright):
  stages a card of generated sample files (7 ConvNet "amps" y = level * tanh(drive * x) of known drive, 5 cab IRs of noise; one cab pack
  has a tone3000.json so the T3K mark shows), runs the host plugin under `tools/readme_demo.cpp` (stdin commands: p/s/lvl/w/dump), builds
  honest_preview scenes from the plugin's own display text and values (so the text, lists, valves and pluck prints are the engine's),
  takes the phone pages' screenshots from the real server (Playwright, 390 px at 2x), and composes the banners with the page's own art
  (make_art's room and valves, knob_big, wave_N, tube_N). Everything in the images is sample data; the amp and cab names are made up.
  Pluck prints of the three test amps (state 1..8 clean, 9..16 crunch, 17..24 high gain): drive 0.8 clean, 12 crunch, 60 high gain.
- Found while doing it, and FIXED the same day (host tests: all 11 programs pass; device build not redone here):
  1. **Header overlap.** The preset readout ("No preset" / "My: <name>") was drawn over the wordmark's "A2 . by Gm0rb" tag, which
     the rename made longer. layout.conf header now: wordmark x 22 w 300 (was 380), preset readout cx 405 (was 335, still 242 wide),
     MY PRESETS cx 608 (was 548), TONE3000 logo x 692 (was 652), status readout cx 976 w 300 (was cx 964 w 360; skin_post role
     shReadout_300x36). Checked with honest_preview for the longest texts. The wordmark is a little smaller (about 22 px of em).
     The device skin and .so must be rebuilt (vst/build.sh) before release; check the header on the Force.
  2. **Idle status text** now "Tap BROWSE TONE3000" (was "Tap GET TONES", but the page has no GET TONES button); plugin_test updated.
  3. Liminal Hz's README still links the repo as gmorb/mpc-vst-nam-a2 (not changed here).

## 2026-10-10 (later still): TONE3000 assets
The sandbox can't download tone3000.com/TONE3000%20Logos.zip (proxy 403), so the logo and T3K mark are still the text placeholders
(plain Titillium text, not an imitation of their logo). `tools/install_t3k_logos.py --zip ...` installs the official files (see
vst/art/README.md); `tools/package.sh` warns until `vst/art/.t3k-official` exists. Tested only with stand-in images (PNG and SVG).
After installing: `vst/build.sh`, `python3 tools/make_readme_images.py` (the header strip and phone pages show the logo), `tools/package.sh`.
The device already showed the placeholders on the strip and the blocks (user, 2026-10-10), so the mechanism works; only the art is missing.

## 2026-10-10 (late): labels as the device draws them; "A2 Full"
User's Force photo of this build: the header fix works (no overlap). It also showed that MPC draws a control's NAME label from the
parameter's name (params.json "name"), not layout.conf's label ("Threshold", not "THRESH"), and knob values in upper case ("0.0 DB": the
skin's knob Value style has case "Upper Case"). tools/honest_preview.py now does the same (Name labels -> parameter name; textStyle case
applied); all README images regenerated with it. Param 35 `full` renamed "Full" -> "A2 Full" (MPC showed "Full" beside the cab's
FULL / SHORT); display name only (7 chars, within VST2's 8), state and presets use keys. Host tests: all 11 pass. Device .so and
release zips rebuilt; catalog_check OK (GLIBC 2.34 note and the TONE3000 placeholder warning are expected).

## 2026-10-10 (late): MPC name "LiminalStack"
User's Force photo: the insert slot showed "Limin.l Stack": MPC fits 12 characters and replaces a letter with a dot beyond that (the kit
docs don't say so; this answers the open item). User chose "LiminalStack": vst.json / params.json / param_json.py name, so the folder is
`Gm0rb - VST - LiminalStack` (vst/build.sh, tools/package.sh, tools/make_readme_images.py, READMEs) and the kit names the catalog zip
`LiminalStack-<v>-mpc-armv7.zip` (catalog entry asset_pattern updated; the distribution zip stays Liminal-Stack-for-MPC-OS-<v>.zip and
doesn't match it). Unchanged: product/catalog name "Liminal Stack", catalog id, repo, liminal_stack.so, uid NmA2, state and preset headers.
User's own test projects need the plugin re-inserted (folder name changed). All 11 tests pass; device build + catalog_check OK.


## 2026-10-10 (afternoon): CI's ARM run
CI failed 4 checks in plugin_test_arm (NAME ON PHONE / edit page / Save and load / A2 FULL from a preset). Cause: tools/test.sh ran the
ARM (qemu) plugin_test without MPCNAM_T3K_PORT=18094 and MPCNAM_T3K_HOST=127.0.0.1, which the PC run sets; the My Presets phone tests
(added after the last ARM run) talk to 127.0.0.1:18094, so the page opened on 8091 at the LAN address and the rest followed. Not a
network problem (qemu-user shares the host's network). Fixed in test.sh only; ARM=1 tools/test.sh: all 12 programs pass.

## 2026-10-10 (evening): 1.0.1, the CPU bench and tested.json
The catalog listing asked for a CPU figure (docs/BENCH.md in the kit) and a tested.json entry.
- **The kit's bench is unsafe for this plugin as is**: its sweep stage sets random values on ALL 80 parameters, which includes
  DELETE (two taps delete the selected file permanently), My Presets SAVE/DELETE, RESCAN and A2 Full. A dry run on copies deleted
  7 of 12 files and wrote 4 presets. And short stages measure before the capture loads (the bench outruns real time).
  `tools/bench_device.sh <ip> [secs=60]` runs the kit's bench.c, unmodified, on the device in /tmp/la-bench with COPIES of one
  capture and one IR (MPCNAM_MEDIA / _PRESETS / _T3K_DEST pointed there), removes it after, and writes bench/bench-<VERSION>.json.
  SO_FILE= picks the .so (e.g. CI's), BENCH_BIN= a prebuilt bench (Zig builds it otherwise). LOCAL=1 is a dry run on the PC.
- **Found and fixed (1.0.1)**: GET TONES / BROWSE TONE3000 / NAME ON PHONE started the phone server inside setParameter
  (socket, thread, dlopen libcurl: 4-6 ms on x86), which the bench counted as a 156-437% block. Now queued (phone_req) and
  started by the worker. Sweep worst block on x86: 7.5-16.6% (PASS). Tests adjusted to wait for the page; all 11 + TSan pass.
- tools/package.sh uses bench/bench-<VERSION>.json when it exists (or BENCH=), so commit it before tagging and CI's release
  records the figure. tested.json (repo root) is read by the catalog from the default branch.
- VERSION 1.0.1 (vst.json 101). Not yet benched on the device or released.

## 2026-10-10 (night): Gen 2 (aarch64) build
- `vst/build.sh aarch64` -> vst/build/aarch64/liminal_amp.so (Zig aarch64-linux-gnu.2.17, -mcpu=cortex_a55, -DPFFFT_ENABLE_NEON);
  exports only VSTPluginMain, glibc <= 2.17, needs libc/libm/libpthread/libdl. `ARM64=1 tools/test.sh`: plugin_test_a64 and
  ir_test_a64 under qemu-aarch64 (needs libc6-arm64-cross): PASS (FFT vs official direct convolution within 2.6e-6).
- tools/package.sh writes dist/Liminal-Amp-for-MPC-OS-<v>-aarch64.zip when the aarch64 .so exists (no bench figure in it).
  tools/bench_device.sh picks an aarch64 bench for an aarch64 SO_FILE and writes bench/bench-<v>-aarch64.json.
  docs/GEN2.md: probe first (tools/probe_device.sh: MPC binary 64-bit? settings key), manual install, bench, report.
- **Found: PFFFT runs scalar in the Gen 1 (armhf) build too.** PFFFT only uses NEON with PFFFT_ENABLE_NEON, which
  build.sh never set, so SIMD_SZ is 1 on armhf; docs that say "PFFFT with NEON" were wrong. The Force measurements (cab IR
  ~3% of a block) are of that scalar build. Enabling it for armhf is a one-flag change (`-DPFFFT_ENABLE_NEON` in the device
  build) that would make the cab cheaper; it needs ir_test under ARM=1 and a new device bench. Not done: not asked.
- Not tested on a Gen 2 device. Unknown: whether Gen 2's MPC is 64-bit, its plugin-list key, and vstscanner on Gen 2.

## 2026-10-10 (late night): Gen 2 the kit's way
User asked to follow sd88me/mpc-vst-plugins for Gen 2. The kit (HEAD 7c4bf25, 148 commits past our 3e50b05 pin) has docs/GEN2.md:
Gen 2 MPC is 64-bit, no 32-bit libs; release.py names `<Name>-<v>-mpc-aarch64.zip` from the ELF and writes arch into the manifest;
both zips on one release; the catalog finds the aarch64 zip by swapping armv7 for aarch64 in asset_pattern (no second entry);
install.sh refuses on a uname mismatch and registers aarch64 into `pluginList-arm-64bit` (confirmed on an MPC Live III, MPC OS 5.0.18,
with Crate Digger and Profit-08, 2026-10-11); aarch64 glibc ceiling 2.39; tested.json device must contain "Gen2".
- Replaced my hand-made `Liminal-Amp-for-MPC-OS-<v>-aarch64.zip` with that: tools/package.sh runs MPC_VST_RELEASE's release.py
  again with vst/build/aarch64/liminal_amp.so -> dist/Liminal-Amp-<v>-mpc-aarch64.zip, catalog_check OK (0 warnings); manifests:
  same id, uid 4e6d4132, version; arch aarch64 / armv7. CI checks out the kit twice: MPC_VST at 3e50b05 (page: tested on the
  Force) and MPC_VST_RELEASE at 7c4bf25 (release.py, catalog_check). Why not bump the page kit too: the newer gen_vst changes
  the built page (toggles' handler "Q-Link" -> "Toggle Switch", some label justifications) and its new skin_check warns
  "TOUCH CAB and FULL overlap by 9x33 px" (the Cab toggle's touch box overlaps the IR length FULL button; true in the
  current layout as well, worth a look). Bumping the page kit = re-test the page on a device.
- VST2 struct layout checked: pointer-sized fields (intptr_t, pointers), as the kit's 64-bit wrapper.

