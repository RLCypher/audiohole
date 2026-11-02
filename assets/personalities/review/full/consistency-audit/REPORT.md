**Historical evidence.** Current Horu rest, scale, talking, working eyes and cup continuity are covered by the [latest report](../horu-revision/REPORT.md). The original source drawings and prior accepted rest remain archived.

# UwU quick visual consistency review — 2026-11-02

This is the historical review of revision 1 at commit `73edab2`. The [corrected candidate and current validation](../consistency-correction/REPORT.md) supersede its operating-file findings. Original source keys and the evidence sheets below are preserved.

**Verdict: needs revision.** Full-body acting improved, but the case and wearable
branding are not consistent enough for approval. The previous technical checks
verify valid files, hashes and playback; they do not certify visual identity.

Reviewed all 48 retained body keys against the canonical chibi reference and
approved IDLE, using native case/attire crops and the compact full-body key sheet.
47 keys are used in operating artwork; the second deadpan key is an unused
intermediate. Repeated animation holds were traced to their exact keys rather
than inspected as separate drawings. This was a quick art pass, not a new
frame-by-frame motion or GUI playback review.

## Confirmed branding defects

The unobstructed `AUDIOHOLE` wordmark is absent in **eight states / 16 pose keys /
101 animation frames / eight static assets**. A preliminary claim about `bored`
was corrected after closer crops: both bored keys retain their wordmark.

| State | Affected animation PNG stems, inclusive | Static |
|---|---|---|
| concerned | `0000`–`0015` | affected |
| confused | `0003`–`0011` | affected |
| happy | `0002`–`0009` | affected |
| sleeping | `0000`–`0015` | affected |
| smug | `0002`–`0009` | affected |
| talking | `0000`–`0011` | affected |
| thinking | `0000`–`0015` | affected |
| working | `0000`–`0015` | affected |

Paths follow `UwU/Generated/chibi/<state>/<stem>.png`; each affected static is
`UwU/Generated/chibi/<state>.png`. These are operating frame numbers, not pose
key numbers. [Machine-readable findings](findings.json) list every affected path.
The finite happy/confused/smug sequences regain the lettering when they return
to accepted IDLE, producing a visible branding change.

[Wordmark comparison](wordmark-comparison.png) shows approved IDLE, missing text
in happy/working, and retained text in celebrating. Case front-panel proportions,
emblem proportions/grid, wheel hardware, latches and connectors also vary across
redrawn poses. Aligning the mint stripe only aligns one feature; it does not
preserve the case construction or logo geometry. Retaining the wordmark alone
does not certify the other states' case designs.

The wearable belt badge changes its closed pink wormhole ellipse into open,
stacked curved bars. Its border, scale and attachment also vary. Clear examples
are thinking keys `0000`/`0001` and celebrating key `0003`; see the
[enlarged badge and strap comparison](badge-and-straps.png). The latter uses
nearest-neighbor enlargement of actual pixels for inspection.

## Wardrobe items requiring correction or a closer model-sheet check

- Garter widths, buckle arrangement and sleeve accent-band placement/width vary
  between poses. Thinking `0000`/`0001`, celebrating `0001`/`0002`/`0003`, and
  bored `0000`/`0001` are useful comparisons. Pose/perspective and occlusion explain
  some differences; this pass does not assign a definitive defect count here.
- Celebrating key `0003` (operating frames `0015`–`0017`) introduces pink palm
  pads and a different glove/palm treatment. The canonical reference does not
  clearly establish that palm design; it remains an unresolved invention.
- Keep the accepted facial-expression direction. The required correction is to
  the consistency of props, branding, costume and their attachments during acting.

Native inspection pages: [case 1](case-1.png), [case 2](case-2.png),
[case 3](case-3.png), [attire 1](attire-1.png), [attire 2](attire-2.png),
[attire 3](attire-3.png). Every page starts with approved IDLE for comparison.
[Key/frame mapping](key-frame-map.json) distinguishes operating drawings from
unused intermediate variants.

## Recommended correction order

1. Reuse one canonical case and its complete logo/wordmark as fixed artwork.
   Use explicit occlusion masks where the character crosses it; do not redraw
   the box independently in every pose.
2. Reuse one canonical badge/decal, positioned with the belt's pose and
   perspective. Preserve the closed emblem rather than generating tiny logos.
3. Freeze a wardrobe model sheet: sleeve-specific accent layout, sweater
   neckline/coverage, glove coverage, per-leg garter/buckle layout, belt and
   accessory attachment points. Check the drawings against it at native size.
4. Correct only failed drawings, retain articulated acting and approved facial
   direction, then inspect native/compact sheets and actual transitions again.

No operating artwork, manifest approval, canonical reference, Norm or Vanilla
asset was changed by this review. All **295 UwU candidate operating files** still
match their manifest hashes, and all **501 approved operating hashes** remain
unchanged. UwU's body set remains unapproved. No generation or application changes
were performed for this check.
