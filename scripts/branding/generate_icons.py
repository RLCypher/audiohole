"""Maintainer-only conversion; never rewrites the canonical PNG.

Requires Pillow 12.3.0 (MIT-CMU); the generated ICO is committed, so ordinary
CMake builds and application launches do not require Python or Pillow.
"""
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parents[2]
source = root / 'assets/branding/audiohole-logo.png'
with Image.open(source) as logo:
    if logo.mode != 'RGBA' or logo.size != (512, 512):
        raise ValueError('Expected the original 512x512 RGBA logo')
    # Pillow resizes RGBA through premultiplied alpha, avoiding dark edge halos.
    # BMP entries preserve 32-bit alpha and do not require a PNG decoder in wx.
    logo.save(root / 'assets/branding/audiohole.ico', format='ICO',
              sizes=[(n, n) for n in (16, 24, 32, 48, 64, 128, 256)],
              bitmap_format='bmp')
