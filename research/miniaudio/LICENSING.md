# Licensing and source reuse

**Verified upstream text**, revision `9634bedb5b5a2ca38c1ee7108a9358a4e233f14d`; [L1 source references](SOURCE_REFERENCES.md). This is an engineering inventory, not a final legal compatibility decision for AudioHole's as-yet-unselected license.

## Core license choice

Root `LICENSE` and the end of `miniaudio.h` offer a choice of public-domain dedication (Unlicense text) or MIT No Attribution (MIT-0). The core MIT-0 text permits use, copying, modification, distribution and sublicensing and omits the ordinary MIT notice-retention condition. Preserve source provenance and license text in dependency inventory as good maintenance practice even when not a license requirement. Root LICENSE says copyright 2025; the current header says 2026. Record both exactly; this does not change the license alternatives.

| Activity | Practical implication |
| --- | --- |
| Study algorithms/implementation | This research is authorized; no production reuse implied. |
| Independently implement a mathematical method | Document independent code/provenance; do not copy source under the label of independent implementation. |
| Include miniaudio as a library | Select a permitted license, pin revision, inventory included code/options, review compatibility with AudioHole's eventual license. Separate production authorization required. |
| Copy an individual component | Code remains sourced from miniaudio even if only a function or DSP section; audit helpers/configuration and applicable licenses. Not authorized for production by this task. |
| Modify/adapt upstream code | Same provenance/license tracking plus maintained patch and tests; requires separate production authorization. No upstream code was modified in this research. |

## Optional components and dependencies

The processing-only probe disables decoding/encoding/engine/resource-manager/node graph/device I/O; it uses no Speex or external codec library. Windows I/O relies on OS facilities rather than bundling a new audio driver. Those platform/toolchain redistribution terms still belong in a future binary inventory.

Upstream contains optional extras and decoder integrations. `extras/stb_vorbis.c` has its own MIT/public-domain alternatives; choosing ordinary MIT entails retaining its notice and permission text. External libvorbis/libopus or other custom resamplers bring their own terms and transitive dependencies; their full distributions were not acquired/audited here, and the core MIT-0 option cannot be assumed to cover them. Node extras can also embed third-party implementations. This is a scoped core review, not blanket approval of the repository.

The built-in resampler enum at this revision is linear/custom; the historical built-in Speex backend was removed. Reintroducing Speex as a custom backend would require a separate version/license/quality review.

The source clone under `upstream/` is downloaded reference material and retains upstream license files. The original C probe includes the header only for research, with no copied upstream production component. No new production dependency, copied production code, distribution, upstream issue, PR, or third-party message was created.
