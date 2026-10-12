# Liminal Amp on Gen 2 devices (64-bit ARM: MPC Live III and kin, RK3588 / RK3588S)

Gen 2 MPCs run a 64-bit MPC program, so a 32-bit (Gen 1) plugin can't load there. Liminal Amp has a second build for
them, `Liminal-Amp-<version>-mpc-aarch64.zip`, on the same release as the Gen 1 zip. This follows the build kit's Gen 2
design ([mpc-vst-plugins docs/GEN2.md](https://github.com/sd88me/mpc-vst-plugins/blob/main/docs/GEN2.md)).

**Status: built and tested under 64-bit ARM emulation only.** The whole plugin test (plugin_test) and the cab IR's FFT
against the official convolution (ir_test) pass under qemu-aarch64; nobody has loaded Liminal Amp on a Gen 2 device yet.
The kit's route itself is proven on a real MPC Live III (MPC OS 5.0.18) with two other plugins (2026-10-11).

## What's in the Gen 2 zip
- The same plugin: same page, parameters, uid, projects and presets (saved projects move between generations).
- Built for 64-bit ARM: Armv8.2-A as the RK3588's Cortex-A55 cores run it (the A76 cores run it too), glibc 2.17 or
  newer (Gen 2 has 2.39), the C++ runtime built in. NEON throughout, including the cab IR's FFT (the Gen 1 build's FFT is
  scalar; see docs/HANDOFF.md).
- Its manifest says `arch: aarch64`, so the zip's `install.sh` refuses to run on a 32-bit device and registers the plugin
  in MPC's 64-bit plugin list (`pluginList-arm-64bit`, confirmed on an MPC Live III by the kit).
- No CPU figure: the release's bench result is from a Gen 1 Force.

## Install
You need root (SSH) on the device. This project doesn't provide a Gen 2 root route; the kit's Gen 2 testers had root by
other means.
- **From the MPC OS Plugin Catalog:** its installer takes the aarch64 zip on an aarch64 device automatically.
- **By hand:** copy `Liminal-Amp-<version>-mpc-aarch64.zip` to the device, unzip it and run its `install.sh` (see its
  `INSTALL.md`). On the kit's MPC Live III the target had to be a removable card's Synths folder (`-t /media/<label>/Synths`):
  `/sdcard` wasn't mounted and the internal Synths folder was read-only.

## Build, test, package
```
vst/build.sh aarch64          # vst/build/aarch64/liminal_amp.so (Zig; checks exports and glibc)
ARM64=1 tools/test.sh         # plugin_test and ir_test under qemu-aarch64 (qemu-user, libc6-arm64-cross)
MPC_VST_RELEASE=<newer kit> REPO=gmorb/mpc-vst-liminal-amp tools/package.sh   # + dist/Liminal-Amp-<v>-mpc-aarch64.zip
```
`MPC_VST_RELEASE` must be a kit whose `release.py` knows Gen 2 (CI pins one: `.github/workflows/build.yml`). The page is
still built with the pinned `MPC_VST`, so both generations get the same tested page.

## Measure it (once a Gen 2 device is reachable)
```
SO_FILE=vst/build/aarch64/liminal_amp.so tools/bench_device.sh <device-ip>
```
It builds the catalog's bench for 64-bit ARM, runs it safely in /tmp on copies of one capture and one IR, and writes
`bench/bench-<version>-aarch64.json` (not put into the release: the catalog's figure is Gen 1's). Never run the kit's
`tools/bench.sh` against this plugin directly: its parameter sweep can press DELETE.

## Tested on
Once it has run on a Gen 2 device, add a `tested.json` entry whose device names it with "Gen2", e.g.
`{"version": "1.0.1", "device": "MPC Live III (Gen2)", "firmware": "5.0.18", "date": "..."}`: the catalog marks
the version "Gen2 · tested" only then.
