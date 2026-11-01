"""Acceptance checks for actual initial IDLE deliverables, not synthetic success."""
import unittest

from PIL import Image

from avatar_assets import ROOT, Resolver, digest, read_json
from animate_idle import character_frame, vanilla_frame
from validate_idle import audit


class InitialIdleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.report = audit()

    def test_all_39_operating_assets_and_preview_frames_pass_contract(self):
        self.assertEqual(self.report['errors'], [])
        self.assertEqual(self.report['operating_asset_count'], 39)

    def test_real_sequences_loop_with_shared_timing_and_stable_ground(self):
        for identity, pack in self.report['packs'].items():
            with self.subTest(identity=identity):
                self.assertEqual(pack['frame_count'], 12)
                self.assertEqual(pack['alignment']['loop_first_last_max_rgba_delta'], 0)
                self.assertEqual(pack['alignment']['ground_region_max_rgba_delta'], 0)
                self.assertGreaterEqual(pack['alignment']['unique_frame_hashes'], 10)
                self.assertEqual(pack['previews']['native']['duration_ms'], 5000)
                self.assertEqual(pack['previews']['compact']['duration_ms'], 5000)

    def test_vanilla_protected_color_geometry_and_alpha_are_exact(self):
        metrics = self.report['packs']['vanilla']['alignment']
        self.assertEqual(metrics['outside_wormhole_max_rgba_delta'], 0)
        self.assertEqual(metrics['whole_image_alpha_max_delta'], 0)

    def test_generation_parameters_reproduce_actual_intermediate_frame(self):
        import math
        config = read_json(ROOT/'provenance/idle-motion.json')
        rigs = read_json(ROOT/'provenance/idle-rigs.json')
        for identity, folder, style in [('default', 'Norm', 'chibi'), ('uwu', 'UwU', 'chibi'), ('vanilla', 'Vanilla', 'logo')]:
            with self.subTest(identity=identity):
                master = Image.open(ROOT/f'{folder}/Generated/{style}/idle.png')
                i = 3
                phase = i/11*2*math.pi
                rebuilt = vanilla_frame(master, phase) if identity == 'vanilla' else character_frame(master, folder, config['breath'][i], phase, rigs[identity])
                expected = Image.open(ROOT/f'{folder}/Generated/{style}/idle/{i:04d}.png')
                self.assertEqual(rebuilt.tobytes(), expected.tobytes())

    def test_animation_falls_back_to_owned_static_if_a_real_frame_is_unavailable(self):
        # Simulate preflight failure without editing production artwork or metadata.
        from unittest.mock import patch
        import avatar_assets
        real_inspect = avatar_assets.inspect_png
        def missing(path):
            if path.name == '0005.png':
                raise avatar_assets.AssetError('simulated unavailable blink frame')
            return real_inspect(path)
        with patch.object(avatar_assets, 'inspect_png', side_effect=missing):
            choice = Resolver(False).resolve(ROOT/'UwU/manifest.json', 'uwu', 'chibi', 'idle')
        self.assertEqual(choice.source, 'static')
        self.assertEqual(choice.paths, ((ROOT/'UwU/Generated/chibi/idle.png').resolve(),))


if __name__ == '__main__':
    unittest.main()
