"""Build combined IDLE contact sheets and an animated-preview gallery."""
from pathlib import Path
import base64
import zipfile

from PIL import Image, ImageDraw, ImageFont

from avatar_assets import ROOT, read_json


def build():
    directory = ROOT/'review/idle-sequences'
    identities = ['default', 'uwu', 'vanilla']
    approval_label='APPROVED IDLE' if (ROOT/'provenance/idle-approval.json').exists() else 'UNAPPROVED'
    sheets = [Image.open(directory/f'{pid}-all-frames.png').convert('RGB') for pid in identities]
    combined = Image.new('RGB', (sheets[0].width, sum(im.height for im in sheets)), '#151923')
    y = 0
    for sheet in sheets:
        combined.paste(sheet, (0, y))
        y += sheet.height
    combined.save(directory/'all-personalities-all-frames.png')
    # Inspect exact native pixel samples separately from compact GUI scaling.
    font = ImageFont.load_default(size=16)
    for identity, folder, style in [('default', 'Norm', 'chibi'), ('uwu', 'UwU', 'chibi'), ('vanilla', 'Vanilla', 'logo')]:
        panel = Image.new('RGBA', (1280, 820), '#1b2130')
        draw = ImageDraw.Draw(panel)
        for col, frame_index in enumerate([0, 5]):
            im = Image.open(ROOT/f'{folder}/Generated/{style}/idle/{frame_index:04d}.png')
            x = col*640
            draw.text((x+8, 4), f'{identity} / frame {frame_index:04d} / native 512 x 768', font=font, fill='white')
            bg = Image.new('RGBA', im.size, '#e0e3e9')
            bg.alpha_composite(im)
            panel.alpha_composite(bg, (x, 34))
            thumb = im.resize((128, 192), Image.Resampling.LANCZOS)
            small = Image.new('RGBA', thumb.size, '#e0e3e9')
            small.alpha_composite(thumb)
            panel.alpha_composite(small, (x+512, 34))
            draw.text((x+515, 230), '128 x 192', font=font, fill='white')
        panel.convert('RGB').save(directory/f'{identity}-native-compact-inspection.png')
    cards = []
    for identity in identities:
        encoded = base64.b64encode((directory/f'{identity}-review.gif').read_bytes()).decode()
        cards.append(f'''<section><h2>{identity}</h2>
<p>GIF review copy · autoplay · five-second loop</p>
<img width="256" height="384" src="data:image/gif;base64,{encoded}" alt="{identity} IDLE animation">
</section>''')
    html = '''<!doctype html><html lang="en"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>AudioHole IDLE loops</title>
<style>body{background:#151923;color:#edf1f9;font:16px/1.5 system-ui;margin:32px}
main{display:flex;gap:28px;flex-wrap:wrap}section{padding:20px;background:#242c3c;border-radius:12px}
img{background:#e0e3e9;max-width:100%;object-fit:contain}
a{color:#a2f6d2}summary{cursor:pointer}h2{margin-top:0}</style>
<h1>AudioHole · Three initial IDLE loops</h1><p>__APPROVAL__ · 12 authored frames per loop · 5.0 s · 100 ms timebase with exposure holds</p>
<p>This page contains its three animated GIFs. It needs no scripts, server, network, or neighboring files.
The checkerboard belongs only to these review copies. Operating frames remain transparent 512 × 768 RGBA PNGs.</p>
<p>Native APNGs, labeled frame sheets and the report are supplied separately in the review folder.</p><main>'''
    (directory/'index.html').write_text(html.replace('__APPROVAL__',approval_label)+'\n'.join(cards)+'</main></html>\n', encoding='utf-8')
    with zipfile.ZipFile(directory/'idle-previews.zip','w',zipfile.ZIP_DEFLATED) as archive:
        for name in ['index.html','all-personalities-all-frames.png',*[f'{pid}-review.gif' for pid in identities]]:
            archive.write(directory/name, name)
        archive.writestr('README.txt','Extract this archive and open index.html in a browser, or open the GIFs directly.\nThese are review copies; operating assets remain the separate transparent 512x768 PNG files in the repository.\n')
    print('Combined 36-frame sheet, native/compact inspection panels, and gallery written.')


if __name__ == '__main__':
    build()
