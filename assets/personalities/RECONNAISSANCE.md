# Avatar reconnaissance and handoff

Audit date: 2026-11-01. Branch: `avatar/asset-pipeline`.

This is the historical initial audit. The user subsequently authorized three
initial 12-frame IDLE sequences. Current results and review status are in
[the sequence report](review/idle-sequences/REPORT.md); the initial capability
observations below describe the starting point, not the completed sequence work.

## Existing infrastructure

The checked-out AudioHole repository initially contained 14 Markdown files and
Git metadata. There was no CMake project, source directory, compiled target,
personality implementation, image loader, avatar renderer, animation scheduler,
test suite, or third-party asset infrastructure to reuse. No existing avatar work
or uncommitted personality changes were present at the start. Other agents'
future changes are outside this audit snapshot.

The reusable interfaces are currently specifications: `PERSONALITY.md` defines
`interface.personality_mode` values `default`, `vanilla`, and `uwu`; D-056 confirms
their presentation-only meaning. `ARCHITECTURE.md` requires structured controller
communication and independence from real-time audio. `GUI.md` describes the
desktop application but selects no rendering framework. D-031 remains open
(Qt/wxWidgets/Dear ImGui); D-032 provisionally favors nlohmann/json. The application
language/build are C++/CMake. An offline HTML preview makes no permanent choice
for that application. No code outside `assets/personalities/` was changed.

## Supplied inventory and visual inspection

The ZIP was inspected before extraction. Its `AGENTS.md` was read and copied
verbatim to the designated root; no preexisting scoped file was overwritten.
Every supplied document and image has a baseline SHA-256 in
`provenance/reference-inventory.json`. The extracted package matches the ZIP.

| Input | Dimensions | Inspection |
|---|---|---|
| Vanilla logo | 512 × 512 RGBA | Mint upper/lavender-pink lower cylinder, black outlines, mint wormhole grid |
| Norm regular | 1201 × 1310 RGBA | Adult human technician, beard, cap/headphones, hoodie, cargo pants, coffee, equipment case |
| Norm chibi | 1199 × 1312 RGBA | Same identity, larger head and compact proportions, restrained smirk |
| UwU regular | 1199 × 1312 RGBA | Adult white feline, lavender shading, blue eyes, pink ears, large tail, headphones, choker, technician equipment |
| UwU chibi | 1199 × 1312 RGBA | Same adult character, exaggerated head/ears/tail, expressive face |

All five reference images were opened individually. The three character
definitions and full shared asset specification were read. The source
`AVATAR_ASSETS.md` and Norm definition contain escaped Markdown and extra blank
lines; they remain verbatim. The supplied character definitions already satisfy
the definition deliverable; they were not replaced with generated descriptions.

## Conflicts and resolution

- Shared asset prose calls the personality “Normal”; the accepted application
  identifier is `default`. Registry and manifest use `default`, character ID
  `norm`, and retain the supplied `Norm/` folder. No config-schema change is made.
- Vanilla's character document says “default, personality-neutral visual
  identity.” This describes its visual role; the application's selected default
  mode remains Normal under D-056. The pipeline never changes that selection.
- Recommended lowercase directory examples differ from the supplied package.
  The original `Norm`, `UwU`, `Vanilla`, `References`, and `Generated` casing is
  retained; style/state names alone use lowercase. No references were relocated.
- References do not have operating dimensions. They are exempt and preserved.
  The image backend also did not honor the requested operating dimensions, so
  sources and explicit exports are separately retained and recorded.
- There is no production loader to reuse. The standalone reference loader and
  preview are scoped tooling; no claim of integrated C++ functionality is made.

No remaining design contradiction blocks the initial tooling or master review.

## Actual generation capability assessment

The available `image_gen.imagegen` tool accepts local reference paths, a prompt,
and a transparent-background request. Two actual independent reference-guided
IDLE generations succeeded for Norm and UwU. Requests are preserved in
`provenance/`. Both returned 1024 × 1536 RGBA PNGs rather than the requested
512 × 768. Their foreground alpha peaks at 254 and tiny low-alpha speckles
extend beyond the main figure. Strict operating export is therefore a separate,
recorded step; validation performs no implicit repair. Two character outputs
still flag substantial semitransparency and require artistic edge review.

No seed, sampling settings, model revision, guaranteed reference identity lock,
frame-to-frame consistency control, or direct animation/video-generation tool is
exposed. New stochastic illustrations are not bitwise reproducible. Retained
sources plus the pinned exporter are reproducible. Image editing is available
for later individual expressions; its consistency across a batch has not been
established. Do not infer animation-generation quality from these two stills.

Pillow 12.3.0 and jsonschema 4.26.0 were actually available. Pillow is used for
lossless Vanilla canvas placement, explicit character exports, alpha checks,
and contact sheets. Frame sequences and the preview can support externally
generated frames after master approval. No new character motion has been
generated. The browser timer was checked only with synthetic repeated static
frame references. Vanilla's later wormhole animation should use fixed protected
logo layers and an explicitly reviewed interior mask; no mask has been invented.

## Architecture and milestones

| Milestone | Result / acceptance | Dependency or remaining work |
|---|---|---|
| 1. Audit | Repository/visual inventory, conflicts, capabilities and interfaces documented | Complete for this checkout |
| 2. Infrastructure | Versioned JSON schema, states, naming, status lifecycle, strict validator, fallback reference implementation | Implemented and checked |
| 3. Masters | Three IDLE operating candidates, retained sources/provenance, labeled contact sheet | **Human visual review pending** |
| 4. Expressions/animation | Contracts for frame timing, loops, priority, interruption, cooldown, transition and anchors are present | Approved masters first; then expression fidelity tests and a scheduler |
| 5. Preview/validation | Offline selector, state/style inspection, background checks, frame controls, missing-pack tests | Initial tooling implemented; real animation continuity still untested |
| 6. Integration | Proposed C++ boundary and fail-closed asset contract in README | Primary GUI/loader implementation and coordination still required |

The schema is for an independent cosmetic asset pack, not a revision to the
application's portable user-configuration schema. The pipeline has no routing,
device, dialogue, DFA, easter-egg, controller, or audio-engine access.

## Verified evidence and limitations

- 19 Python unit tests passed: real PNG headers/channels/dimensions, corrupt
  images, opaque/empty backgrounds, hashes, anchors, frame timing, displacement,
  default/disabled styles, missing IDLE, approval filtering, duplicate JSON keys,
  path/symlink ownership, animation-to-static fallback, last-frame handling,
  personality-switch isolation, deterministic export, and exact Vanilla pixels.
- Strict validation passed for all three operating masters and immutable inputs.
  Norm/UwU have review warnings. Approved-only validation is intentionally
  unsuccessful because no human has approved these masters.
- Eight browser smoke checks passed using local Chromium and Playwright:
  decoding, selections/fallbacks, missing packs, approval filtering, disabled
  styles, backgrounds/guides, synthetic playback, and JavaScript error checks.
  Evidence is in `review/browser-checks.json` and `preview-screenshot.png`.
- No application target exists to compile. No Windows GUI integration,
  audio/hardware behavior, production rendering performance, real motion,
  priority scheduling, or cross-expression character consistency is claimed.
- Full pixel-based identity, halo, and geometry recognition is not an automated
  guarantee. The validator provides technical rejection and review heuristics;
  the owner retains artistic approval.

The next action is review of the three IDLE masters. Feedback may approve each
individually or request corrections to framing, character design, lettering,
edges, or logo presentation. Continue independent documentation and validation
as needed; do not generate bulk expressions/animations before that review.
