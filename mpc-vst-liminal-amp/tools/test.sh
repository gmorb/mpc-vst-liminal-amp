#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 the mpc-vst-liminal-amp contributors
# All tests on this PC (run vst/build.sh host first, or let this do it):
#   library_test   the models/ and irs/ search on a made-up /media
#   wav_test       engine/wav_reader vs the official loader (identical where it reads; more formats, stereo)
#   ir_test        the FFT cabinet IR against the official plugin's direct convolution
#   a2_shape_test  A2 files laid out as TONE3000 serves them take NAM Core's A2 fast path (both sizes)
#   engine_test    the amp chain: levels, gate, EQ, swaps while audio runs, cost
#   pkce_test      SHA-256 / base64url / PKCE against published test vectors
#   http_abi_check libcurl option numbers (declared by engine/http.cpp) against the real curl/curl.h
#   t3k_test       TONE3000 end to end against tools/t3k_mock.py (a strict stand-in: PKCE, one-time codes, Bearer)
#   presets_test, presets_web_test  My Presets: the files, and the phone pages (list, save, edit) over a real socket
#   plugin_test    the .so as MPC uses it: browsing, loading, broken and stereo files, projects, rescan, threads,
#                  and GET TONES (with the mock)
# SANITIZE=asan|tsan builds the plugin and plugin_test with AddressSanitizer+UBSan or ThreadSanitizer instead.
# ARM=1 also runs plugin_test against the device .so (vst/build/liminal_amp.so) under qemu-arm (slow: ~10 min).
set -euo pipefail
cd "$(dirname "$0")/.."
R="$PWD"; N="$R/src/nam-core"; A="$R/src/plugin/AudioDSPTools/dsp"
EIGEN="${EIGEN:-/usr/include/eigen3}"
T="$R/vst/.test"; rm -rf "$T"; mkdir -p "$T"
# the mock checks the real client id: the key from its #define (not a comment's "t3k_pub_...")
KEY=$(sed -n 's/^#define MPCNAM_T3K_KEY "\(t3k_pub_[A-Za-z0-9_-]\{8,\}\)"$/\1/p' engine/tone3000.cpp)
[ -n "$KEY" ] || { echo "error: no MPCNAM_T3K_KEY in engine/tone3000.cpp" >&2; exit 1; }
python3 tools/make_test_models.py "$T/in" >/dev/null
IN="$T/in"
INC="-Iengine -Iengine/pffft -I$N -I$N/NAM -I$N/Dependencies/nlohmann -Isrc/plugin/AudioDSPTools -I$EIGEN"
FL="-O2 -std=c++20 -w -D_FILE_OFFSET_BITS=64 -DNAM_ENABLE_A2_FAST -DNAM_SAMPLE_FLOAT -include engine/compat.h $INC"
case "${SANITIZE:-}" in
  asan) SAN="-g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined" ;;
  tsan) SAN="-g -fsanitize=thread" ;;
  *) SAN="" ;;
esac
fail=0
run() { local name="$1"; shift; echo "== $name"; if "$@" > "$T/$name.log" 2>&1; then tail -1 "$T/$name.log"; else grep -E "FAIL|ERROR|runtime error|WARNING: ThreadSanitizer" "$T/$name.log" | head -20; tail -1 "$T/$name.log"; fail=1; fi; }

if [ -z "$SAN" ]; then
  # engine-level tests (objects built once)
  O="$T/obj"; mkdir -p "$O"
  gcc -O2 -w -Iengine/pffft -c engine/pffft/pffft.c -o "$O/pffft.o"
  gcc -O2 -w -Iengine/pffft -c engine/pffft/pffft_common.c -o "$O/pffft_common.o"
  JOBS="${JOBS:-3}"   # parallel compiles: -O2 C++ needs ~1 GB each, so not all at once
  throttle() { while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do wait -n || true; done; }
  for f in $N/NAM/*.cpp $N/NAM/wavenet/*.cpp; do throttle; g++ $FL -c "$f" -o "$O/nam_$(basename ${f%.cpp}).o" & done; wait
  for f in $A/NoiseGate.cpp $A/RecursiveLinearFilter.cpp $A/dsp.cpp $A/wav.cpp $A/ImpulseResponse.cpp; do
    throttle; g++ $FL -c "$f" -o "$O/adt_$(basename ${f%.cpp}).o" & done; wait
  for f in engine/nam_engine.cpp engine/ir_convolver.cpp engine/wav_reader.cpp engine/library.cpp; do throttle; g++ $FL -c "$f" -o "$O/our_$(basename ${f%.cpp}).o" & done; wait
  for o in "$O"/nam_*.o "$O"/adt_*.o "$O"/our_*.o; do [ -s "$o" ] || { echo "error: $o was not built" >&2; exit 1; }; done
  g++ $FL tools/library_test.cpp "$O/our_library.o" -o "$T/library_test" -ldl
  g++ $FL tools/ir_test.cpp "$O/our_ir_convolver.o" "$O/our_wav_reader.o" "$O/adt_ImpulseResponse.o" "$O/adt_dsp.o" "$O/adt_wav.o" "$O"/pffft*.o -o "$T/ir_test"
  g++ $FL tools/engine_test.cpp "$O"/nam_*.o "$O/our_nam_engine.o" "$O/our_ir_convolver.o" "$O/our_wav_reader.o" "$O"/adt_NoiseGate.o \
      "$O/adt_RecursiveLinearFilter.o" "$O/adt_dsp.o" "$O/adt_wav.o" "$O"/pffft*.o -o "$T/engine_test" -lpthread
  g++ $FL tools/pkce_test.cpp engine/pkce.cpp -o "$T/pkce_test"
  g++ -std=c++20 -c tools/http_abi_check.cpp -o "$T/http_abi_check.o"     # compiles only if the constants match
  g++ $FL tools/t3k_test.cpp engine/tone3000.cpp engine/http.cpp engine/pkce.cpp "$O/our_library.o" -o "$T/t3k_test" -ldl -lpthread
  run pkce_test "$T/pkce_test"
  g++ $FL tools/presets_test.cpp engine/presets.cpp "$O/our_library.o" -o "$T/presets_test" -ldl
  run presets_test "$T/presets_test"
  g++ $FL tools/presets_web_test.cpp engine/presets_web.cpp engine/presets.cpp engine/tone3000.cpp engine/http.cpp engine/pkce.cpp \
      "$O/our_library.o" -o "$T/presets_web_test" -ldl -lpthread
  run presets_web_test "$T/presets_web_test"
  echo "== http_abi_check"; echo "PASSED (libcurl constants match curl/curl.h)"
  mkdir -p "$T/t3k/files" "$T/t3k/dest"; cp "$IN/a2_slimmable.nam" "$T/t3k/files/a2.nam"; cp "$IN/ir_short.wav" "$T/t3k/files/ir.wav"
  python3 tools/t3k_mock.py 18990 "$T/t3k/files" "$KEY" & MOCK=$!; sleep 1
  run t3k_test "$T/t3k_test" 18990 18091 "$T/t3k/dest" "$T/t3k/files/a2.nam" "$T/t3k/files/ir.wav"
  kill $MOCK 2>/dev/null || true
  python3 tools/make_test_wavs.py "$T/wavs" > /dev/null
  g++ $FL tools/wav_test.cpp "$O/our_wav_reader.o" "$O/adt_wav.o" "$O/adt_dsp.o" -o "$T/wav_test"
  run wav_test "$T/wav_test" "$T/wavs"
  run library_test "$T/library_test"
  run ir_test "$T/ir_test"
  g++ $FL tools/a2_shape_test.cpp "$O"/nam_*.o -o "$T/a2_shape_test" -lpthread
  run a2_shape_test "$T/a2_shape_test" "$IN/a2_slimmable.nam"
  run engine_test "$T/engine_test" "$IN/a2_slimmable.nam" "$IN/ir_long.wav" "$IN/ir_short.wav"
fi

# the plugin itself
if [ -n "$SAN" ]; then HOST_FLAGS="-O1 $SAN" JOBS="${JOBS:-3}" vst/build.sh host > "$T/build.log" 2>&1 || { tail -20 "$T/build.log"; exit 1; }
# (the PC plugin is rebuilt when it's missing or older than any source: never test a stale build)
elif [ ! -f vst/build/host/liminal_amp.so ] || [ -n "$(find engine vst src -newer vst/build/host/liminal_amp.so \( -name '*.cpp' -o -name '*.c' -o -name '*.h' -o -name '*.hpp' -o -name '*.json' -o -name '*.conf' -o -name '*.py' \) -not -path 'vst/build/*' 2>/dev/null | head -1)" ]; then vst/build.sh host > "$T/build.log" 2>&1 || { tail -20 "$T/build.log"; exit 1; }; fi
g++ -O1 -std=c++20 -w $SAN tools/plugin_test.cpp -o "$T/plugin_test" -ldl -lpthread
mkdir -p "$T/t3k/files"; cp "$IN/a2_slimmable.nam" "$T/t3k/files/a2.nam"; cp "$IN/ir_short.wav" "$T/t3k/files/ir.wav"
python3 tools/t3k_mock.py 18993 "$T/t3k/files" "$KEY" & MOCK=$!; sleep 1
run "plugin_test${SANITIZE:+_$SANITIZE}" env ASAN_OPTIONS=detect_leaks=1 MPCNAM_T3K_API=http://127.0.0.1:18993/api/v1 \
    MPCNAM_T3K_PORT=18094 MPCNAM_T3K_HOST=127.0.0.1 "$T/plugin_test" vst/build/host/liminal_amp.so \
    "$IN/a2_slimmable.nam" "$IN/a2_3.nam" "$IN/ir_long.wav" "$IN/ir_short.wav"
kill $MOCK 2>/dev/null || true

if [ -n "$SAN" ]; then   # the TONE3000 flow's threads under the same sanitizer
  g++ -O1 $SAN -std=c++20 -w -D_FILE_OFFSET_BITS=64 -Iengine -Isrc/nam-core/Dependencies/nlohmann tools/t3k_test.cpp \
      engine/tone3000.cpp engine/http.cpp engine/pkce.cpp engine/library.cpp -o "$T/t3k_test_san" -ldl -lpthread
  mkdir -p "$T/t3k/files" "$T/t3k/dest"; cp "$IN/a2_slimmable.nam" "$T/t3k/files/a2.nam"; cp "$IN/ir_short.wav" "$T/t3k/files/ir.wav"
  python3 tools/t3k_mock.py 18990 "$T/t3k/files" "$KEY" & MOCK=$!; sleep 1
  run "t3k_test_$SANITIZE" env ASAN_OPTIONS=detect_leaks=1 "$T/t3k_test_san" 18990 18091 "$T/t3k/dest" "$T/t3k/files/a2.nam" "$T/t3k/files/ir.wav"
  kill $MOCK 2>/dev/null || true
fi

if [ "${ARM:-0}" = 1 ]; then
  ZIG="${ZIG:-python3 -m ziglang}"
  $ZIG c++ -target arm-linux-gnueabihf.2.36 -mcpu=cortex_a17 -O2 -std=c++20 -w tools/plugin_test.cpp -o "$T/plugin_test_arm" -ldl -lpthread 2>/dev/null
  run plugin_test_arm env QEMU_LD_PREFIX=/usr/arm-linux-gnueabihf MPCNAM_EXPECT_NO_CURL=1 qemu-arm "$T/plugin_test_arm" "$R/vst/build/liminal_amp.so" \
      "$IN/a2_slimmable.nam" "$IN/a2_3.nam" "$IN/ir_long.wav" "$IN/ir_short.wav"
fi
exit $fail
