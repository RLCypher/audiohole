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
regions are 200×220 / 120×132 logical pixels. The original image stays immutable.
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
