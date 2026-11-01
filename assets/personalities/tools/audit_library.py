"""Audit all enabled personality states and preserve the accepted IDLE baseline."""
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageChops

from avatar_assets import ROOT, digest, inspect_png, load_pack, read_json, validate_pack
from avatar_assets import personality_name, review_key
from pipeline import inventory, packs, write_json


def difference(a,b,mask=None):
    diff=ImageChops.difference(a,b)
    if mask is not None:
        diff=Image.merge('RGBA',[ImageChops.multiply(c,mask) for c in diff.split()])
    return max(bounds[1] for bounds in diff.getextrema())


def audit():
    catalog=read_json(ROOT/'states.json')
    accepted=read_json(ROOT/'provenance/idle-approval.json')
    errors=list(inventory()['errors'])
    for relative,sha in accepted['assets'].items():
        if not (ROOT/relative).is_file() or digest(ROOT/relative)!=sha:
            errors.append(f'accepted IDLE changed: {relative}')
    for relative,sha in read_json(ROOT/'provenance/accepted-assets.json')['assets'].items():
        if not (ROOT/relative).is_file() or digest(ROOT/relative)!=sha:
            errors.append(f'approved artwork changed: {relative}')
    result={'approval':'Norm and Vanilla approved; UwU IDLE approved; non-IDLE body revision candidates pending review',
            'errors':errors,'packs':{},'totals':{'static_assets':0,'animation_sequences':0,'animation_frames':0,'operating_pngs':0}}
    for identity,manifest in packs():
        data=load_pack(manifest,identity);style=data['default_style'];info=data['styles'][style]
        report=validate_pack(manifest,identity)
        pack_errors=list(report['errors']);states={}
        master=Image.open(manifest.parent/f'Generated/{style}/idle.png').copy()
        protected=None
        if identity=='vanilla':
            protected=ImageChops.invert(Image.open(manifest.parent/'Intermediate/wormhole-motion-mask.png'))
        for state,entry in info['states'].items():
            ready=entry['static']['status'] in {'validated','approved'}
            animated=entry['animation']['status'] in {'validated','approved'}
            if state in catalog['standard'] and not ready:
                pack_errors.append(f'{state}: mandatory core static/reduced-motion expression missing')
            behavior=entry.get('behavior',{})
            frames=entry['animation'].get('frames',[])
            description={'static_status':entry['static']['status'],'animation_status':entry['animation']['status'],
                         'static_path':entry['static'].get('path'),'frame_count':len(frames),
                         'frame_durations_ms':[f['duration_ms'] for f in frames],
                         'duration_ms':sum(f['duration_ms'] for f in frames),'behavior':behavior,
                         'note':entry.get('note'),'fallback':entry.get('fallback')}
            if ready:
                static=Image.open(manifest.parent/entry['static']['path']).copy()
                if protected is not None and difference(master,static,protected):
                    pack_errors.append(f'{state}: Vanilla static changed protected exterior')
                result['totals']['static_assets']+=1
                result['totals']['operating_pngs']+=1
            if animated:
                result['totals']['animation_sequences']+=1
                result['totals']['animation_frames']+=len(frames)
                result['totals']['operating_pngs']+=len(frames)
                first=last=None;boxes=[]
                for index,frame in enumerate(frames):
                    path=manifest.parent/frame['path'];metadata=inspect_png(path);image=Image.open(path).copy()
                    if first is None:first=image.copy()
                    last=image
                    boxes.append(metadata['bbox'])
                    if protected is not None and difference(master,image,protected):
                        pack_errors.append(f'{state}/{index}: Vanilla frame changed protected exterior')
                    if protected is not None and difference(master.getchannel('A').convert('RGBA'),image.getchannel('A').convert('RGBA')):
                        pack_errors.append(f'{state}/{index}: Vanilla alpha changed')
                    if metadata['bbox'][0]<=0 or metadata['bbox'][1]<=0 or metadata['bbox'][2]>=512 or metadata['bbox'][3]>=768:
                        pack_errors.append(f'{state}/{index}: artwork touches canvas; inspect clipping')
                description['loop_endpoint_max_rgba_delta']=difference(first,last)
                if description['loop_endpoint_max_rgba_delta']:
                    pack_errors.append(f'{state}: endpoints do not join or return to rest')
                if behavior.get('mode')=='finite' and (difference(first,master) or difference(last,master)):
                    pack_errors.append(f'{state}: finite reaction does not start/end at accepted rest')
                centers=[((b[0]+b[2])/2,(b[1]+b[3])/2) for b in boxes]
                description['max_adjacent_bbox_center_shift_px']=max([max(abs(a[j]-b[j]) for j in [0,1]) for a,b in zip(centers,centers[1:])] or [0])
                if behavior:
                    animation=entry['animation']
                    if animation['loop'] != (behavior['mode']=='persistent'):
                        pack_errors.append(f'{state}: persistence/loop metadata conflict')
                    for key in ['priority','interruptible','cooldown_ms']:
                        if animation[key]!=behavior[key]:pack_errors.append(f'{state}: conflicting {key}')
                if state!='idle':
                    for tag,size in [('native',(512,768)),('compact',(128,192))]:
                        preview=ROOT/f'review/full/{review_key(identity)}/{state}-{tag}.png'
                        with Image.open(preview) as apng:
                            total=0
                            for index in range(apng.n_frames):
                                apng.seek(index);apng.load();total+=round(apng.info['duration'])
                            if apng.size!=size or total!=description['duration_ms']:
                                pack_errors.append(f'{state}: {tag} preview size/timing mismatch')
                            description[f'{tag}_preview_frames']=apng.n_frames
                    with Image.open(ROOT/f'review/full/{review_key(identity)}/{state}.gif') as gif:
                        total=0
                        for index in range(gif.n_frames):gif.seek(index);total+=gif.info['duration']
                        if total!=description['duration_ms']:pack_errors.append(f'{state}: GIF exposure timing mismatch')
                        if behavior.get('mode')=='finite' and 'loop' in gif.info:pack_errors.append(f'{state}: finite GIF repeats')
                        if behavior.get('mode')=='persistent' and gif.info.get('loop')!=0:pack_errors.append(f'{state}: persistent GIF does not loop')
            states[state]=description
        result['packs'][identity]={'display_name':personality_name(identity),'character_id':data['character_id'],'style':style,'core_complete':not any(not states[s]['static_path'] for s in catalog['standard']),
                                   'states':states,'errors':pack_errors,'warnings':sorted(set(report['warnings'])),
                                   'disabled_styles':[name for name,s in data['styles'].items() if not s['enabled']]}
        result['errors'].extend(f'{identity}: {error}' for error in pack_errors)
    return result


if __name__=='__main__':
    report=audit();write_json(ROOT/'review/full/inventory.json',report)
    lines=['# Full avatar inventory','','Norm and Vanilla are approved. UwU IDLE is approved; its revised non-IDLE body performances await human review. The `default` machine ID is retained only for application compatibility.','',
           '| Personality | State | Static | Frames | Duration ms | Playback | Priority | Cooldown ms |',
           '|---|---|---|---:|---:|---|---:|---:|']
    for identity,pack in report['packs'].items():
        for state,entry in pack['states'].items():
            b=entry['behavior']
            lines.append(f"| {personality_name(identity)} | {state} | {entry['static_status']} | {entry['frame_count']} | {entry['duration_ms']} | {b.get('mode','fallback')} | {b.get('priority','—')} | {b.get('cooldown_ms','—')} |")
    (ROOT/'review/full/INVENTORY.md').write_text('\n'.join(lines)+'\n')
    print(report['totals'])
    print('Errors:',report['errors'])
    raise SystemExit(bool(report['errors']))
