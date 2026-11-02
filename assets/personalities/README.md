# AudioHole avatar pipeline

The library contains 52 static expressions, 49 sequences and 744 animation frames across Norm, **Horu** and Vanilla. Norm and Vanilla are approved. Horu's revised IDLE, talking, working and prop-continuity assets await visual review. Horu is the character name; `uwu` is the stable application mode and `UwU/` is the legacy source pack directory. Public review names and filenames use `horu`, as `norm` is used for Norm.

See the [current Horu correction report](review/full/horu-revision/REPORT.md), [native/compact scale comparisons](review/full/horu-revision/scale-compact.png) and [animated review](review/full/horu-revision/preview.gif). Earlier milestone descriptions below are historical where superseded by that report.

Start with the [full catalog](review/full/index.html), [all-state sheet](review/full/all-states-contact-sheet.png),
[inventory](review/full/INVENTORY.md), and [integration handoff](review/full/HANDOFF.md).
The original repository audit remains in [RECONNAISSANCE.md](RECONNAISSANCE.md).

## Review the library and accepted baseline

- Open `review/preview.html` in a browser. It is a self-contained offline snapshot.
- Inspect `review/idle-contact-sheet.png` and `review/validation.json`.
- Inspect all 36 frames in `review/idle-sequences/all-personalities-all-frames.png`.
  Each personality also has a labeled `*-all-frames.png` sheet and transparent
  animated PNG previews at `*-native.png` and `*-compact.png` in that directory.
  Compact 128 × 192 is a representative review size; no production GUI size has
  been selected. The HTML preview supports native/compact sizing and frame stepping.
- The gallery is now self-contained and script-free. Direct `*-review.gif` files
  and `idle-previews.zip` bypass HTML/APNG viewer compatibility problems. GIFs
  have a checkerboard review background; operating PNG transparency is unchanged.
  The frame inspector uses display-only WebP compression to reduce its size;
  use the native APNG or individual PNGs for exact pixel/edge inspection.
- The latest authorized revision closes Horu's mouth and replaces both heavy
  steam plumes with smaller, fainter, independently animated layers. Original
  generated masters remain archived in `*/Intermediate/idle-before-steam-revision.png`.
- Norm and Horu use reference-guided generated chibi illustrations. Vanilla is the
  original logo pasted pixel-for-pixel onto a transparent portrait canvas.
- Check Norm's facial hair, cap, headphones, fingers, coffee mug, clothing, cables,
  and case. Check Horu's adult identity, ears, tail, eyes, forelock, accessories,
  and clothing. Check small case lettering and emblems particularly closely.
- Both character candidates have transparency warnings. The backend source has
  near-opaque foreground alpha capped at 254; the source also contained tiny
  low-alpha speckles. The explicit export frames around alpha ≥ 4 bounds, but
  does not remove backgrounds, repaint edges, or force opacity. Inspect light,
  dark, and checkerboard backgrounds. Technical checks cannot judge character
  fidelity, a baked checkerboard inside artwork, or subtle color fringes.
- Vanilla's entire canonical 512-square RGBA image is preserved at `(0, 128)`.
  Its floating logo is intentionally above the character ground guide.

The latest user authorization accepts IDLE and permits full generation without
routine batch approvals. All 39 accepted IDLE image hashes are frozen in
`provenance/idle-approval.json`. Norm and Vanilla approvals freeze 501 operating PNG hashes (including all
approved IDLE files) in `provenance/accepted-assets.json`. Horu's 19 new statics
and 18 revised body sequences remain candidates; generation authorization does
not grant artistic approval.
See `review/full/` for per-personality review ZIPs and one small inspector per
performance. Norm/Horu deadpan and Vanilla sleeping are intentionally static;
Vanilla optional emotional labels explicitly use its own IDLE.

## Run the tools

Use Python 3.11+ from the repository root. The development tools use Pillow
12.3.0 (HPND license) and jsonschema 4.26.0 (MIT); these are optional offline
tooling dependencies, not a second application build system or C++ dependencies.
Install into a virtual environment with `python -m pip install -r
assets/personalities/requirements-tools.txt` when needed. Neither library is
vendored. Project and supplied-artwork licensing remain the owner's decision
before distribution, as already recorded in the repository README.

```sh
python assets/personalities/tools/pipeline.py inventory
python assets/personalities/tools/pipeline.py validate
python assets/personalities/tools/pipeline.py validate --approved-only
python assets/personalities/tools/pipeline.py review
python assets/personalities/tools/validate_idle.py
python assets/personalities/tools/audit_library.py
python assets/personalities/tools/build_review_library.py
python -m unittest discover -s assets/personalities/tools -p 'test_*.py' -v
```

`validate` is read-only and exits 1 on errors. Warnings require visual inspection.
`--approved-only` reports the remaining unapproved generated reactions; approved Norm/Vanilla states are usable; Horu candidates retain their own approved IDLE fallback. The
optional browser smoke check uses an already installed Playwright and Chromium:

```sh
python assets/personalities/tools/check_preview.py --chromium /usr/bin/chromium
```

The managed environment blocks browser local socket setup under its shell
sandbox and blocks `file://` navigation by Chromium policy. Verification ran
Chromium outside that shell sandbox and loaded the self-contained HTML directly
with Playwright `set_content`, without a server or remote requests. Normal local
browsers can open the exported file directly.

## Reproduce an operating export

The image-generation backend is invoked interactively by the agent; there is no
pretend local image-generation command. Exact prompts and available settings are
in `provenance/*-idle-request.json`. Raw backend outputs remain in each
personality's `Intermediate/idle-source.png`. Generation does not expose a seed,
model version, or deterministic replay guarantee. Operating exports from those
retained sources are deterministic with the pinned Pillow version:

```sh
python assets/personalities/tools/pipeline.py export-idle default assets/personalities/Norm/Intermediate/idle-source.png --replace
python assets/personalities/tools/pipeline.py export-idle uwu assets/personalities/Horu/Intermediate/idle-source.png --replace
python assets/personalities/tools/pipeline.py export-idle vanilla "assets/personalities/Vanilla/References/Vanilla Reference.png" --replace
python assets/personalities/tools/pipeline.py review
```

The export commands above recreate the earlier masters. To rebuild the current
review after those exports, run `python assets/personalities/tools/prepare_idle_revision.py`
and then `python assets/personalities/tools/animate_idle.py`. This reapplies the
closed mouth, separate subtle steam layers, and recorded animation changes. See
[the revision notes](review/idle-sequences/REVISION.md) for the current review workflow.

Export is explicitly separate from validation: it crops near-invisible source
padding using alpha ≥ 4 bounds, fits the figure proportionally into 448 × 640,
and places it at anchor `(256, 704)` on a 512 × 768 transparent canvas. Vanilla
uses no crop or resampling. The exporter refuses to overwrite approved artwork
or modify its source. Every export records source and output SHA-256, dimensions,
crop, resampling size, and placement. No implicit resizing occurs in validation.

## Layout and metadata

| Location | Purpose |
|---|---|
| `AGENTS.md`, `AVATAR_ASSETS.md`, `*/CHARACTER.md` | Supplied baseline, preserved verbatim |
| `*/References/` | Five immutable canonical inputs, original names and sizes |
| `*/Intermediate/` | Raw backend outputs; never runtime assets |
| `*/Generated/<style>/idle.png` | Accepted IDLE masters; separate reaction candidates |
| `*/manifest.json` | Editable per-personality states, assets, anchors, statuses |
| `registry.json` | Existing application mode IDs mapped to personality manifests |
| `states.json` | Shared required and optional state catalog, fallback order |
| `schemas/manifest.schema.json` | JSON Schema 2020-12, version 1 |
| `provenance/` | Canonical checksums, generation requests, explicit export records |
| `review/` | Regenerable contact sheet, offline snapshot, validation and browser evidence |
| `tools/` | Validator, reference loader, exporter, preview template, tests |

Static paths are `Generated/<style>/<state>.png`. Animation paths are
`Generated/<style>/<state>/<zero-based-four-digit-frame>.png`. All operating
images are 512 × 768, PNG color type 6, bit depth 8. Each frame carries a hash and
the style's exact anchor. Animation metadata includes ordered frame durations
(16–60000 ms), loop, priority, interruptibility, cooldown, and a cut or
finish-then-idle transition. A sequence is capped at 240 frames. Each initial
IDLE sequence has 12 frames over five seconds: a common 100 ms timebase, authored
integer exposure holds, a brief blink, and long quiet poses. This is limited
animation, not a claim of ten unique smooth drawings per second.

The manifest uses `not_generated`, `generated`, `validated`, `approved`, `invalid`,
`unavailable`, and `missing`. Validation also diagnoses files that go missing or
change independently of metadata. Generated/validated status is not approval.
Approval requires the human's recorded decision, reviewer, and timestamp in a
`review` object. No command automatically grants approval. Regular styles remain
disabled and explicitly `not_generated`; enabling one requires its own valid
IDLE. Required emotional state names exist for Vanilla as presentation intents;
they currently explicitly fall back to its neutral logo.

## Loader and integration contract

`tools/avatar_assets.py` is an executable Python reference implementation for
offline tools, **not** a production C++ integration. The preview and tests reuse
this loader. Given `(manifest, personality_id, style, state)`, `Resolver` returns
validated owned frame paths, duration/loop information, resolution source, and
diagnostics. Production approval filtering defaults on; preview opts into
technically validated candidates. Each preview selection has a separately
resolved production result, so an unapproved animation cannot suppress an
approved static fallback.

Fallback order is requested animation → corresponding static → selected style's
IDLE animation/static → last valid frame of the same personality/style → hide
with a diagnostic. Unknown states use IDLE. Explicit v1 fallback mappings point
directly to IDLE; cycles and other targets are rejected. All animation frames
must pass preflight before playback. Paths must stay under the selected pack's
Generated directory, including after symlink resolution. The registry's selected
mode must match the manifest. No personality fallback or mode mutation exists.
Changing pack, personality, or style clears the last-frame cache; a malformed
manifest also clears it. `remember_presented` records the current animation
frame; cached paths are revalidated before reuse. The preview clears the image
and cancels playback timers on selection changes and pauses when hidden.

Future C++ integration should port these tested semantics behind an independent
asset-loader/renderer interface after the main GUI choice. Preload and decode
away from audio callbacks, key caches by personality/style/path/hash, pass
structured presentation requests from the personality layer, and keep rendering
failures inside the GUI. Avoid repeated decoding in production. This offline
reference intentionally revalidates on resolution and is not a performance
benchmark. Priority arbitration, interruption, cooldowns and duplicate suppression now
have an independent reference implementation in `avatar_player.py`; the frame
inspector remains a manual inspection tool rather than the main application scheduler.

To add a personality, add its own references and definition, create a manifest
with a unique ID and enabled default style, produce and review its own IDLE, and
add its path to the registry. Custom state/style names are supported without
changing the renderer. A new application personality mode still belongs to the
primary application's configuration workflow. Do not change routing, dialogue,
DFA, persistent settings, or shared GUI code from this workspace.


## Full-generation playback policy

`avatar_player.py` now implements priority, interruption, cooldowns, redundant
event suppression, persistent return context and reduced-motion selection as
an independent reference policy. It preflights during requests and performs no
filesystem/decode work during timer ticks. New state behavior and performance
metadata are validated against manifest schema v1. Per-state inspectors and
portable replay pages are generated by `build_review_library.py`.

The historical IDLE export/revision commands above describe its provenance.
They intentionally refuse approved masters/sequences and must not be used to
regenerate the accepted baseline. Rebuild new states with `generate_states.py`
and retained face keys, then run the full audit and review builder. Detailed
commands, limitations, cache guidance, checks and remaining approval items are
in the full handoff. No production C++ integration is claimed.


Horu body revisions are packaged with `python assets/personalities/tools/revise_uwu_body.py`.
The model draws the articulated key poses; the packager extracts complete
silhouettes along clear sheet gutters, registers them by a case stripe, applies
a common fit per state, and uses deliberate exposure holds. It adds no whole
sprite translation/rotation animation. New source sheets, exact prompts, key
indices and a per-state provenance record are retained.


Horu consistency revision 2 uses a fixed case and canonical badge layers. See
[the correction report](review/full/consistency-correction/REPORT.md). Rebuild
with `python assets/personalities/tools/correct_uwu_consistency.py --package`,
then rebuild the body review, audit and catalog. The earlier body-only command
now refuses to overwrite the corrected library. Human artistic approval remains
pending for the 19 non-IDLE static states and 18 sequences.
