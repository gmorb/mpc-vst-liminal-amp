# Liminal Amp for MPC OS

*Liminal Amp: a NAM A2 head and IR cab for MPC.*

An unofficial port; not affiliated with or endorsed by the Neural Amp Modeler project, TONE3000 or Akai
Professional.

A lightweight, official-sound Neural Amp Modeler (NAM): amp captures as a native insert effect for Gen 1 Akai Force and MPC units, with a cabinet
IR, noise gate and bass / middle / treble, and a touchscreen page to browse the captures and IRs on your cards.
A2 captures run at their Lite size by default, which this hardware handles comfortably; A2 FULL plays their
Full size, for simple projects or faster devices. Version @VERSION@.

## What's in the folder
`Gm0rb - VST - Liminal Amp/` is the whole plugin: `liminal_amp.so`, its page (`Plugin Skins/`), `plugin-meta.xml`,
`version.xml`, `NAMA2.json` (parameter list), licences, the phone pages' typeface (`art/fonts`, OFL), and three
folders kept across updates: `models/` for .nam captures, `irs/` for cabinet IRs and `presets/` for My Presets.

## Requirements
- A Gen 1 Akai Force or MPC (Live, Live II, One, X, Key 61)
- SSH access to the device (modded firmware, e.g. MockbaMod); you should be comfortable running shell commands
- The `Synths` folder with `vstscanner.sh` from the Force VST plugins distribution
- NAM captures (.nam; A2 captures recommended, e.g. from TONE3000) and cabinet IRs (.wav, mono)

## Install
**Assumption: you use MockbaMod and its memory card is `/media/662522`** (for anything else see Other firmware).

1. Copy the `Gm0rb - VST - Liminal Amp` folder into the `Synths` folder on the card (`/media/662522/Synths`, next to
   `vstscanner.sh`). Copy the folder itself, directly into `Synths`.
2. Put captures in a `models` folder and IRs in an `irs` folder (see Your captures and IRs).
3. On the device, run the scanner: `ssh ip-of-Force 'sh /media/662522/Synths/vstscanner.sh'`
   (after its first run it is installed as the command `vstscanner`).
4. MPC restarts. The plugin is under the VST category, manufacturer "Gm0rb", as "Liminal Amp".

To remove it, delete the folder and run the scanner again. To update, delete the old folder (keep your
`models`/`irs` if they're inside it), copy the new one, and run the scanner.

## Your captures and IRs
The plugin looks for folders named `models` (for .nam files) and `irs` (for .wav files), in any capitalisation:
- inside its own folder: `Synths/Gm0rb - VST - Liminal Amp/models/` and `.../irs/`;
- anywhere on your cards and the internal drive, up to four folders deep, e.g. `/media/662522/models/` or
  `/media/662522/NAM/irs/`.
Subfolders are categories: `models/Fender/Twin.nam` shows as "Twin" with "Fender" beside its number.
After adding or removing files, press RESCAN on the page.

- **Captures:** A2 captures (from NAM 0.13 / TONE3000, June 2026 on) run at their Lite size. Other NAM
  captures (standard WaveNet, LSTM) also load, but can use far more CPU: try one at a time.
- **IRs:** mono .wav, 16/24/32-bit or 32-bit float, any sample rate (as the official NAM plugin). Longer than 8192
  samples is cut to 8192; IR LENGTH "Short" cuts them to 4096 for lower CPU. A stereo file shows "IR must be mono".

## Getting captures and IRs from TONE3000
Liminal Amp has partnered with TONE3000 to give you access to a massive library of Neural Amp Modeler (NAM) captures
and IRs of real analog gear, created by a global community of musicians. You browse and pick on your phone; the
files download straight to the plugin.

1. The Force (or MPC) and your phone must be on the same Wi-Fi network.
2. Press **GET TONES** on the plugin's page. The TONE3000 line shows an address, e.g. `Open 192.168.1.20:8091`.
3. Open `http://` and that address in your phone's browser, e.g. `http://192.168.1.20:8091`, and choose
   **amp captures (A2)** or **cabinet IRs**.
4. Sign in to TONE3000 (a free account; the phone remembers you), browse or search, and pick a tone.
5. The line shows the download's progress; when it's done the plugin selects the new capture or IR.

Downloads go into the plugin's own folder: `models/TONE3000/<tone>/` and `irs/TONE3000/<tone>/`, each with a
`tone3000.json` that credits the tone's creator, its licence and its TONE3000 page. Captures and IRs belong to their
creators and keep their own licences (many ask for credit). Only A2 captures are fetched, the ones this hardware runs.

- Nothing about your account is stored on the device: each pick signs in through your phone's browser.
- Each plugin has its own page port (this one 8091; Liminal Hz uses 8191), and a second instance takes the next free port
  (8091+1, ...), so several can be open at once: the TONE3000 line always shows the address to open.
- The page on port 8091 is open only after you press GET TONES and closes itself after a while without use.
- It uses the device's own `libcurl`. If the line says `Needs libcurl`, your firmware doesn't have it; the rest of
  the plugin works as normal and you can still copy files onto the card.
- Other lines: `No network` (not connected), `Port 8091 in use` (another plugin is using it), `Canceled`
  (nothing picked on the phone).

## The page
One page, in the same family as Liminal Hz: dark, big text, amber only for live values.
- **AMP** and **CAB** blocks, side by side. Each shows the tone pack (a TONE3000 tone's title, or the folder the
  file is in) and the model or IR name in large type. The T3K mark appears on a block whose tone came from TONE3000.
  - The amp's detail (for TONE3000 tones the gear type, format and creator, e.g. "Amp · NAM · @amalgamaudio"; for
    your own files "Model 2 of 4 · Local") is at the right of its pack line; the cab's is under its name.
  - **The amp's pluck print**, under its name: one plucked note played through the loaded capture, drawn as a
    waveform. A clean amp shows a sharp attack that fades; crunch holds the note and evens it out; high gain is a
    solid wall. It's measured in the background each time a capture loads.
  - ◀ ▶ step through the files (wrapping around); **PREV PACK / NEXT PACK** jump between packs; a Q-Link on the
    name scrolls. A new file loads in the background; the old one keeps playing until it's ready.
  - **AMP LIST / CAB LIST** open a list over the block: four files to a page as "pack · name", the one in use
    marked; tap one to load it (the list stays open), **PREV / NEXT** page, **CLOSE** closes. Long names slide.
  - **BROWSE TONE3000**: opens the phone page for that block (captures or IRs); see Getting captures and IRs.
  - **DELETE**: tap twice to delete the file in use **permanently** (it can't be undone). The first tap only asks
    ("Tap DELETE again to delete it permanently"); it's forgotten after 4 seconds.
- **Fast start:** the plugin remembers where your captures and IRs were found (hidden `.models-index` and
  `.irs-index` files in its folder), so a project's amp and cab load as soon as it opens, before the full search of
  the cards finishes. **RESCAN** searches again after you add or remove files.
- **Header:** the preset in use and **MY PRESETS** (see My Presets), and the **TONE3000 line**: what the phone page is
  doing (the address to open, downloading, done).
- **Controls:** input (Input, Threshold, with **GATE**), tone (Bass, Middle, Treble, with **EQ**), out (Output, with
  **NORMALIZE**: every capture to the same loudness, as the official plugin).
- **amp:** **ECO** (off by default: runs the capture without resampling for lower CPU; the tone shifts slightly,
  about 8% lower in frequency) and **A2 FULL** (off by default: A2 captures at their Full size instead of Lite; a
  little more accurate but about 57% of an audio block on a Gen 1 Force, Lite about 12%, and 75-80% of the MPC's meter
  with a capture and IR; the amp's detail then ends in "· Full"). It takes **two taps** to turn on: the first shows
  "A2 FULL: very heavy on CPU / Best on a G2. Own risk." and stays off, a second tap within six seconds turns it on
  (turning it off is one tap; a project or preset saved with it on opens with it on). **cab:** **CAB** on/off and **FULL / SHORT** IR length.
- **The valves** (lower right) glow with the plugin's output: the heaters idle dull red in silence and warm up one
  after another as you play, cooling over a couple of seconds.
Projects keep your settings and the capture and IR by name: moved to another card or folder, they're found again.
**Updating from an earlier 1.0.0 build:** its parameter list grew, so remove and re-insert the plugin in projects
saved with the earlier build.

## My Presets
- Tap **MY PRESETS** (in the header): your presets open over the AMP block, three to a page (**PREV / NEXT / CLOSE**).
- **SAVE** keeps the current sound: every setting, the amp and the cab. It's named after the amp ("Plexi Crunch",
  "Plexi Crunch 2"...) and selected; the header shows "My: <name>".
- Tap a row to load that preset. If its amp or cab has moved to another card or folder it's found again; if it's gone,
  the settings still apply and the list says which file is missing. A project remembers which preset it uses.
- **DELETE** removes the selected preset: tap twice (the first tap asks). It can't be undone.
- **NAME ON PHONE**: the MPC has no keyboard, so names are typed on your phone. The TONE3000 line in the header shows
  an address such as `Open 192.168.1.20:8091`; open `http://192.168.1.20:8091/presets` on a phone on the same Wi-Fi
  (no internet needed). There you can save the current sound under a name, and **Load**, **Edit**, **Rename** or
  **Delete** any preset. **Edit** shows the preset's amp and cab (pick others from the plugin's lists) and all its
  settings; **Save** writes the preset, **Save and load** also plays it now.
- Each preset is a small text file in the plugin's `presets/` folder (`<name>.na2p`), kept across upgrades. You can also
  rename or delete the files from a computer; press **RESCAN** and the list follows.

## Other firmware
The scanner registers every plugin folder in a `Synths` folder on any drive under `/media`. It needs MPC's
settings file; it looks at MockbaMod's `/data/Settings/MPC/MPC.settings` unless given another as its first argument.

1. SSH into the device. `ls /media` lists the drives; pick one and create `/media/<drive>/Synths` if needed.
2. Copy `vstscanner.sh` and the `Gm0rb - VST - Liminal Amp` folder into it.
3. Find MPC's settings file: `find / -name MPC.settings 2>/dev/null` (usually
   `/media/az01-internal/Settings/MPC/MPC.settings`) and back it up: `cp <that path> <that path>.bak`
4. Run the scanner with it: `sh /media/<drive>/Synths/vstscanner.sh <that path>`
5. If the plugin shows MPC's plain parameter list instead of its page, MPC doesn't look for pages on that drive:
   check `grep SynthContentLocations <settings path>` and use a drive whose `Synths` folder is listed there.

## Notes
- Looking for more (favorites, pitch, delay, reverb)? See "NAM" by jacob-sabella in the MPC OS Plugin Catalog.
  Both can be installed at the same time.
- **CPU:** measured on a Force with a real A2 capture, the whole chain with the longest IR uses about 17% of an
  audio block (up to ~21% on the busiest blocks); MPC's meter shows its total, e.g. 8-10% for the project alone
  and 30-34% with the plugin (27-29% with ECO). Good for one or two instances; shorter IRs cost less.
- **One install method per device:** the scanner rebuilds MPC's whole plugin list from the plugin folders it finds,
  so don't mix this with the catalog's `install.sh` zips.
- Latency: the resampler (MPC runs at 44.1 kHz, captures at 48 kHz) adds 27 samples (0.6 ms).

## Licence and source
This port is MIT-licensed (`LICENSE`); `NOTICE.md` lists every component (NeuralAmpModelerCore, AudioDSPTools and
the official plugin's tone stack by Steven Atkinson, MIT; PFFFT; Eigen, MPL-2.0; and others) with its licence, and
`licenses/` holds the texts. Source code: @SOURCE@
