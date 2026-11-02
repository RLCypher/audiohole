"""Small looping review montage and every retained Horu body drawing."""
from PIL import Image, ImageDraw, ImageFont

from avatar_assets import ROOT, read_json
from animate_idle import checker


def build():
    out=ROOT/'review/full';font=ImageFont.load_default(size=12)
    records=[read_json(p) for p in sorted((ROOT/'provenance').glob('uwu-*-body-revision.json'))]
    entries=[(record['state'],i,record.get('keys_directory',f'UwU/Intermediate/BodyRevision/Keys/{record["state"]}')) for record in records for i in range(record['drawn_pose_count'])]
    sheet=Image.new('RGB',(6*128,((len(entries)+5)//6)*224+40),'#151923')
    draw=ImageDraw.Draw(sheet);draw.text((8,10),'Horu / DRAWN BODY KEYS / CANDIDATES FOR REVIEW',font=font,fill='white')
    for index,(state,key,directory) in enumerate(entries):
        image=Image.open(ROOT/directory/f'{key:04d}.png')
        tile=checker((128,192));tile.alpha_composite(image.resize(tile.size,Image.Resampling.LANCZOS))
        x,y=(index%6)*128,(index//6)*224+40;sheet.paste(tile.convert('RGB'),(x,y))
        draw.text((x+4,y+195),f'{state} {key:04d}',font=font,fill='white')
    sheet.save(out/'horu-body-keyposes.png')
    states=read_json(ROOT/'UwU/manifest.json')['styles']['chibi']['states']
    selected=['celebrating','angry','panicking'];frames=[]
    for time in range(0,3000,100):
        canvas=Image.new('RGB',(384,228),'#151923');d=ImageDraw.Draw(canvas)
        for column,state in enumerate(selected):
            meta=states[state]['animation']['frames'];elapsed=0;chosen=meta[-1]
            for entry in meta:
                if elapsed+entry['duration_ms']>time:
                    chosen=entry;break
                elapsed+=entry['duration_ms']
            image=Image.open(ROOT/'UwU'/chosen['path'])
            tile=checker((128,192));tile.alpha_composite(image.resize(tile.size,Image.Resampling.LANCZOS))
            canvas.paste(tile.convert('RGB'),(column*128,30))
            d.text((column*128+5,8),state,font=font,fill='white')
        frames.append(canvas)
    mosaic=Image.new('RGB',(384*4,228*((len(frames)+3)//4)))
    for i,frame in enumerate(frames):mosaic.paste(frame,((i%4)*384,(i//4)*228))
    palette=mosaic.quantize(colors=256)
    indexed=[frame.quantize(palette=palette,dither=Image.Dither.NONE) for frame in frames]
    indexed[0].save(out/'horu-body-revision-loop.gif',save_all=True,append_images=indexed[1:],duration=100,loop=0,disposal=1)
    with Image.open(out/'horu-body-revision-loop.gif') as check:
        duration=0
        for i in range(check.n_frames):check.seek(i);duration+=check.info['duration']
        assert duration==3000 and check.info['loop']==0
    print('Body key sheet and 3-second looping review montage built.')


if __name__=='__main__':build()
