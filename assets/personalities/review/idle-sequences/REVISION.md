# Steam, mouth and preview revision

The two coffee plumes now have their own animated alpha layers. The old heavy
cartoon ribbons were isolated from the prior generated masters, reduced to
26 × 46 pixels for Norm and 24 × 46 for UwU, and reduced to 38% source opacity.
They rise up to eight native pixels, curl gently, and fade during the loop.
Shorter frame holds make that movement visible without increasing the twelve-
frame batch or changing the five-second loop length. UwU's IDLE mouth is now a
small closed smile, including during the blink; the original nose is preserved
pixel-for-pixel. Vanilla's operating artwork is unchanged.

The previous steam implementation was inadequate: Norm had a tiny sideways
deformation and UwU had no steam-specific motion. Both now use the separate
plume compositor, with regression checks on actual generated frames.

The previous gallery referenced neighboring APNG files. The revised gallery
embeds three GIFs, contains no scripts, and needs no neighboring files, server,
or network. Its size is approximately 623 KB. The separate frame inspector was
reduced from about 12 MB to 2.6 MB with display-only WebP compression; native
APNGs and individual operating PNGs remain available for exact pixel review.

The app-specific “error inside the visualization” cannot be exercised from the
workspace browser. The new gallery was tested offline in Chromium, including
visible GIF advancement and zero external requests. Direct `*-review.gif` files
and `idle-previews.zip` are provided so review does not depend on that viewer.
Extract the ZIP and open `index.html` in a regular browser, or open the GIFs
directly. Their checkerboard is a review background only; all 39 operating PNGs
retain real transparency and the exact 512 × 768 8-bit RGBA contract.

Validation passed for all operating assets, alpha backgrounds, anchors, loop
endpoints, protected case/ground regions, and manifests. Twenty-seven Python
tests and twelve browser checks passed. New tests cover plume rise/fade,
nonstatic steam pixels in both real sequences, the closed mouth throughout the
blink, the preserved nose, and GIF timing. The current frame exposures are
500, 500, 400, 400, 400, 100, 100, 400, 500, 500, 600, 600 ms.

Original generated masters are archived under each personality's Intermediate
directory. Canonical references and supplied definitions are unchanged. Rebuild
with `prepare_idle_revision.py` before `animate_idle.py`; revision sources,
patch coordinates, layer metadata and hashes are in `provenance/`.

The inherited alpha warnings and some eyelid-patch softness still require human
visual review. These remain limited-motion candidates with twelve authored
frames, not a claim of fluid continuous motion. The batch remains unapproved
and stops here for review.
