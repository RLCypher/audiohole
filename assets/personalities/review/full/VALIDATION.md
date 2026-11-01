# Verification results

- **48 Python tests passed**, including contract validation, real-library
  acceptance, playback timing, priority/interruption, cooldowns, reduced motion,
  persistent return context, invalid/missing assets, recovery and personality
  isolation.
- **796 operating PNGs passed** exact dimensions, raw PNG 8-bit RGBA color
  format, decoding, real alpha, hashes, anchors, framing and manifest checks.
  There are no orphaned or missing operating PNGs relative to the manifests.
- **52 static expressions, 49 sequences and 744 frames** are inventoried in
  `inventory.json`. All required core states are present for all three enabled
  presentations. The three deliberate static cases and eight Vanilla optional
  fallbacks are explicit.
- **39 accepted IDLE image hashes remain unchanged.** The human acceptance and
  authorization are recorded in `provenance/idle-approval.json` and
  `provenance/authorizations/`.
- **All 52 native preview files decode completely.** Native preview samples
  match their operating images. Preview timing is checked against manifests.
  Finite animations return to exact canonical rest; persistent loops join at
  exact matching endpoints.
- **Vanilla exterior RGBA and complete alpha remain exact** for every static
  and frame. Norm/UwU static case and boot regions retain the accepted pixels.
- **Both offline Chromium suites passed.** The IDLE suite reports twelve checks;
  the full-library suite reports five groups covering all 52 catalog thumbnails,
  representative theatrical/static/reduced-motion pages, visible finite replay,
  state inspector controls, approval filtering, missing assets and ZIP links.
- Browser reports are in `browser-checks.json` and `../browser-checks.json`.
  The Codex embedded visualization host itself is not directly testable from
  this workspace. Self-contained pages, direct GIFs and portable ZIPs are supplied.

Inherited near-opaque alpha and localized facial-edge softness remain artistic
review warnings, not silently repaired or marked approved. Technical validation
does not replace human approval. No application C++ build target or Windows audio
hardware integration exists in this checkout, so no such result is claimed.
