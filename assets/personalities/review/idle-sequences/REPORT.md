# Initial IDLE sequences — review handoff

**Status:** Completed limited batch; all artwork remains **unapproved**.

See [the latest revision](REVISION.md) for the corrected steam, closed mouth,
and direct GIF previews that bypass the HTML visualization viewer.
Only the three IDLE masters and three initial IDLE sequences are delivered.
No additional emotional state, main GUI, or audio-engine change was made.

## Deliverables

| Personality | Master | Individual animation frames | Animated previews |
|---|---|---|---|
| Norm (`default`) | `../../Norm/Generated/chibi/idle.png` | `../../Norm/Generated/chibi/idle/0000.png` through `0011.png` | `default-native.png`, `default-compact.png` |
| UwU (`uwu`) | `../../UwU/Generated/chibi/idle.png` | `../../UwU/Generated/chibi/idle/0000.png` through `0011.png` | `uwu-native.png`, `uwu-compact.png` |
| Vanilla (`vanilla`) | `../../Vanilla/Generated/logo/idle.png` | `../../Vanilla/Generated/logo/idle/0000.png` through `0011.png` | `vanilla-native.png`, `vanilla-compact.png` |

Open [the animated gallery](index.html) or [the frame inspector](../preview.html).
Vanilla's master is unchanged. Norm's master has a smaller, fainter steam plume;
UwU's has the same steam revision plus the requested closed-mouth smile. All
other character artwork is retained. The prior masters are archived, and the
localized changes are recorded in `../../provenance/*-idle-revision.json`.
All individual operating assets are 512 × 768 PNG, color type 6, 8-bit RGBA,
with fully transparent background pixels and transparent corners. APNG review
previews are explicitly separate from single-frame operating assets. Compact
previews are 128 × 192; that is a representative review target, not an approved
production GUI requirement.

Every frame appears in [the combined contact sheet](all-personalities-all-frames.png).
Individual `default-all-frames.png`, `uwu-all-frames.png`, and
`vanilla-all-frames.png` sheets label frame indices and exposure times. The
`*-native-compact-inspection.png` panels show open and blink/peak-motion frames
at exact native pixels alongside compact downscales.

## Motion and timing

All three sequences share a **100 ms / 10 Hz exposure timebase** and **5,000 ms
loop duration**. This is limited animation with twelve authored frames and
integer frame holds, not twelve evenly spaced frames or ten unique drawings per
second. Long neutral holds avoid a conspicuous blink every 1.2 seconds.

| Frame | 0000 | 0001 | 0002 | 0003 | 0004 | 0005 | 0006 | 0007 | 0008 | 0009 | 0010 | 0011 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Hold (ms) | 500 | 500 | 400 | 400 | 400 | 100 | 100 | 400 | 500 | 500 | 600 | 600 |

Norm and UwU close their eyes on frames 0005–0006, at 2.2–2.4 seconds; both eyes
reopen at 2.4 seconds. Neutral frames 0000 and 0011 are exact master copies,
giving 1.1 seconds of rest across the loop boundary. There are eleven distinct
frame hashes per sequence because the endpoint intentionally repeats the master.

- **Norm:** restrained 1.4-pixel maximum breathing displacement, a slight mug/hand
  adjustment of at most 0.6 pixels, and small steam movement. The steam now uses an independent alpha layer, with up to eight pixels of
  rise, gentle curl, and fading. Its neutral opacity is 38% of the source and
  its footprint is 26 × 46 pixels. No sip or arm lift was invented. The ground and protected case region are pinned.
- **UwU:** 1.2-pixel breathing displacement, subtle ear flex around 1.2 pixels,
  and a local tail swish up to 3.5 pixels. The ground and protected case region
  are pinned. The mouth is closed throughout IDLE, including the blink.
  Coffee steam has its own 24 × 46 alpha layer, gentle rise/curl and fading.
- **Vanilla:** grid movement is restricted to a conservative interior wormhole
  ellipse, with at most 1.7 pixels horizontal and 2.4 pixels vertical sampling
  displacement. The whole logo alpha and every RGBA pixel outside that mask are
  identical to the master. There is no logo rotation, hue replacement, exterior
  deformation, anthropomorphic feature, or global lighting change.

## Generation method and limitations

Two actual image-backend blink edits were generated from the existing chibi
masters. The backend returned 1024 × 1536 outputs and reinterpreted body details,
despite instructions to change eyes only. Those full redraws are retained as
intermediates but **are not animation frames**. Only registered, feathered eye
patches are composited into the original masters, preserving their alpha and all
pixels outside the eye-mask support. A deterministic 2D mesh compositor then
produces anchored breathing and local prop/ear/tail motion. Vanilla uses only
its canonical pixels and a protected interior mask; it is not redrawn by AI.

This is an actual limited-motion cutout workflow, not a claim of generative video
or independently illustrated in-between poses. The image tool offers no seed or
identity-lock guarantee. The retained blink sources, eye-mask coordinates, motion
parameters, master/frame hashes, and compositor hash are recorded in
`../../provenance/*-idle-animation.json`, `idle-motion.json`, `idle-rigs.json`, and
the two `*-blink-request.json` files. Rebuilding from these retained sources is
deterministic with pinned Pillow; invoking the image backend again is not.

## Verification

Results are recorded in [validation.json](validation.json), the broader
[pack report](../validation.json), and [browser-checks.json](../browser-checks.json).

- All **39 operating images** (three masters, 36 frames) pass PNG dimensions,
  decoded format, transparency, hashes, anchors, and manifest checks.
- All three manifests resolve their own IDLE animations and fall back to their
  own static master when frame preflight fails. Personality switching clears
  previous ownership; production approval filtering still rejects the batch.
- First/last frame maximum RGBA difference is **0** for every loop. Ground and
  protected case-region maximum RGBA difference is **0**. Adjacent alpha-bbox
  centers shift at most **0.5 native pixels** for Norm/UwU and **0** for Vanilla.
- Vanilla's exterior RGBA difference and whole-image alpha difference are **0**
  across all twelve frames. All five canonical references and supplied documents
  remain byte-for-byte unchanged.
- Native and compact APNGs each decode to twelve frames, loop indefinitely, sum
  to exactly five seconds, and match the operating pixels or documented Lanczos
  downscales frame-for-frame.
- **27 Python tests passed.** **Twelve browser checks passed**, including actual
  frame playback, blink stepping, loop wrap, fallback, approval filtering,
  native/compact display dimensions, backgrounds, and JavaScript error checks.
  The one-shot return path was exercised as a preview-tool check; the delivered
  IDLE sequences themselves loop. There is no application target to compile.

## Visual findings and recommended corrections

Frame sheets, enlarged eye patches, and representative open/blink frames were
inspected at native detail and compact scale. Browser checks exercised timed
playback; these checks do not certify fluidity or artistic approval.

1. **Transparency:** Norm/UwU retain the masters' substantial semitransparency
   warnings. Much of the original foreground is alpha 254 rather than 255.
   Fully transparent backgrounds are real; no checkerboard is baked in. Inspect
   edges on white/dark backgrounds before approval. A future reviewed alpha
   cleanup should preserve antialiased edges rather than thresholding all pixels.
2. **Blink corners:** The narrow far eye on Norm and the hair-adjacent eye corner
   on UwU show some patch softness at enlarged native inspection. Compact blinks
   remain legible. If the edges are objectionable, revise those localized eyelid
   patches against the master; do not regenerate the body or change the face.
3. **Limited motion:** The small breathing/coffee/ear/tail shifts are intentionally
   restrained. At 128 × 192, breathing and wormhole movement are very subtle;
   the blink is the clearest character motion. Small case lettering is not
   comfortably readable at that size. There is no anatomy-changing morph or
   attempt to force all silhouettes into matching boxes.
4. **Smoothness:** Twelve drawings with holds are sufficient for this restrained
   first test, but do not establish a fluid loop. If review asks for smoother
   continuous breathing/tail motion, recommend 24–36 motion samples at 12 Hz
   within the moving portion while retaining quiet holds and a roughly five-
   second blink interval. For a softer blink, add a reviewed half-closed eyelid
   keyframe with 50–100 ms exposures rather than stretching the closed-eye hold.
5. **Vanilla:** The conservative mask protects the brand but allows only a small
   grid flex, not a convincing tunnel-flow cycle. If more depth is desired,
   approve a separately layered wormhole grid and more samples, keeping the rim
   and exterior fixed. Do not increase motion merely to make a small loop visible.

## Rebuild and review boundary

From the repository root:

```sh
python assets/personalities/tools/prepare_idle_revision.py
python assets/personalities/tools/animate_idle.py
python assets/personalities/tools/pipeline.py review
python assets/personalities/tools/validate_idle.py
python assets/personalities/tools/review_idle.py
python -m unittest discover -s assets/personalities/tools -p 'test_*.py'
python assets/personalities/tools/check_preview.py --chromium /usr/bin/chromium
```

The optional browser command needs installed Playwright and local Chromium.
The compositor never grants approval and refuses to overwrite approved animation.
The user's instruction is **“Stop after completing these three IDLE sequences
and their previews. Await human review and approval before proceeding further.”**
This batch is complete and stops at that checkpoint. Review can accept each
sequence individually or request local corrections; no further artwork batch
or production integration is authorized by this handoff.
