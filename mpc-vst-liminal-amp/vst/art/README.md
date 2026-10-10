# Page artwork

| File | What | Status |
|---|---|---|
| `tone3000-logo.png` | TONE3000 logo (full), shown in the TONE3000 strip | **placeholder** (a text wordmark) until `tools/install_t3k_logos.py` has run |
| `t3k-mark.png` | TONE3000's compact T3K mark, shown on a block whose tone came from TONE3000 | **placeholder** (text) until `tools/install_t3k_logos.py` has run |
| `blank.png` | transparent: the "Local" state of the mark | final |

TONE3000's design requirements (https://www.tone3000.com/api#design-requirements, "Logo usage") ask for its
official logo and mark. Download them from that page ("Download TONE3000 Logos"), export the full logo as
`tone3000-logo.png` (light version, transparent, about 300x60) and the T3K mark as `t3k-mark.png` (about 128x52),
replace the files here, and rebuild (`vst/build.sh`). Users see the full logo (TONE3000 strip) before the T3K
marks (on the blocks), as the requirements ask.

The phone page uses the official logo too when the plugin folder has `art/tone3000-logo.svg` or `.png`
(tools/package.sh copies `tone3000-logo.png` there).

## Installing the official files (one command)
```
curl -L -o "TONE3000 Logos.zip" "https://www.tone3000.com/TONE3000%20Logos.zip"   # or save it from tone3000.com/api
python3 tools/install_t3k_logos.py --zip "TONE3000 Logos.zip"       # lists the zip, picks the light full logo and the T3K mark
#   if it can't tell which is which:  --logo "<file in the zip>" --mark "<file in the zip>"   (PNG, JPG, WebP or SVG)
vst/build.sh && python3 tools/make_readme_images.py && tools/package.sh
```
It writes `tone3000-logo.png` (300x60) and `t3k-mark.png` (128x52), light on transparent (the page is near-black), changing
nothing but scale and padding, and a `.t3k-official` marker so `tools/package.sh` stops warning. Keep TONE3000's usage rules
(tone3000.com/api, "Design requirements"): the full logo is shown before the T3K mark, and the mark is for compact places.
