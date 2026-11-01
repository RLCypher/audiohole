"""Playback-policy regressions using temporary synthetic asset packs."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from PIL import Image, ImageDraw

from avatar_assets import ROOT, digest, read_json
from avatar_player import AvatarPlayer


class PlayerTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.pack=self.root/'Norm';self.pack.mkdir()
        self.manifest=self.pack/'manifest.json'
        data=copy.deepcopy(read_json(ROOT/'Norm/manifest.json'))
        for style in data['styles'].values():
            for state,entry in style['states'].items():
                entry['static']={'status':'not_generated'};entry['animation']={'status':'not_generated'}
        for state,priority,persistent,cooldown,interruptible in [
            ('idle',0,True,0,True),('working',10,True,500,True),('thinking',10,True,500,True),
            ('happy',30,False,2500,False),('confused',35,False,1000,True),
            ('angry',65,False,1000,True),('error',95,False,6000,True)]:
            entry=data['styles']['chibi']['states'][state]
            frames=[]
            for index in range(2):
                p=self.pack/f'Generated/chibi/{state}/{index:04d}.png';p.parent.mkdir(parents=True,exist_ok=True)
                im=Image.new('RGBA',(512,768));ImageDraw.Draw(im).rectangle((100+index,100,300,704),fill=(100,130,170,255));im.save(p)
                frames.append({'path':p.relative_to(self.pack).as_posix(),'sha256':digest(p),'anchor':[256,704],'duration_ms':100})
            static=self.pack/f'Generated/chibi/{state}.png';static.write_bytes(p.read_bytes())
            entry['static']={'status':'validated','path':static.relative_to(self.pack).as_posix(),'sha256':digest(static),'anchor':[256,704]}
            entry['animation']={'status':'validated','frames':frames,'loop':persistent,'priority':priority,'interruptible':interruptible,'cooldown_ms':cooldown,'transition':'cut' if persistent else 'finish_then_persistent'}
            entry['behavior']={'mode':'persistent' if persistent else 'finite','category':'ambient' if persistent else 'expressive','priority':priority,'interruptible':interruptible,'cooldown_ms':cooldown,'static_hold_ms':300,'return_to':'persistent','reduced_motion':'static'}
        self.manifest.write_text(json.dumps(data))
        self.player=AvatarPlayer(approved_only=False)

    def request(self,state,time,**kwargs):
        return self.player.request(self.manifest,'default','chibi',state,time,**kwargs)

    def test_exact_timing_and_loop_wrap(self):
        self.assertTrue(self.request('idle',0).accepted)
        self.assertEqual(self.player.frame_at(99).frame_index,0)
        self.assertEqual(self.player.frame_at(100).frame_index,1)
        self.assertEqual(self.player.frame_at(200).frame_index,0)

    def test_duplicate_events_do_not_restart_or_bypass_cooldown(self):
        self.request('happy',0)
        self.assertEqual(self.request('happy',100).reason,'already_active')
        self.assertEqual(self.player.frame_at(200).state,'idle')
        self.assertEqual(self.request('happy',1000).reason,'cooldown')
        self.assertTrue(self.request('happy',2500).accepted)

    def test_high_priority_interrupts_and_lower_reactions_wait(self):
        self.request('idle',0);self.request('angry',10)
        self.assertEqual(self.request('confused',20).reason,'priority_or_interrupt_policy')
        self.assertTrue(self.request('error',30).accepted)
        self.assertEqual(self.player.frame_at(40).state,'error')

    def test_equal_priority_noninterruptible_reaction_is_not_restarted(self):
        self.request('happy',0)
        data=read_json(self.manifest);entry=data['styles']['chibi']['states']['confused'];entry['behavior']['priority']=30;entry['animation']['priority']=30;self.manifest.write_text(json.dumps(data))
        self.assertEqual(self.request('confused',20).reason,'priority_or_interrupt_policy')

    def test_finite_reaction_returns_to_updated_persistent_context(self):
        self.request('working',0);self.request('error',100)
        self.assertEqual(self.request('thinking',150).reason,'persistent_context_updated')
        self.assertEqual(self.player.frame_at(299).state,'error')
        self.assertEqual(self.player.frame_at(300).state,'thinking')

    def test_reduced_motion_uses_static_and_finite_hold(self):
        self.request('happy',0,reduced_motion=True)
        frame=self.player.frame_at(100)
        self.assertEqual(frame.source,'reduced_motion_static');self.assertEqual(frame.frame_index,0)
        self.assertEqual(self.player.frame_at(299).state,'happy')
        self.assertEqual(self.player.frame_at(300).state,'idle')

    def test_switch_and_invalid_manifest_never_reuse_previous_personality(self):
        self.request('happy',0)
        broken=self.root/'UwU/manifest.json';broken.parent.mkdir();broken.write_text('{broken')
        result=self.player.request(broken,'uwu','chibi','idle',50)
        self.assertFalse(result.accepted)
        frame=self.player.frame_at(60)
        self.assertEqual(frame.personality,'uwu');self.assertIsNone(frame.path);self.assertEqual(frame.source,'hide')

    def test_disabled_style_also_hides_in_reduced_motion(self):
        self.request('idle',0)
        result=self.player.request(self.manifest,'default','regular','idle',10,reduced_motion=True)
        self.assertFalse(result.accepted);self.assertIsNone(self.player.frame_at(20).path)

    def test_animation_ticks_and_return_do_not_read_files(self):
        self.request('working',0);self.request('error',10)
        with (patch('avatar_player.load_pack',side_effect=AssertionError('unexpected file access')),
              patch('avatar_assets.inspect_png',side_effect=AssertionError('unexpected decode'))):
            self.assertEqual(self.player.frame_at(110).state,'error')
            self.assertEqual(self.player.frame_at(210).state,'working')

    def test_default_policy_rejects_unapproved_art(self):
        production=AvatarPlayer()
        production.request(self.manifest,'default','chibi','happy',0)
        self.assertIsNone(production.frame_at(1).path)

    def test_changing_motion_preference_refreshes_same_state(self):
        self.request('idle',0)
        self.assertTrue(self.request('idle',10,reduced_motion=True).accepted)
        self.assertEqual(self.player.frame_at(11).source,'reduced_motion_static')

    def test_backwards_or_invalid_time_is_explicit(self):
        self.request('idle',100)
        with self.assertRaises(ValueError):self.player.frame_at(99)
        with self.assertRaises(ValueError):self.player.frame_at(True)

    def test_repaired_manifest_can_recover_the_same_selected_state(self):
        valid=self.manifest.read_text()
        self.request('idle',0)
        self.manifest.write_text('{broken')
        self.assertFalse(self.request('idle',10).accepted)
        self.assertIsNone(self.player.frame_at(11).path)
        self.manifest.write_text(valid)
        self.assertTrue(self.request('idle',20).accepted)
        self.assertIsNotNone(self.player.frame_at(21).path)


if __name__=='__main__':unittest.main()
