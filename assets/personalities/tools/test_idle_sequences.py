"""Acceptance checks for actual initial IDLE deliverables, not synthetic success."""
import unittest

from PIL import Image

from avatar_assets import ROOT, Resolver, digest, read_json
from animate_idle import compose_character_frame, steam_frame, vanilla_frame
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
                if identity == 'vanilla':
                    rebuilt = vanilla_frame(master, phase)
                else:
                    clean = Image.open(ROOT/f'{folder}/Intermediate/idle-clean-base.png')
                    steam = Image.open(ROOT/f'{folder}/Intermediate/idle-steam-layer.png')
                    rebuilt = compose_character_frame(master,clean,steam,folder,config['breath'][i],phase,rigs[identity])
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

    def test_both_steam_layers_rise_curl_and_fade(self):
        import math
        from PIL import ImageChops
        for folder in ['Norm','UwU']:
            with self.subTest(folder=folder):
                layer = Image.open(ROOT/f'{folder}/Intermediate/idle-steam-layer.png')
                self.assertLessEqual(layer.getchannel('A').getextrema()[1], 100)
                a = steam_frame(layer,0,0,folder)
                b = steam_frame(layer,math.pi,1,folder)
                self.assertNotEqual(a.getchannel('A').getbbox(),b.getchannel('A').getbbox())
                self.assertLess(b.getchannel('A').getextrema()[1],a.getchannel('A').getextrema()[1])
                self.assertIsNotNone(ImageChops.difference(a,b).getbbox())
                # The actual operating sequence must contain different steam pixels.
                frame_a = Image.open(ROOT/f'{folder}/Generated/chibi/idle/0001.png')
                frame_b = Image.open(ROOT/f'{folder}/Generated/chibi/idle/0005.png')
                box = (376,210,415,270) if folder == 'Norm' else (374,270,400,325)
                self.assertIsNotNone(ImageChops.difference(frame_a.crop(box),frame_b.crop(box)).getbbox())

    def test_uwu_closed_mouth_and_nose_survive_blink(self):
        master = Image.open(ROOT/'UwU/Generated/chibi/idle.png')
        blink = Image.open(ROOT/'UwU/Intermediate/idle-blink-keyframe.png')
        original = Image.open(ROOT/'UwU/Intermediate/idle-before-steam-revision.png')
        self.assertEqual(master.crop((253,264,302,294)).tobytes(),blink.crop((253,264,302,294)).tobytes())
        self.assertEqual(master.crop((287,247,314,264)).tobytes(),original.crop((287,247,314,264)).tobytes())
        def tongue_pixels(image):
            return sum(r > g*1.25 and r > b*.95 and g > 60 for r,g,b,a in image.crop((262,270,292,288)).get_flattened_data())
        self.assertGreater(tongue_pixels(original),40)
        self.assertEqual(tongue_pixels(master),0)

    def test_portable_gif_previews_have_twelve_frames_and_matching_timing(self):
        durations = self.report['timing']['frame_durations_ms']
        for identity in ['default','uwu','vanilla']:
            with Image.open(ROOT/f'review/idle-sequences/{identity}-review.gif') as image:
                self.assertEqual(image.n_frames,12)
                actual=[]
                for i in range(image.n_frames):
                    image.seek(i)
                    actual.append(image.info['duration'])
                self.assertEqual(actual,durations)
                self.assertEqual(image.info['loop'],0)


if __name__ == '__main__':
    unittest.main()
