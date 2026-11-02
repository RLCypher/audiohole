"""Regressions for name compatibility, rest alignment, props and local repairs."""
import unittest

from PIL import Image, ImageChops

from avatar_assets import ROOT, digest, personality_name, read_json, review_key
from revise_horu_idle import PREVIOUS, render


class HoruRevisionTests(unittest.TestCase):
    def test_character_name_changes_without_changing_application_identity(self):
        data=read_json(ROOT/'UwU/manifest.json')
        self.assertEqual((data['personality_id'],data['character_id']),('uwu','horu'))
        self.assertEqual(personality_name('uwu'),'Horu')
        self.assertEqual(review_key('uwu'),'horu')
        self.assertEqual(read_json(ROOT/'registry.json')['packs']['uwu'],'UwU/manifest.json')

    def test_rest_uses_shared_case_and_uniform_scale_in_every_exposure(self):
        revision=read_json(ROOT/'provenance/horu-idle-revision.json')
        self.assertEqual(revision['character_scale'],.88)
        self.assertEqual(revision['pivot'][1],704)
        self.assertEqual(digest(ROOT/revision['case_path']),revision['case_sha256'])
        old=Image.open(PREVIOUS/'idle.png').convert('RGBA')
        new=Image.open(ROOT/'UwU/Generated/chibi/idle.png').convert('RGBA')
        self.assertEqual(render(old).tobytes(),new.tobytes())
        for i in range(12):
            source=Image.open(PREVIOUS/f'idle/{i:04d}.png').convert('RGBA')
            expected=Image.open(ROOT/f'UwU/Generated/chibi/idle/{i:04d}.png').convert('RGBA')
            self.assertEqual(render(source).tobytes(),expected.tobytes())
        original=read_json(PREVIOUS/'accepted-assets.json')['assets']
        for name,sha in original.items():
            if name.startswith('UwU/Generated/chibi/idle'):
                self.assertEqual(digest(PREVIOUS/name.removeprefix('UwU/Generated/chibi/')),sha)

    def test_every_acting_key_accounts_for_cup_and_parked_cup_is_visible(self):
        directory=ROOT/'UwU/Intermediate/ConsistencyRevision'
        records=read_json(directory/'composition.json')
        self.assertEqual(len(records),50)
        for item in records:
            cup=item['cup'];self.assertIn(cup['location'],['hand','case'])
            if cup['location']=='hand':continue
            layer=Image.open(ROOT/cup['layer']).convert('RGBA')
            rendered=Image.open(directory/f'Keys/{item["state"]}/{item["key"]:04d}.png').convert('RGBA')
            # Foreground tabletop cup remains visible even in the tail-puff
            # recoil. Its opaque pixels are exact copies of the one prop raster.
            count=0
            for expected,actual in zip(layer.get_flattened_data(),rendered.get_flattened_data()):
                if expected[3]==255:
                    self.assertEqual(expected,actual,(item['state'],item['key']))
                    count+=1
            self.assertGreater(count,1000)

    def test_talking_has_four_drawings_one_held_cup_and_unclipped_boots(self):
        directory=ROOT/'UwU/Intermediate/ConsistencyRevision'
        records=[r for r in read_json(directory/'composition.json') if r['state']=='talking']
        self.assertEqual(len(records),4)
        for item in records:
            self.assertEqual(item['cup']['location'],'hand')
            self.assertEqual(item['source'],'talking-source.png')
            image=Image.open(directory/f'Characters/talking/{item["key"]:04d}.png')
            alpha=image.getchannel('A').point(lambda v:255 if v>=128 else 0)
            bottom=alpha.getbbox()[3]
            self.assertEqual(bottom,704)
            self.assertLess(sum(bool(v) for v in alpha.crop((0,bottom-1,512,bottom)).get_flattened_data()),100,
                            'broad flat boot edge indicates truncation before vertical fitting')

    def test_working_eye_repair_preserves_every_pixel_outside_spill_masks(self):
        directory=ROOT/'UwU/Intermediate/HoruPerformanceRepair'
        for i in range(2):
            original=Image.open(directory/f'Previous/Characters/working/{i:04d}.png').convert('RGBA')
            current=Image.open(ROOT/f'UwU/Intermediate/ConsistencyRevision/Characters/working/{i:04d}.png').convert('RGBA')
            mask=Image.open(directory/f'EyeEdits/{i:04d}-mask.png').convert('L')
            exterior=Image.composite(current,original,mask.point(lambda v:0 if v else 255))
            self.assertEqual(exterior.tobytes(),original.tobytes())
            self.assertEqual(original.getchannel('A').tobytes(),current.getchannel('A').tobytes())
            count=0
            for weight,pixel in zip(mask.get_flattened_data(),current.get_flattened_data()):
                if weight:
                    self.assertGreaterEqual(min(pixel[:3]),200)
                    self.assertLessEqual(max(pixel[:3])-min(pixel[:3]),20)
                    count+=1
            self.assertGreater(count,100)


if __name__=='__main__':unittest.main()
