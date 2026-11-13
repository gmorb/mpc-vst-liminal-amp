# Liminal Amp

*A NAM A2 head and IR cab for MPC.* Version @VERSION@, by Gm0rb.

A lightweight, official-sound Neural Amp Modeler (NAM) amp as a native insert effect for Akai MPC and Force units:
A2 captures with a cabinet IR, noise gate and bass / middle / treble, processed as the official NAM plugin processes
them, and a touchscreen page to browse the captures and IRs on your drives. A2 captures run at their Lite size by
default; A2 FULL plays their Full size, for simple projects or faster devices.

An unofficial plugin: not affiliated with or endorsed by the Neural Amp Modeler project, TONE3000 or Akai Professional.
Third-party plugins run inside the MPC application and are not supported by inMusic: save your work often.

## Install (MPC OS 3.10.1 and later)
1. On the MPC: **Preferences → Enhancements (Beta)**, turn on **Enable Enhancements** and **External Plugin Support**.
   (These switches exist from MPC OS 3.10.1; update the MPC first if you don't see them.)
2. Copy the folder **`Gm0rb - VST - Liminal Amp`** (the whole folder, not its contents) into a folder named exactly
   **`VST2 Plugins`** at the root of a USB drive, an SD card or the MPC's internal drive. Create `VST2 Plugins` if
   it isn't there.
3. Put the drive in the MPC. On an audio track, add an insert effect: **External Plugins → Liminal Amp**.

The same folder works on every MPC: 32-bit models (Force, MPC X, X SE, Live, Live II, One, One+, Key 61, Key 37) load
`liminal_amp-arm32.so`, 64-bit models (MPC Live III, MPC XL) load `liminal_amp-arm64.so`; `plugin.json` tells each
which. No root access, scripts or firmware changes are needed.

- **Update:** replace the folder with the new one (keep your `models`, `irs` and `presets` folders if you keep them
  inside it), then remove and re-add the plugin, or restart the MPC.
- **Remove:** delete the folder. Projects that used it open without it.
- **Older firmware (MPC OS 3.9.x and earlier, modded):** use the `-mpc-armv7.zip` / `-mpc-aarch64.zip` package and its
  `install.sh`, or the MPC OS Plugin Catalog's Plugin Manager.

## Your captures and IRs
The plugin looks for folders named `models` (for .nam captures) and `irs` (for .wav IRs), in any capitalisation:
- inside its own folder: `VST2 Plugins/Gm0rb - VST - Liminal Amp/models/` and `.../irs/`;
- anywhere on your drives, up to four folders deep: USB drives and cards (`/media/...`) and the MPC Live III's
  internal drive (`/nvme`), e.g. `<drive>/models/` or `<drive>/NAM/irs/`.
Subfolders are packs: `models/Fender/Twin.nam` shows as "Twin" in the pack "Fender". After adding or removing files,
press **RESCAN** on the page.

- **Captures:** A2 captures (NAM 0.13 / TONE3000, June 2026 on) run at their Lite size. Other NAM captures (standard
  WaveNet, LSTM) also load, but can use far more CPU: try one at a time.
- **IRs:** .wav, 16/24/32-bit or 32-bit float, any sample rate, as the official NAM plugin. A stereo IR is mixed to
  mono. Longer than 8192 samples is cut to 8192 (FULL); **SHORT** cuts to 4096 for lower CPU.

## Getting captures and IRs from TONE3000
TONE3000 is a community library of NAM captures and IRs of real gear. You browse and pick on your phone; the files
download straight to the plugin.

1. The MPC and your phone must be on the same Wi-Fi network.
2. Tap **BROWSE TONE3000** above the amp (for A2 captures) or above the cab (for IRs). The TONE3000 line in the header
   shows an address, e.g. `Open 192.168.1.20:8091`.
3. Open `http://` and that address in your phone's browser, e.g. `http://192.168.1.20:8091`.
4. Sign in to TONE3000 (a free account; the phone remembers you), browse or search, and pick a tone.
5. The line shows the download's progress; when it's done the plugin selects the new capture or IR.

Downloads go into the plugin's own folder, `models/TONE3000/<tone>/` and `irs/TONE3000/<tone>/`, each with a
`tone3000.json` that credits the tone's creator, its licence and its TONE3000 page. Captures and IRs belong to their
creators and keep their own licences. Only A2 captures are fetched.

- Nothing about your account is stored on the MPC: each pick signs in through your phone's browser.
- The page uses port 8091 (a second instance takes the next free one, up to 8098); it opens only after you tap a phone
  button and closes itself after 15 minutes without use.
- Downloads use the MPC's own `libcurl`. If the line says `Needs libcurl`, the firmware doesn't offer it: everything
  else works, and you can copy files onto a drive instead.
- Other lines: `No network` (not connected), `Ports 8091-8098 in use`, `Canceled` (nothing picked on the phone).

## The page
- **AMP** and **CAB** blocks: the pack and the capture / IR name, ◀ ▶ to step through files, **PREV PACK / NEXT PACK**,
  **AMP LIST / CAB LIST** (tap-to-load lists, four to a page), **BROWSE TONE3000**, and **DELETE** (two taps; deletes
  the file in use from the drive, permanently). A **T3K** mark shows on a block whose file came from TONE3000.
- **The pluck print** under the amp's name: one plucked note through the capture. Clean fades, crunch holds, high gain
  is a solid wall.
- **Header:** the My Preset in use, **MY PRESETS**, the TONE3000 line, and **RESCAN**.
- **Controls:** input (Input, Threshold, **GATE**), tone (Bass, Middle, Treble, **EQ**), out (Output, **NORMALIZE**:
  every capture to the same loudness, as the official plugin); amp: **ECO** (lower CPU, tone shifts about 8% lower in
  frequency) and **A2 FULL** (the capture's Full size: very heavy on a 32-bit MPC; two taps to turn on, the first shows
  a warning); cab: **CAB** on/off and **FULL / SHORT** IR length.
- **The valves** glow with the output level.
- **Fast start:** the plugin remembers where your folders are (hidden `.models-index` / `.irs-index` files), so a
  project's amp and cab load at once.
Projects keep every setting and the capture and IR by path and name: moved to another drive or folder, they're found again.

## My Presets
- **MY PRESETS** opens your presets over the amp block. **SAVE** keeps the whole rig (every setting, amp and cab),
  named after the amp. Tap a row to load it; **DELETE** (two taps) removes it.
- **NAME ON PHONE:** the address in the header with `/presets`, e.g. `http://192.168.1.20:8091/presets` (no internet
  needed): save under a name, **Load**, **Edit** (amp, cab and every setting), **Rename**, **Delete**.
- Presets are small files in the plugin's `presets/` folder (`<name>.na2p`). Keep that folder when you update.

## Notes
- **CPU:** on a 32-bit Force, the whole chain with the longest IR is about 16% of an audio block; one or two instances
  fit alongside a project. ECO, SHORT and avoiding non-A2 captures lower it.
- Latency: 27 samples (0.6 ms), from resampling 44.1 ↔ 48 kHz around the capture.
- Looking for more (favourites, effects)? See "NAM" by jacob-sabella in the MPC OS Plugin Catalog; both can be installed.

## Licence and source
MIT-licensed (`LICENSE`). `NOTICE.md` lists every component (NeuralAmpModelerCore, AudioDSPTools and the official
plugin's tone stack by Steven Atkinson, MIT; PFFFT; Eigen, MPL-2.0; Titillium Web, OFL; and others) with its licence;
`licenses/` holds the texts. Source code: @SOURCE@
