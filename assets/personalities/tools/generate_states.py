"""Controlled core/optional generation from accepted IDLE and retained face keys.

Never regenerates IDLE. Face-key sheets are model outputs; all motion is a
documented 2D compositing performance, not independently generated video frames.
"""
from __future__ import annotations

import argparse
from collections import deque
import hashlib
import math
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

from animate_idle import (checker, compose_character_frame, mesh_warp,
                         vanilla_frame, wormhole_mask)
from avatar_assets import ROOT, AssetError, digest, inspect_png, load_pack, read_json, validate_pack
from pipeline import write_json


CORE_A = ['happy','thinking','working','talking','confused','concerned']
CORE_B = ['error','angry','surprised','celebrating','sleeping']
OPTIONAL = ['smug','embarrassed','laughing','suspicious','panicking','bored','mischievous','deadpan']
POLICY = {
    'happy': ('expressive','finite',30,2500,12,1400),
    'thinking': ('ambient','persistent',10,500,16,3200),
    'working': ('ambient','persistent',15,500,16,2400),
    'talking': ('ambient','persistent',20,300,12,1200),
    'confused': ('expressive','finite',35,3000,14,1800),
    'concerned': ('expressive','persistent',45,1000,16,3200),
    'error': ('theatrical','finite',95,6000,18,2000),
    'angry': ('theatrical','finite',65,6000,16,1800),
    'surprised': ('theatrical','finite',70,5000,14,1500),
    'celebrating': ('theatrical','finite',55,8000,22,2400),
    'sleeping': ('ambient','persistent',5,1000,16,4000),
    'smug': ('expressive','finite',30,4000,12,1600),
    'embarrassed': ('expressive','finite',40,5000,16,1800),
    'laughing': ('expressive','finite',40,5000,18,2000),
    'suspicious': ('expressive','finite',35,4000,12,1800),
    'panicking': ('theatrical','finite',85,8000,20,2200),
    'bored': ('ambient','persistent',5,1000,12,4000),
    'mischievous': ('expressive','finite',35,5000,14,1800),
    'deadpan': ('expressive','finite',35,4000,0,1600),
}
FACE = {
    'default': {'folder':'Norm','box':(236,134,364,262),
                'polygon':[(258,153),(289,153),(308,164),(332,175),(343,184),(342,206),(335,220),(324,230),(323,243),(298,242),(290,231),(268,229),(250,208)],
                'polygons':[
                    [(258,148),(287,150),(308,163),(308,192),(298,203),(266,201),(251,185)],
                    [(314,166),(334,170),(343,182),(341,204),(329,206),(317,202),(310,191)],
                    [(269,205),(304,209),(331,223),(331,237),(313,247),(286,247),(269,232)]],
                'nose_region':(310,196,340,221),'preserve_nose':(313,208,337,219),
                'registration':[-2,9],
                'edit_nose':True,
                'nose_polygon':[(299,205),(340,205),(339,220),(329,224),(320,223),(299,220)],
                'preserve_rectangles':[(334,205,350,262)],
                'mouth':(272,208,334,243)},
    'uwu': {'folder':'UwU','box':(188,155,364,331),
            'polygon':[(210,171),(280,170),(299,186),(320,190),(343,201),(346,245),(332,269),(309,292),(282,302),(251,290),(218,271),(205,237)],
            'nose_region':(278,240,320,278),'preserve_nose':(289,250,312,266),
            'nose_polygon':[(289,250),(311,254),(307,264),(300,266),(293,259)],
            'mouth':(253,255,305,296)},
}


def pink_components(image, region, skin=False):
    pixels = image.load()
    candidates = set()
    for y in range(region[1],region[3]):
        for x in range(region[0],region[2]):
            r,g,b,a = pixels[x,y]
            valid = (r>g*1.25 and r>b*.95 and g>60) if not skin else (r>180 and 75<g<180 and 30<b<135 and r>g*1.4 and g>b*1.2)
            if a>100 and valid:
                candidates.add((x,y))
    result = []
    while candidates:
        queue = [candidates.pop()]
        points = []
        while queue:
            x,y = queue.pop()
            points.append((x,y))
            for p in [(x-1,y),(x+1,y),(x,y-1),(x,y+1)]:
                if p in candidates:
                    candidates.remove(p)
                    queue.append(p)
        if len(points)>8:
            result.append((len(points),sum(x for x,y in points)/len(points),sum(y for x,y in points)/len(points)))
    return result


def face_key(identity, batch, index, master, state):
    spec = FACE[identity]
    directory = ROOT/spec['folder']
    source = directory/f'Intermediate/{batch}-source.png'
    with Image.open(source) as sheet:
        cols,rows=(2,2) if batch.startswith('optional-') else (2,3)
        if sheet.mode != 'RGBA' or sheet.width*rows != sheet.height*cols:
            raise AssetError('face sheet has incorrect RGBA grid geometry')
        w,h = sheet.width//cols,sheet.height//rows
        tile = sheet.crop(((index%2)*w,(index//2)*h,(index%2+1)*w,(index//2+1)*h))
        box = spec['box']
        patch = Image.new('RGBA',master.size)
        patch.paste(tile.resize((box[2]-box[0],box[3]-box[1]),Image.Resampling.LANCZOS),box[:2])
    dx,dy = spec.get('registration',(0.0,0.0))
    if identity in {'uwu','default'}:
        region=spec['nose_region'] if identity=='uwu' else (299,193,342,239)
        target = pink_components(master,region,skin=identity=='default')
        candidates = pink_components(patch,region,skin=identity=='default')
        if not target or not candidates:
            raise AssetError(f'{state}: cannot register nose in face sheet')
        # The canonical nose is the component closest to its known center.
        reference=(303,258) if identity=='uwu' else (316,212)
        target = min(target,key=lambda p:(p[1]-reference[0])**2+(p[2]-reference[1])**2)
        hint=(304,200) if identity=='default' and batch=='optional-a' else (target[1],target[2])
        candidate = min(candidates,key=lambda p:(p[1]-hint[0])**2+(p[2]-hint[1])**2)
        dx,dy = target[1]-candidate[1],target[2]-candidate[2]
        limit=18 if identity=='default' and batch=='optional-a' else 14
        if abs(dx)>limit or abs(dy)>limit:
            raise AssetError(f'{state}: face-key displacement too large: {dx},{dy}')
    patch = patch.transform(master.size,Image.Transform.AFFINE,(1,0,-dx,0,1,-dy),Image.Resampling.BICUBIC)
    mask = Image.new('L',master.size)
    for polygon in spec.get('polygons',[spec['polygon']])+([[(299,198),(338,198),(339,231),(301,231)]] if spec.get('edit_nose') else []):
        ImageDraw.Draw(mask).polygon(polygon,fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(1.3))
    result = Image.composite(patch,master,mask)
    result.putalpha(master.getchannel('A'))
    keep = spec['preserve_nose']
    if spec.get('edit_nose'):
        pass
    elif 'nose_polygon' in spec:
        nose_mask=Image.new('L',master.size)
        ImageDraw.Draw(nose_mask).polygon(spec['nose_polygon'],fill=255)
        result=Image.composite(master,result,nose_mask)
    else:
        result.paste(master.crop(keep),keep)
    for rectangle in spec.get('preserve_rectangles',[]):
        result.paste(master.crop(rectangle),rectangle)
    out = directory/'Intermediate/Expressions'
    out.mkdir(exist_ok=True)
    result.save(out/f'{state}.png')
    mask.save(out/f'{state}-face-mask.png')
    return result, {'source':source.relative_to(ROOT).as_posix(),'source_sha256':digest(source),
                    'grid':[cols,rows],'cell_index':index,'face_box':list(spec['box']),
                    'registration_translation':[dx,dy],'face_polygon':spec['polygon'],
                    'nose_preserved_box':None if spec.get('edit_nose') else list(keep),'key_sha256':digest(out/f'{state}.png')}


def behavior(state, vanilla=False):
    category,mode,priority,cooldown,n,total = POLICY[state]
    return {'category':'status' if vanilla else category,'mode':mode,'priority':priority,
            'interruptible':state not in {'angry','surprised'},'cooldown_ms':cooldown,
            'static_hold_ms':total,'return_to':'persistent','reduced_motion':'static'}


def durations(state, count=None):
    category,mode,priority,cooldown,n,total = POLICY[state]
    n = n if count is None else count
    if n == 0:
        return []
    weights = [1]*n
    if mode == 'finite':
        weights[n//2] = 3
        weights[min(n-2,n//2+1)] = 2
        weights[-1] = 2
    raw = [max(20,round(total*w/sum(weights)/10)*10) for w in weights]
    raw[-1] += total-sum(raw)
    return raw


def effect_layer(identity, state, progress, intensity=1):
    overlay = Image.new('RGBA',(512,768))
    d = ImageDraw.Draw(overlay)
    mint = (158,251,212,round(255*intensity))
    pink = (238,158,251,round(255*intensity))
    black = (10,10,16,round(230*intensity))
    def line(points,fill=mint,width=4):
        d.line(points,fill=black,width=width+3,joint='curve')
        d.line(points,fill=fill,width=width,joint='curve')
    def star(x,y,size,fill=mint):
        vertices=[(x,y-size),(x+size*.25,y-size*.25),(x+size,y),(x+size*.25,y+size*.25),
                  (x,y+size),(x-size*.25,y+size*.25),(x-size,y),(x-size*.25,y-size*.25)]
        d.polygon(vertices,fill=fill,outline=black,width=2)
    def drop(x,y):
        d.polygon([(x,y-14),(x-8,y+6),(x-6,y+12),(x+5,y+12),(x+8,y+5)],fill=(92,203,244,round(240*intensity)),outline=black,width=2)
    scale = .6 if identity=='default' else 1
    if state in {'happy','celebrating','smug','mischievous'}:
        star(440,125+math.sin(progress*math.tau)*4,9*scale+4*intensity,pink)
        if state in {'happy','celebrating'}:
            star(60,125,8*scale,mint)
        if state=='celebrating':
            star(450,360,9,pink)
            star(55,335,7,mint)
    elif state in {'confused','thinking','suspicious'}:
        if state=='confused':
            d.arc((427,103,449,127),180,490,fill=black,width=7)
            d.arc((427,103,449,127),180,490,fill=mint,width=4)
            line([(438,124),(436,136)])
            d.ellipse((433,143,439,149),fill=mint,outline=black,width=1)
        else:
            for j in range(3):
                x=424+j*12
                d.ellipse((x,132+(j%2)*3,x+5,137+(j%2)*3),fill=mint,outline=black,width=1)
    elif state in {'concerned','embarrassed','panicking'}:
        drop(435,154+math.sin(progress*math.tau)*6)
        if state=='panicking':
            drop(75,170-math.sin(progress*math.tau)*5)
            line([(441,85),(452,66)],pink,3)
            line([(463,100),(479,91)],mint,3)
    elif state=='angry':
        for box,angles in [((321,76,337,92),(0,90)),((340,76,356,92),(90,180)),
                           ((321,95,337,111),(270,360)),((340,95,356,111),(180,270))]:
            d.arc(box,*angles,fill=black,width=7)
            d.arc(box,*angles,fill=pink,width=4)
    elif state in {'error','surprised'}:
        for a,b in [((422,102),(434,80)),((439,119),(461,106)),((439,141),(462,144))]:
            line([a,b],pink if state=='error' else mint,4)
    elif state=='working':
        center=(441,132)
        d.ellipse((427,118,455,146),outline=black,width=6)
        d.ellipse((427,118,455,146),outline=mint,width=3)
        for j in range(6):
            angle=progress*math.tau+j*math.tau/6
            line([(center[0]+math.cos(angle)*15,center[1]+math.sin(angle)*15),
                  (center[0]+math.cos(angle)*21,center[1]+math.sin(angle)*21)],mint,3)
    elif state=='sleeping':
        x=427+progress*7
        y=145-progress*10
        line([(x,y),(x+16,y),(x,y+17),(x+16,y+17)],mint,3)
    elif state=='talking':
        for j in range(2):
            d.arc((413-j*3,176-j*8,437+j*10,194+j*8),285,75,fill=mint,width=3)
    elif state=='laughing':
        line([(429,122),(439,110)],pink,3)
        line([(439,133),(455,130)],mint,3)
    return overlay


def close_mouth(expression, master, identity):
    mask=Image.new('L',master.size)
    ImageDraw.Draw(mask).rectangle(FACE[identity]['mouth'],fill=255)
    mask=mask.filter(ImageFilter.GaussianBlur(1))
    result=Image.composite(master,expression,mask)
    result.putalpha(master.getchannel('A'))
    return result


def pose(image, identity, state, progress, finite):
    amplitude = .42 if identity=='default' else 1.0
    envelope = math.sin(math.pi*progress) if finite else math.sin(progress*math.tau)
    dx=dy=angle=0.0
    sx=sy=1.0
    if state=='celebrating':
        dy=-14*amplitude*abs(math.sin(progress*math.tau))*math.sin(math.pi*progress)
        angle=1.8*amplitude*math.sin(progress*math.tau)
    elif state in {'error','surprised'}:
        anticipation=math.sin(progress*math.pi)
        dy=-8*amplitude*anticipation
        dx=5*amplitude*math.sin(progress*math.tau)*anticipation
        angle=2.0*amplitude*math.sin(progress*math.tau)
        sx=1+.025*amplitude*anticipation
        sy=1-.018*amplitude*anticipation
    elif state=='angry':
        dx=2.2*amplitude*math.sin(progress*math.tau*2)*math.sin(progress*math.pi)
        dy=-3.5*amplitude*abs(math.sin(progress*math.tau*2))*math.sin(progress*math.pi)
    elif state=='panicking':
        dx=5*amplitude*math.sin(progress*math.tau*3)*math.sin(progress*math.pi)
        angle=2.4*amplitude*math.sin(progress*math.tau*2)*math.sin(progress*math.pi)
    elif state in {'confused','suspicious','smug','mischievous'}:
        angle={'confused':1.6,'suspicious':1.1,'smug':.7,'mischievous':1.2}[state]*amplitude*envelope
    elif state in {'happy','laughing','embarrassed'}:
        dy=-3.5*amplitude*abs(math.sin(progress*math.tau))*math.sin(math.pi*progress)
        angle=.8*amplitude*math.sin(progress*math.tau)
    if abs(dx)+abs(dy)+abs(angle)+abs(sx-1)+abs(sy-1)<1e-8:
        return image
    # Explicit reversible sticker motion about the established ground anchor.
    radians=math.radians(angle)
    a=math.cos(radians)/sx;b=math.sin(radians)/sx
    d=-math.sin(radians)/sy;e=math.cos(radians)/sy
    cx,cy=256,704
    c=cx-a*(cx+dx)-b*(cy+dy)
    f=cy-d*(cx+dx)-e*(cy+dy)
    return image.transform(image.size,Image.Transform.AFFINE,(a,b,c,d,e,f),Image.Resampling.BICUBIC)


def character_performance(identity,state,expression,master,progress,index,count,rig):
    folder=FACE[identity]['folder']
    finite=POLICY[state][1]=='finite'
    if finite and index in (0,count-1):
        return master.copy()
    if not finite and index in (0,count-1):
        result=expression.copy()
        result.alpha_composite(effect_layer(identity,state,0,.65))
        return result
    source=expression
    if state=='talking' and index%3!=1:
        source=close_mouth(expression,master,identity)
    if state in {'thinking','working','concerned','bored'} and index==count-4:
        closed=Image.open(ROOT/f'{folder}/Intermediate/idle-blink-keyframe.png')
        mask=Image.new('L',master.size)
        eyes=[(259,170,307,198),(314,182,339,210)] if identity=='default' else [(207,204,281,247),(293,228,340,259)]
        draw=ImageDraw.Draw(mask)
        for box in eyes:draw.rectangle(box,fill=255)
        mask=mask.filter(ImageFilter.GaussianBlur(1))
        source=Image.composite(closed,source,mask)
        source.putalpha(master.getchannel('A'))
    clean=Image.open(ROOT/f'{folder}/Intermediate/idle-clean-base.png').copy()
    steam=Image.open(ROOT/f'{folder}/Intermediate/idle-steam-layer.png').copy()
    # The clean base removes only neutral steam pixels; keep the new face.
    breath=.5-.5*math.cos(progress*math.tau)
    result=compose_character_frame(source,clean,steam,folder,breath,progress*math.tau,rig)
    result=pose(result,identity,state,progress,finite)
    intensity=min(1,math.sin(math.pi*progress)*1.7) if finite else .65+.15*math.sin(progress*math.tau)
    result.alpha_composite(effect_layer(identity,state,progress,max(0,intensity)))
    return result


def logo_performance(master,state,progress,index,count):
    finite=POLICY[state][1]=='finite'
    if index in (0,count-1):
        return master.copy()
    phase=progress*math.tau
    strengths={'happy':1.4,'thinking':.65,'working':1.1,'talking':.7,'confused':.8,
               'concerned':.5,'error':1.8,'angry':1.4,'surprised':1.5,'celebrating':1.9,'sleeping':0}
    strength=strengths[state]
    mask=wormhole_mask()
    def motion(x,y):
        radius=((x-256)/130)**2+((y-499)/28)**2
        weight=max(0,1-radius)**2
        dx=2.0*strength*math.sin(phase+(y-499)/20)*weight
        dy=1.5*strength*(1-math.cos(phase))*weight
        if state=='working':dx+=1.5*math.sin(phase*2)*weight
        return dx,dy
    moved=mesh_warp(master,motion,cell=4,resample=Image.Resampling.BILINEAR)
    # Interior-only modulation of existing mint lines, never recoloring the logo.
    gain=1.0
    if state in {'concerned','error','angry'}:
        gain=.55+.45*abs(math.cos(phase*(2 if state=='error' else 1)))
    elif state=='sleeping':gain=.35
    elif state in {'happy','celebrating'}:gain=.75+.25*math.cos(phase)
    if gain<1:
        channels=list(moved.split())
        for c in range(3):channels[c]=channels[c].point(lambda v:round(v*gain))
        moved=Image.merge('RGBA',channels)
    result=Image.composite(moved,master,mask)
    result.putalpha(master.getchannel('A'))
    return result


def make_previews(identity,state,frames,times,static,behavior):
    out=ROOT/f'review/full/{identity}'
    out.mkdir(parents=True,exist_ok=True)
    images=frames or [static]
    playback_times=times or [behavior['static_hold_ms']]
    for tag,size in [('native',(512,768)),('compact',(128,192))]:
        previews=[im.copy() if im.size==size else im.resize(size,Image.Resampling.LANCZOS) for im in images]
        previews[0].save(out/f'{state}-{tag}.png',save_all=len(previews)>1,append_images=previews[1:],
                         duration=playback_times,loop=0 if behavior['mode']=='persistent' else 1,disposal=0,blend=0)
    # GIFs are review copies on a checkerboard. Finite reactions play once;
    # opening the file again or the preview's Replay control repeats them.
    flat=[]
    for im in images:
        canvas=checker((192,288));canvas.alpha_composite(im.resize(canvas.size,Image.Resampling.LANCZOS));flat.append(canvas.convert('RGB'))
    palette_image=Image.new('RGB',(192*4,288*math.ceil(len(flat)/4)))
    for index,im in enumerate(flat):palette_image.paste(im,((index%4)*192,(index//4)*288))
    palette=palette_image.quantize(colors=256)
    gif=[im.quantize(palette=palette,dither=Image.Dither.NONE) for im in flat]
    args={'loop':0} if behavior['mode']=='persistent' else {}
    gif[0].save(out/f'{state}.gif',save_all=len(gif)>1,append_images=gif[1:],duration=playback_times if len(gif)>1 else playback_times[0],optimize=True,disposal=1,**args)
    thumb=checker((128,192));thumb.alpha_composite(static.resize(thumb.size,Image.Resampling.LANCZOS));thumb.convert('RGB').save(out/f'{state}-static-preview.png')
    cols=min(6,len(images));rows=math.ceil(len(images)/cols)
    sheet=Image.new('RGB',(cols*128,rows*220+36),'#151923');draw=ImageDraw.Draw(sheet);font=ImageFont.load_default(size=12)
    draw.text((8,8),f'{identity} / {state} / {len(frames)} frames / {sum(times)} ms / UNAPPROVED',font=font,fill='white')
    for index,im in enumerate(images):
        x,y=(index%cols)*128,(index//cols)*220+36
        canvas=checker((128,192));canvas.alpha_composite(im.resize(canvas.size,Image.Resampling.LANCZOS));sheet.paste(canvas.convert('RGB'),(x,y))
        draw.text((x+4,y+195),f'{index:04d} | {playback_times[index]} ms',font=font,fill='white')
    sheet.save(out/f'{state}-frames.png')


def generate(identity,batch):
    if identity not in {'default','uwu','vanilla'}:
        raise AssetError('this generator only owns the initial three personalities')
    folder={'default':'Norm','uwu':'UwU','vanilla':'Vanilla'}[identity]
    manifest=ROOT/folder/'manifest.json';data=load_pack(manifest,identity);style=data['default_style']
    master_path=manifest.parent/f'Generated/{style}/idle.png'
    master=Image.open(master_path).copy()
    accepted=read_json(ROOT/'provenance/idle-approval.json')['assets']
    if digest(master_path)!=accepted[master_path.relative_to(ROOT).as_posix()]:
        raise AssetError('accepted master changed')
    states=CORE_A if batch=='core-a' else CORE_B if batch=='core-b' else OPTIONAL
    rig=None if identity=='vanilla' else read_json(ROOT/'provenance/idle-rigs.json')[identity]
    records=[]
    for index,state in enumerate(states):
        entry=data['styles'][style]['states'][state]
        if entry['static']['status']=='approved' or entry['animation']['status']=='approved':
            raise AssetError(f'refusing to regenerate approved {state}')
        if identity=='vanilla' and batch=='optional':
            entry['static']={'status':'unavailable'};entry['animation']={'status':'unavailable'}
            entry['note']='No emotional personality; this optional expression intentionally uses the selected logo IDLE.'
            continue
        if identity=='vanilla':
            expression=logo_performance(master,state,.5,1,3)
            provenance={'source':master_path.relative_to(ROOT).as_posix(),'protected_exterior':'wormhole mask only'}
        else:
            source_batch=('optional-a' if index<4 else 'optional-b') if batch=='optional' else batch
            expression,provenance=face_key(identity,source_batch,index%4 if batch=='optional' else index,master,state)
        policy=behavior(state,identity=='vanilla')
        static=expression.copy()
        if identity!='vanilla':static.alpha_composite(effect_layer(identity,state,.5,1))
        path=manifest.parent/f'Generated/{style}/{state}.png';static.save(path);checked=inspect_png(path)
        entry['static']={'status':'validated','path':path.relative_to(manifest.parent).as_posix(),'sha256':checked['sha256'],'anchor':data['styles'][style]['anchor']}
        entry['behavior']=policy
        entry.pop('fallback',None)
        n=POLICY[state][4]
        if identity=='vanilla' and state=='sleeping':n=0
        times=durations(state,n);frames=[];frame_meta=[]
        for i in range(n):
            progress=i/(n-1)
            if identity=='vanilla':frame=logo_performance(master,state,progress,i,n)
            else:frame=character_performance(identity,state,expression,master,progress,i,n,rig)
            target=manifest.parent/f'Generated/{style}/{state}/{i:04d}.png';target.parent.mkdir(exist_ok=True);frame.save(target)
            checked=inspect_png(target);frames.append(frame)
            frame_meta.append({'path':target.relative_to(manifest.parent).as_posix(),'sha256':checked['sha256'],'anchor':data['styles'][style]['anchor'],'duration_ms':times[i]})
        if n:
            entry['animation']={'status':'validated','frames':frame_meta,'loop':policy['mode']=='persistent',
                                'priority':policy['priority'],'interruptible':policy['interruptible'],'cooldown_ms':policy['cooldown_ms'],
                                'transition':'cut' if policy['mode']=='persistent' else 'finish_then_persistent'}
        else:
            entry['animation']={'status':'not_generated'}
            entry['note']='Intentional static quiet/comedic hold; reduced motion uses the same expression.'
        make_previews(identity,state,frames,times,static,policy)
        record={'personality_id':identity,'state':state,'status':'validated','approval':'pending human review',
                'master_sha256':digest(master_path),'generation':'reference-guided face key plus controlled 2D compositing' if identity!='vanilla' else 'canonical logo interior-only status composition',
                'provenance':provenance,'static':entry['static'],'animation':entry['animation'],'behavior':policy,
                'generator_sha256':digest(Path(__file__))}
        records.append(record);write_json(ROOT/f'provenance/{identity}-{state}-generation.json',record)
        write_json(manifest,data)
        print(identity,state,'frames',n,'duration',sum(times),'saved',flush=True)
    write_json(manifest,data)
    report=validate_pack(manifest,identity)
    write_json(ROOT/f'review/full/{identity}-{batch}-validation.json',report)
    if report['errors']:
        raise AssetError('; '.join(report['errors']))
    return records


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('personality',choices=['default','uwu','vanilla'])
    parser.add_argument('batch',choices=['core-a','core-b','optional'])
    args=parser.parse_args()
    generate(args.personality,args.batch)
