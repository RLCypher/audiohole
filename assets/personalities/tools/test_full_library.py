"""Acceptance tests for the complete generated library and real playback policy."""
from bisect import bisect_right
import unittest
from pathlib import Path

from PIL import Image

from avatar_assets import ROOT, Resolver, digest, read_json
from avatar_player import AvatarPlayer
from audit_library import audit, difference
from avatar_assets import personality_name, review_key
from pipeline import packs


class FullLibraryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.report=audit()

    def test_complete_catalog_and_all_operating_images_pass(self):
        self.assertEqual(self.report['errors'],[])
        self.assertEqual(self.report['totals'],{'static_assets':52,'animation_sequences':49,'animation_frames':744,'operating_pngs':796})
        self.assertTrue(all(pack['core_complete'] for pack in self.report['packs'].values()))

    def test_human_approvals_are_recorded_and_accepted_pixels_are_immutable(self):
        for relative,sha in read_json(ROOT/'provenance/idle-approval.json')['assets'].items():
            self.assertEqual(digest(ROOT/relative),sha,relative)
        for identity,path in packs():
            data=read_json(path);states=data['styles'][data['default_style']]['states']
            self.assertEqual(states['idle']['static']['status'],'approved')
            self.assertEqual(states['idle']['animation']['status'],'approved')
            for state,entry in states.items():
                for kind in ['static','animation']:
                    asset=entry[kind]
                    if identity in {'default','vanilla'} and asset.get('path',asset.get('frames')):
                        self.assertEqual(asset['status'],'approved')
                    elif identity=='uwu' and state!='idle':
                        self.assertNotEqual(asset['status'],'approved')
        for relative,sha in read_json(ROOT/'provenance/accepted-assets.json')['assets'].items():
            self.assertEqual(digest(ROOT/relative),sha,relative)

    def test_theatrical_uwu_reactions_have_real_finite_animation_and_cooldowns(self):
        states=self.report['packs']['uwu']['states']
        for state in ['error','angry','surprised','celebrating','panicking']:
            with self.subTest(state=state):
                self.assertGreaterEqual(states[state]['frame_count'],14)
                self.assertEqual(states[state]['behavior']['mode'],'finite')
                self.assertGreaterEqual(states[state]['behavior']['cooldown_ms'],5000)
                self.assertEqual(states[state]['loop_endpoint_max_rgba_delta'],0)

    def test_norm_static_clothing_case_and_shoes_keep_the_accepted_artwork(self):
        for identity,path in packs():
            if identity!='default':continue
            data=read_json(path);style=data['default_style'];master=Image.open(path.parent/f'Generated/{style}/idle.png')
            boxes=[(90,500,220,600),(70,630,430,704)] if identity=='default' else [(88,520,211,615),(210,630,380,704)]
            for state,entry in data['styles'][style]['states'].items():
                if entry['static']['status'] not in {'validated','approved'}:continue
                image=Image.open(path.parent/entry['static']['path'])
                for box in boxes:self.assertEqual(difference(master.crop(box),image.crop(box)),0,(identity,state,box))

    def test_reviewed_packs_are_usable_and_uwu_candidates_stay_filtered(self):
        for pid,folder,style,state in [('default','Norm','chibi','happy'),('vanilla','Vanilla','logo','error'),('uwu','UwU','chibi','error')]:
            selected=Resolver().resolve(ROOT/f'{folder}/manifest.json',pid,style,state)
            self.assertEqual(selected.state,'idle' if pid=='uwu' else state)
            self.assertTrue(selected.paths)

    def test_uwu_performances_use_distinct_drawn_body_keys_without_sticker_motion(self):
        from PIL import ImageChops
        manifest=read_json(ROOT/'UwU/manifest.json');states=manifest['styles']['chibi']['states']
        for state,entry in states.items():
            if state=='idle':continue
            record=read_json(ROOT/f'provenance/uwu-{state}-body-revision.json')
            directory=record.get('keys_directory',f'UwU/Intermediate/BodyRevision/Keys/{state}')
            keys=[ROOT/directory/f'{i:04d}.png' for i in range(record['drawn_pose_count'])]
            hashes=[digest(path) for path in keys]
            self.assertEqual(hashes,record['key_sha256'])
            self.assertEqual(len(set(hashes)),len(keys))
            a,b=[Image.open(path).convert('RGBA').crop((180,330,496,712)) for path in keys[:2]]
            changed=ImageChops.difference(a,b)
            self.assertGreater(sum(any(pixel) for pixel in changed.get_flattened_data()),1000,(state,'no meaningful body change'))
            for frame,key in zip(entry['animation'].get('frames',[]),record['frame_source_key_indices']):
                expected=states['idle']['static']['sha256'] if key is None else hashes[key]
                self.assertEqual(frame['sha256'],expected,(state,frame['path'],'unexpected sprite transform'))

    def test_uwu_case_and_wordmark_use_one_fixed_layer_in_every_pose(self):
        from PIL import ImageChops
        directory=ROOT/'UwU/Intermediate/ConsistencyRevision'
        case=Image.open(directory/'canonical-case.png').convert('RGBA')
        master=Image.open(ROOT/'UwU/Generated/chibi/idle.png').convert('RGBA')
        brand=(94,498,213,596)
        self.assertEqual(case.crop(brand).tobytes(),master.crop(brand).tobytes())
        word=(95,574,213,594)
        word_pixels=list(master.crop(word).get_flattened_data())
        letter_indices=[i for i,p in enumerate(word_pixels) if min(p[:3])>185 and p[3]>200]
        self.assertGreater(len(letter_indices),150)
        for item in read_json(directory/'composition.json'):
            state,key=item['state'],item['key']
            self.assertEqual(item['case_sha256'],digest(directory/'canonical-case.png'))
            character=Image.open(directory/f'Characters/{state}/{key:04d}.png').convert('RGBA')
            expected=case.copy();expected.alpha_composite(character)
            actual=Image.open(directory/f'Keys/{state}/{key:04d}.png').convert('RGBA')
            self.assertEqual(actual.tobytes(),expected.tobytes(),(state,key,'case or layering changed'))
            alpha=list(character.crop(word).getchannel('A').get_flattened_data())
            visible=[i for i in letter_indices if alpha[i]==0]
            self.assertGreater(len(visible)/len(letter_indices),.75,(state,key,'wordmark excessively occluded'))
            rendered=list(actual.crop(word).get_flattened_data())
            for i in visible:self.assertEqual(rendered[i],word_pixels[i],(state,key,'visible lettering altered'))

    def test_uwu_badges_reuse_the_canonical_emblem_in_the_actual_card(self):
        directory=ROOT/'UwU/Intermediate/ConsistencyRevision'
        for item in read_json(directory/'composition.json'):
            meta=item['badge'];x0,y0,x1,y1=meta['box'];top=meta['assembly_top']
            self.assertGreaterEqual(x1-x0,20,(item['state'],item['key'],'badge confused with buckle'))
            self.assertGreaterEqual(y1-y0,25)
            template=directory/meta['template']
            self.assertEqual(digest(template),meta['template_sha256'])
            patch=Image.open(template).convert('RGBA').resize((x1-x0,y1-top),Image.Resampling.LANCZOS)
            character=Image.open(directory/f"Characters/{item['state']}/{item['key']:04d}.png").convert('RGBA').crop((x0,top,x1,y1))
            for expected,actual in zip(patch.get_flattened_data(),character.get_flattened_data()):
                if expected[3]==255:self.assertEqual(actual,expected,(item['state'],item['key'],'canonical badge changed'))

    def test_native_preview_samples_are_the_actual_operating_frames(self):
        for identity,path in packs():
            data=read_json(path);style=data['default_style'];states=data['styles'][style]['states']
            for state in ['talking','error','celebrating']:
                meta=states[state]['animation']['frames'];times=[f['duration_ms'] for f in meta]
                with Image.open(ROOT/f'review/full/{review_key(identity)}/{state}-native.png') as preview:
                    decoded=[];exposures=[]
                    for frame in range(preview.n_frames):
                        preview.seek(frame);preview.load();decoded.append(preview.convert('RGBA').copy());exposures.append(round(preview.info['duration']))
                    for selected in [0,len(meta)//2,len(meta)-1]:
                        target_time=sum(times[:selected]);elapsed=0
                        for frame,duration in enumerate(exposures):
                            if elapsed+duration>target_time:break
                            elapsed+=duration
                        expected=Image.open(path.parent/meta[selected]['path'])
                        self.assertEqual(decoded[frame].tobytes(),expected.tobytes(),(identity,state,selected))

    def test_real_finite_reaction_returns_to_current_persistent_state(self):
        player=AvatarPlayer(approved_only=False);manifest=ROOT/'UwU/manifest.json'
        player.request(manifest,'uwu','chibi','working',0)
        self.assertTrue(player.request(manifest,'uwu','chibi','error',100).accepted)
        self.assertEqual(player.request(manifest,'uwu','chibi','concerned',200).reason,'persistent_context_updated')
        self.assertEqual(player.frame_at(2099).state,'error')
        self.assertEqual(player.frame_at(2100).state,'concerned')

    def test_reduced_motion_and_production_approval_use_owned_static_fallback(self):
        player=AvatarPlayer(approved_only=False);manifest=ROOT/'UwU/manifest.json'
        player.request(manifest,'uwu','chibi','surprised',0,reduced_motion=True)
        frame=player.frame_at(1)
        self.assertEqual(frame.source,'reduced_motion_static')
        self.assertEqual(frame.path.name,'surprised.png')
        production=AvatarPlayer()
        production.request(manifest,'uwu','chibi','surprised',0,reduced_motion=True)
        self.assertEqual(production.frame_at(1).path.name,'idle.png')

    def test_intentional_static_states_and_logo_optional_fallbacks_are_explicit(self):
        for pid in ['default','uwu']:
            state=self.report['packs'][pid]['states']['deadpan']
            self.assertEqual(state['frame_count'],0)
            self.assertEqual(state['behavior']['mode'],'finite')
            self.assertTrue(state['note'])
        logo=self.report['packs']['vanilla']['states']
        self.assertEqual(logo['sleeping']['frame_count'],0)
        for state in read_json(ROOT/'states.json')['optional']:
            self.assertEqual(logo[state]['static_status'],'unavailable')
            self.assertEqual(logo[state]['fallback'],'idle')


if __name__=='__main__':unittest.main()
