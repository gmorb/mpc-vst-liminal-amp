// SPDX-License-Identifier: MIT
// Copyright (c) 2026 the mpc-vst-liminal-amp contributors
/* web_theme.h -- the phone pages' look, shared by the TONE3000 pages (tone3000.cpp) and My Presets (presets_web.cpp).
 *
 * The plugin page's own: a warm black chassis with the valves' amber glow rising from the lower right, faceplate cards
 * with a hairline edge, ivory text, quiet lowercase captions, the Liminal Amp wordmark, Titillium Web throughout (served
 * from the plugin folder's art/fonts; the system font if it's missing). Amber marks only what's live (the preset in
 * use, a notice, the main action), as on the MPC. Buttons are the MPC's: dark, engraved caps, 48 px tall. No scripts,
 * no outside requests: the pages work on a phone with no internet.
 */
#pragma once
#include <string>

namespace mpcnam {

inline const char* web_css() {
  return
      "@font-face{font-family:NA2;src:url(/font/regular.ttf) format('truetype');font-weight:400}"
      "@font-face{font-family:NA2;src:url(/font/semibold.ttf) format('truetype');font-weight:600 800}"
      ":root{--bg:#0b0a09;--plate:rgba(18,17,16,.9);--line:#2c2a27;--ink:#e6dfd2;--dim:#9a958c;--faint:#615c55;"
      "--amber:#ee9848;--amber-dim:rgba(238,152,72,.12)}"
      "*{box-sizing:border-box}"
      "html{background:var(--bg)}"
      "body{margin:0;min-height:100vh;color:var(--ink);font-family:NA2,system-ui,sans-serif;line-height:1.45;"
      "font-variant-numeric:tabular-nums;"
      "background:radial-gradient(ellipse 70% 38% at 88% 104%,rgba(255,150,64,.22),transparent 72%),"
      "radial-gradient(ellipse 34% 22% at 80% 100%,rgba(255,176,96,.16),transparent 70%),"
      "radial-gradient(ellipse 90% 60% at 10% -10%,rgba(120,110,100,.06),transparent 70%),var(--bg)}"
      ".wrap{max-width:34em;margin:auto;padding:1.4em 1.1em 3em}"
      ".wm{display:flex;align-items:baseline;gap:.5em;margin:.3em 0 1.3em;flex-wrap:wrap}"
      ".wm1{font-weight:400;letter-spacing:.3em;font-size:1.55em;color:#cdc5b6}.wm2{font-weight:600;font-size:1.55em}"
      ".wx{display:flex;align-items:center;gap:.5em;flex-basis:100%}.by{color:var(--dim);font-size:.82em}.x{color:var(--faint);margin:0 .15em}.logoimg{height:1.25em;align-self:center}"
      ".logo{font-weight:800;letter-spacing:.06em;font-size:1.15em}"
      ".cap{color:var(--dim);font-size:.95em;font-weight:600;margin:1.4em 0 .4em;letter-spacing:.02em}"
      ".card{background:var(--plate);border:1px solid var(--line);border-top-color:#3a3732;border-radius:10px;"
      "padding:1em 1.05em;margin:.4em 0 1em;box-shadow:inset 0 1px 0 rgba(255,255,255,.03)}"
      "h1{font-size:1.3em;font-weight:600;margin:.1em 0 .5em}p{margin:.5em 0}small,.sub{color:var(--dim)}"
      ".msg{border:1px solid var(--line);border-left:3px solid var(--amber);background:var(--amber-dim);"
      "padding:.65em .85em;margin:.8em 0;border-radius:6px}"
      "a.b,button{display:inline-flex;align-items:center;justify-content:center;min-height:48px;font:inherit;font-weight:600;"
      "font-size:.8em;letter-spacing:.14em;text-transform:uppercase;text-decoration:none;text-align:center;color:var(--ink);"
      "background:#17181b;border:1px solid #2e2d2a;border-radius:6px;padding:0 1.1em;margin:.35em .35em 0 0;cursor:pointer;"
      "box-shadow:inset 0 1px 0 rgba(255,255,255,.04)}"
      "a.b{display:flex;margin:.6em 0}"
      "a.b.go,button.go{background:var(--amber);color:#16100a;border-color:var(--amber);box-shadow:0 0 18px rgba(238,152,72,.25)}"
      "button.del{color:#e8a08f;border-color:#4a2a24}"
      "button:active,a.b:active{transform:translateY(1px)}"
      "input[type=text],input[type=number],select{width:100%;min-height:48px;font:inherit;font-size:1.05em;padding:.5em .7em;"
      "border-radius:6px;border:1px solid var(--line);background:#080808;color:var(--ink);margin:.3em 0}"
      "select{appearance:none;background-image:linear-gradient(45deg,transparent 50%,var(--dim) 50%),"
      "linear-gradient(135deg,var(--dim) 50%,transparent 50%);background-position:calc(100% - 18px) 55%,calc(100% - 12px) 55%;"
      "background-size:6px 6px;background-repeat:no-repeat;padding-right:2em}"
      "input:focus,select:focus{outline:none;border-color:var(--amber)}"
      "input[type=checkbox]{width:1.4em;height:1.4em;accent-color:var(--amber);vertical-align:middle;margin:0 .5em 0 0}"
      "label{font-size:.92em;color:var(--dim)}.row{border-top:1px solid var(--line);padding:.9em 0}"
      ".row:first-child{border-top:0;padding-top:.15em}.name{font-weight:600;font-size:1.15em}"
      ".cur{color:var(--amber);font-size:.78em;margin-left:.55em;letter-spacing:.12em;text-transform:uppercase}"
      ".rig{color:var(--dim);font-size:.9em;margin:.15em 0 .2em}"
      "form{display:inline}.ren{display:flex;gap:.4em;margin-top:.45em;align-items:center}.ren input{flex:1;margin:0}"
      ".ren button{margin:0}.nav{margin-top:1.6em}"
      ".grid{display:grid;grid-template-columns:1fr 1fr;gap:.2em .8em}.grid .f{display:flex;flex-direction:column}"
      ".sw{display:flex;align-items:center;min-height:48px;color:var(--ink);font-size:1em}"
      ".unit{color:var(--faint);font-size:.85em}.full{display:block}"
      ".grp{color:var(--dim);font-size:.9em;font-weight:600;margin:.9em 0 .1em;grid-column:1/-1}.grp:first-child{margin-top:0}"
      ".grid .sw{grid-column:1/-1;min-height:44px}";
}

inline std::string web_page(const std::string& title, const std::string& body) {
  return "<!doctype html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
         "<meta name=theme-color content='#0b0a09'><title>" + title + "</title><style>" + web_css() + "</style></head>"
         "<body><div class=wrap>" + body + "</div></body></html>";
}

// the wordmark, as on the plugin page (extra: what follows it, e.g. " x TONE3000")
inline std::string web_wordmark(const std::string& extra = "") {
  return "<div class=wm><span class=wm1>LIMINAL</span><span class=wm2>AMP</span><span class=by>A2 &middot; by Gm0rb</span>" + (extra.empty() ? std::string() : "<span class=wx>" + extra + "</span>") + "</div>";
}

}  // namespace mpcnam
