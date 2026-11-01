"""Build combined IDLE contact sheets and an animated-preview gallery."""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

from avatar_assets import ROOT


def build():
    directory = ROOT/'review/idle-sequences'
    identities = ['default', 'uwu', 'vanilla']
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
        cards.append(f'''<section><h2>{identity}</h2>
<p>Compact 128 × 192 · autoplay, five-second loop</p>
<img width="128" height="192" src="{identity}-compact.png" alt="{identity} compact IDLE animation">
<details><summary>Native 512 × 768 animated preview</summary>
<img width="512" height="768" src="{identity}-native.png" alt="{identity} native IDLE animation"></details>
<p><a href="{identity}-all-frames.png">All 12 labeled frames</a></p></section>''')
    html = '''<!doctype html><html lang="en"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>AudioHole IDLE loops</title>
<style>body{background:#151923;color:#edf1f9;font:16px/1.5 system-ui;margin:32px}
main{display:flex;gap:28px;flex-wrap:wrap}section{padding:20px;background:#242c3c;border-radius:12px}
img{background:repeating-conic-gradient(#c5c7ce 0% 25%,#e4e5eb 0% 50%) 0 0 / 24px 24px;max-width:100%;object-fit:contain}
a{color:#a2f6d2}summary{cursor:pointer}h2{margin-top:0}</style>
<h1>AudioHole · Three initial IDLE loops</h1><p>UNAPPROVED · 12 authored frames per loop · 5.0 s · 100 ms timebase with exposure holds</p>
<p>These animated PNGs are review previews. Individual operating frames remain 512 × 768 RGBA PNGs.
Blink once per loop; compare native detail with the representative compact GUI size.</p>
<p><a href="../preview.html">Frame-by-frame inspector</a> · <a href="all-personalities-all-frames.png">All 36 frames</a> · <a href="REPORT.md">Report</a></p><main>'''
    (directory/'index.html').write_text(html+'\n'.join(cards)+'</main></html>\n', encoding='utf-8')
    print('Combined 36-frame sheet, native/compact inspection panels, and gallery written.')


if __name__ == '__main__':
    build()
