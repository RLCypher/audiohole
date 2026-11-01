# Full avatar library and integration handoff

The accepted IDLE assets now anchor complete core presentations for Norm, UwU,
and Vanilla. Norm and UwU also have all eight requested optional expressions.
The batch contains **52 static expressions, 49 animation sequences, 744 animation
frames, and 796 operating PNGs**. All operating images are exactly 512 × 768,
8-bit RGBA PNG with genuine alpha transparency. Only IDLE is human-approved;
the remaining **49 statics and 46 sequences** await artistic approval.

| Personality | Enabled style | Statics | Sequences | Frames |
|---|---|---:|---:|---:|
| Normal / Norm (`default`) | chibi | 20 | 19 | 288 |
| UwU (`uwu`) | chibi | 20 | 19 | 288 |
| Vanilla (`vanilla`) | logo | 12 | 11 | 168 |

## Inventory and review

- [All-state contact sheet](all-states-contact-sheet.png): every generated static.
- [Playable catalog](index.html): static previews, individual replay pages,
  reduced motion, GIFs, native PNG previews, per-state frame inspectors and sheets.
- [Inventory table](INVENTORY.md) and [machine-readable audit](inventory.json):
  every state, approval status, frame count, exposure array, duration, playback
  category, priority, cooldown and fallback.
- Each personality directory has `states-contact-sheet.png`, `face-details.png`,
  a `*-frames.png` sheet for every sequence, transparent native/compact PNG
  previews, and direct GIF review copies.
- `default-review.zip`, `uwu-review.zip`, and `vanilla-review.zip` provide portable
  review pages, GIFs, inspectors and frame sheets. Extract and open `index.html`
  in a regular browser. Their catalog links resolve within each archive. Native
  operating PNGs and large native APNGs stay in the repository rather than the
  compact ZIPs. Individual `*-preview.html` pages embed their own media.

Per-state inspectors avoid one enormous embedded visualization. They offer
native 512 × 768 and representative compact 128 × 192 display sizes, stepping,
playback, alpha-background inspection, approval filtering and static reduced
motion. Their frame images use display-only WebP compression; use the native
PNG preview or operating PNG for exact pixels. GIF checkerboards are review
backgrounds and are never stored in operating artwork. The Codex visualization
host cannot be directly tested from this workspace; direct GIFs and ZIPs avoid
depending on it.

## Creative implementation

Norm remains dry and restrained in ambient/ordinary reactions. His surprise and
panic keys provide a rare stronger contrast. UwU's facial vocabulary includes
closed-eye happiness, focused working eyes, asymmetry, worried brows, puffed
cheeks, white-eye shock, starry celebration, heavy blush, winks, spiral-eye panic,
and dot-eye deadpan. The face leads; breathing, ears, tail, head/sticker movement,
and selective manga punctuation reinforce it. Talking alternates mouth shapes.

Raw face-key sheets were generated from square crops of the accepted masters
plus the complete accepted sprite. New artwork was registered and composited
locally; the accepted clothing, equipment and body artwork were reused. Early
mask/registration defects were corrected before retaining the batches. Raw
backend keys, prompts, registration data, hashes and a pinned reproducer are in
`provenance/` and `*/Intermediate/`. No defective composite became a generation
reference, and no IDLE operating image was regenerated.

Body choreography uses controlled 2D motion of the accepted artwork. Some finite
reactions move the complete cutout, including its equipment case, as a chibi
sticker performance. This is not a library of newly drawn articulated arm poses
or curled-body drawings. Additional independent body layers/pose art can be
reviewed if that degree of articulation is desired. All reactions return to
canonical rest and remain inside the canvas.

Vanilla retains exact exterior RGBA pixels and the complete alpha silhouette.
Status changes affect only a protected wormhole interior: perspective/grid
movement, working oscillation, acknowledgment pulses and altered attention/error
rhythms. The logo does not gain emotions or anthropomorphic features. Sleeping
is an intentionally still, dimmed interior. Optional emotional states explicitly
use the selected logo's own IDLE fallback.

The original character documents remain byte-for-byte as prefixes, with accepted
direction appended: rare expressive contrast for Norm, normative anime/manga
expression and timing for UwU, and stronger brand-faithful interior effects for
Vanilla. The shared specification has an appended full-generation/runtime
extension. `document-revisions.json` links these changes to the user's preserved
authorization. All five canonical reference images remain unchanged.

## Timing and validation

Ambient performances generally loop; ordinary/theatrical reactions are finite.
Counts vary from 12 to 22 authored frames for new animated states, with deliberate
holds and abrupt expression changes. Durations range from 1.2 to 4 seconds, plus
the accepted five-second IDLE loops. Norm/UwU deadpan are intentional finite
static holds, not missing animation; Vanilla sleeping is intentionally static.
No required core state is deferred. Exact exposure lists live in the manifests.

The full audit checks PNG decoding, raw IHDR bit depth/color type, dimensions,
alpha/corners, hashes, anchors, clipping, manifests, loop/rest endpoints,
persistence/loop agreement, metadata consistency, preview timing and decoding,
and protected Vanilla exterior/alpha. Tests also compare native preview samples
to operating pixels, preserve the accepted static case/boots, test real finite
return and reduced-motion fallback, and reject missing/invalid/cross-personality
resources. Every accepted IDLE hash is checked.

Review includes native face/detail sheets, representative native motion frames,
compact state/frame sheets, and browser playback. Technical validity does not
grant artistic approval. Some native facial patch edges remain softer than the
original illustration, and the source's near-opaque alpha (often 254) continues
to trigger transparency-review warnings. The accepted IDLE's alpha was not
silently changed. Some compact case lettering is too small to read comfortably.
Held/limited animation is intentional; smoother or more articulated motion is a
future artistic choice rather than a claimed property of every sequence.

## Runtime boundary for the primary AudioHole agent

`tools/avatar_player.py` is a Python reference policy for porting to the selected
C++ GUI stack; it is not a new application dependency or an audio-engine change.

```python
player = AvatarPlayer(approved_only=True)
player.request(manifest, personality_id, style, state, monotonic_ms,
               reduced_motion=preference)
frame = player.frame_at(monotonic_ms)
```

1. The personality layer supplies presentation intent; the renderer does not
   infer audio-engine state. Preserve `default`, `vanilla`, `uwu` mode IDs and
   existing dialogue, DFA, diagnostics and routing behavior.
2. Load/validate/preflight assets during requests away from real-time callbacks.
   Predecode or asynchronously decode images in a bounded GUI-owned cache keyed
   by personality, style, path and hash. Do not preload the entire library.
   One native RGBA frame is 1.5 MiB; a 22-frame reaction is 33 MiB before GPU
   overhead. Budget current/persistent/fallback resources explicitly.
3. `frame_at` uses preflighted selections and does no file loading or decoding,
   including return to a cached persistent context. It requires nonnegative,
   nondecreasing monotonic integer milliseconds.
4. Persistent requests update the return context. Duplicate active events do
   not restart playback; per-state cooldowns suppress repeated reactions. Higher
   priorities preempt lower ones. A noninterruptible finite reaction blocks
   equal/lower priorities; a strictly higher priority can preempt it.
5. After a finite reaction, restore the latest persistent context, with IDLE as
   the default. Reduced motion selects a same-personality static expression and
   uses its finite hold metadata when applicable. It never swaps personalities.
6. Production approval filtering defaults on. New candidates therefore fall
   back to the accepted IDLE until reviewed. Preview tools explicitly allow
   validated candidates. A malformed pack hides the avatar with diagnostics;
   image/render failures must remain cosmetic and must not block routing.
7. Switching personality/style clears presentation caches and cooldown state;
   hide the previous image before loading the next. Keep all generation,
   filesystem operations and GUI work off real-time audio threads.

The GUI framework is still an application-level decision. No main GUI, audio
engine, shared user-configuration schema, dialogue catalog, DFA or easter-egg
implementation was changed. No production Windows integration or hardware
performance result is claimed. All changes remain under `assets/personalities/`.

## Rebuild and remaining approval

Use pinned `requirements-tools.txt`. The image backend exposes no replay seed
or model version; retained raw keys plus this repository revision reproduce the
compositing workflow. `full-generation-settings.json` records the reproducer,
face settings, policies and dependency hashes. Do not run the historical IDLE
preparation/generation tools against accepted assets; they refuse approved data.

```sh
python assets/personalities/tools/generate_states.py default core-a
python assets/personalities/tools/generate_states.py default core-b
python assets/personalities/tools/generate_states.py default optional
# Repeat those three batches for uwu and vanilla.
python assets/personalities/tools/audit_library.py
python assets/personalities/tools/build_review_library.py
python -m unittest discover -s assets/personalities/tools -p 'test_*.py'
```

Regular styles remain disabled/unproduced, with canonical regular references
preserved. The separate `despair` example is documented creative vocabulary,
not an additional requested catalog state. Vanilla's eight optional emotional
states intentionally fall back to its own IDLE. No core state is left ungenerated.

Human review remains for the 49 new static expressions, 46 new sequences,
localized facial key quality, motion/effect timing, and the complete-cutout
choreography choice. IDLE approval and expanded direction are already recorded;
they do not automatically approve newly generated artwork. Primary GUI
integration and any future regular/articulated pose expansion remain separate.
