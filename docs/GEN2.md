# Liminal Amp on 64-bit MPCs (MPC Live III, MPC XL: RK3588 / RK3588S)

64-bit MPCs run a 64-bit MPC program, so they need a 64-bit build of the plugin. Liminal Amp has one, built from the
same source: same page, parameters, uid, projects and presets (saved projects move between the two kinds of MPC).

## MPC OS 3.10.1 and later: nothing special
Since 1.1.0 the release's **`Liminal-Amp-<version>-MPC-OS-3.10.1.zip`** is one plugin folder for every MPC, in Akai's plugin
folder format: `plugin.json` names `liminal_amp-arm32.so` (`file32`) and `liminal_amp-arm64.so` (`file64`), and each MPC
loads its own. Copy the folder into `<drive>/VST2 Plugins/` and turn on **Preferences → Enhancements (Beta) → Enable
Enhancements** and **External Plugin Support** (see the README). No root access is needed.

The plugin also looks for `models` and `irs` folders on the Live III's internal drive, `/nvme`, which isn't under `/media`.

## Older firmware (MPC OS 3.9.x, modded)
`Liminal-Amp-<version>-mpc-aarch64.zip` is the MPC OS Plugin Catalog's 64-bit package. Its manifest says
`arch: aarch64`, so its `install.sh` refuses a 32-bit device and registers the plugin in MPC's 64-bit plugin list
(`pluginList-arm-64bit`). It needs root (SSH), and on the kit's Live III the target had to be a removable card's Synths
folder (`-t /media/<label>/Synths`). This follows the build kit's Gen 2 design
([mpc-vst-plugins docs/GEN2.md](https://github.com/sd88me/mpc-vst-plugins/blob/main/docs/GEN2.md)).

## The build
- Armv8.2-A as the RK3588's Cortex-A55 cores run it (the A76 cores run it too), glibc 2.17 or newer (MPC OS 3 has 2.39),
  the C++ runtime built in, NEON throughout including the cab IR's FFT (the 32-bit build too since 1.0.2).
- Tested: the whole plugin test (plugin_test), the cab IR's FFT against the official convolution (ir_test) and Akai's
  host simulator (`tools/akai_check.sh`, ARM=1) under qemu-aarch64 on every build. `tested.json` records 1.0.1 on an MPC
  Live 3 (MPC OS 3.9.1).

```
vst/build.sh aarch64          # vst/build/aarch64/liminal_amp.so (Zig; checks exports and glibc)
ARM64=1 tools/test.sh         # plugin_test and ir_test under qemu-aarch64 (qemu-user, libc6-arm64-cross)
```

## Measure it
```
SO_FILE=<the 64-bit liminal_amp .so> tools/bench_device.sh <device-ip>
```
It needs SSH on the device. It builds the catalog's bench for 64-bit ARM, runs it safely in /tmp on copies of one capture
and one IR, and writes `bench/bench-<version>-aarch64.json`. Never run the kit's `tools/bench.sh` against this plugin
directly: its parameter sweep can press DELETE.

## Tested on
Add a `tested.json` entry whose device names it with "Gen2", e.g.
`{"version": "1.1.0", "device": "MPC Live III (Gen2)", "firmware": "3.10.1", "date": "..."}`.
