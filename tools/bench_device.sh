#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 the mpc-vst-liminal-amp contributors
# tools/bench_device.sh <device-ip> [seconds-per-stage]   -- the catalog's CPU bench on the device, safely
#
# Runs mpc-vst-plugins' tools/bench.c (unmodified) on the device against vst/build/liminal_amp.so, but inside a scratch
# folder in /tmp: the plugin sees only COPIES of one capture and one IR from your card, and its presets, downloads and
# indexes go there too. Everything is removed afterwards. Why: the bench's sweep stage sets random values on every
# parameter, and on this plugin that includes DELETE (two taps delete the selected file, permanently), My Presets SAVE
# and DELETE, RESCAN and A2 Full. Never run the kit's tools/bench.sh against this plugin directly on a card with your files.
#
# Stages run 60 s each by default: the bench runs faster than real time while the plugin loads its capture in the
# background, so short stages measure the plugin with no amp loaded (a misleadingly low figure).
#
#   CAPTURE=/media/.../x.nam IR=/media/.../y.wav   files on the device to copy (default: the first .nam / .wav under /media)
#   SO_FILE=path                                   the device plugin to bench (default: vst/build/liminal_amp.so; e.g. the
#                                                  liminal_amp.so from CI's build or the release zip, so nothing needs building)
#   BENCH_BIN=path                                 the bench built for armhf (default: $MPC_VST/build/bench-armhf;
#                                                  built with Zig if missing, as `python3 -m ziglang cc`)
#   LOCAL=1 tools/bench_device.sh x [secs]         run the same steps on this PC with the host build (a dry run)
# A Gen 2 (aarch64) SO_FILE writes bench/bench-<VERSION>-aarch64.json instead (not used by package.sh: the catalog's
# figure is the Gen 1 one).
# Output: the bench's table (build/bench.txt) and its JSON line in bench/bench-<VERSION>.json, which tools/package.sh
# puts into the release. Commit that file (a LOCAL=1 dry run writes build/bench-local.json instead: never release it).
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT="$PWD"
MPC_VST="${MPC_VST:-$ROOT/../mpc-vst-plugins}"
DEV="${1:?usage: tools/bench_device.sh <device-ip> [seconds-per-stage]}"
SECS="${2:-60}"
mkdir -p build
OUT="$ROOT/bench/bench-$(cat VERSION).json"
file "${SO_FILE:-vst/build/liminal_amp.so}" 2>/dev/null | grep -q aarch64 && OUT="$ROOT/bench/bench-$(cat VERSION)-aarch64.json"
[ "${LOCAL:-0}" = 1 ] && OUT="$ROOT/build/bench-local.json"
mkdir -p "$(dirname "$OUT")"

# the steps on the device: a scratch plugin folder with copies, the plugin confined to it, then clean up
REMOTE='
set -e
L=/tmp/la-bench
rm -rf "$L"; mkdir -p "$L/plug/models/Bench" "$L/plug/irs/Bench" "$L/media"
cap="${CAPTURE:-$(find /media -maxdepth 7 -name "*.nam" ! -name "._*" ! -path "*/.*" 2>/dev/null | head -n 1)}"
ir="${IR:-$(find /media -maxdepth 7 -path "*/irs/*" -name "*.wav" ! -name "._*" ! -path "*/.*" 2>/dev/null | head -n 1)}"
[ -n "$cap" ] && [ -f "$cap" ] || { echo "no .nam capture found: set CAPTURE=/path/on/the/device.nam" >&2; rm -rf "$L"; exit 2; }
echo "capture: $cap"; echo "IR: ${ir:-none (cab off)}"
# four copies of each: the sweep can delete the selected file, and the rest of the stages still have an amp and a cab
for k in 1 2 3 4; do cp "$cap" "$L/plug/models/Bench/capture $k.nam"; [ -n "$ir" ] && cp "$ir" "$L/plug/irs/Bench/ir $k.wav"; done
mv "$SO" "$L/plug/liminal_amp.so"
cd "$L"
set +e
MPCNAM_MEDIA="$L/media" MPCNAM_PRESETS="$L/plug/presets" MPCNAM_T3K_DEST="$L/plug" MPCNAM_T3K_PORT=18391 \
  $PIN "$BIN" "$L/plug/liminal_amp.so" -s "$SECS" -j
r=$?
rm -rf "$L" "$BIN"
exit $r
'

if [ "${LOCAL:-0}" = 1 ]; then
  SO_SRC="$ROOT/vst/build/host/liminal_amp.so"
  [ -f "$SO_SRC" ] || { echo "build the host plugin first: vst/build.sh host" >&2; exit 1; }
  BIN_L="$(mktemp -d)/bench-host"; gcc -O2 -o "$BIN_L" "$MPC_VST/tools/bench.c" -ldl -lm
  SO_L="$(mktemp -d)/liminal_amp.so"; cp "$SO_SRC" "$SO_L"
  TMPMEDIA="$(mktemp -d)"; mkdir -p "$TMPMEDIA/c/models" "$TMPMEDIA/c/irs"
  python3 tools/make_test_models.py "$TMPMEDIA/in" >/dev/null
  cp "$TMPMEDIA/in/a2_slimmable.nam" "$TMPMEDIA/c/models/test.nam"; cp "$TMPMEDIA/in/ir_long.wav" "$TMPMEDIA/c/irs/test.wav"
  env SO="$SO_L" BIN="$BIN_L" SECS="$SECS" PIN="" CAPTURE="$TMPMEDIA/c/models/test.nam" IR="$TMPMEDIA/c/irs/test.wav" \
    sh -c "$REMOTE" | tee build/bench.txt || true      # a FAIL verdict exits 1: still keep its JSON
  rm -rf "$TMPMEDIA"
else
  SO_DEV="${SO_FILE:-vst/build/liminal_amp.so}"
  [ -f "$SO_DEV" ] || { echo "no device plugin at $SO_DEV: build it (vst/build.sh) or set SO_FILE" >&2; exit 1; }
  file "$SO_DEV" 2>/dev/null | grep -q ARM || { echo "$SO_DEV is not an ARM build" >&2; exit 1; }
  # the bench must match the plugin: 32-bit for a Gen 1 build, 64-bit (aarch64) for a Gen 2 build
  if file "$SO_DEV" | grep -q aarch64; then BT="aarch64-linux-gnu.2.17 -mcpu=cortex_a55"; BN=bench-aarch64
  else BT="arm-linux-gnueabihf.2.17 -mcpu=cortex_a17"; BN=bench-armhf; fi
  BIN="${BENCH_BIN:-$MPC_VST/build/$BN}"
  if [ ! -f "$BIN" ]; then
    mkdir -p "$(dirname "$BIN")"
    python3 -m ziglang cc -target $BT -O2 -o "$BIN" "$MPC_VST/tools/bench.c" -ldl -lm
  fi
  scp -q "$BIN" "root@$DEV:/tmp/la-bench-bin"
  scp -q "$SO_DEV" "root@$DEV:/tmp/la-bench-so"
  # pinned to core 1 at normal priority, as the kit's bench.sh does: MPC's audio always wins
  ssh "root@$DEV" "SO=/tmp/la-bench-so BIN=/tmp/la-bench-bin SECS=$SECS PIN='taskset 2' CAPTURE='${CAPTURE:-}' IR='${IR:-}' sh -c '$(printf "%s" "$REMOTE" | sed "s/'/'\\\\''/g")'" \
    | tee build/bench.txt || true                       # a FAIL verdict exits 1: still keep its JSON
fi
grep '^{' build/bench.txt | tail -n 1 > "$OUT" || true
[ -s "$OUT" ] && echo "bench JSON: $OUT" || { echo "no bench result" >&2; exit 1; }
