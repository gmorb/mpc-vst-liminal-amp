#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 the mpc-vst-liminal-amp contributors
# Package Liminal Amp (run vst/build.sh first): the self-contained plugin folder (Force VST distribution layout,
# docs/SELF_CONTAINED_PORTS.md) and, with REPO=owner/name (GITHUB_REPOSITORY in CI), the MPC OS Plugin Catalog zip.
#   tools/package.sh   -> dist/Liminal-Amp-for-MPC-OS-<VERSION>.zip, dist/Liminal-Amp-<VERSION>-mpc-armv7.zip, SHA256SUMS
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION=$(cat VERSION)
MPC_VST="${MPC_VST:-$PWD/../mpc-vst-plugins}"
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
python3 "$MPC_VST/tools/release.py" "${REL_ARGS[@]}" -o dist/work >/dev/null
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
rm -rf dist/stage

# 3. the catalog zip, only with a source repo (the catalog needs it in the manifest)
if [ -n "$REPO" ]; then
  mv "$CZ" dist/
  for z in dist/*-mpc-armv7.zip; do
    python3 "$MPC_VST/tools/catalog_check.py" "$z" --catalog >/dev/null || { python3 "$MPC_VST/tools/catalog_check.py" "$z" --catalog >&2; exit 1; }
    echo "catalog zip ok: $z"
  done
else
  echo "no REPO / GITHUB_REPOSITORY: catalog zip skipped (set REPO=owner/name)"
fi
rm -rf dist/work
(cd dist && sha256sum *.zip > SHA256SUMS)
awk -v v="$VERSION" '/^## /{on = ($2 == v)} on && !/^## /' CHANGELOG.md > dist/RELEASE_NOTES.md
ls -la dist
