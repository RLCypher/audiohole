"""Remove exact IDLE-pose bookends, retaining encoded PNGs and exposure timing.

Run with --apply to change files. The default is a read-only plan. Only contiguous
leading/trailing frames of non-IDLE sequences are candidates. IDLE itself,
interior poses, playback policy and static expressions are never changed.
"""
import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image


def pixel_digest(path):
    with Image.open(path) as image:
        assert image.mode == 'RGBA' and image.size == (512, 768), path
        return hashlib.sha256(image.tobytes()).hexdigest()


def trim(root, apply=False):
    changes = []
    for manifest_path in sorted((root / 'personalities').glob('*/avatar/pack/manifest.json')):
        pack = manifest_path.parent
        manifest = json.loads(manifest_path.read_text())
        for style, variant in manifest['styles'].items():
            if not variant.get('enabled', False):
                continue
            states = variant['states']
            idle = states['idle']
            references = [idle['static']['path']] + [
                frame['path'] for frame in idle.get('animation', {}).get('frames', [])]
            idle_pixels = {pixel_digest(pack / name) for name in references}
            for state, entry in states.items():
                frames = entry.get('animation', {}).get('frames', [])
                if state == 'idle' or not frames:
                    continue
                matches = [pixel_digest(pack / frame['path']) in idle_pixels for frame in frames]
                first, end = 0, len(frames)
                while first < end and matches[first]:
                    first += 1
                while end > first and matches[end - 1]:
                    end -= 1
                if first == 0 and end == len(frames):
                    continue
                if first == end:
                    raise ValueError(f'Entire sequence is IDLE: {manifest_path}/{style}/{state}')
                kept = []
                payloads = []
                mapping = []
                for index, old in enumerate(frames[first:end]):
                    data = (pack / old['path']).read_bytes()
                    assert hashlib.sha256(data).hexdigest() == old['sha256']
                    new = dict(old)
                    new['path'] = f'Generated/{style}/{state}/{index:04d}.png'
                    kept.append(new)
                    payloads.append(data)
                    mapping.append({'from': old['path'], 'to': new['path'],
                                    'sha256': old['sha256'], 'duration_ms': old['duration_ms']})
                changes.append({'manifest': manifest_path.relative_to(root).as_posix(),
                                'state': state, 'style': style, 'before_frames': len(frames),
                                'after_frames': len(kept), 'removed_indices':
                                list(range(first)) + list(range(end, len(frames))),
                                'removed_ms': sum(f['duration_ms'] for f in frames[:first] + frames[end:]),
                                'retained_frames': mapping})
                if apply:
                    for new, data in zip(kept, payloads):
                        (pack / new['path']).write_bytes(data)
                    retained = {frame['path'] for frame in kept}
                    for old in frames:
                        if old['path'] not in retained:
                            (pack / old['path']).unlink()
                    entry['animation']['frames'] = kept
        if apply:
            manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
    return changes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument('--apply', action='store_true')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    changes = trim(args.root, args.apply)
    report = {'operation': 'Remove exact IDLE-pose bookends; preserve retained PNG bytes/timing.',
              'sequences': changes, 'removed_frames': sum(
                  item['before_frames'] - item['after_frames'] for item in changes)}
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(f'{len(changes)} sequences; {report["removed_frames"]} IDLE padding frames' +
          (' removed.' if args.apply else ' found (read-only).'))


if __name__ == '__main__':
    main()
