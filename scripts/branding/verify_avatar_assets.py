"""Verify the approved PR6 artwork after relocation into owned packages. No image editing."""
import argparse
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[2]
ledger = json.loads((root / 'assets/personalities/provenance/deployed-assets.json').read_text(encoding="utf-8"))
assert ledger['schema_version'] == 1
pngs = 0
for relative, expected in ledger['files'].items():
    path = Path(relative)
    assert not path.is_absolute() and '..' not in path.parts
    data = (root / path).read_bytes()
    assert hashlib.sha256(data).hexdigest() == expected, relative
    if path.suffix == '.png':
        pngs += 1
        assert data[:8] == b'\x89PNG\r\n\x1a\n', relative
        assert int.from_bytes(data[16:20], 'big') == 512, relative
        assert int.from_bytes(data[20:24], 'big') == 768, relative
        assert data[24:26] == bytes((8, 6)), relative  # 8-bit RGBA
assert pngs == ledger['approved_png_count']
print(f'PASS: {pngs} byte-preserved approved RGBA operating PNGs and owned manifests/registries')

parser = argparse.ArgumentParser()
parser.add_argument('--decoded-report', type=Path)
args = parser.parse_args()
if args.decoded_report:
    reference = json.loads((root / 'assets/personalities/runtime/validation/native-tests.json').read_text(encoding="utf-8"))
    actual = json.loads(args.decoded_report.read_text(encoding="utf-8"))
    assert actual['failed'] == 0, actual
    assert actual['decoded_rgba_sha256'] == reference['decoded_rgba_sha256'], 'Native decoded RGBA differs from approved validation'
    print(f'PASS: native decoder matches all {pngs} reference RGBA buffers')
