"""Native/compact evidence and a portable GIF for Horu's latest revision."""
from PIL import Image, ImageDraw, ImageFont

from avatar_assets import ROOT, read_json
from animate_idle import checker


def panel(entries,size,title,path):
    w,h=size;font=ImageFont.load_default(size=14)
    image=Image.new('RGB',(len(entries)*w,h+52),'#171c28');draw=ImageDraw.Draw(image)
    draw.text((6,4),title,font=font,fill='white')
    for i,(label,source) in enumerate(entries):
        sprite=Image.open(source).convert('RGBA').resize(size,Image.Resampling.LANCZOS)
        tile=checker(size);tile.alpha_composite(sprite);image.paste(tile.convert('RGB'),(i*w,28))
        draw.line((i*w,28+704*h/768,(i+1)*w,28+704*h/768),fill='#e55979')
        draw.text((i*w+5,h+30),label,font=font,fill='white')
    image.save(path)


def build():
    out=ROOT/'review/full/horu-revision';out.mkdir(exist_ok=True)
    base=ROOT/'UwU';keys=base/'Intermediate/ConsistencyRevision/Keys'
    entries=[('prior IDLE',base/'Intermediate/HoruIdleRevision/Previous/idle.png'),
             ('revised IDLE',base/'Generated/chibi/idle.png'),
             ('talking',keys/'talking/0000.png'),('working',keys/'working/0001.png'),
             ('celebrating',keys/'celebrating/0002.png'),('surprised',keys/'surprised/0002.png')]
    panel(entries,(512,768),'Native scale comparison / ground y=704',out/'scale-native.png')
    panel(entries,(128,192),'Compact scale comparison / 128 x 192',out/'scale-compact.png')
    panel([(f'talking {i:04d}',keys/f'talking/{i:04d}.png') for i in range(4)],
          (512,768),'Four talking drawings / two hands and one held cup',out/'talking-keys-native.png')
    sheet=Image.new('RGB',(600,220),'#d9dde5');draw=ImageDraw.Draw(sheet)
    for i in range(2):
        for j,(label,source) in enumerate([
            ('before',base/f'Intermediate/HoruPerformanceRepair/Previous/Characters/working/{i:04d}.png'),
            ('after',keys/f'working/{i:04d}.png')]):
            crop=Image.open(source).crop((240,212,390,284)).resize((300,144),Image.Resampling.NEAREST)
            x=i*300;y=j*100;crop.thumbnail((300,86));sheet.paste(crop,(x,y+20),crop)
            draw.text((x+5,y+3),f'working {i:04d} / {label}',fill='black')
    sheet.save(out/'working-eyes.png')
    mug_sheet=Image.new('RGB',(640,320),'#171c28');d=ImageDraw.Draw(mug_sheet)
    for i,(label,name) in enumerate([('before / conflicting base','mug-source-before-perspective.png'),
                                    ('corrected / solid curved base','mug-source.png')]):
        source=base/'Intermediate/HoruPerformanceRepair'/name
        tile=checker((320,272));tile.alpha_composite(Image.open(source).convert('RGBA').resize(tile.size,Image.Resampling.LANCZOS))
        mug_sheet.paste(tile.convert('RGB'),(i*320,26));d.text((i*320+6,6),label,fill='white')
    mug_sheet.save(out/'mug-before-after.png')
    states=read_json(base/'manifest.json')['styles']['chibi']['states']
    frames=[]
    for ms in range(0,6000,100):
        canvas=Image.new('RGB',(3*192,318),'#171c28');draw=ImageDraw.Draw(canvas)
        for col,state in enumerate(['idle','talking','working']):
            meta=states[state]['animation']['frames'];duration=sum(f['duration_ms'] for f in meta)
            time=ms%duration;elapsed=0;selected=meta[-1]
            for item in meta:
                if elapsed+item['duration_ms']>time:selected=item;break
                elapsed+=item['duration_ms']
            sprite=Image.open(base/selected['path']).convert('RGBA').resize((192,288),Image.Resampling.LANCZOS)
            tile=checker(sprite.size);tile.alpha_composite(sprite);canvas.paste(tile.convert('RGB'),(col*192,26))
            draw.text((col*192+8,7),'Horu / '+state,fill='white')
        frames.append(canvas)
    mosaic=Image.new('RGB',(576*6,318*10))
    for i,frame in enumerate(frames):mosaic.paste(frame,(i%6*576,i//6*318))
    palette=mosaic.quantize(colors=256)
    indexed=[im.quantize(palette=palette,dither=Image.Dither.NONE) for im in frames]
    indexed[0].save(out/'preview.gif',save_all=True,append_images=indexed[1:],duration=100,loop=0,disposal=1)
    print('Horu native/compact comparisons, talking keys, eye detail and GIF montage built.')


if __name__=='__main__':build()
