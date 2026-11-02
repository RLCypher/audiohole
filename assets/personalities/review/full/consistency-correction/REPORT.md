**Historical evidence.** Current Horu rest, scale, talking, working eyes and cup continuity are covered by the [latest report](../horu-revision/REPORT.md). The original source drawings and prior accepted rest remain archived.

# UwU consistency correction — candidate revision 2

All 19 non-IDLE states now use character-only cleanup drawings, one frozen case
carrier, and the canonical badge artwork. The 18 sequences retain their existing
frame counts, timing and held-pose acting. Accepted IDLE and all Norm/Vanilla
operating PNGs are unchanged. **This correction remains pending human art review.**

## Corrected artwork

- **Case:** remove independently redrawn boxes from the pose drawings. Complete
  the previously obscured prop once from its reference, then reuse that same
  raster and position in every pose. The entire visible logo/wordmark block at
  `(94, 498)–(213, 596)` is copied exactly from approved IDLE. Character layers
  provide explicit natural occlusion; the box itself never changes between keys.
- **Badge:** transfer the approved closed wormhole emblem and card rim from one
  canonical template. No model-generated logos remain on the cards. Restore two
  omitted badges at the belt attachment. Use card-sized placement checks so a
  garter buckle cannot be mistaken for a portrait badge.
- **Costume:** freeze the sleeve-specific accents, sweater coverage, glove palms,
  belt/pouch/badge layout, primary garter and opposite stocking top in a
  [wardrobe reference](wardrobe-model.png). Remove the introduced pink palm pads,
  duplicate buckle hardware and extra mint bands. Eight residual sleeve accents
  were corrected with localized model edits and masks; other drawing pixels and
  silhouette alpha outside those edit masks are preserved.
- **Acting:** retain articulated anticipation, raised arms, recoils, stomps,
  resting poses and follow-through. Source registration uses ear landmarks,
  explicit overrides for folded ears, and a body-height fit to the established
  ground. Finished animation frames hold those drawings; no moving rest-cutout
  substitutes for the acting.

[Before/after branding](brand-before-after.png) · [Fixed artwork layers](locked-layers.png)

## Review at native and compact sizes

- [UwU catalog](../horu/index.html): per-state replay, native/compact inspection,
  reduced motion, native PNG previews and every operating frame.
- [Looping three-state GIF](../horu-body-revision-loop.gif) and
  [all 48 retained keys](../horu-body-keyposes.png).
- Native outfit/detail crops: [1](wardrobe-1.png), [2](wardrobe-2.png),
  [3](wardrobe-3.png), [4](wardrobe-4.png), [5](wardrobe-5.png), [6](wardrobe-6.png).
- [Portable UwU review ZIP](../horu-review.zip). Extract and open `index.html`
  in a regular browser if the embedded visualization fails.

47 keys are used in operating artwork; one deadpan variant remains an unused
intermediate. The GIF and WebP inspector copies are display formats; operating
art remains transparent 512 × 768, 8-bit RGBA PNG. Compact review uses 128 × 192.

## Validation and review limits

The full image audit passes: 796 operating PNGs, 52 statics, 49 sequences and
744 frames, with no missing/orphaned operating files. **52 Python tests passed**,
including the new fixed-case/visible-lettering and canonical-card regressions.
Every finished key is reconstructed from the same case layer plus its character
layer; visible lettering pixels match approved IDLE exactly. Badge templates and
source/overlay hashes are retained. The 501 approved PNG hashes remain unchanged.
Both offline Chromium suites passed (12 IDLE checks and six full-library
groups). All 52 native previews decode completely. Natural character occlusion
leaves at least 85.6% of the wordmark letter pixels unobstructed in every key;
those exposed pixels are exact approved artwork. Details are in [validation.json](validation.json).

The historical missing-wordmark review applies to revision 1 at commit `73edab2`;
its original keys and evidence remain in `../consistency-audit/` and the original
body-key directory. Current operating files use this corrected candidate.

These are limited 2–4-key performances with deliberate holds, not smooth fully
in-betweened animation. The cleanup backend can redraw small facial/fabric
features; inspect them along with transitions and layering. The newly revealed
case hardware, covered glove palms and least-added-detail opposite thigh are
reviewable completions of areas not fully established by the resting references.
The approved rest image itself was not altered. Some quiet poses retain the
canonical purple mug as a recurring prop. Technical locks do not grant artistic
approval or certify every anatomical/cloth-fold choice.

## Rebuild and scope

`tools/correct_uwu_consistency.py --package` composes the retained character
sources, masked material edits and fixed brand assets. The old body-only generator
refuses to replace the corrected library. `provenance/uwu-consistency-revision.json`
pins the compositor, inputs, dependency hashes and prompts. Previous manifests
and body metadata are archived under `provenance/consistency-revision/previous/`.
The backend supplies no replay seed/model version; retained model outputs allow
reproducible compositing. No main GUI, audio engine, new personality or new state
was changed or added.
