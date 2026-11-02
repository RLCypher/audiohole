"""Recompose Horu's authorized IDLE revision; preserve existing acting drawings.

The image backend supplies an equipment-removal matte. All visible character
pixels come from archived accepted frames, uniformly scaled about the ground.
The shared case is never scaled or animated. This does not generate new poses.
"""
from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

from avatar_assets import ROOT, AssetError, digest, inspect_png, read_json
from pipeline import write_json

DIRECTORY = ROOT/'UwU/Intermediate/HoruIdleRevision'
PREVIOUS = DIRECTORY/'Previous'
CASE = ROOT/'UwU/Intermediate/ConsistencyRevision/canonical-case.png'
SCALE = .88
PIVOT = (300, 704)


def transform(image):
    x, y = PIVOT
    return image.transform((512,768), Image.Transform.AFFINE,
                           (1/SCALE,0,x-x/SCALE,0,1/SCALE,y-y/SCALE),
                           Image.Resampling.BICUBIC)


def make_matte():
    source = Image.open(DIRECTORY/'character-separation-source.png').convert('RGBA')
    source = source.resize((512,768), Image.Resampling.LANCZOS)
    # Use only model alpha, not its redrawn RGB or its diffuse glow. The matte
    # separates the overlapping sleeve/hand and wearable earpiece from the case.
    matte = source.getchannel('A').point(lambda a:255 if a>=180 else 0)
    draw = ImageDraw.Draw(matte)
    draw.rectangle((0,0,511,398), fill=255)
    draw.rectangle((300,0,511,767), fill=255)
    # Remove residual prop pixels below the accepted fingertips and beside the
    # stocking/boot. These regions contain no character in the accepted source.
    draw.rectangle((0,477,220,497), fill=0)
    draw.rectangle((0,498,222,589), fill=0)
    draw.rectangle((0,590,209,767), fill=0)
    matte.save(DIRECTORY/'character-matte.png')
    return matte


def character_layer(source, matte):
    image = source.convert('RGBA').copy()
    image.putalpha(ImageChops.multiply(image.getchannel('A'),matte))
    # Discard hidden RGB from the removed prop; alpha-composite onto clear RGBA.
    clean = Image.new('RGBA',image.size)
    clean.alpha_composite(image)
    result=transform(clean)
    # Bicubic downsampling can overshoot faint steam alpha. Keep the accepted
    # maximum opacity rather than making a smaller plume more opaque.
    steam=Image.open(PREVIOUS/'idle-steam-layer.png').convert('RGBA')
    support=transform(steam).getchannel('A').point(lambda a:255 if a else 0)
    alpha=result.getchannel('A')
    result.putalpha(Image.composite(alpha.point(lambda a:min(a,100)),alpha,support))
    return result


def render(source, matte=None):
    if matte is None:
        matte = Image.open(DIRECTORY/'character-matte.png').convert('L')
    result = Image.open(CASE).convert('RGBA')
    result.alpha_composite(character_layer(source,matte))
    return result


def frame(index):
    """Rebuild directly from the archived accepted animation, including blink."""
    return render(Image.open(PREVIOUS/f'idle/{index:04d}.png'))


def archive_approval_paths():
    """Keep old approval hashes truthful; never approve the revised drawing."""
    prefix = 'UwU/Generated/chibi/'
    destination = PREVIOUS.relative_to(ROOT).as_posix()+'/'
    for name in ['idle-approval.json','accepted-assets.json']:
        original = read_json(PREVIOUS/name)
        original['assets'] = {
            (destination+path[len(prefix):] if path.startswith(prefix+'idle') else path):sha
            for path,sha in original['assets'].items()
        }
        original['superseded_idle'] = {
            'character':'Horu', 'personality_id':'uwu',
            'authorization':'provenance/authorizations/horu-name-idle-scale.md',
            'archive':PREVIOUS.relative_to(ROOT).as_posix(),
            'note':'Prior acceptance applies to archived pixels. Revised IDLE is validated, pending human review.'
        }
        for entry in original.get('entries',[]):
            if entry.get('personality_id')=='uwu':
                entry['name']='Horu'
                entry['scope']='Archived prior IDLE; replaced operating IDLE is pending review.'
        if 'uwu' in original:
            original['uwu']='Prior approved IDLE preserved in archive; revised Horu IDLE and body performances pending visual review.'
        write_json(ROOT/'provenance'/name,original)
        for path,sha in original['assets'].items():
            if digest(ROOT/path)!=sha:raise AssetError(f'accepted pixels changed: {path}')


def package():
    from animate_idle import previews
    from generate_states import make_previews

    matte = make_matte()
    master = render(Image.open(PREVIOUS/'idle.png'),matte)
    master_path = ROOT/'UwU/Generated/chibi/idle.png'
    manifest = ROOT/'UwU/manifest.json'
    data = read_json(manifest);data['character_id']='horu'
    states = data['styles']['chibi']['states'];anchor=data['styles']['chibi']['anchor']
    master.save(master_path)
    master_character=character_layer(Image.open(PREVIOUS/'idle.png'),matte)
    master_character.save(DIRECTORY/'idle-character.png')
    blink=render(Image.open(PREVIOUS/'idle-blink-keyframe.png'),matte)
    blink.save(ROOT/'UwU/Intermediate/idle-blink-keyframe.png')
    blink_mask=transform(Image.open(PREVIOUS/'idle-blink-mask.png').convert('RGBA')).getchannel('R')
    blink_mask=blink_mask.point(lambda v:255 if v else 0).filter(ImageFilter.MaxFilter(5))
    blink_mask.save(ROOT/'UwU/Intermediate/idle-blink-mask.png')
    character_layer(Image.open(PREVIOUS/'idle-steam-layer.png'),matte).save(DIRECTORY/'idle-steam-layer.png')
    entry=states['idle'];entry['static']={'status':'validated','path':'Generated/chibi/idle.png','sha256':digest(master_path),'anchor':anchor}
    metadata=[];frames=[]
    old=read_json(PREVIOUS/'manifest.json')['styles']['chibi']['states']['idle']
    times=[item['duration_ms'] for item in old['animation']['frames']]
    for index,ms in enumerate(times):
        image=frame(index);path=ROOT/f'UwU/Generated/chibi/idle/{index:04d}.png'
        image.save(path);inspect_png(path);frames.append(image)
        metadata.append({'path':path.relative_to(manifest.parent).as_posix(),'sha256':digest(path),'anchor':anchor,'duration_ms':ms})
    entry['animation']={**{k:v for k,v in old['animation'].items() if k not in ['status','frames','review']},'status':'validated','frames':metadata}
    entry['note']='Horu: archived closed-mouth acting at 88% character scale with fixed canonical case; revised IDLE pending visual review.'
    previews('uwu',frames,times)
    make_previews('uwu','idle',frames,times,master,entry['behavior'])
    changed_endpoints=0
    for state,entry in states.items():
        if state=='idle':continue
        record_path=ROOT/f'provenance/uwu-{state}-body-revision.json'
        record=read_json(record_path)
        sources=record['frame_source_key_indices']
        for index,selected in enumerate(sources):
            if selected is not None:continue
            item=entry['animation']['frames'][index]
            master.save(manifest.parent/item['path']);item['sha256']=digest(master_path)
            changed_endpoints+=1
        sequence=[Image.open(manifest.parent/item['path']).convert('RGBA') for item in entry['animation'].get('frames',[])]
        durations=[item['duration_ms'] for item in entry['animation'].get('frames',[])]
        make_previews('uwu',state,sequence,durations,Image.open(manifest.parent/entry['static']['path']),entry['behavior'])
        record.update(character_name='Horu',master_sha256=digest(master_path),animation=entry['animation'],
                      rest_revision='provenance/horu-idle-revision.json')
        write_json(record_path,record)
    write_json(manifest,data)
    archive_approval_paths()
    inputs=[PREVIOUS/'idle.png',PREVIOUS/'manifest.json',PREVIOUS/'idle-blink-keyframe.png',
            PREVIOUS/'idle-blink-mask.png',PREVIOUS/'idle-steam-layer.png',
            DIRECTORY/'character-separation-source.png',DIRECTORY/'character-matte.png',CASE,
            ROOT/'provenance/authorizations/horu-name-idle-scale.md',*sorted((PREVIOUS/'idle').glob('*.png'))]
    info={'character_name':'Horu','character_id':'horu','personality_id':'uwu',
          'approval':'pending human review','authorization':'provenance/authorizations/horu-name-idle-scale.md',
          'method':'Backend-assisted occlusion matte; accepted RGB and animation uniformly resampled; fixed case recomposited.',
          'character_scale':SCALE,'pivot':list(PIVOT),'case_path':CASE.relative_to(ROOT).as_posix(),
          'case_sha256':digest(CASE),'master_sha256':digest(master_path),
          'frame_durations_ms':times,'frames':metadata,'replaced_rest_endpoint_frames':changed_endpoints,
          'source_archive':PREVIOUS.relative_to(ROOT).as_posix(),
          'generator':Path(__file__).relative_to(ROOT).as_posix(),'generator_sha256':digest(Path(__file__)),
          'inputs':{f.relative_to(ROOT).as_posix():digest(f) for f in inputs},
          'limits':['Uniform reduction is applied to IDLE only; expressive drawings and their intentional squash/perspective remain unchanged.',
                    'The backend alpha is used only near equipment; no generated character RGB replaces the accepted face or costume.',
                    'Revised IDLE is not yet human-approved; approved-only playback hides Horu until review.']}
    write_json(ROOT/'provenance/horu-idle-revision.json',info)
    print('Horu: 12 IDLE frames and',changed_endpoints,'reaction rest frames updated; no acting keys redrawn.')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package',action='store_true',required=True)
    parser.parse_args();package()
