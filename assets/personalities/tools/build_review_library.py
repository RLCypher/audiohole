"""Build a portable state catalog, per-performance inspectors and review ZIPs."""
from __future__ import annotations

import base64
from html import escape
from io import BytesIO
import math
from pathlib import Path
import re
import zipfile

from PIL import Image, ImageDraw, ImageFont

from animate_idle import checker
from avatar_assets import ROOT, load_pack, personality_name, review_key
from pipeline import packs, review


STYLE='''body{background:#151923;color:#e9edf5;font:15px/1.5 system-ui;margin:28px}
a{color:#a0f7d6}main{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:18px}
article{background:#242c3c;padding:14px;border-radius:10px}h1{font-size:28px}h2{font-size:19px}
img{max-width:100%;background:#d5d9e2}button,select{font:inherit;padding:8px;background:#303c50;color:white;border:1px solid #677184;border-radius:5px}
.status{color:#f0c578}.muted{color:#b5bece}code{font-size:12px}'''


def header(title):
    return f'<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>{escape(title)}</title><style>{STYLE}</style>'


def thumbnail(path):
    with Image.open(path) as im:
        image=im.resize((128,192),Image.Resampling.LANCZOS)
        stream=BytesIO();image.save(stream,format='WEBP',quality=88)
    return 'data:image/webp;base64,'+base64.b64encode(stream.getvalue()).decode()


def sheet(identity,manifest,data):
    label=personality_name(identity);key=review_key(identity)
    style=data['default_style'];states=data['styles'][style]['states']
    available=[(state,entry) for state,entry in states.items() if entry['static']['status'] in {'validated','approved'}]
    cols=6;tile_w,tile_h=160,240;rows=math.ceil(len(available)/cols)
    out=Image.new('RGB',(cols*tile_w,rows*tile_h+56),'#151923');draw=ImageDraw.Draw(out)
    font=ImageFont.load_default(size=12)
    draw.text((12,10),f'{label} / {len(available)} expressions / '+('APPROVED' if all(e['static']['status']=='approved' for _,e in available) else 'BODY REVISION / REVIEW PENDING'),font=font,fill='white')
    faces=Image.new('RGB',(cols*180,math.ceil(len(available)/cols)*200+40),'#d9dce5');face_draw=ImageDraw.Draw(faces)
    face_draw.text((10,8),f'{label} native face / logo details',font=font,fill='black')
    face_box=(236,134,364,262) if identity=='default' else (188,155,364,331) if identity=='uwu' else (116,456,397,540)
    for i,(state,entry) in enumerate(available):
        image=Image.open(manifest.parent/entry['static']['path'])
        x,y=(i%cols)*tile_w,(i//cols)*tile_h+56
        canvas=checker((128,192));canvas.alpha_composite(image.resize(canvas.size,Image.Resampling.LANCZOS));out.paste(canvas.convert('RGB'),(x+16,y))
        count=len(entry['animation'].get('frames',[]));duration=sum(f['duration_ms'] for f in entry['animation'].get('frames',[]))
        draw.text((x+8,y+195),state,font=font,fill='white');draw.text((x+8,y+213),f'{count}f / {duration}ms' if count else 'static hold',font=font,fill='#b5bece')
        crop=image.crop(face_box);crop.thumbnail((176,166),Image.Resampling.LANCZOS)
        fx,fy=(i%cols)*180,(i//cols)*200+40;faces.paste(crop,(fx+(180-crop.width)//2,fy),crop)
        face_draw.text((fx+5,fy+170),state,font=font,fill='black')
    directory=ROOT/f'review/full/{review_key(identity)}';directory.mkdir(parents=True,exist_ok=True)
    out.save(directory/'states-contact-sheet.png');faces.save(directory/'face-details.png')
    return out,available


def build():
    directory=ROOT/'review/full';directory.mkdir(exist_ok=True)
    catalog_cards=[];sheets=[]
    for identity,manifest in packs():
        label=personality_name(identity);key=review_key(identity)
        data=load_pack(manifest,identity);style=data['default_style']
        contact,available=sheet(identity,manifest,data);sheets.append(contact)
        cards=[]
        for state,entry in available:
            static=manifest.parent/entry['static']['path']
            count=len(entry['animation'].get('frames',[]));duration=sum(f['duration_ms'] for f in entry['animation'].get('frames',[]))
            status='APPROVED' if entry['static']['status']=='approved' else 'BODY REVISION CANDIDATE'
            thumb=thumbnail(static)
            if state=='idle':
                for target_name,source_name in [('idle-native.png',f'{key}-native.png'),('idle-compact.png',f'{key}-compact.png'),('idle.gif',f'{key}-review.gif'),('idle-frames.png',f'{key}-all-frames.png')]:
                    (directory/key/target_name).write_bytes((ROOT/'review/idle-sequences'/source_name).read_bytes())
            native_link=f'{state}-native.png'
            gif_link=f'{state}.gif'
            frames_link=f'{state}-frames.png'
            inspector=directory/key/f'{state}-inspector.html'
            review(only_states={state,'idle'},only_personalities={identity},output_path=inspector,initial_state=state,summary=False)
            # Each portable performance page embeds its GIF and static thumbnail;
            # it can be opened independently without its parent directory.
            source=ROOT/f'review/full/{key}/{state}.gif'
            gif='data:image/gif;base64,'+base64.b64encode(source.read_bytes()).decode()
            performance=header(f'{label} · {state}')+f'''<h1>{label} · {state}</h1><p class="status">{status}</p>
<p>{count} operating frames · {duration} ms · {escape(entry.get('behavior',{}).get('mode','persistent'))}</p>
<button id="replay">Replay</button> <label><input id="reduced" type="checkbox"> Reduced motion / static expression</label>
<div><img id="animation" width="192" height="288" src="{gif}" alt="{label} {state}"></div>
<p class="muted">GIF is a checkerboard review copy; operating PNGs retain real alpha. Finite animations stop at rest; static expressions remain visible for inspection. This page embeds its images and needs no server.</p>
<script>const still={thumb!r};let counter=0;const image=document.getElementById('animation');const moving=image.getAttribute('src');
function show(){{image.src=document.getElementById('reduced').checked?still:moving+'#replay='+counter++;}}
document.getElementById('replay').onclick=show;document.getElementById('reduced').onchange=show;show();</script></html>'''
            (directory/key/f'{state}-preview.html').write_text(performance)
            card=f'''<article><h2>{state}</h2><img width="128" height="192" src="{thumb}" alt="{label} {state} static">
<p class="status">{status}</p><p>{count} frames · {duration} ms</p>
<p><a href="{state}-preview.html">Play / reduced motion</a><br><a href="{state}-inspector.html">Native / compact frame inspector</a><br>
<a href="{gif_link}">Direct GIF</a> · <a href="{native_link}">Native PNG preview</a> · <a href="{frames_link}">Every frame</a></p></article>'''
            cards.append(card)
            catalog_cards.append(f'''<article><h2>{label} · {state}</h2><img width="128" height="192" src="{thumb}" alt="{label} {state}"><p class="status">{status}</p><p>{count}f · {duration}ms</p>
<p><a href="{key}/{state}-preview.html">Playable preview</a><br><a href="{key}/{state}-inspector.html">Frame inspector</a><br><a href="{key}/{gif_link}">Direct GIF</a></p></article>''')
            print(identity,state,'review pages built',flush=True)
        page=header(f'AudioHole {label} review')+f'<h1>{label} · Full expression catalog</h1><p>Norm and Vanilla approved. UwU body revisions await review; accepted IDLE is preserved.</p><p><a href="states-contact-sheet.png">State sheet</a> · <a href="face-details.png">Native detail sheet</a></p><main>'+''.join(cards)+'</main></html>'
        (directory/key/'index.html').write_text(page)
        # Compact portable review package: pages, GIFs, and frame sheets. Native
        # operating PNGs/manifests stay in their repository pack for integration.
        with zipfile.ZipFile(directory/f'{key}-review.zip','w',zipfile.ZIP_DEFLATED) as archive:
            for path in (directory/key).iterdir():
                if path.suffix in {'.html','.gif'} or path.name in {'states-contact-sheet.png','face-details.png'} or path.name.endswith('-frames.png'):
                    if path.name=='index.html':
                        portable=re.sub(r'<a href="[^"]+-native\.png">Native PNG preview</a>','<span>Native PNGs supplied in repository</span>',path.read_text())
                        archive.writestr(path.name,portable)
                    else:
                        archive.write(path,path.name)
            archive.writestr('README.txt','Extract and open index.html in a browser. Individual *-preview.html pages embed their media and also work alone. Norm and Vanilla are approved. UwU body revisions remain candidates; IDLE is accepted. Runtime assets are the separate transparent 512x768 PNG packs in the repository.\n')
    combined=Image.new('RGB',(max(s.width for s in sheets),sum(s.height for s in sheets)),'#151923');y=0
    for image in sheets:combined.paste(image,(0,y));y+=image.height
    combined.save(directory/'all-states-contact-sheet.png')
    index=header('AudioHole full avatar catalog')+'''<h1>AudioHole · Full avatar catalog</h1>
<p>Norm and Vanilla approved. UwU faces accepted; revised body performances require human review. Static expressions also serve reduced motion. Select a playable page to replay a reaction or inspect individual operating frames.</p>
<p><a href="uwu-body-revision-loop.gif">UwU body demo</a> · <a href="uwu-body-keyposes.png">Every body key</a> · <a href="UWU_BODY_REVISION.md">Body revision report</a></p>
<p><a href="all-states-contact-sheet.png">All generated expressions</a> · <a href="inventory.json">Full inventory and validation</a> · <a href="HANDOFF.md">Integration handoff</a></p>
<p><a href="norm-review.zip">Norm review ZIP</a> · <a href="uwu-review.zip">UwU review ZIP</a> · <a href="vanilla-review.zip">Vanilla review ZIP</a></p><main>'''+''.join(catalog_cards)+'</main></html>'
    (directory/'index.html').write_text(index)
    print('Full catalog, per-state inspectors and portable review ZIPs built.',flush=True)


if __name__=='__main__':build()
