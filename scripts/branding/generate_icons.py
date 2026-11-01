"""Maintainer-only conversion; never rewrites the canonical PNG.

Requires Pillow 12.3.0 (MIT-CMU); the generated ICO is committed, so ordinary
CMake builds and application launches do not require Python or Pillow.
"""
from pathlib import Path
from io import BytesIO
import struct
from PIL import Image

root = Path(__file__).resolve().parents[2]
source = root / 'assets/branding/audiohole-logo.png'
with Image.open(source) as logo:
    if logo.mode != 'RGBA' or logo.size != (512, 512):
        raise ValueError('Expected the original 512x512 RGBA logo')
    payloads = []
    sizes = (16, 24, 32, 48, 64, 128, 256)
    for size in sizes:
        # Pillow resizes RGBA through premultiplied alpha. Its ICO writer omits
        # the AND mask for 32-bit frames, so write the complete DIB layout here
        # for shell/legacy icon consumers as well as alpha-aware window drawing.
        frame = logo.resize((size, size), Image.Resampling.LANCZOS)
        stream = BytesIO()
        frame.save(stream, format='DIB')
        dib = bytearray(stream.getvalue())  # Header plus bottom-up BGRA pixels.
        if struct.unpack_from('<I', dib)[0] != 40 or len(dib) != 40 + size * size * 4:
            raise ValueError('Unexpected 32-bit DIB layout')
        struct.pack_into('<i', dib, 8, size * 2)  # ICO height includes XOR and AND planes.
        stride = ((size + 31) // 32) * 4  # Mask rows are DWORD aligned.
        mask = bytearray(stride * size)
        for y in range(size):
            for x in range(size):
                if dib[40 + (y * size + x) * 4 + 3] == 0:
                    mask[y * stride + x // 8] |= 0x80 >> (x % 8)
        payloads.append(dib + mask)

    directory = bytearray(struct.pack('<HHH', 0, 1, len(sizes)))
    offset = 6 + 16 * len(sizes)
    for size, payload in zip(sizes, payloads):
        edge = size if size < 256 else 0
        directory.extend(struct.pack('<BBBBHHII', edge, edge, 0, 0, 1, 32, len(payload), offset))
        offset += len(payload)
    (root / 'assets/branding/audiohole.ico').write_bytes(directory + b''.join(payloads))
