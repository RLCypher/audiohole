"""Package Horu's talking redraw, local working-eye repair and cup continuity.

Retained model outputs supply the four talking drawings, the eye-white material
samples and one completed cup. Other character drawings remain unchanged.
"""
from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

from avatar_assets import ROOT, AssetError, digest, inspect_png, read_json
from pipeline import write_json
from correct_uwu_consistency import register_character, apply_badge
from revise_uwu_body import separate_sheet

DIRECTORY=ROOT/'UwU/Intermediate/HoruPerformanceRepair'
PREVIOUS=DIRECTORY/'Previous'
ACTIVE=ROOT/'UwU/Intermediate/ConsistencyRevision'
HELD={('bored',0),('concerned',0),('concerned',1),('embarrassed',0),('embarrassed',1),
      *(('talking',i) for i in range(4))}
PARKED_BOX=(113,372,181,435)
# Existing iris envelopes, inspected in native crops. Only cyan outside these
# envelopes is a spill; original outlines/pupils and all other RGB remain exact.
EYES=[[(258,225,315,263),(283,228,311,261)],
      [(247,220,311,257),(278,222,308,258)]]


def eye_repair(character,index):
    source=Image.open(DIRECTORY/'working-eyes-source.png').convert('RGBA')
    source=source.resize((1024,512),Image.Resampling.LANCZOS)
    patch=source.crop((index*512,100,(index+1)*512,346)).resize((150,72),Image.Resampling.LANCZOS)
    origin=(240,212);material=Image.new('RGBA',(512,768));material.alpha_composite(patch,origin)
    mask=Image.new('L',(512,768));pixels=character.load();mp=mask.load()
    region,iris=EYES[index];x0,y0,x1,y1=region
    cx,cy=(iris[0]+iris[2])/2,(iris[1]+iris[3])/2
    rx,ry=(iris[2]-iris[0])/2,(iris[3]-iris[1])/2
    neutral=[(x,y) for y in range(4,68) for x in range(4,146)
             if min(patch.getpixel((x,y))[:3])>205 and max(patch.getpixel((x,y))[:3])-min(patch.getpixel((x,y))[:3])<15]
    if not neutral:raise AssetError('eye repair contains no model-produced ivory material')
    for y in range(y0,y1):
        for x in range(x0,x1):
            r,g,b,a=pixels[x,y]
            outside=((x-cx)/rx)**2+((y-cy)/ry)**2>1
            if outside and a>200 and g-r>25 and b-r>35 and g>110 and b>140:
                mp[x,y]=255
                rgb=material.getpixel((x,y))
                if min(rgb[:3])<200 or max(rgb[:3])-min(rgb[:3])>20:
                    # Sample nearby repaired sclera, never borrow a blue iris
                    # or overwrite the original outline with a redrawn one.
                    px,py=min(neutral,key=lambda p:(p[0]-(x-origin[0]))**2+(p[1]-(y-origin[1]))**2)
                    material.putpixel((x,y),patch.getpixel((px,py)))
    result=Image.composite(material,character,mask)
    result.putalpha(character.getchannel('A'))
    edits=DIRECTORY/'EyeEdits';edits.mkdir(exist_ok=True)
    material.save(edits/f'{index:04d}-material.png');mask.save(edits/f'{index:04d}-mask.png')
    return result,{'mask':f'EyeEdits/{index:04d}-mask.png','material':f'EyeEdits/{index:04d}-material.png',
                   'iris_envelope':iris,'pixels_changed':sum(bool(v) for v in mask.get_flattened_data())}


def make_cup():
    source=Image.open(DIRECTORY/'mug-source.png').convert('RGBA')
    alpha=source.getchannel('A').point(lambda a:0 if a<8 else 255 if a>=245 else a)
    source.putalpha(alpha);bbox=alpha.point(lambda a:255 if a>=128 else 0).getbbox()
    cup=source.crop(bbox).resize((PARKED_BOX[2]-PARKED_BOX[0],PARKED_BOX[3]-PARKED_BOX[1]),Image.Resampling.LANCZOS)
    layer=Image.new('RGBA',(512,768));layer.alpha_composite(cup,PARKED_BOX[:2])
    layer.save(DIRECTORY/'parked-cup.png')
    return layer


def talking_characters():
    # Reject the model's diffuse halo before component/gutter separation.
    source=Image.open(DIRECTORY/'talking-source.png').convert('RGBA')
    source.putalpha(source.getchannel('A').point(lambda a:0 if a<200 else min(255,round((a-200)*255/54))))
    clean=Image.new('RGBA',source.size);clean.alpha_composite(source)
    clean.save(DIRECTORY/'talking-clean-source.png')
    drawings,boxes=separate_sheet(DIRECTORY/'talking-clean-source.png')
    results=[]
    for index,(drawing,box) in enumerate(zip(drawings,boxes)):
        tile=Image.new('RGBA',(768,1024))
        tile.alpha_composite(drawing,(box[0]-(index%2)*512+128,box[1]-(index//2)*768+128))
        original=Image.open(PREVIOUS/f'Keys/talking/{index%2:04d}.png').convert('RGBA')
        character,registration=register_character(tile,original,working_height=1024)
        character,badge=apply_badge(character)
        # The new tail curls behind the case to viewer left. Preserve the
        # source drawing in a background layer; the fixed case occludes it.
        # Only the tail's lower-left region is behind the prop, not the hand.
        tailmask=Image.new('L',(512,768));d=ImageDraw.Draw(tailmask)
        d.polygon([(0,450),(210,450),(240,500),(250,570),(230,650),(0,650)],fill=255)
        tailmask=ImageChops.multiply(tailmask,character.getchannel('A'))
        background=character.copy();background.putalpha(tailmask)
        # The carrier has near-opaque (254) source alpha. Do not let a hidden
        # tail tint even one brand pixel through that archival source alpha.
        case_alpha=Image.open(ACTIVE/'canonical-case.png').getchannel('A')
        background.putalpha(ImageChops.multiply(background.getchannel('A'),case_alpha.point(lambda a:0 if a else 255)))
        foreground=character.copy()
        foreground.putalpha(ImageChops.subtract(character.getchannel('A'),tailmask))
        background.save(DIRECTORY/f'talking-{index:04d}-background.png')
        results.append((foreground,background,{'source':'talking-source.png','source_cell':index,
                                              'registration':registration,'badge':badge,
                                              'background_layer':f'talking-{index:04d}-background.png'}))
    return results


def compose(case,cup,character,background=None,parked=False):
    result=Image.new('RGBA',(512,768))
    if background is not None:result.alpha_composite(background)
    result.alpha_composite(case)
    result.alpha_composite(character)
    # The cup rests on the front edge of the case, in front of a curled tail or
    # leaning sleeve. Do not let it vanish behind the error recoil drawing.
    if parked:result.alpha_composite(cup)
    return result


def package():
    current=read_json(ROOT/'UwU/manifest.json')['styles']['chibi']['states']['idle']['static']
    if current['status']=='approved':
        raise AssetError('Approved generation baseline is frozen. Prepare an authorized versioned revision before replacing operating artwork.')
    from generate_states import make_previews
    case=Image.open(ACTIVE/'canonical-case.png').convert('RGBA');cup=make_cup()
    drawings=talking_characters()
    old=read_json(PREVIOUS/'composition.json');records=[];by_key={(r['state'],r['key']):r for r in old}
    for state,key in [*by_key,('talking',2),('talking',3)]:
        record=dict(by_key.get((state,key),by_key[('talking',key%2)]))
        record.update(state=state,key=key)
        character=Image.open(PREVIOUS/f'Characters/{state}/{key%2 if state=="talking" else key:04d}.png').convert('RGBA')
        background=None
        if state=='talking':
            character,background,new=drawings[key];record.update(new)
            record.pop('garment_edits',None)
        elif state=='working':
            character,edit=eye_repair(character,key);record['eye_repair']=edit
        elif state=='error' and key==2:
            # The puffed tail curls in front of the left case top in the source.
            # Move that occluded tail portion behind the carrier so the parked
            # cup has a visible supporting rail rather than sitting on fur.
            behind=Image.new('L',(512,768));draw=ImageDraw.Draw(behind)
            draw.polygon([(0,399),(187,399),(204,447),(204,489),(0,489)],fill=255)
            cut=ImageChops.multiply(character.getchannel('A'),behind)
            background=character.copy()
            background.putalpha(ImageChops.multiply(cut,case.getchannel('A').point(lambda a:0 if a else 255)))
            character=character.copy();character.putalpha(ImageChops.subtract(character.getchannel('A'),cut))
            background.save(DIRECTORY/'error-0002-background.png')
            record['background_layer']='error-0002-background.png'
        parked=(state,key) not in HELD
        result=compose(case,cup,character,background,parked)
        character.save(ACTIVE/f'Characters/{state}/{key:04d}.png')
        result.save(ACTIVE/f'Keys/{state}/{key:04d}.png')
        record.update(character_sha256=digest(ACTIVE/f'Characters/{state}/{key:04d}.png'),
                      key_sha256=digest(ACTIVE/f'Keys/{state}/{key:04d}.png'),
                      cup={'location':'case' if parked else 'hand',
                           'layer':(DIRECTORY/'parked-cup.png').relative_to(ROOT).as_posix() if parked else None},
                      revision='horu-prop-continuity-3')
        records.append(record)
    write_json(ACTIVE/'composition.json',records)
    manifest=ROOT/'UwU/manifest.json';data=read_json(manifest);states=data['styles']['chibi']['states']
    master=Image.open(ROOT/'UwU/Generated/chibi/idle.png').convert('RGBA')
    for state,entry in states.items():
        if state=='idle':continue
        record_path=ROOT/f'provenance/uwu-{state}-body-revision.json';record=read_json(record_path)
        items=sorted([r for r in records if r['state']==state],key=lambda r:r['key'])
        keys=[Image.open(ACTIVE/f'Keys/{state}/{r["key"]:04d}.png').convert('RGBA') for r in items]
        static_index=0
        previous=read_json(ROOT/f'provenance/consistency-revision/previous/uwu-{state}-body-revision.json')
        if state!='talking':static_index=previous['key_sha256'].index(previous['static']['sha256'])
        static=keys[static_index];path=manifest.parent/entry['static']['path'];static.save(path)
        entry['static']['sha256']=digest(path)
        timeline=record['frame_source_key_indices']
        if state=='talking':timeline=[0,0,1,1,2,2,3,3,0,0,0,0]
        frames=[]
        for index,key in enumerate(timeline):
            image=master.copy() if key is None else keys[key].copy()
            item=entry['animation']['frames'][index];path=manifest.parent/item['path'];image.save(path)
            item['sha256']=digest(path);frames.append(image)
        durations=[item['duration_ms'] for item in entry['animation'].get('frames',[])]
        make_previews('uwu',state,frames,durations,static,entry['behavior'])
        entry['note']='Horu: cup remains held or parked on the fixed case; drawn acting with deliberate holds; visual approval pending.'
        if state=='talking':entry['note']='Horu: four redrawn two-arm speech poses, one continuously held cup; mouth shapes, elbow gestures and restrained follow-through; visual approval pending.'
        record.update(character_name='Horu',revision='horu-prop-continuity-3',
                      drawn_pose_count=len(items),frame_source_key_indices=timeline,
                      composition=items,key_sha256=[r['key_sha256'] for r in items],
                      static=entry['static'],animation=entry['animation'],
                      rest_revision='provenance/horu-idle-revision.json',
                      performance_revision='provenance/horu-performance-repair.json')
        record.setdefault('base_character_generator_sha256',record.get('generator_sha256'))
        record['generator_sha256']=digest(Path(__file__))
        record['generator_snapshot']=f'provenance/generators/{record["generator_sha256"]}.py'
        if state=='talking':
            record.update(source=(DIRECTORY/'talking-source.png').relative_to(ROOT).as_posix(),
                          source_sha256=digest(DIRECTORY/'talking-source.png'),
                          generation='Four newly drawn two-arm talking poses with one held cup; fixed case and canonical badge composition.')
        write_json(record_path,record)
        print(state,'cup continuity:',len(items),'keys',flush=True)
    write_json(manifest,data)
    sources=[DIRECTORY/'talking-source.png',DIRECTORY/'working-eyes-source.png',DIRECTORY/'mug-source.png',
             DIRECTORY/'parked-cup.png',PREVIOUS/'composition.json',
             *sorted((PREVIOUS/'Characters').glob('*/*.png')),*sorted((PREVIOUS/'Keys').glob('*/*.png')),
             *sorted((DIRECTORY/'EyeEdits').glob('*.png')),
             ROOT/'provenance/authorizations/horu-name-idle-scale.md',
             ROOT/'provenance/authorizations/horu-talking-working-props.md']
    write_json(ROOT/'provenance/horu-performance-repair.json',{
        'character_name':'Horu','personality_id':'uwu','approval':'pending human review',
        'authorization':'provenance/authorizations/horu-talking-working-props.md',
        'talking_drawn_keys':4,'total_drawn_keys':len(records),
        'cup_rule':'One purple cup is visible: held in IDLE/talking and selected quiet poses; otherwise parked on the equipment case.',
        'parked_box':list(PARKED_BOX),'held_keys':[[s,k] for s,k in sorted(HELD)],
        'working_edit':'Only model-produced ivory material inside cyan-spill masks; original iris, outlines and all exterior RGBA retained.',
        'generator':Path(__file__).relative_to(ROOT).as_posix(),'generator_sha256':digest(Path(__file__)),
        'inputs':{f.relative_to(ROOT).as_posix():digest(f) for f in sources},
        'limits':['Pose cuts show the cup held or parked; setting it down and picking it up are not separately in-betweened.',
                  'Anatomy and cup presence are visually reviewed; technical masks cannot automatically count illustrated hands.']})


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package',action='store_true',required=True)
    parser.parse_args();package()
