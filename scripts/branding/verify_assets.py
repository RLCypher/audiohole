"""Dependency-free structural/integrity checks for committed branding assets."""
import argparse
import hashlib
from pathlib import Path
import struct
import zlib

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--executable', type=Path, help='Also verify original PNG and all ICO DIB payloads are embedded')
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
source = (root / 'assets/branding/audiohole-logo.png').read_bytes()
expected = 'cc3bba6fa536eb00c5d6c4e232c8eecaa683637076b1b82a5396abc5f1b8d655'
if hashlib.sha256(source).hexdigest() != expected:
    raise ValueError('Canonical PNG differs from the original supplied file')
if source[:8] != b'\x89PNG\r\n\x1a\n':
    raise ValueError('Invalid PNG signature')
position = 8
while position < len(source):
    length = struct.unpack_from('>I', source, position)[0]
    chunk = source[position + 4:position + 8 + length]
    crc = struct.unpack_from('>I', source, position + 8 + length)[0]
    if zlib.crc32(chunk) != crc:
        raise ValueError('PNG chunk CRC mismatch')
    if chunk[:4] == b'IHDR' and struct.unpack('>IIBBBBB', chunk[4:]) != (512, 512, 8, 6, 0, 0, 0):
        raise ValueError('Expected a non-interlaced 512x512 RGBA8 source')
    position += length + 12
icon = (root / 'assets/branding/audiohole.ico').read_bytes()
if struct.unpack_from('<HHH', icon) != (0, 1, 7):
    raise ValueError('Expected seven ICO entries')
sizes = []
payloads = []
for index in range(7):
    width, height, _, _, planes, depth, length, offset = struct.unpack_from('<BBBBHHII', icon, 6 + index * 16)
    width, height = width or 256, height or 256
    sizes.append(width)
    payloads.append(icon[offset:offset + length])
    if width != height or depth != 32 or planes not in (0, 1) or offset + length > len(icon):
        raise ValueError('Invalid ICO directory')
    header, dib_width, dib_height, _, bits = struct.unpack_from('<IiiHH', icon, offset)
    if (header, dib_width, dib_height, bits) != (40, width, height * 2, 32):
        raise ValueError('Expected alpha-preserving 32-bit DIB icon entry')
    alpha = icon[offset + 43:offset + 40 + width * height * 4:4]
    if len(alpha) != width * height or not any(a == 0 for a in alpha) or not any(a > 0 for a in alpha):
        raise ValueError('Icon alpha is missing or flattened')
if sizes != [16, 24, 32, 48, 64, 128, 256]:
    raise ValueError('Unexpected icon sizes')
print('PASS: original PNG hash/CRC/RGBA dimensions and seven transparent ICO sizes')

if args.executable:
    binary = args.executable.read_bytes()
    if source not in binary or any(payload not in binary for payload in payloads):
        raise ValueError('Executable does not embed the original PNG and all ICO image payloads')
    print('PASS: executable embeds the exact canonical PNG and all seven ICO image payloads')
