"""Package drawn UwU body keys as deliberately held anime performances.

Image generation supplies the actual articulated drawings. This tool separates
them, applies one common fit per state, and exposes them with timed holds. It
never rotates/translates the finished sprite to impersonate body articulation.
"""
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageChops

from avatar_assets import ROOT, AssetError, digest, inspect_png, read_json, validate_pack
from generate_states import POLICY, durations, make_previews
from pipeline import write_json


SOURCE = ROOT/'UwU/Intermediate/BodyRevision'
MAPPING = {
    **{state: (state, [0,1,2,3]) for state in ['error','angry','surprised','celebrating','panicking']},
    **{state: (batch, [row*2,row*2+1])
       for batch, pair in {
           'quiet-a': ['happy','thinking'], 'quiet-b': ['working','talking'],
           'quiet-c': ['confused','concerned'], 'quiet-d': ['sleeping','smug'],
           'quiet-e': ['embarrassed','laughing'], 'quiet-f': ['suspicious','bored'],
           'quiet-g': ['mischievous','deadpan'],
       }.items() for row,state in enumerate(pair)},
}


def quiet_gap(counts, center, radius):
    """Find an actual empty gutter; reject a sheet with overlapping drawings."""
    candidates=[x for x in range(max(1,center-radius),min(len(counts)-1,center+radius))
                if counts[x]==0]
    if not candidates:
        raise AssetError('pose drawings overlap: no clear extraction gutter')
    # Prefer a wide run so its center preserves antialiased outlines on either side.
    runs=[];start=last=candidates[0]
    for value in candidates[1:]:
        if value!=last+1:
            runs.append((start,last));start=value
        last=value
    runs.append((start,last))
    a,b=max(runs,key=lambda pair: (pair[1]-pair[0],-abs((pair[0]+pair[1])/2-center)))
    return (a+b)//2


def winding_gutter(alpha):
    """Separate staggered poses along a clear, winding background corridor."""
    lo,hi=alpha.width*3//8,alpha.width*5//8
    width=hi-lo;pixels=alpha.load();previous=[0.0]*width;history=[]
    for y in range(alpha.height):
        current=[];parents=bytearray(width)
        for j in range(width):
            choices=[k for k in (j-1,j,j+1) if 0<=k<width]
            parent=min(choices,key=lambda k:previous[k]+abs(k-j)*.05)
            a=pixels[lo+j,y]
            current.append(previous[parent]+(10000 if a>=32 else a/255)+abs(j-width/2)*.0001)
            parents[j]=parent-j+1
        history.append(parents);previous=current
    j=min(range(width),key=lambda k:previous[k]);path=[]
    for y in range(alpha.height-1,-1,-1):
        path.append(lo+j);j+=history[y][j]-1
    path.reverse()
    if any(pixels[x,y]>=32 for y,x in enumerate(path)):
        raise AssetError('pose drawings touch/overlap: no clear winding gutter')
    return path


def separate_sheet(path):
    with Image.open(path) as source:
        sheet=source.convert('RGBA')
    alpha=sheet.getchannel('A')
    strong=alpha.point(lambda a: 255 if a>=64 else 0)
    # Pillow projections keep this offline workflow on the existing pinned stack.
    row_counts=[sum(strong.crop((0,y,sheet.width,y+1)).getdata()) for y in range(sheet.height)]
    split_y=quiet_gap(row_counts,sheet.height//2,sheet.height//12)
    drawings=[];boxes=[]
    for row,(top,bottom) in enumerate([(0,split_y),(split_y,sheet.height)]):
        strip=strong.crop((0,top,sheet.width,bottom))
        column_counts=[sum(strip.crop((x,0,x+1,strip.height)).getdata()) for x in range(strip.width)]
        try:
            split_x=quiet_gap(column_counts,sheet.width//2,sheet.width//9)
            boundaries=[split_x]*strip.height
        except AssetError:
            boundaries=winding_gutter(alpha.crop((0,top,sheet.width,bottom)))
        from PIL import ImageDraw
        mask=Image.new('L',strip.size);draw=ImageDraw.Draw(mask)
        for y,x in enumerate(boundaries):
            draw.line((0,y,x-1,y),fill=255)
        for column,selection in enumerate([mask,ImageChops.invert(mask)]):
            tile=sheet.crop((0,top,sheet.width,bottom))
            tile.putalpha(ImageChops.multiply(tile.getchannel('A'),selection))
            # Remove negligible background alpha without repainting illustration.
            tile.putalpha(tile.getchannel('A').point(lambda a: 0 if a<=6 else a))
            bbox=tile.getchannel('A').getbbox()
            if not bbox:
                raise AssetError('empty pose key')
            # Outer sheet edges can represent genuine cropped anatomy. Do not hide
            # that by simply shrinking it into a compliant runtime canvas.
            solid=tile.getchannel('A').point(lambda a:255 if a>=128 else 0).getbbox()
            if ((column==0 and solid[0]==0) or (column==1 and solid[2]==tile.width)
                    or (top==0 and solid[1]==0) or (bottom==sheet.height and solid[3]==tile.height)):
                raise AssetError(f'pose touches outer source edge: {path.name}, row {row}, column {column}, {solid}')
            drawings.append(tile.crop(bbox))
            boxes.append([bbox[0],top+bbox[1],bbox[2],top+bbox[3]])
    return drawings,boxes


def case_anchor(image):
    """Locate a substantial leftmost mint case stripe, excluding boot laces."""
    pixels=image.load();points=set()
    for y in range(round(image.height*.42),image.height):
        for x in range(round(image.width*.58)):
            r,g,b,a=pixels[x,y]
            if a>128 and 60<r<210 and g>150 and g>r*1.22 and b>r*1.1:
                points.add((x,y))
    components=[]
    while points:
        stack=[points.pop()];component=[]
        while stack:
            x,y=stack.pop();component.append((x,y))
            for point in [(x-1,y),(x+1,y),(x,y-1),(x,y+1)]:
                if point in points:
                    points.remove(point);stack.append(point)
        components.append(component)
    candidates=[component for component in components if len(component)>=300]
    if not candidates:
        raise AssetError('cannot identify case stripe for stable scene registration')
    centers={id(c):sum(x for x,y in c)/len(c) for c in candidates}
    leftmost=min(centers.values())
    component=max((c for c in candidates if centers[id(c)]<=leftmost+20),key=len)
    return (sum(x for x,y in component)/len(component),sum(y for x,y in component)/len(component))


def fitted_keys(drawings, master):
    # One scale for all drawings in a state, anchored to a stationary prop rather
    # than a silhouette center that shifts when an arm or tail extends.
    target=case_anchor(master);anchors=[case_anchor(im) for im in drawings]
    scale=min([1.0]+[min((target[0]-16)/cx,(496-target[0])/(im.width-cx),
                         (target[1]-40)/cy,(712-target[1])/(im.height-cy))
                       for im,(cx,cy) in zip(drawings,anchors)])
    result=[]
    for image,(cx,cy) in zip(drawings,anchors):
        sprite=image.resize((round(image.width*scale),round(image.height*scale)),Image.Resampling.LANCZOS)
        frame=Image.new('RGBA',(512,768))
        frame.alpha_composite(sprite,(round(target[0]-cx*scale),round(target[1]-cy*scale)))
        result.append(frame)
    return result,scale,anchors,target


def key_timeline(count, drawn_count, finite):
    if not count:
        return []
    if finite:
        steps=[None,0,1,2,1,2,3,0,None] if drawn_count==4 else [None,0,1,1,0,None]
    else:
        steps=[0,0,1,1,0,0]
    selected=[steps[min(len(steps)-1,i*len(steps)//count)] for i in range(count)]
    if finite:
        selected[0]=selected[-1]=None
    else:
        selected[-1]=selected[0]
    return selected


def generate(selected=None):
    manifest=ROOT/'UwU/manifest.json'
    data=read_json(manifest);info=data['styles']['chibi'];master_path=manifest.parent/'Generated/chibi/idle.png'
    master=Image.open(master_path).copy()
    approved=read_json(ROOT/'provenance/idle-approval.json')['assets']
    if digest(master_path)!=approved[master_path.relative_to(ROOT).as_posix()]:
        raise AssetError('accepted IDLE changed')
    cache={};records={}
    for state,(batch,indices) in MAPPING.items():
        if selected and state not in selected:
            continue
        entry=info['states'][state]
        if any(entry[k]['status']=='approved' for k in ['static','animation']):
            raise AssetError(f'refusing to regenerate approved {state}')
        source=SOURCE/f'{batch}-source.png'
        if batch not in cache:
            cache[batch]=separate_sheet(source)
        raw,boxes=cache[batch]
        keys,scale,anchors,target=fitted_keys([raw[i] for i in indices],master)
        key_dir=SOURCE/'Keys'/state;key_dir.mkdir(parents=True,exist_ok=True)
        for i,key in enumerate(keys):
            key.save(key_dir/f'{i:04d}.png')
        static=keys[1 if len(keys)==4 else 0]
        static_path=manifest.parent/f'Generated/chibi/{state}.png';static.save(static_path)
        entry['static']={'status':'validated','path':static_path.relative_to(manifest.parent).as_posix(),
                         'sha256':digest(static_path),'anchor':info['anchor']}
        policy=entry['behavior'];finite=policy['mode']=='finite';count=POLICY[state][4]
        times=durations(state);timeline=key_timeline(count,len(keys),finite)
        frames=[];metadata=[]
        for i,key_index in enumerate(timeline):
            image=master.copy() if key_index is None else keys[key_index].copy()
            path=manifest.parent/f'Generated/chibi/{state}/{i:04d}.png';path.parent.mkdir(exist_ok=True)
            image.save(path);checked=inspect_png(path)
            frames.append(image)
            metadata.append({'path':path.relative_to(manifest.parent).as_posix(),'sha256':checked['sha256'],
                             'anchor':info['anchor'],'duration_ms':times[i]})
        if count:
            old=entry['animation']
            entry['animation']={**{k:v for k,v in old.items() if k not in {'review','frames','status'}},
                                'status':'validated','frames':metadata}
        entry['note']=('Drawn articulated body poses with deliberate anime holds; '
                       'no whole-sprite rotation/translation. Facial designs follow accepted expression references. '
                       +('Intentional static deadpan freeze.' if not count else ''))
        make_previews('uwu',state,frames,times,static,policy)
        record={'personality_id':'uwu','state':state,'status':'validated','approval':'pending human review',
                'revision':'articulated-body-1','generation':'reference-guided full-body pose drawings with timed holds',
                'master_sha256':digest(master_path),'source':source.relative_to(ROOT).as_posix(),'source_sha256':digest(source),
                'source_indices':indices,'source_crop_boxes':[boxes[i] for i in indices],
                'common_scale':scale,'registration':'mint case stripe centroid',
                'source_prop_anchors':anchors,'target_prop_anchor':target,
                'drawn_pose_count':len(keys),'frame_source_key_indices':timeline,
                'key_sha256':[digest(key_dir/f'{i:04d}.png') for i in range(len(keys))],
                'static':entry['static'],'animation':entry['animation'],'behavior':policy,
                'generator_sha256':digest(Path(__file__))}
        write_json(ROOT/f'provenance/uwu-{state}-body-revision.json',record)
        records[state]=record;write_json(manifest,data)
        print(state,len(keys),'drawn poses;',count,'operating frames',flush=True)
    write_json(ROOT/'provenance/uwu-body-revision.json',{
        'authorization':'provenance/authorizations/norm-vanilla-approval-uwu-body-revision.md',
        'status':'candidate','approved_idle_unchanged':True,
        'method':'Model-drawn articulated key poses, common fit per state, deliberate exposure holds, exact rest endpoints.',
        'limitations':['Limited key animation, not fully in-betweened traditional animation.',
                      'Generated clothing/prop details and head proportions may vary; inspect the new drawings before approval.',
                      'Facial expression designs are retained through reference guidance; newly drawn poses are not pixel-identical faces.'],
        'states':{s:{k:r[k] for k in ['drawn_pose_count','frame_source_key_indices','source_sha256']} for s,r in records.items()}})
    report=validate_pack(manifest,'uwu')
    if report['errors']:
        raise AssetError('; '.join(report['errors']))


if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('states',nargs='*')
    args=parser.parse_args();generate(set(args.states) if args.states else None)
