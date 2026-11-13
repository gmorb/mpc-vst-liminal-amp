#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 the mpc-vst-liminal-amp contributors
# Package Liminal Amp (run vst/build.sh and vst/build.sh aarch64 first), with REPO=owner/name (GITHUB_REPOSITORY in CI):
#   dist/Liminal-Amp-<VERSION>-mpc-sdk.zip      MPC OS 3.10.1 and later, every MPC: ONE plugin folder in Akai's format
#                                               (plugin.json, file32 + file64) to copy into <drive>/VST2 Plugins/
#   dist/Liminal-Amp-<VERSION>-mpc-armv7.zip    the MPC OS Plugin Catalog packages (Gen 1 / Gen 2), with install.sh for
#   dist/Liminal-Amp-<VERSION>-mpc-aarch64.zip  older modded firmware; their folders carry a plugin.json too
#   SHA256SUMS, RELEASE_NOTES.md
# MPC_VST is the kit the page is built with (pinned, tested on the device). MPC_VST_RELEASE (default MPC_VST) is the kit
# whose release.py, catalog_check.py and sdk_zip.py package and check the zips: a newer one that writes plugin.json
# (MPC OS 3.10.1) and knows Gen 2, pinned in .github/workflows/build.yml.
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION=$(cat VERSION)
MPC_VST="${MPC_VST:-$PWD/../mpc-vst-plugins}"
MPC_VST_RELEASE="${MPC_VST_RELEASE:-$MPC_VST}"
REPO="${REPO:-${GITHUB_REPOSITORY:-}}"
SOURCE="${REPO:+https://github.com/$REPO (tag v$VERSION)}"
SOURCE="${SOURCE:-the GitHub repository this release was published from (tag v$VERSION)}"
B=vst/build
[ -f "$B/liminal_amp.so" ] || { echo "run vst/build.sh first" >&2; exit 1; }
FOLDER="Gm0rb - VST - Liminal Amp"
rm -rf dist; mkdir -p dist/work
python3 tools/param_json.py dist/work/NAMA2.json
sed -e "s|@VERSION@|$VERSION|" -e "s|@SOURCE@|$SOURCE|" packaging/README.md > dist/work/README.md

[ -f vst/art/.t3k-official ] || echo "warning: the TONE3000 logo and T3K mark in vst/art are text placeholders; install TONE3000's official files first (tools/install_t3k_logos.py, vst/art/README.md)" >&2
# 1. the plugin folder, built by mpc-vst-plugins' release.py (its portable layout, docs/CATALOG_SPEC.md): the .so,
#    plugin-meta.xml, version.xml and the page, plus our extras; models/ and irs/ are the user's folders (kept by the
#    installer across upgrades and uninstalls)
REL_ARGS=(--so "$B/liminal_amp.so" --skin "$B/skin/$FOLDER" --entry "$B/pluginlist-entry.xml" --version "$VERSION"
          --id liminal-amp --license MIT
          --about "$(python3 -c "import json;print(json.load(open('vst/vst.json'))['about'])")"
          --extra "LICENSE:LICENSE" --extra "NOTICE.md:NOTICE.md" --extra "dist/work/NAMA2.json:NAMA2.json"
          --extra "packaging/models-README.txt:models/README.txt" --extra "packaging/irs-README.txt:irs/README.txt"
          --extra "vst/art/tone3000-logo.png:art/tone3000-logo.png"
          --extra "packaging/presets-README.txt:presets/README.txt"
          --extra "dist/work/README.md:README.md"
          --extra "src/nam-core/LICENSE:licenses/NeuralAmpModelerCore-MIT.txt"
          --extra "src/plugin/AudioDSPTools/LICENSE:licenses/AudioDSPTools-MIT.txt"
          --extra "src/plugin/LICENSE:licenses/NeuralAmpModelerPlugin-MIT.txt"
          --extra "engine/pffft/LICENSE.txt:licenses/PFFFT.txt"
          --extra "$MPC_VST/tools/html_art/fonts/TitilliumWeb-Regular.ttf:art/fonts/TitilliumWeb-Regular.ttf"
          --extra "$MPC_VST/tools/html_art/fonts/TitilliumWeb-SemiBold.ttf:art/fonts/TitilliumWeb-SemiBold.ttf"
          --extra "$MPC_VST/tools/html_art/fonts/OFL.txt:art/fonts/OFL.txt"
          --user-data models --user-data irs --user-data presets
          --requires "Optional: Wi-Fi and a phone for TONE3000 downloads and naming presets (a small page on the local network)")
[ -n "$REPO" ] && REL_ARGS+=(--repo "$REPO")
# the device bench's JSON line (tools/bench_device.sh writes bench/bench-<version>.json; commit it so CI's release has it):
# recorded in the release's INSTALL.md and manifest. BENCH=file overrides; none: the release has no CPU figure.
[ -z "${BENCH:-}" ] && [ -s "bench/bench-$VERSION.json" ] && BENCH="bench/bench-$VERSION.json"
if [ -n "${BENCH:-}" ]; then [ -s "$BENCH" ] || { echo "error: BENCH=$BENCH is empty or missing" >&2; exit 1; }; REL_ARGS+=(--bench "$BENCH"); fi
python3 "$MPC_VST_RELEASE/tools/release.py" "${REL_ARGS[@]}" -o dist/work >/dev/null
CZ=$(ls dist/work/*-mpc-armv7.zip)

# 2b. Gen 2 (aarch64): when vst/build/aarch64/liminal_amp.so exists (vst/build.sh aarch64), release.py once more with that
#     .so: it names the zip from the ELF (Liminal-Amp-<v>-mpc-aarch64.zip) and its manifest says arch aarch64, so the
#     installer registers it in MPC's 64-bit plugin list. Same id, uid and version as the armv7 zip; both go on the same
#     release (the catalog finds this one by swapping armv7 for aarch64 in asset_pattern). No --bench: that's Gen 1's.
A64SO="$B/aarch64/liminal_amp.so"
[ -f "$A64SO" ] || { echo "run vst/build.sh aarch64 first: every release has both builds (the 3.10.1 folder holds both)" >&2; exit 1; }
if [ -f "$A64SO" ]; then
  [ -n "$REPO" ] || { echo "error: the Gen 2 zip needs REPO=owner/name (a catalog zip)" >&2; exit 1; }
  R64=(); skip=0
  for x in "${REL_ARGS[@]}"; do
    if [ "$skip" = 1 ]; then skip=0; continue; fi
    case "$x" in --bench) skip=1; continue;; esac
    R64+=("$x")
  done
  for k in "${!R64[@]}"; do [ "${R64[$k]}" = "--so" ] && R64[$((k + 1))]="$A64SO"; done
  mkdir -p dist/work64
  python3 "$MPC_VST_RELEASE/tools/release.py" "${R64[@]}" -o dist/work64 >/dev/null
  Z64=$(ls dist/work64/*-mpc-aarch64.zip 2>/dev/null || true)
  [ -n "$Z64" ] || { echo "error: no -mpc-aarch64.zip: MPC_VST_RELEASE's release.py is too old for Gen 2" >&2; exit 1; }
  mv "$Z64" dist/; rm -rf dist/work64
fi

# 3. the catalog zip, only with a source repo (the catalog needs it in the manifest)
if [ -n "$REPO" ]; then
  mv "$CZ" dist/
  for z in dist/*-mpc-armv7.zip dist/*-mpc-aarch64.zip; do
    [ -f "$z" ] || continue
    python3 "$MPC_VST_RELEASE/tools/catalog_check.py" "$z" --catalog >/dev/null || { python3 "$MPC_VST_RELEASE/tools/catalog_check.py" "$z" --catalog >&2; exit 1; }
    echo "catalog zip ok: $z"
  done
  # 4. the MPC OS 3.10.1 folder: both builds in one folder (Akai's plugin folder format), made from the two catalog zips
  python3 "$MPC_VST_RELEASE/tools/sdk_zip.py" "dist/Liminal-Amp-$VERSION-mpc-sdk.zip" dist/*-mpc-armv7.zip dist/*-mpc-aarch64.zip >/dev/null
  echo "3.10.1 folder zip: dist/Liminal-Amp-$VERSION-mpc-sdk.zip"
else
  echo "no REPO / GITHUB_REPOSITORY: catalog zip skipped (set REPO=owner/name)"
fi
rm -rf dist/work dist/stage
(cd dist && sha256sum *.zip > SHA256SUMS)
awk -v v="$VERSION" '/^## /{on = ($2 == v)} on && !/^## /' CHANGELOG.md > dist/RELEASE_NOTES.md
ls -la dist
