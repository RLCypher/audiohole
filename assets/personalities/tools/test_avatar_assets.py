"""Contract and isolation regressions; fixtures are synthetic, never artwork."""
import copy
import json
import tempfile
import unittest
from pathlib import Path

from PIL import Image, ImageDraw

from avatar_assets import (ROOT, AssetError, Resolver, digest, inspect_png,
                           load_pack, owned_path, read_json, validate_pack)
from pipeline import export_idle, inventory


class AvatarContractTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.pack = self.root / 'Norm'
        self.pack.mkdir()
        self.manifest = self.pack / 'manifest.json'
        self.data = copy.deepcopy(read_json(ROOT / 'Norm/manifest.json'))
        # No production art is copied into test fixtures.
        for style in self.data['styles'].values():
            for entry in style['states'].values():
                entry['animation'] = {'status': 'not_generated'}
                entry['static'] = {'status': 'not_generated'}
        self.frame('Generated/chibi/idle.png')
        self.data['styles']['chibi']['states']['idle']['static'] = self.static('idle')
        self.save()

    def frame(self, relative, mode='RGBA', size=(512, 768), fill=None, rect=(160, 100, 350, 704)):
        p = self.pack / relative
        p.parent.mkdir(parents=True, exist_ok=True)
        im = Image.new(mode, size, fill)
        if rect:
            ImageDraw.Draw(im).rectangle(rect, fill=(90, 110, 130, 255) if mode == 'RGBA' else 90)
        im.save(p)
        return p

    def static(self, state, status='validated'):
        rel = f'Generated/chibi/{state}.png'
        return {'status': status, 'path': rel, 'sha256': digest(self.pack / rel), 'anchor': [256, 704]}

    def save(self):
        self.manifest.write_text(json.dumps(self.data))

    def test_valid_pack_and_unknown_state_use_own_idle(self):
        self.assertEqual(validate_pack(self.manifest, 'default')['errors'], [])
        choice = Resolver(False).resolve(self.manifest, 'default', 'chibi', 'custom_unknown')
        self.assertEqual(choice.source, 'idle_static')
        self.assertEqual(choice.paths[0].parent.parent.parent, self.pack)

    def test_dimensions_rejected_without_repair(self):
        p = self.frame('small.png', size=(256, 256), rect=None)
        before = p.read_bytes()
        with self.assertRaisesRegex(AssetError, '512x768'):
            inspect_png(p)
        self.assertEqual(before, p.read_bytes())

    def test_rgb_and_palette_rejected(self):
        for mode in ['RGB', 'P', 'L', 'I;16']:
            with self.subTest(mode=mode):
                p = self.frame('wrong.png', mode=mode, rect=None)
                with self.assertRaises(AssetError):
                    inspect_png(p)

    def test_empty_opaque_and_fake_transparency_rejected(self):
        for fill, rect in [((0, 0, 0, 0), None), ((255, 255, 255, 255), None), ((0, 0, 0, 255), (1, 1, 10, 10))]:
            with self.subTest(fill=fill):
                p = self.frame('invalid.png', fill=fill, rect=rect)
                with self.assertRaises(AssetError):
                    inspect_png(p)
        p = self.frame('hole.png', fill=(255, 255, 255, 255), rect=None)
        with Image.open(p) as im:
            im.putpixel((256, 384), (0, 0, 0, 0))
            im.save(p)
        with self.assertRaisesRegex(AssetError, 'corners'):
            inspect_png(p)

    def test_corrupt_or_disguised_non_png_rejected(self):
        p = self.pack / 'broken.png'
        for content in [b'not png', (self.pack / 'Generated/chibi/idle.png').read_bytes()[:50]]:
            p.write_bytes(content)
            with self.assertRaises(AssetError):
                inspect_png(p)

    def test_canonical_references_and_supplied_documents_unchanged(self):
        self.assertEqual(inventory()['errors'], [])

    def test_bad_manifest_duplicate_keys_and_wrong_identity(self):
        self.manifest.write_text('{"schema_version":1,"schema_version":1}')
        with self.assertRaisesRegex(AssetError, 'duplicate'):
            load_pack(self.manifest)
        self.save()
        with self.assertRaisesRegex(AssetError, 'personality'):
            load_pack(self.manifest, 'uwu')
        self.data['technical']['bit_depth'] = 16
        self.save()
        self.assertTrue(validate_pack(self.manifest)['errors'])

    def test_mandatory_idle_missing_fails_validation_and_hides(self):
        (self.pack / 'Generated/chibi/idle.png').unlink()
        report = validate_pack(self.manifest)
        self.assertTrue(any('mandatory idle' in e for e in report['errors']))
        self.assertEqual(Resolver(False).resolve(self.manifest, 'default', 'chibi', 'idle').source, 'hide')

    def test_disabled_regular_style_does_not_borrow_chibi(self):
        self.assertEqual(Resolver(False).resolve(self.manifest, 'default', 'regular', 'idle').source, 'hide')
        self.data['styles']['regular']['enabled'] = True
        self.save()
        self.assertTrue(any('regular: missing/invalid mandatory idle' in e for e in validate_pack(self.manifest)['errors']))

    def test_changed_content_rejected_by_hash(self):
        self.frame('Generated/chibi/idle.png', rect=(150, 100, 350, 704))
        report = validate_pack(self.manifest)
        self.assertTrue(any('content changed' in e for e in report['errors']))

    def test_paths_cannot_escape_personality_even_with_symlinks(self):
        for value in ['../UwU/Generated/chibi/idle.png', '/tmp/idle.png', 'Generated/../../UwU/idle.png', 'Generated\\UwU\\idle.png', 'C:/idle.png']:
            with self.subTest(value=value), self.assertRaises(AssetError):
                owned_path(self.pack, value)
        sibling = self.root / 'UwU'
        sibling.mkdir()
        (self.pack / 'Generated/link').symlink_to(sibling, target_is_directory=True)
        with self.assertRaises(AssetError):
            owned_path(self.pack, 'Generated/link/idle.png')

    def test_state_path_and_anchor_must_match_metadata(self):
        idle = self.data['styles']['chibi']['states']['idle']['static']
        for key, value in [('path', 'Generated/chibi/happy.png'), ('anchor', [250, 700])]:
            with self.subTest(key=key):
                original = idle[key]
                idle[key] = value
                self.save()
                self.assertTrue(validate_pack(self.manifest)['errors'])
                idle[key] = original

    def test_approval_is_explicit_and_production_defaults_closed(self):
        self.assertEqual(Resolver().resolve(self.manifest, 'default', 'chibi', 'idle').source, 'hide')
        idle = self.data['styles']['chibi']['states']['idle']['static']
        idle['status'] = 'approved'
        self.save()
        self.assertTrue(validate_pack(self.manifest)['errors'])
        idle['review'] = {'reviewer': 'Test fixture only', 'reviewed_at': '2026-11-01T20:00:00Z'}
        self.save()
        self.assertEqual(validate_pack(self.manifest, approved_only=True)['errors'], [])
        self.assertEqual(Resolver().resolve(self.manifest, 'default', 'chibi', 'idle').source, 'static')

    def test_fallback_cycles_and_missing_default_style_rejected(self):
        self.data['styles']['chibi']['states']['happy']['fallback'] = 'happy'
        self.save()
        self.assertTrue(validate_pack(self.manifest)['errors'])
        self.data['styles']['chibi']['states']['happy']['fallback'] = 'idle'
        self.data['default_style'] = 'unknown'
        self.save()
        self.assertTrue(validate_pack(self.manifest)['errors'])

    def test_last_valid_frame_and_personality_switch_clear_cache(self):
        self.frame('Generated/chibi/happy.png')
        self.data['styles']['chibi']['states']['happy']['static'] = self.static('happy')
        self.save()
        resolver = Resolver(False)
        old = resolver.resolve(self.manifest, 'default', 'chibi', 'happy')
        (self.pack / 'Generated/chibi/idle.png').unlink()
        self.assertEqual(resolver.resolve(self.manifest, 'default', 'chibi', 'unknown').source, 'last_valid_frame')
        other = self.root / 'UwU'
        other.mkdir()
        alt = copy.deepcopy(self.data)
        alt['personality_id'] = 'uwu'
        (other / 'manifest.json').write_text(json.dumps(alt))
        self.assertEqual(resolver.resolve(other / 'manifest.json', 'uwu', 'chibi', 'idle').source, 'hide')
        with self.assertRaises(AssetError):
            resolver.remember_presented(old)

    def animation(self):
        frames = []
        for index in range(2):
            rel = f'Generated/chibi/happy/{index:04d}.png'
            self.frame(rel, rect=(160 + index * 4, 100, 350, 704))
            frames.append({'path': rel, 'sha256': digest(self.pack / rel), 'duration_ms': 100, 'anchor': [256, 704]})
        animation = {'status': 'validated', 'frames': frames, 'loop': False, 'priority': 20,
                     'interruptible': True, 'cooldown_ms': 500, 'transition': 'finish_then_idle'}
        self.data['styles']['chibi']['states']['happy']['animation'] = animation
        self.save()
        return animation

    def test_animation_frames_timing_and_broken_frame_fall_back_to_static(self):
        self.frame('Generated/chibi/happy.png')
        self.data['styles']['chibi']['states']['happy']['static'] = self.static('happy')
        animation = self.animation()
        resolver = Resolver(False)
        choice = resolver.resolve(self.manifest, 'default', 'chibi', 'happy')
        self.assertEqual(choice.source, 'animation')
        self.assertEqual(choice.durations_ms, (100, 100))
        resolver.remember_presented(choice, 1)
        self.assertEqual(resolver.last[0], choice.paths[1])
        (self.pack / animation['frames'][1]['path']).unlink()
        self.assertEqual(resolver.resolve(self.manifest, 'default', 'chibi', 'happy').source, 'static')
        animation['frames'][0]['duration_ms'] = 0
        self.save()
        self.assertTrue(validate_pack(self.manifest)['errors'])

    def test_animation_anchor_mismatch_and_large_displacement(self):
        animation = self.animation()
        animation['frames'][1]['anchor'] = [256, 700]
        self.save()
        self.assertTrue(validate_pack(self.manifest)['errors'])
        animation['frames'][1]['anchor'] = [256, 704]
        rel = animation['frames'][1]['path']
        self.frame(rel, rect=(200, 150, 390, 754))
        animation['frames'][1]['sha256'] = digest(self.pack / rel)
        self.save()
        self.assertTrue(any('displacement' in w for w in validate_pack(self.manifest)['warnings']))

    def test_vanilla_master_pixels_equal_canonical(self):
        with Image.open(ROOT / 'Vanilla/References/Vanilla Reference.png') as ref:
            with Image.open(ROOT / 'Vanilla/Generated/logo/idle.png') as operating:
                self.assertEqual(ref.tobytes(), operating.crop((0, 128, 512, 640)).tobytes())

    def test_explicit_export_reproducible_and_preserves_source(self):
        # Export tested against a temporary registry, keeping real candidates untouched.
        import pipeline
        from unittest.mock import patch
        provenance = self.root / 'provenance'
        provenance.mkdir()
        src = self.frame('Intermediate/source.png', size=(512, 768))
        before = src.read_bytes()
        with patch.object(pipeline, 'ROOT', self.root), patch.object(pipeline, 'packs', return_value=iter([('default', self.manifest)])):
            first = export_idle('default', src, replace=True)
        with patch.object(pipeline, 'ROOT', self.root), patch.object(pipeline, 'packs', return_value=iter([('default', self.manifest)])):
            second = export_idle('default', src, replace=True)
        self.assertEqual(first['output_sha256'], second['output_sha256'])
        self.assertEqual(before, src.read_bytes())


if __name__ == '__main__':
    unittest.main()
