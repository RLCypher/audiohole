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

    # PE named resources precede integer IDs. Merely finding the logo bytes does
    # not establish which group Explorer extracts at index zero (wx also embeds
    # stock icons). Verify the actual first group and every referenced image.
    pe = struct.unpack_from('<I', binary, 0x3c)[0]
    if binary[pe:pe + 4] != b'PE\0\0':
        raise ValueError('Not a PE executable')
    sections = struct.unpack_from('<H', binary, pe + 6)[0]
    optional_size = struct.unpack_from('<H', binary, pe + 20)[0]
    optional = pe + 24
    if struct.unpack_from('<H', binary, optional)[0] != 0x20b:
        raise ValueError('Expected Windows x64 PE32+')
    resource_rva = struct.unpack_from('<I', binary, optional + 112 + 16)[0]
    section_table = optional + optional_size

    def file_offset(rva):
        for section in range(sections):
            header = section_table + section * 40
            virtual_size, start, raw_size, raw_offset = struct.unpack_from('<IIII', binary, header + 8)
            if start <= rva < start + max(virtual_size, raw_size):
                return raw_offset + rva - start
        raise ValueError('Resource RVA outside section table')

    resource_root = file_offset(resource_rva)

    def entries(relative):
        header = resource_root + relative
        named, numeric = struct.unpack_from('<HH', binary, header + 12)
        result = []
        for index in range(named + numeric):
            name, child = struct.unpack_from('<II', binary, header + 16 + index * 8)
            if name & 0x80000000:
                string_offset = resource_root + (name & 0x7fffffff)
                count = struct.unpack_from('<H', binary, string_offset)[0]
                name = binary[string_offset + 2:string_offset + 2 + count * 2].decode('utf-16le')
            result.append((name, child & 0x7fffffff))
        return result

    def data(relative):
        rva, size = struct.unpack_from('<II', binary, resource_root + relative)
        offset = file_offset(rva)
        return binary[offset:offset + size]

    types = dict(entries(0))
    first_name, first_group = entries(types[14])[0]  # RT_GROUP_ICON
    if first_name != 'AAA_AUDIOHOLE':
        raise ValueError(f'First executable icon group is {first_name!r}, not AudioHole')
    group = data(entries(first_group)[0][1])
    if struct.unpack_from('<HHH', group) != (0, 1, 7):
        raise ValueError('First shell icon group does not contain seven sizes')
    icon_entries = dict(entries(types[3]))  # RT_ICON
    for index, expected_payload in enumerate(payloads):
        identifier = struct.unpack_from('<H', group, 6 + index * 14 + 12)[0]
        actual = data(entries(icon_entries[identifier])[0][1])
        if actual != expected_payload:
            raise ValueError('First shell icon group references noncanonical artwork')
    print('PASS: first shell icon group is AudioHole, with all seven canonical images')
