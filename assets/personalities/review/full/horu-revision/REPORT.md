# Horu — name, rest scale, talking, eyes and props

The character is **Horu**. Display names, the manifest's `character_id`, character documentation, review headings, public review folders and filenames now use Horu/`horu`. The application mode ID `uwu`, legacy source-pack directory `UwU/`, canonical reference filenames and historical provenance retain their compatibility meanings. No application configuration, main GUI or audio engine was changed.

## IDLE and scale

- Replace the old IDLE case with the **same fixed canonical case raster** used by the other states. The box is never scaled, moved or regenerated per frame.
- Reduce only the IDLE character to **88%** of the prior size, uniformly about `(300, 704)`. The feet retain the ground; the head, clothing, accessories, mug and tail receive the same scale. Existing expressive poses retain their acting rather than being resized to equal total heights during crouches/recoils.
- Preserve the accepted closed-mouth face and costume RGB. The image backend supplied an equipment-removal matte; its redrawn character colors and diffuse halo are not used. Local matte cleanup removes prop remnants at the sleeve/hand/accessory overlap. Bicubic steam alpha is capped at its accepted maximum to avoid opacity overshoot.
- Preserve all **12 IDLE exposures**, blinking, subtle breathing, ears, tail and steam, **5.0 seconds**, on the same **100 ms timebase with holds**. Individual frame durations remain in the manifest.
- Replace **53 reaction rest exposures** with the new master so theatrical reactions enter/leave the current rest. These are held drawings with pose cuts; smooth in-between animation is not claimed.

[Native scale comparison](scale-native.png) · [Compact 128 × 192 comparison](scale-compact.png) · [Every IDLE frame](../horu/idle-frames.png) · [IDLE GIF](../horu/idle.gif)

The comparison includes the prior larger IDLE, revised IDLE, talking, working, celebration and surprise, with one ground guide and fixed canvas scale. Head/headset scale is now closer across rest and actions. Intentional posture, ear rotation, perspective and squash still change the visible silhouette; this is a visual consistency pass, not a certified 3D rig.

## Talking and working

**Talking is redrawn with four full-body keys.** Each key has two continuous arms: one hand gestures and the other holds the purple cup. There is no extra choker/chest hand. Mouth openings/closures, palm turns, an index gesture and ear/tail follow-through supply the speech performance. All four cup holdings are retained throughout the 12-frame, **1.2-second** loop. The curled tail is layered behind the fixed case; it cannot obscure or tint the branding. The registration keeps the full source body on a taller working canvas before fitting to the ground, preventing boot truncation.

[Four talking drawings at native size](talking-keys-native.png) · [Talking replay](../horu/talking-preview.html) · [Talking GIF](../horu/talking.gif) · [Every talking frame](../horu/talking-frames.png)

**Working's cyan spill is repaired locally.** Only 254 pixels in key 0000 and 364 in key 0001 change, using ivory sclera material from the retained model correction. Original eye outlines, iris/pupil art, face, clothing, accessories and silhouette outside the spill masks remain exact. The cup rests visibly on the box while both hands work with the cable/connector. The existing 16-frame, **2.4-second** loop is preserved.

[Eye before/after](working-eyes.png) · [Working replay](../horu/working-preview.html) · [Working GIF](../horu/working.gif)

## Prop continuity and review

Every acting key now accounts for the coffee cup. Nine keys hold it; 41 use one reusable cup raster parked on the front of the equipment case. This also corrects the held/missing alternation in bored. Parked cups are layered in front of a curled tail or leaning sleeve so the prop remains visible. The cup is a separately completed canonical-reference prop, not independently regenerated in each pose. Its conflicting bottom ellipse has also been removed: only the top opening is visible, with a solid curved base below. See the [mug perspective correction](mug-before-after.png). Other acting drawings remain unchanged apart from this prop overlay; existing branding and badge locks remain active.

Pose cuts can switch the cup between a hand and the box. Setting down/picking up is not separately drawn, so placement changes can still be abrupt. The talking loop keeps the cup held throughout. Additional transition drawings would be needed for continuous placement motion, and are not generated in this pass.

[Current Horu catalog](../horu/index.html) · [Portable review ZIP](../horu-review.zip) · [Six-second review montage](preview.gif) · [All 50 retained body keys](../horu-body-keyposes.png)

The montage samples IDLE, talking and working at a 100 ms review cadence; the individual previews preserve each state's exact timing. Operating assets remain genuine-alpha **512 × 768, 8-bit RGBA PNGs**. GIF checkerboards belong only to review media. Extract the ZIP and open `index.html` in a regular browser if the embedded visualization fails.

## Validation and acceptance

Current validation results are recorded in [validation.json](validation.json), with the executed test log in [python-tests.txt](python-tests.txt). The library remains 52 statics, 49 sequences, 744 frames and 796 operating PNGs. No states were added. Native previews and compact review media are rebuilt from current manifests.

Norm and Vanilla remain approved and pixel-identical. The prior 13 accepted Horu IDLE PNGs are preserved byte-for-byte under `UwU/Intermediate/HoruIdleRevision/Previous/`, along with their original manifest and approvals. The active approval ledger redirects those old hashes to the archive; it does not mark revised images approved. All 501 prior approved PNGs remain available: 488 unchanged operating images plus 13 archived prior Horu IDLE assets.

**Revised Horu artwork awaits human visual review.** Approved-only preview mode hides Horu until the revised IDLE receives approval. Technical tests cannot automatically count illustrated hands or certify anatomy; the four talking drawings and outfit/prop layers were inspected at native and compact sizes.

Rebuild order: `tools/revise_horu_idle.py --package`, then `tools/repair_horu_performances.py --package`, then the review builders. Inputs, model outputs, prompts, masks, generator snapshots and hashes are retained under `provenance/horu-*-revision.json`, `provenance/horu-performance-repair.json`, and the two `Horu*` intermediate directories. No backend replay seed is exposed; the retained outputs make compositing reproducible.
