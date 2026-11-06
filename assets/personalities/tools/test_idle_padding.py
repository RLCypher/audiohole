"""Regression checks for padding removal against its preserved Git baseline."""
import hashlib
import json
from pathlib import Path
import subprocess
import unittest

from PIL import Image

from trim_idle_padding import trim

ROOT = Path(__file__).resolve().parents[3]
PROVENANCE = ROOT / 'assets/personalities/provenance'


class PaddingRemovalTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.report = json.loads((PROVENANCE / 'idle-padding-removal.json').read_text())
        cls.ledger = json.loads((PROVENANCE / 'deployed-assets.json').read_text())
        cls.before = json.loads(subprocess.check_output([
            'git', 'show', cls.report['baseline_commit'] +
            ':assets/personalities/provenance/deployed-assets.json'], cwd=ROOT))

    def test_retained_frames_keep_original_bytes_and_exposures(self):
        self.assertEqual(len(self.report['sequences']), 34)
        self.assertEqual(self.report['removed_frames'], 97)
        for sequence in self.report['sequences']:
            manifest_path = sequence['manifest']
            old = json.loads(subprocess.check_output([
                'git', 'show', self.report['baseline_commit'] + ':' + manifest_path], cwd=ROOT))
            current = json.loads((ROOT / manifest_path).read_text())
            old_state = old['styles'][sequence['style']]['states'][sequence['state']]
            state = current['styles'][sequence['style']]['states'][sequence['state']]
            old_frames = {frame['path']: frame for frame in old_state['animation']['frames']}
            self.assertEqual(state['behavior'], old_state['behavior'])
            self.assertEqual(state['animation']['loop'], old_state['animation']['loop'])
            pack = str(Path(manifest_path).parent)
            for index, mapping in enumerate(sequence['retained_frames']):
                frame = state['animation']['frames'][index]
                self.assertEqual(frame['path'], f'Generated/{sequence["style"]}/{sequence["state"]}/{index:04d}.png')
                self.assertEqual(frame['duration_ms'], old_frames[mapping['from']]['duration_ms'])
                actual = hashlib.sha256((ROOT / pack / frame['path']).read_bytes()).hexdigest()
                self.assertEqual(actual, self.before['files'][pack + '/' + mapping['from']])

    def test_idle_and_static_artwork_is_unchanged(self):
        for relative, digest in self.before['files'].items():
            if '/Generated/' not in relative or not relative.endswith('.png'):
                continue
            if '/idle/' in relative or Path(relative).parent.name in {'chibi', 'logo'}:
                self.assertEqual(hashlib.sha256((ROOT / relative).read_bytes()).hexdigest(), digest)

    def test_no_padding_stale_files_or_invalid_pixels(self):
        self.assertEqual(trim(ROOT), [])
        files = {p.relative_to(ROOT).as_posix() for p in (ROOT / 'personalities').glob('*/avatar/pack/Generated/**/*.png')}
        expected = {name for name in self.ledger['files'] if name.endswith('.png')}
        self.assertEqual(files, expected)
        self.assertEqual(len(files), self.ledger['approved_png_count'])
        allowlist = set((ROOT / 'scripts/runtime-files.txt').read_text().splitlines())
        self.assertTrue(files <= allowlist)
        self.assertEqual({name for name in allowlist if '/avatar/pack/Generated/' in name}, files)
        for name in files:
            with Image.open(ROOT / name) as image:
                self.assertEqual(image.size, (512, 768))
                self.assertEqual(image.mode, 'RGBA')
                self.assertEqual(image.getchannel('A').getextrema(), (0, 255))


if __name__ == '__main__':
    unittest.main()
