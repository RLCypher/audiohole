"""Compose UwU costume-cleanup drawings with reusable canonical brand layers.

The image backend supplies pose-preserving garment edits and character-only
layers. Registration normalizes the source drawings; runtime frames only hold
finished keys. Accepted artwork is never overwritten.
"""
from __future__ import annotations

import argparse
import math
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

from avatar_assets import ROOT, AssetError, digest, read_json, inspect_png, validate_pack
from pipeline import write_json
from revise_uwu_body import separate_sheet


DIRECTORY=ROOT/'UwU/Intermediate/ConsistencyRevision'
OLD=ROOT/'UwU/Intermediate/BodyRevision/Keys'
MASTER=ROOT/'UwU/Generated/chibi/idle.png'
BRAND_BOX=(94,498,213,596)


def components(points):
    points=set(points);result=[]
    while points:
        stack=[points.pop()];component=[]
        while stack:
            x,y=stack.pop();component.append((x,y))
            for neighbor in [(x-1,y),(x+1,y),(x,y-1),(x,y+1)]:
                if neighbor in points:
                    points.remove(neighbor);stack.append(neighbor)
        result.append(component)
    return result


def ear_candidates(image, offset=(0,0)):
    pixels=image.load();points=[]
    for y in range(20+offset[1],390+offset[1]):
        for x in range(80+offset[0],480+offset[0]):
            r,g,b,a=pixels[x,y]
            if a>180 and r>185 and r-g>50 and r-b>15 and 80<g<230:
                points.append((x,y))
    choices=[]
    for component in components(points):
        if len(component)<80:continue
        xs=[p[0] for p in component];ys=[p[1] for p in component]
        if max(xs)-min(xs)<10 or max(ys)-min(ys)<10:continue
        choices.append((sum(xs)/len(xs),sum(ys)/len(ys),len(component)))
    return sorted(choices,key=lambda c:c[1])


def ear_anchors(image, offset=(0,0)):
    choices=ear_candidates(image,offset)
    pairs=[sorted([a,b]) for i,a in enumerate(choices) for b in choices[i+1:]
           if 100<abs(a[0]-b[0])<280 and abs(a[1]-b[1])<150]
    if not pairs:raise AssetError(f'cannot identify separated ear interiors: {choices}')
    ears=min(pairs,key=lambda pair:sum(p[1] for p in pair)/2+.05*abs(pair[0][1]-pair[1][1])
             -8*math.log(min(p[2] for p in pair)))
    return ears


def register_character(image, original, override=None):
    if override:
        old=[[*p,0] for p in override['target']]
        new=[[p[0]+128,p[1]+128,0] for p in override['source']]
    else:
        old=ear_anchors(original);new=ear_anchors(image, (128,128))
    distance=lambda ears:math.hypot(ears[1][0]-ears[0][0],ears[1][1]-ears[0][1])
    scale=distance(old)/distance(new)
    if not .65<scale<1.25:raise AssetError(f'unexpected character source scale {scale}')
    old_center=[sum(p[j] for p in old)/2 for j in [0,1]]
    new_center=[sum(p[j] for p in new)/2 for j in [0,1]]
    dx,dy=[old_center[j]-scale*new_center[j] for j in [0,1]]
    frame=image.transform((512,768),Image.Transform.AFFINE,(1/scale,0,-dx/scale,0,1/scale,-dy/scale),Image.Resampling.BICUBIC)
    frame.putalpha(frame.getchannel('A').point(lambda a:0 if a<=6 else 255 if a>=245 else a))
    bbox=frame.getchannel('A').point(lambda a:255 if a>=128 else 0).getbbox()
    if not bbox:raise AssetError('empty character layer')
    neck=round(old_center[1]+105*scale)
    body_scale=(704-neck)/(bbox[3]-neck)
    if not .78<body_scale<1.3:raise AssetError(f'excessive body normalization {body_scale}')
    upper=frame.crop((0,0,512,neck))
    body=frame.transform((512,768),Image.Transform.AFFINE,(1,0,0,0,1/body_scale,neck-neck/body_scale),Image.Resampling.BICUBIC)
    # The transform is continuous at the neck; preserve the upper drawing exactly.
    body.paste(upper,(0,0))
    clean=Image.new('RGBA',body.size);clean.alpha_composite(body);body=clean
    return body,{'ear_source':new,'ear_target':old,'manual_landmarks':bool(override),'uniform_scale':scale,'translation':[dx,dy],
                 'neck_y':neck,'body_vertical_fit':body_scale,'ground_y':704}


def canonical_case():
    original=Image.open(MASTER).convert('RGBA')
    generated=Image.open(DIRECTORY/'case-clean-source.png').convert('RGBA').resize((512,768),Image.Resampling.LANCZOS)
    bbox=generated.getchannel('A').point(lambda a:255 if a>=128 else 0).getbbox()
    target=(43,403,249,694)
    sx=(target[2]-target[0])/(bbox[2]-bbox[0]);sy=(target[3]-target[1])/(bbox[3]-bbox[1])
    dx=target[0]-bbox[0]*sx;dy=target[1]-bbox[1]*sy
    generated=generated.transform((512,768),Image.Transform.AFFINE,(1/sx,0,-dx/sx,0,1/sy,-dy/sy),Image.Resampling.BICUBIC)
    # Complete the once-hidden prop once, then freeze it. Its generated carrier
    # is reference-guided; the complete visible brand block is the exact approved
    # raster. A narrow blend stays outside the protected block.
    generated.putalpha(generated.getchannel('A').point(lambda a:0 if a<=6 else a))
    known=Image.new('L',(512,768));draw=ImageDraw.Draw(known)
    draw.rectangle((90,494,222,600),fill=255)
    known=known.filter(ImageFilter.GaussianBlur(1.5))
    ImageDraw.Draw(known).rectangle(BRAND_BOX,fill=255)
    result=Image.composite(original,generated,known)
    # The exact visible brand block is the accepted raster, not a model redraw.
    result.paste(original.crop(BRAND_BOX),BRAND_BOX)
    clean=Image.new('RGBA',result.size);clean.alpha_composite(result);result=clean
    result.save(DIRECTORY/'canonical-case.png');known.save(DIRECTORY/'case-known-pixels-mask.png')
    return result


def blank_badge_box(image):
    pixels=image.load();points=[]
    dark=[]
    for y in range(350,590):
        for x in range(220,390):
            r,g,b,a=pixels[x,y]
            if a>180 and min(r,g,b)>175 and max(r,g,b)-min(r,g,b)<70:points.append((x,y))
            if a>180 and r<110 and g<90 and b<110:dark.append((x,y))
    candidates=[]
    # Find the enclosed dark card face. Its white rim may connect to a hanger
    # or pale fur, so a white connected-component box alone is insufficient.
    for component in components(dark):
        if len(component)<150:continue
        xs=[p[0] for p in component];ys=[p[1] for p in component]
        x0,y0,x1,y1=min(xs),min(ys),max(xs)+1,max(ys)+1;w,h=x1-x0,y1-y0
        if not(14<=w<=35 and 20<=h<=45 and .45<w/h<1.05 and len(component)/(w*h)>.55):continue
        cx,cy=(x0+x1)//2,(y0+y1)//2
        rim=[pixels[x0-2,cy],pixels[x1+1,cy],pixels[cx,y0-2],pixels[cx,y1+1]]
        if sum(min(p[:3])>150 and max(p[:3])-min(p[:3])<80 for p in rim)<2:continue
        candidates.append((x0-3,y0-3,x1+3,y1+3))
    if candidates:
        return min(candidates,key=lambda b:((b[0]+b[2])/2-290)**2+((b[1]+b[3])/2-490)**2)
    for component in components(points):
        if len(component)<30:continue
        xs=[p[0] for p in component];ys=[p[1] for p in component]
        box=(min(xs),min(ys),max(xs)+1,max(ys)+1);w,h=box[2]-box[0],box[3]-box[1]
        if not(20<=w<=42 and 25<=h<=55 and .42<w/h<.98):continue
        cx,cy=(box[0]+box[2])//2,(box[1]+box[3])//2
        if max(pixels[cx,cy][:3])>125:continue
        if len(component)/(w*h)>.5:continue
        candidates.append(box)
    if not candidates:raise AssetError('cannot identify blank portrait badge')
    return min(candidates,key=lambda b:((b[0]+b[2])/2-290)**2+((b[1]+b[3])/2-490)**2)


def apply_badge(character, box=None, add_hanger=False):
    box=box or blank_badge_box(character)
    # Source includes the full approved badge rim and closed wormhole emblem.
    # The finished graphic is transformed only as a physical decal, never redrawn.
    name='canonical-badge-assembly.png' if add_hanger else 'canonical-badge.png'
    template=Image.open(DIRECTORY/name).convert('RGBA')
    x0,y0,x1,y1=box
    top=y0-round((y1-y0)*18/41) if add_hanger else y0
    patch=template.resize((x1-x0,y1-top),Image.Resampling.LANCZOS)
    character=character.copy();character.alpha_composite(patch,(x0,top))
    return character,{'box':list(box),'assembly_top':top,'template':name,'template_sha256':digest(DIRECTORY/name)}


def apply_garment_edits(character, state, key):
    path=DIRECTORY/'garment-edits.json'
    if not path.exists():return character,[]
    records=[]
    for edit in read_json(path):
        if edit['state']!=state or edit['key']!=key:continue
        patch=Image.open(DIRECTORY/edit['patch']).convert('RGBA')
        mask=Image.open(DIRECTORY/edit['mask']).convert('L')
        alpha=character.getchannel('A').copy()
        character=Image.composite(patch,character,mask)
        character.putalpha(alpha)
        records.append({'patch':edit['patch'],'patch_sha256':digest(DIRECTORY/edit['patch']),
                        'mask':edit['mask'],'mask_sha256':digest(DIRECTORY/edit['mask'])})
    return character,records


def corrected_keys(batch, case):
    layouts=read_json(DIRECTORY/'guide-layout.json')
    overrides=read_json(DIRECTORY/'placement-overrides.json')
    with Image.open(DIRECTORY/f'{batch}-source.png') as source:
        source=source.convert('RGBA')
        if source.size!=(1024,1536):raise AssetError('cleanup source must retain the 2x2 512x768 grid')
        drawings,boxes=separate_sheet(DIRECTORY/f'{batch}-source.png')
        records=[]
        for index,(state,key) in enumerate(layouts[batch]):
            # A clear gutter may wind around an extended arm/tail. Keep the
            # complete drawing on a padded canvas instead of clipping at x=512.
            tile=Image.new('RGBA',(768,1024))
            tile.alpha_composite(drawings[index],(boxes[index][0]-(index%2)*512+128,
                                                  boxes[index][1]-(index//2)*768+128))
            original=Image.open(OLD/state/f'{key:04d}.png').convert('RGBA')
            try:
                selected=overrides.get(f'{state}/{key:04d}',{})
                character,registration=register_character(tile,original,selected.get('ears'))
                character,badge=apply_badge(character,selected.get('badge_box'),selected.get('add_badge_hanger',False))
                character,garment_edits=apply_garment_edits(character,state,key)
            except AssetError as error:
                raise AssetError(f'{state}/{key:04d}: {error}') from error
            composite=case.copy();composite.alpha_composite(character)
            out=DIRECTORY/'Keys'/state;out.mkdir(parents=True,exist_ok=True)
            layers=DIRECTORY/'Characters'/state;layers.mkdir(parents=True,exist_ok=True)
            character.save(layers/f'{key:04d}.png');composite.save(out/f'{key:04d}.png')
            records.append({'state':state,'key':key,'source':f'{batch}-source.png','source_cell':index,
                            'source_crop_box':boxes[index],
                            'registration':registration,'badge':badge,'case_sha256':digest(DIRECTORY/'canonical-case.png'),
                            'garment_edits':garment_edits,
                            'character_sha256':digest(layers/f'{key:04d}.png'),'key_sha256':digest(out/f'{key:04d}.png')})
    return records


def package(records):
    """Replace only unapproved UwU presentations after all layer keys preflight."""
    from generate_states import make_previews
    manifest=ROOT/'UwU/manifest.json';data=read_json(manifest);info=data['styles']['chibi']
    approved=read_json(ROOT/'provenance/accepted-assets.json')['assets']
    for relative,sha in approved.items():
        if digest(ROOT/relative)!=sha:raise AssetError(f'approved image changed: {relative}')
    grouped={}
    for record in records:grouped.setdefault(record['state'],[]).append(record)
    expected=set(read_json(ROOT/'states.json')['standard']+read_json(ROOT/'states.json')['optional'])-{'idle'}
    if set(grouped)!=expected or len(records)!=48:
        raise AssetError('packaging requires all 19 states and all 48 reviewed pose keys')
    for state,items in grouped.items():
        if any(info['states'][state][kind]['status']=='approved' for kind in ['static','animation']):
            raise AssetError(f'refusing to replace approved {state}')
        for item in items:
            path=DIRECTORY/f"Keys/{state}/{item['key']:04d}.png"
            metadata=inspect_png(path);box=metadata['bbox']
            if metadata['sha256']!=item['key_sha256'] or min(box[:2])<=0 or box[2]>=512 or box[3]>=768:
                raise AssetError(f'key changed or touches canvas: {state}/{item["key"]}')
    master=Image.open(MASTER).convert('RGBA');summaries={}
    for state,items in grouped.items():
        items.sort(key=lambda r:r['key']);entry=info['states'][state]
        previous=read_json(ROOT/f'provenance/consistency-revision/previous/uwu-{state}-body-revision.json')
        static_index=previous['key_sha256'].index(previous['static']['sha256'])
        keys=[Image.open(DIRECTORY/f'Keys/{state}/{item["key"]:04d}.png').convert('RGBA') for item in items]
        static=keys[static_index];static_path=manifest.parent/f'Generated/chibi/{state}.png';static.save(static_path)
        entry['static']={'status':'validated','path':static_path.relative_to(manifest.parent).as_posix(),
                         'sha256':digest(static_path),'anchor':info['anchor']}
        timeline=previous['frame_source_key_indices'];times=[f['duration_ms'] for f in previous['animation'].get('frames',[])]
        frames=[];meta=[]
        for index,selected in enumerate(timeline):
            image=master.copy() if selected is None else keys[selected].copy()
            path=manifest.parent/f'Generated/chibi/{state}/{index:04d}.png';image.save(path);frames.append(image)
            meta.append({'path':path.relative_to(manifest.parent).as_posix(),'sha256':digest(path),
                         'anchor':info['anchor'],'duration_ms':times[index]})
        if meta:entry['animation']={**{k:v for k,v in previous['animation'].items() if k not in {'frames','review'}},'frames':meta,'status':'validated'}
        entry['note']='Articulated costume-cleanup keys with one fixed case, canonical badge decals and timed holds; artistic approval pending.'
        make_previews('uwu',state,frames,times,static,entry['behavior'])
        record={'personality_id':'uwu','state':state,'status':'validated','approval':'pending human review',
                'revision':'articulated-body-2-consistency','generation':'costume cleanup drawings plus reusable case and canonical badge layers',
                'master_sha256':digest(MASTER),'source':(DIRECTORY/items[0]['source']).relative_to(ROOT).as_posix(),
                'source_sha256':digest(DIRECTORY/items[0]['source']),
                'keys_directory':(DIRECTORY/f'Keys/{state}').relative_to(ROOT).as_posix(),
                'character_layers_directory':(DIRECTORY/f'Characters/{state}').relative_to(ROOT).as_posix(),
                'case_path':(DIRECTORY/'canonical-case.png').relative_to(ROOT).as_posix(),
                'case_sha256':digest(DIRECTORY/'canonical-case.png'),'composition':items,
                'drawn_pose_count':len(keys),'frame_source_key_indices':timeline,'key_sha256':[r['key_sha256'] for r in items],
                'static':entry['static'],'animation':entry['animation'],'behavior':entry['behavior'],
                'generator_sha256':digest(Path(__file__))}
        write_json(ROOT/f'provenance/uwu-{state}-body-revision.json',record)
        summaries[state]={k:record[k] for k in ['drawn_pose_count','frame_source_key_indices','source_sha256']}
        print(state,'packaged',len(frames),'frames',flush=True)
    write_json(manifest,data)
    write_json(ROOT/'provenance/uwu-body-revision.json',{'authorization':'provenance/authorizations/uwu-consistency-correction.md',
        'status':'candidate','revision':'articulated-body-2-consistency','approved_idle_unchanged':True,
        'method':'Drawn character layers with fixed case and canonical badge compositing; timed key holds.',
        'limitations':['Limited key animation; no additional in-between drawings.',
                       'Newly revealed prop/wardrobe details and source registration remain human art review items.'],
        'states':summaries})
    report=validate_pack(manifest,'uwu')
    if report['errors']:raise AssetError('; '.join(report['errors']))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('batches',nargs='*');parser.add_argument('--package',action='store_true')
    args=parser.parse_args();case=canonical_case();records=[]
    for batch in (args.batches or read_json(DIRECTORY/'guide-layout.json')):records.extend(corrected_keys(batch,case))
    write_json(DIRECTORY/'composition.json',records)
    print('Corrected keys composed:',len(records))
    if args.package:package(records)
