# Static GUI reference placeholder

`placeholder-avatar.png` is the byte-exact `Norm Reference.png` supplied in
Placeholders.zip. Its actual dimensions are **1201×1310 RGBA**, rather than the
512×768 described in the assignment. The UI preserves these actual proportions
and alpha; it does not crop, redraw, recolor, or stretch the source. The supplied
logo matched the existing canonical branding asset; it was not replaced.

The style image was supplied inline in the task, not as a file in this archive.
Its layout and dark mint/magenta palette guide this iteration; exact native visual
comparison remains a Windows acceptance task.

The existing WIC loader embeds/decodes this PNG once. AgentDisplay accepts a
preferred logical size and caches by full client bounds; normal/compact avatar
regions are 144×216 / 96×144 logical pixels. The original image stays immutable.
Vanilla retains the canonical logo. Other personalities share this temporary static
placeholder; no permanently bound Norm asset system, animation, mood logic, new
messages, schema, or separately developed personality-module code is introduced.

GUI palette constants live in gui/palette.h. SurfacePanel paints restrained rounded
borders around native sizer-owned controls. Follow System, Light and Windows high
contrast remain available. Commentary is a bounded scrolling region separate from
the image, so message length cannot enlarge the header. Footer counts come from
the controller snapshot and viewed draft: errors count retained error/scan-failed
log entries. Warning severity/count is not exposed by this controller and is labelled
unavailable rather than fabricated. Endpoint tuples are selected settings, not
negotiated measurements. Automatic summary values are never asserted as verified.

The current backend remains the existing simulator and settings remain session-only.
No device discovery, synchronization, buffer policy, routing or persistence logic
was changed. Windows rendering/DPI/shell behavior must be reviewed manually; a
cross-build is not proof of visual fidelity. Native screenshots continue to be
produced by the existing Windows GUI smoke workflow when that environment permits.

## Resize correction

Routing, Active and Viewed each have their own line in a fixed 280×120 logical
status region; longer names are ellipsized with full text available in its tooltip.
The branding column reserves 300 logical pixels, independently of status content.
Commentary is a nonstretching 156-logical-pixel card with scrolling overflow.
Its width and left/right edges follow the natural width of the complete routing
and exit button row, rather than expanding with the window. The avatar follows
the card with a fixed 14-logical-pixel gap; unused width stays to its right. The
minimum window size is DPI-scaled so the controls remain accessible.
Native resize notifications are coalesced after wxFrame settles its client bounds;
wrapping is measured only after layout, then the frame is invalidated for repaint.
Rounded surfaces request full repaint on resize so their previous curved edges are
erased, not retained outside newly exposed strips. Neither resizing nor status
updates select new commentary. Rounded surfaces inset content by 12 logical pixels so native child
backgrounds cannot erase the curved stroke. Header geometry smoke checks exercise
minimum, intermediate and desktop widths in both directions with long status names,
checking button/card edge alignment, constant card size and the avatar gap.

Avatar presentation regions use 2:3 proportions for future 512×768 artwork. The
current 1201×1310 source is still rendered proportionally and unchanged, so it has
more empty space in that region until the final portrait asset arrives.
