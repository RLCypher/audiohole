"""Apply the requested closed-mouth/steam revision from retained source artwork.

Canonical references are never changed. The model contributes only a registered
mouth patch; steam is isolated from the prior master into a smaller alpha layer.
"""
from collections import deque
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

from avatar_assets import ROOT, AssetError, digest, inspect_png, load_pack, read_json
from pipeline import write_json


def steam_mask(image, box):
    """Find the connected lavender steam, excluding dark mug rims and fur."""
    pixels = image.load()
    candidates = set()
    for y in range(box[1], box[3]):
        for x in range(box[0], box[2]):
            r, g, b, a = pixels[x, y]
            if a >= 4 and r >= 105 and g >= 80 and b >= 115 and b >= r*.95:
                candidates.add((x, y))
    components = []
    while candidates:
        seed = candidates.pop()
        queue, component = deque([seed]), [seed]
        while queue:
            x, y = queue.popleft()
            for point in [(x-1,y),(x+1,y),(x,y-1),(x,y+1)]:
                if point in candidates:
                    candidates.remove(point)
                    component.append(point)
                    queue.append(point)
        components.append(component)
    points = max(components, key=len)
    if len(points) < 500:
        raise AssetError('could not isolate the original steam safely')
    mask = Image.new('L', image.size)
    for point in points:
        mask.putpixel(point, 255)
    # Capture antialias fringes but never erase the black mug rim.
    expanded = mask.filter(ImageFilter.MaxFilter(7))
    for y in range(box[1], box[3]):
        for x in range(box[0], box[2]):
            r, g, b, a = pixels[x, y]
            # Low-alpha edge pixels can carry arbitrary RGB; erase those too.
            # Preserve the opaque black mug rim where the plume meets the cup.
            preserve_rim = y >= box[3]-6 and max(r,g,b) < 75 and a > 180
            if expanded.getpixel((x,y)) and not preserve_rim:
                mask.putpixel((x,y), 255)
    return mask


def prepare():
    settings = read_json(ROOT/'provenance/idle-revision.json')
    for identity, folder in [('default','Norm'), ('uwu','UwU')]:
        directory = ROOT/folder
        manifest = directory/'manifest.json'
        data = load_pack(manifest, identity)
        if data['styles']['chibi']['states']['idle']['static']['status'] == 'approved':
            raise AssetError('approved master requires a reviewed revision')
        target = directory/'Generated/chibi/idle.png'
        archive = directory/'Intermediate/idle-before-steam-revision.png'
        if not archive.exists():
            archive.write_bytes(target.read_bytes())
        original = Image.open(archive).copy()
        base = original.copy()
        provenance = {'approval':'pending human review', 'source':archive.relative_to(ROOT).as_posix(),
                      'source_sha256':digest(archive), 'changes':['subtler separate steam layer']}
        if identity == 'uwu':
            source = directory/'Intermediate/closed-mouth-source.png'
            with Image.open(source) as im:
                patch = im.resize((512,768), Image.Resampling.LANCZOS)
            mouth = settings['uwu_mouth']
            dx, dy = mouth['source_offset']
            patch = patch.transform(base.size, Image.Transform.AFFINE, (1,0,dx,0,1,dy), Image.Resampling.BICUBIC)
            mask = Image.new('L', base.size)
            ImageDraw.Draw(mask).polygon([tuple(p) for p in mouth['polygon']], fill=255)
            mask = mask.filter(ImageFilter.GaussianBlur(1.1))
            base = Image.composite(patch, base, mask)
            base.putalpha(original.getchannel('A'))
            keep = tuple(mouth['preserve_box'])
            base.paste(original.crop(keep), keep)
            mask.save(directory/'Intermediate/closed-mouth-mask.png')
            provenance.update({'mouth_source':source.relative_to(ROOT).as_posix(), 'mouth_source_sha256':digest(source),
                               'mouth_patch':mouth})
            provenance['changes'].append('closed-mouth IDLE smile')
        spec = settings[identity]
        mask = steam_mask(original, spec['original_steam_box'])
        mask.save(directory/'Intermediate/original-steam-mask.png')
        old_layer = original.copy()
        old_layer.putalpha(ImageChops.multiply(original.getchannel('A'), mask))
        plume = old_layer.crop(old_layer.getchannel('A').getbbox())
        plume = plume.resize(tuple(spec['plume_size']), Image.Resampling.LANCZOS)
        plume.putalpha(plume.getchannel('A').point(lambda v: round(v*spec['opacity'])))
        # Clearing only isolated steam preserves the original coffee cup and pose.
        base = Image.composite(Image.new('RGBA',base.size), base, mask)
        layer = Image.new('RGBA',base.size)
        bx, by = spec['base_point']
        layer.paste(plume,(bx-plume.width//2,by-plume.height))
        base.save(directory/'Intermediate/idle-clean-base.png')
        layer.save(directory/'Intermediate/idle-steam-layer.png')
        revised = Image.alpha_composite(base, layer)
        revised.save(target)
        checked = inspect_png(target)
        entry = data['styles']['chibi']['states']['idle']['static']
        entry['sha256'] = checked['sha256']
        entry['status'] = 'validated'
        write_json(manifest, data)
        provenance.update({'steam':spec, 'master_sha256':checked['sha256'],
                           'clean_base_sha256':digest(directory/'Intermediate/idle-clean-base.png'),
                           'steam_layer_sha256':digest(directory/'Intermediate/idle-steam-layer.png'),
                           'preparation_sha256':digest(Path(__file__)), 'settings_sha256':digest(ROOT/'provenance/idle-revision.json')})
        write_json(ROOT/f'provenance/{identity}-idle-revision.json',provenance)
        print(identity, 'revised master prepared')


if __name__ == '__main__':
    prepare()
