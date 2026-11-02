"""Compare the actual native decoder against Pillow for all operating RGBA pixels."""
import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    report = json.loads(args.report.read_text())
    assert report['failed'] == 0, report
    hashes = report['decoded_rgba_sha256']
    assert len(hashes) == 796, len(hashes)
    for relative, expected in hashes.items():
        with Image.open(args.root / relative) as image:
            assert image.size == (512, 768) and image.mode == 'RGBA', relative
            actual = hashlib.sha256(image.tobytes()).hexdigest()
            assert actual == expected, f'Native RGBA differs from the accepted PNG: {relative}'
    print('PASS: 796 native-decoded RGBA buffers match Pillow byte-for-byte.')


if __name__ == '__main__':
    main()
