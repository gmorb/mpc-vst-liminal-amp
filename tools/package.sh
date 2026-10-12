#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 the mpc-vst-liminal-amp contributors
# Package Liminal Amp (run vst/build.sh first): the self-contained plugin folder (Force VST distribution layout,
# docs/SELF_CONTAINED_PORTS.md) and, with REPO=owner/name (GITHUB_REPOSITORY in CI), the MPC OS Plugin Catalog zip.
#   tools/package.sh   -> dist/Liminal-Amp-for-MPC-OS-<VERSION>.zip, dist/Liminal-Amp-<VERSION>-mpc-armv7.zip, SHA256SUMS
#                         (+ dist/Liminal-Amp-<VERSION>-mpc-aarch64.zip, Gen 2, after vst/build.sh aarch64: docs/GEN2.md)
# MPC_VST is the kit the page is built with (pinned, tested on the device). MPC_VST_RELEASE (default MPC_VST) is the kit
# whose release.py and catalog_check.py package and check the zips: a newer one that knows Gen 2 (aarch64 zips, the
# installer's 64-bit plugin list), pinned in .github/workflows/build.yml.
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
COLL="Liminal Amp for MPC OS"
rm -rf dist; mkdir -p dist/work
python3 tools/param_json.py dist/work/NAMA2.json

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
          --extra "$MPC_VST/tools/html_art/fonts/TitilliumWeb-Regular.ttf:art/fonts/TitilliumWeb-Regular.ttf"
          --extra "$MPC_VST/tools/html_art/fonts/TitilliumWeb-SemiBold.ttf:art/fonts/TitilliumWeb-SemiBold.ttf"
          --extra "$MPC_VST/tools/html_art/fonts/OFL.txt:art/fonts/OFL.txt"
          --user-data models --user-data irs --user-data presets)
[ -n "$REPO" ] && REL_ARGS+=(--repo "$REPO")
# the device bench's JSON line (tools/bench_device.sh writes bench/bench-<version>.json; commit it so CI's release has it):
# recorded in the release's INSTALL.md and manifest. BENCH=file overrides; none: the release has no CPU figure.
[ -z "${BENCH:-}" ] && [ -s "bench/bench-$VERSION.json" ] && BENCH="bench/bench-$VERSION.json"
if [ -n "${BENCH:-}" ]; then [ -s "$BENCH" ] || { echo "error: BENCH=$BENCH is empty or missing" >&2; exit 1; }; REL_ARGS+=(--bench "$BENCH"); fi
python3 "$MPC_VST_RELEASE/tools/release.py" "${REL_ARGS[@]}" -o dist/work >/dev/null
CZ=$(ls dist/work/*-mpc-armv7.zip)

# 2. the Force VST distribution zip: the same plugin folder, with the collection's README and licences
mkdir -p "dist/stage/$COLL"
(cd dist/work && unzip -q "$(basename "$CZ")")
cp -r dist/work/*/portable/"$FOLDER" "dist/stage/$COLL/"
C="dist/stage/$COLL"
cp LICENSE NOTICE.md "$C/"
mkdir -p "$C/licenses"
cp src/nam-core/LICENSE "$C/licenses/NeuralAmpModelerCore-MIT.txt"
cp src/plugin/AudioDSPTools/LICENSE "$C/licenses/AudioDSPTools-MIT.txt"
cp src/plugin/LICENSE "$C/licenses/NeuralAmpModelerPlugin-MIT.txt"
cp engine/pffft/LICENSE.txt "$C/licenses/PFFFT.txt"
sed -n '/^\/\*/,/\*\//p' src/plugin/AudioDSPTools/dsp/ResamplingContainer/ResamplingContainer.h | head -40 > "$C/licenses/iPlug2-notice.txt"
sed -e "s|@VERSION@|$VERSION|" -e "s|@SOURCE@|$SOURCE|" packaging/README.md > "$C/README.md"
find dist/stage -exec touch -d "2026-01-01 00:00:00" {} +
(cd dist/stage && find "$COLL" -type f | LC_ALL=C sort | zip -q -X -@ "../Liminal-Amp-for-MPC-OS-$VERSION.zip")

# 2b. Gen 2 (aarch64): when vst/build/aarch64/liminal_amp.so exists (vst/build.sh aarch64), release.py once more with that
#     .so: it names the zip from the ELF (Liminal-Amp-<v>-mpc-aarch64.zip) and its manifest says arch aarch64, so the
#     installer registers it in MPC's 64-bit plugin list. Same id, uid and version as the armv7 zip; both go on the same
#     release (the catalog finds this one by swapping armv7 for aarch64 in asset_pattern). No --bench: that's Gen 1's.
A64SO="$B/aarch64/liminal_amp.so"
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
else
  echo "no REPO / GITHUB_REPOSITORY: catalog zip skipped (set REPO=owner/name)"
fi
rm -rf dist/work dist/stage
(cd dist && sha256sum *.zip > SHA256SUMS)
awk -v v="$VERSION" '/^## /{on = ($2 == v)} on && !/^## /' CHANGELOG.md > dist/RELEASE_NOTES.md
ls -la dist
