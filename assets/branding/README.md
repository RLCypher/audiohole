# AudioHole branding

`audiohole-logo.png` is the owner's original **512×512 RGBA** PNG, copied byte-for-byte
from `AudioHole Logo 512.zip`. Its SHA-256 is:

`cc3bba6fa536eb00c5d6c4e232c8eecaa683637076b1b82a5396abc5f1b8d655`

Do not redraw, tint, recolor, crop, flatten, or overwrite it. Mint represents line
output/speakers and pink represents microphone input; the wireframe conduit connects
them. Transparency and the original proportions/artwork are intentional.

`audiohole.ico` is a generated multi-resolution derivative containing transparent
32-bit DIB entries at 16, 24, 32, 48, 64, 128 and 256 pixels. It has no status color.
Maintainer regeneration only (ordinary builds do **not** require these tools):

```powershell
python -m pip install Pillow==12.3.0
python scripts/branding/generate_icons.py
python scripts/branding/verify_assets.py
```

Pillow 12.3.0 uses the MIT-CMU license and is a conversion-only development tool,
not linked, bundled, or required at runtime. Its ICO writer uses high-quality
resizing from the canonical image. `verify_assets.py` uses Python's standard library
only; CI checks the original hash/PNG CRCs, ICO sizes/alpha storage, and exact
PNG/DIB payloads in each built executable. Generated
output is committed; neither CMake nor the application rewrites the source image.

## Runtime architecture

- `app.rc` embeds the original PNG as RCDATA and the ICO as the primary application
  icon. Numeric icon resource 1 precedes framework stock icons. CMake explicitly
  tracks both assets as RC dependencies. Window/taskbar/Alt+Tab icons use the ICO
  bundle. Current packaging creates no shortcuts; no new shortcut system is added.
- `gui/branding.cpp` decodes the PNG once on the existing GUI STA with Windows
  Imaging Component, converting to straight RGBA without a color transform. COM
  references are locally owned/released. WIC and shlwapi are Windows system
  components; wx's disabled libpng/zlib features remain disabled.
- `AgentDisplay` is a wx-owned child in the upper-right header, approximately
  128×128 logical pixels. It caches aspect-preserving scaled bitmaps and paints
  alpha over the current application surface. Image replacement and DPI/size
  changes update the cache; normal painting does not decode or resample. The visual refinement now uses a separate header branding logo and a static
  placeholder for non-Vanilla personalities; Vanilla retains the canonical logo.
  See ../presentation/README.md for actual asset dimensions and compact sizing. Existing commentary sits to its
  left; sprites, animation, expressions and speech bubbles remain unimplemented.
- The existing tray uses `TrayIconCache`: size/color-keyed native icons, updated only
  when state, tooltip, effective DPI size or tray installation changes. The existing
  GUI simulation timer reads state, but does not redraw an unchanged icon. The frame's
  DPI supplies a practical size hint; Explorer's actual multi-monitor tray sizing
  still requires manual validation. Requested physical icon sizes are bounded to
  16–64 pixels, keeping the finite cache small.
- `presentation/icon_outline.cpp` is independent of wx/Windows and audio work. It
  adds a 1–2 physical pixel stroke outside the logo-specific horizontal alpha
  envelope, closing internal openings/wireframe and interpolating transparent layer
  seams. This deliberately models the supplied stacked conduit, not arbitrary
  future sprite geometry. Every scaled source pixel with nonzero alpha is copied
  unchanged. Interior transparency stays clear; no background is added. Unit tests
  cover antialias preservation, holes/seams, bounds and state mapping.
- Stopped and Ready use neutral gray; Running green; Degraded amber; Failed red;
  Starting/Stopping/Recovering blue. Invalid/unknown states use gray. These are
  presentation mappings of the existing aggregate state, not new health signals
  or a replacement aggregation/priority system. Tooltip/menu keep canonical text.
  The existing GUI status dot is independent and retains its accepted colors.

PNG and ICO reference copies are installed at `assets/branding/` in each package;
application rendering uses embedded resources and works without these loose copies.
The extracted-package smoke test moves the copies outside the package before launch.
No online image fetching, source-tree/working-directory dependency, runtime Pillow,
new agent behavior, real audio, or persistent configuration is introduced.

## GitHub social preview (manual; no repository settings changed)

The README references the original PNG through a relative path. GitHub social
preview is a separate repository setting, and no repository-settings tool is exposed
in this session. With repository-owner authorization, open **Settings → General →
Social preview → Edit → Upload an image**. Try the original PNG only if GitHub accepts
its dimensions. GitHub recommends a wide 1280×640 image; the supplied 512×512 original
may not meet its upload requirements. If rejected, obtain owner approval for a
separate social-preview composition/size. Do not add a background, stretch, or alter
the logo to manufacture a social card without approval. Preview changes do not
publish through a README commit.

## Validation limits

Portable tests and renderer previews do not establish native Windows visual quality.
Check the taskbar, Alt+Tab, Explorer, tray at 16/32 pixels on light/dark taskbars,
monitor/DPI changes, high contrast, window resize, and Explorer restart on Windows.
No claim of actual audio health follows from the simulated controller's status.
