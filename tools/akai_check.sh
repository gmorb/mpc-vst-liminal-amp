#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 the mpc-vst-liminal-amp contributors
# tools/akai_check.sh: check Liminal Amp with Akai's own tools (inmusic-software/mpc-plugin-builder-sdk), as an
# MPC OS 3.10.1 plugin:
#   1. host_sim (Akai's command-line host that behaves like the MPC's host) runs its test on the PC builds at the MPC's
#      block sizes (128, 32, 512 and an odd 100): load sequence, parameter-0 quirk, in-place processing, state round
#      trip, silence, NaN/Inf, a random parameter sweep, teardown and unload. Two PC builds: GCC's (vst/build.sh host)
#      and Zig's (vst/build.sh zighost), the toolchain and C++ runtime of the device builds.
#   2. With ARM=1, the same test on the two device binaries in the 3.10.1 folder (liminal_amp-arm32.so / -arm64.so),
#      each run by host_sim built for that CPU, under qemu-arm / qemu-aarch64 (slow: a few minutes each).
#   3. validate.py checks the folder in dist/Liminal-Amp-<VERSION>-mpc-sdk.zip: plugin.json, folder name, both .so
#      (architecture, dependencies, glibc, exports), the skins and version.xml, against what host_sim read from the plugin.
# Run after vst/build.sh host, vst/build.sh zighost and tools/package.sh. AKAI_SDK = a checkout of the SDK (default
# ../akai-sdk).
#
# One kind of host_sim FAIL is expected and allowed: "set x but getParameter returned y" on a switch or choice. Like every
# plugin built with mpc-vst-plugins, Liminal Amp treats a value between two options as a one-step nudge from the current
# option (that is how a Q-Link or the data wheel steps a switch on the MPC), so host_sim's random in-between values don't
# read back as written. Any other FAIL, or a crash, fails this check.
#
# host_sim's random sweep presses DELETE and SAVE like any other parameter: it runs on copies in a scratch folder
# (MPCNAM_MEDIA / _PRESETS / _T3K_DEST), never on your files.
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT="$PWD"
VERSION=$(cat VERSION)
AKAI_SDK="${AKAI_SDK:-$ROOT/../akai-sdk}"
ZIG="${ZIG:-python3 -m ziglang}"
[ -f "$AKAI_SDK/tools/validate.py" ] || { echo "no Akai SDK at $AKAI_SDK (set AKAI_SDK)" >&2; exit 1; }
GCC_SO="$ROOT/vst/build/host/liminal_amp.so"
ZIG_SO="$ROOT/vst/build/zighost/liminal_amp.so"
ZIP="$ROOT/dist/Liminal-Amp-$VERSION-mpc-sdk.zip"
[ -f "$GCC_SO" ] || { echo "build the PC plugin first: vst/build.sh host" >&2; exit 1; }
[ -f "$ZIG_SO" ] || { echo "build the Zig PC plugin first: vst/build.sh zighost" >&2; exit 1; }
[ -f "$ZIP" ] || { echo "package first: tools/package.sh (no $ZIP)" >&2; exit 1; }

W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT
HS="$AKAI_SDK/tools/host-sim/host_sim.cpp"
g++ -std=c++17 -O2 -I "$AKAI_SDK/framework" -o "$W/host_sim" "$HS" -ldl

mkdir -p "$W/media" "$W/gen" "$W/folder"
python3 tools/make_test_models.py "$W/gen" >/dev/null
(cd "$W/folder" && unzip -q "$ZIP")
FOLDER=$(ls -d "$W/folder"/*/); FOLDER="${FOLDER%/}"
export MPCNAM_MEDIA="$W/media" MPCNAM_T3K_PORT=18391 MPCNAM_T3K_HOST=127.0.0.1
CHOICES=$(python3 -c "
import json
print(' '.join(str(i) for i, p in enumerate(json.load(open('vst/params.json'))['params']) if len(p.get('options', [])) > 1))")

# a scratch copy of the 3.10.1 folder with the plugin under test and one capture and one IR (again before every run: the
# sweep may delete them); its presets and downloads stay in the copy
fresh() {   # $1 the .so to test, $2 its name in the folder
  rm -rf "$W/plug"; cp -r "$FOLDER" "$W/plug"
  mkdir -p "$W/plug/models/Test" "$W/plug/irs/Test"
  cp "$W/gen/a2_slimmable.nam" "$W/plug/models/Test/"; cp "$W/gen/ir_long.wav" "$W/plug/irs/Test/"
  cp "$1" "$W/plug/$2"
}

fail=0
run_test() {   # $1 label, $2 .so, $3 its name in the folder, $4 block size, $5... the host_sim command
  local label="$1" so="$2" name="$3" b="$4" r bad passes; shift 4
  fresh "$so" "$name"
  set +e
  "$@" "$W/plug/$name" test --seconds 2 --block "$b" > "$W/test.txt" 2>&1
  r=$?
  set -e
  if [ "$r" = 2 ]; then echo "host_sim, $label, block $b: CRASH"; tail -6 "$W/test.txt"; fail=1; return; fi
  bad=$(python3 - "$W/test.txt" "$CHOICES" <<'EOF'
import re, sys
choices = set(int(x) for x in sys.argv[2].split())
bad = []
for line in open(sys.argv[1]):
    if not line.startswith("FAIL"):
        continue
    m = re.match(r'FAIL\s+param (\d+) ".*": set [\d.]+ but getParameter returned', line)
    if m and int(m.group(1)) in choices:
        continue                                   # a switch / choice: the one-step nudge (see the header)
    if re.match(r"FAIL\s+parameter sweep: \d+ of \d+ setParameter calls did not round-trip", line):
        continue                                   # the summary of the lines above
    bad.append(line.rstrip())
print("\n".join(bad))
EOF
)
  passes=$(grep -c '^PASS' "$W/test.txt" || true)
  if [ -n "$bad" ] || [ "$passes" -lt 15 ]; then echo "host_sim, $label, block $b: FAIL"; echo "$bad"; fail=1
  else echo "host_sim, $label, block $b: OK ($passes checks passed; switch nudges allowed)"; fi
}

for b in 128 32 512 100; do
  run_test "PC (GCC)" "$GCC_SO" liminal_amp-arm32.so "$b" "$W/host_sim"
  run_test "PC (Zig, the device toolchain)" "$ZIG_SO" liminal_amp-arm32.so "$b" "$W/host_sim"
done
"$W/host_sim" "$GCC_SO" dump > "$W/params.json"

if [ "${ARM:-0}" = 1 ]; then
  $ZIG c++ -target arm-linux-gnueabihf.2.36 -mcpu=cortex_a17 -std=c++17 -O2 -w -I "$AKAI_SDK/framework" -o "$W/hs32" "$HS" -ldl
  $ZIG c++ -target aarch64-linux-gnu.2.17 -std=c++17 -O2 -w -I "$AKAI_SDK/framework" -o "$W/hs64" "$HS" -ldl
  run_test "Gen 1 device binary (qemu-arm)" "$FOLDER/liminal_amp-arm32.so" liminal_amp-arm32.so 128 \
    env QEMU_LD_PREFIX=/usr/arm-linux-gnueabihf qemu-arm "$W/hs32"
  run_test "Gen 2 device binary (qemu-aarch64)" "$FOLDER/liminal_amp-arm64.so" liminal_amp-arm64.so 128 \
    env QEMU_LD_PREFIX=/usr/aarch64-linux-gnu qemu-aarch64 "$W/hs64"
fi

python3 "$AKAI_SDK/tools/validate.py" "$FOLDER" --params "$W/params.json" | grep -v '^OK     TUI.json: image' || fail=1
[ "$fail" = 0 ] && echo "Akai SDK checks: OK" || { echo "Akai SDK checks: FAILED" >&2; exit 1; }
