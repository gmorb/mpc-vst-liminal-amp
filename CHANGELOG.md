# Changelog

## 1.0.2
- Gen 1 (Force, MPC Live / Live II / One / X / Key 61) now runs the cab IR's FFT with NEON, as the Gen 2 build does.
  Before, the Gen 1 build compiled PFFFT's scalar code (PFFFT only uses NEON when asked, and the Gen 1 build never
  asked), so the same plugin was slower on Gen 1 than it needed to be. Same sound: the IR stays the official NAM
  plugin's (cubic resampling, up to 8192 samples, -18 dB), checked against the official direct convolution on 32-bit
  ARM (`ARM=1 tools/test.sh` now also runs ir_test as the device build makes it). Less CPU for the cab, which leaves
  more room for A2 FULL. Nothing else changes: same controls, presets and project format.

## 1.0.1
- Tapping GET TONES, BROWSE TONE3000 or NAME ON PHONE no longer does its work on the tapping thread: the phone page's
  server (network socket, its thread, loading libcurl) now starts on the plugin's background thread. The first tap
  used to take several milliseconds right where it was made, which the catalog's CPU bench counted as an overrun of a
  whole audio block. Nothing else changes; the page opens a moment after the tap, as before.
- Gen 2 (64-bit ARM: MPC Live III and kin, RK3588 / RK3588S): `Liminal-Amp-1.0.1-mpc-aarch64.zip` on the same release,
  made the build kit's way (its release.py and catalog check, pinned newer for packaging only; the page is unchanged), so
  its installer registers it in MPC's 64-bit plugin list and the catalog offers it on Gen 2. Tested under 64-bit ARM
  emulation only (whole plugin test, and the FFT against the official convolution); see docs/GEN2.md. Its cab IR's FFT
  uses NEON.
- The release records a CPU figure measured on a Force with the catalog's bench (`tools/bench_device.sh`, which runs
  it on copies of a capture and an IR in /tmp, so the bench's random parameter sweep can't delete or save anything on
  your card).

## 1.0.0
- New page (v3), in the design system of the companion plugin Liminal Hz (one page, near-black textured background,
  large type, engraved knobs, one accent that means "a live value") with this plugin's own identity: a guitar amp.
  Warm valve glow in the lower right, faint drifting air across the page, amber knob arcs. Same controls, bigger text.
- AMP LIST and CAB LIST: tap-to-load lists (four 44 px rows to a page, "pack . name", PREV / NEXT / CLOSE, the one in use
  marked), opened over their block, as Liminal Hz's IR LIST. Long names slide. Parameters 36-59 (appended; state files
  and automation do not include them).
- Fast start: the folders found last time are kept in a hidden index (`.models-index`, `.irs-index`) in the plugin
  folder. A project's saved amp and cab load from it as soon as the plugin opens, before the full search of the drives
  finishes; the search then refreshes the lists. (Liminal Hz's library code.)
- My Presets (as Liminal Hz): the header shows the preset in use ("My: <name>") beside **MY PRESETS**, which opens a
  list over the AMP block (three to a page, PREV / NEXT / CLOSE). **SAVE** keeps the current sound (every setting, the
  amp and the cab) named after its amp ("Plexi Crunch", "Plexi Crunch 2"...); tap a row to load it (the amp and cab are
  found again even if a card moved; a missing one is named); **DELETE** asks twice. A project remembers the selected
  preset. Presets are `presets/<name>.na2p` text files, kept across upgrades, renamable from a computer (RESCAN).
  Parameters 75-79 (appended; not saved in projects, not automatable); amp_menu gained a third option (Presets).
- The phone pages in the Liminal Amp look (warm black, the valves' amber glow, faceplate cards, the Liminal Amp wordmark,
  Titillium Web shipped in `art/fonts` with its OFL licence, 48 px buttons). The home page offers TONE3000 and
  **My Presets**. **NAME ON PHONE** opens `<address>/presets` (no internet or libcurl needed): save the current sound
  under a typed name, load, rename, delete, and **edit** a preset: its amp and cab (from the plugin's lists) and every
  setting, grouped as on the MPC (input, tone, out, cab, amp), then Save or Save and load.
- The amp's pluck print: under the amp's name, one plucked A string played through the loaded capture, drawn as a
  waveform (14 slices; parameters 61-74, read-only, not saved, not automatable). A clean amp shows a sharp attack that
  fades; crunch holds the note and evens its peaks; high gain is a solid wall. Measured when a capture loads, in the
  background (the capture alone, at its own rate, 1.2 s of audio), then the capture is reset before it plays. The amp's
  detail ("Amp · NAM · @creator", "Model 2 of 4 · Local") moved to the right of its pack line.
- Glowing valves (lower right): three KT88 power valves (glass, silvered getter, wide ribbed anode with two slots, mica,
  heater loops) on a chassis lip show the amp's output level (parameter 60 `tubes`, read-only, 25 states): heaters glow
  dull red in silence and heat up one after another as you play, cooling over about 2.5 s. Not saved, not automatable;
  one peak scan per audio block.
- A2 FULL switch (off by default): plays an A2 capture at its Full size (8 channels) instead of Lite (3 channels),
  reloaded in the background; the detail line then ends in "· Full". On a Gen 1 Force the Full size alone takes
  about 57% of an audio block (Lite about 12%): for simple projects, or faster hardware.
- A2 FULL takes two taps: the first shows a warning ("A2 FULL: very heavy on CPU", "Best on a G2. Own risk.", "Tap A2 FULL again to turn on") and stays
  off; a second tap within six seconds turns it on. Off is one tap. A project or preset saved with it on loads it as saved.
- The phone page uses this plugin's own port (8091) and, when that's taken (another instance), the next free one;
  a page that closed itself after 15 idle minutes opens again on the next press (it used to stay closed).
- Brand: "Liminal Amp" by the manufacturer "Gm0rb" (folder `Gm0rb - VST - Liminal Amp`, `liminal_amp.so`, catalog id
  `liminal-amp`). Renamed before release from "NAM A2", then "Liminal Stack", which MPC's insert slot showed as "Limin.l Stack"
  (also without the space). The internal plugin uid (`NmA2`) and the project and preset formats are unchanged.
- README images (`docs/images/`: banner, page, signal chain, controls, lists, My Presets, pluck print and valves, A2 FULL,
  the phone pages), made by `tools/make_readme_images.py` from the running plugin and its real web server
  (`tools/readme_demo.cpp` drives the host build).
- Header: the preset readout no longer overprints the wordmark (wordmark slightly smaller; the preset, MY PRESETS, TONE3000
  logo and status line moved right; the status line is narrower). Before first use the TONE3000 line says "Tap BROWSE TONE3000".
- The A2 FULL switch's parameter is named "A2 Full" (was "Full", which MPC showed next to the cab's FULL / SHORT). Only the
  shown name changed: projects and presets keep it by key. tools/honest_preview.py now draws a control's name as MPC does (the
  parameter's name, and knob values in upper case), so the README images match the device.
- DELETE (in each block's header): deletes a capture or a cab IR permanently, in two taps (the first asks: "Tap DELETE again to
  delete it permanently", for 4 s). When the last file of a downloaded TONE3000 pack goes, its tone3000.json and
  folder go too. Only .nam / .wav files inside the scanned folders can be deleted.
- Cab IRs: our own WAV reader (engine/wav_reader) instead of AudioDSPTools' loader. It reads the files IR
  libraries ship that the official loader refuses: WAVE_FORMAT_EXTENSIBLE without a `fact` chunk (written by many
  DAWs; the official loader calls them "invalid"), 64-bit float, and stereo (averaged to mono; the chain is mono).
  Identical samples to the official loader wherever it reads a file, except 32-bit integer PCM, which it loads with
  inverted polarity (a `1 << 31` overflow); here the polarity is correct.
- CAB's BROWSE TONE3000 shows cabinet IRs only (gears=cab): reverb "Spaces" IRs need far longer convolution than
  a cab section (they're for the separate Liminal Hz plugin).
- New page (one page, dark): AMP and CAB blocks, each with the tone pack (TONE3000 tone or folder), the model
  name in large type, a detail line (TONE3000: gear type, format and @creator; local: "Model 2 of 4"), PREV/NEXT
  PACK, and BROWSE TONE3000; the T3K mark on a block whose tone came from TONE3000; the full TONE3000 logo with the
  status line; all controls in one row. Laid out to TONE3000's design requirements (entry point at the block,
  partnership splash, loaded-tone attribution, logo before mark); tone images aren't possible on an MPC page.
- ECO switch (off by default): runs the capture at MPC's 44.1 kHz without resampling, for lower CPU; the
  capture's tone then shifts slightly (about 8% lower in frequency). Switching is faded to avoid a click.
- NeuralAmpModelerCore v0.5.4 (was v0.5.2): A2 captures as TONE3000 serves them now take NAM's A2 fast path.
  v0.5.2 required head_scale == 0.01, which trained captures don't have, and ran them on the generic path at
  about 3x the CPU (a real capture measured 34% on a Force for the model alone). The fast path's output matches
  the generic path's within float rounding; tools/a2_shape_test guards it.
- First release: a lightweight, official-sound Neural Amp Modeler for Gen 1 Akai Force / MPC. A2 captures at
  their Lite size with the official NAM plugin's signal chain (noise gate, tone stack, cabinet IR, normalized
  output), resampled 44.1 <-> 48 kHz around the model as the official plugin does.
- Browses .nam models and .wav cabinet IRs in any `models` and `irs` folders on the device's drives (and in the
  plugin's own folder), with subfolders as categories; loads in the background, so browsing never cuts the sound.
- GET TONES: pick A2 captures or cab IRs on TONE3000 from a phone on the same Wi-Fi (TONE3000's Select flow); they
  download into the plugin's models/ and irs/ folders with a credit file, and load. Uses the device's libcurl; no
  account data is stored on the device.
- Projects keep the model and IR by name: found again when a card or folder changes.
- Cabinet IRs by FFT convolution (identical to the official plugin's, far cheaper); optional Short IR length.
- Measured on a Force (RK3288): the whole chain with an 8192-sample IR uses ~16% of an audio block (p99 ~19%).
