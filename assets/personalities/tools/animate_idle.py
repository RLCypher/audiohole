"""Reproducible, limited-motion IDLE compositor; no other state is generated.

Uses retained image-backend eyelid edits and anchored mesh deformations of the
existing masters. This is a 2D cutout workflow, not generative video or a new pose.
"""
from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

from avatar_assets import ROOT, AssetError, digest, inspect_png, load_pack, read_json, validate_pack
from pipeline import packs, write_json
from avatar_assets import personality_name, review_key


def envelope(value, low, high, feather):
    if value <= low or value >= high:
        return 0.0
    t = min(1.0, (value-low)/feather, (high-value)/feather)
    return t*t*(3-2*t)


def ellipse(x, y, cx, cy, rx, ry):
    radius = math.sqrt(((x-cx)/rx)**2 + ((y-cy)/ry)**2)
    t = max(0.0, min(1.0, (1-radius)*3))
    return t*t*(3-2*t)


def mesh_warp(image, displacement, cell=8, resample=Image.Resampling.BICUBIC):
    mesh = []
    moving = Image.new('L', image.size)
    moving_draw = ImageDraw.Draw(moving)
    for top in range(0, 768, cell):
        for left in range(0, 512, cell):
            right, bottom = min(left+cell, 512), min(top+cell, 768)
            quad = []
            changed = False
            for x, y in [(left, top), (left, bottom), (right, bottom), (right, top)]:
                dx, dy = displacement(x, y)
                quad += [x-dx, y-dy]
                changed |= abs(dx)+abs(dy) > 1e-8
            mesh.append(((left, top, right, bottom), tuple(quad)))
            if changed:
                moving_draw.rectangle((left, top, right-1, bottom-1), fill=255)
    warped = image.transform((512, 768), Image.Transform.MESH, mesh, resample=resample)
    # Pillow premultiplies RGBA during resampling. Bypass that conversion for
    # stationary cells so their original RGB/alpha do not acquire rounding drift.
    return Image.composite(warped, image, moving)


def blink_keyframe(folder, master, settings):
    source = ROOT / folder / 'Intermediate/blink-source.png'
    with Image.open(source) as raw:
        if raw.mode != 'RGBA':
            raise AssetError('blink backend source must be RGBA')
        # Backend output is retained; this resize only registers its eye regions.
        normalized = raw.resize((512, 768), Image.Resampling.LANCZOS)
    result = master.copy()
    union = Image.new('L', master.size)
    for patch in settings['eye_patches']:
        mask = Image.new('L', master.size)
        ImageDraw.Draw(mask).polygon([tuple(p) for p in patch['polygon']], fill=255)
        mask = mask.filter(ImageFilter.GaussianBlur(1.3))
        dx, dy = patch.get('source_offset', [0, 0])
        aligned = normalized.transform(master.size, Image.Transform.AFFINE,
                                       (1, 0, dx, 0, 1, dy), Image.Resampling.BICUBIC)
        # Restrict the model's changes to the eye surface. Alpha and all clothing,
        # hair, body, mouth, equipment, and silhouette come from the master.
        result = Image.composite(aligned, result, mask)
        union = ImageChops.lighter(union, mask)
    result.putalpha(master.getchannel('A'))
    target = ROOT / folder / 'Intermediate/idle-blink-keyframe.png'
    result.save(target)
    union.save(ROOT / folder / 'Intermediate/idle-blink-mask.png')
    return result, {'source': source.relative_to(ROOT).as_posix(), 'source_sha256': digest(source),
                    'keyframe_sha256': digest(target), 'eye_mask_bbox': list(union.getbbox()),
                    'normalized_registration_size': [512, 768], 'patches': settings['eye_patches']}


def character_frame(image, folder, breath, phase, settings):
    def motion(x, y):
        if y >= settings['ground_lock_y']:
            return 0.0, 0.0
        if x <= settings['case_lock_x'] and y >= settings['case_lock_y']:
            return 0.0, 0.0
        body = envelope(x, *settings['body_x'], 25) * envelope(y, *settings['body_y'], 40)
        dx = .3 * breath * body * (x-260)/140
        dy = -settings['breath_px'] * breath * body
        if folder == 'Norm':
            coffee = ellipse(x, y, 399, 295, 53, 108)
            dx += .4 * math.sin(phase) * coffee
            dy -= .6 * breath * coffee
        else:
            tail = ellipse(x, y, 410, 518, 66, 91)
            dx += 3.5 * math.sin(phase) * tail
            dy += .6 * math.sin(phase) * tail
            for cx, cy, sign in [(181, 112, -1), (335, 128, 1)]:
                ear = ellipse(x, y, cx, cy, 46, 61)
                dx += sign * 1.2 * math.sin(phase) * ear
                dy -= .7 * breath * ear
        return dx, dy
    return mesh_warp(image, motion)


def steam_frame(layer, phase, breath, folder):
    """Independent plume rise, curl and fade; cup and character are not warped."""
    wind = math.sin(phase)
    rise = 4 * (1-math.cos(phase))
    cup_y = -.6*breath if folder == 'Norm' else -.8*breath
    def motion(x,y):
        curl = 1.3*math.sin((y-220)/12 + phase)*wind
        return 2.5*wind+curl, -rise+cup_y
    frame = mesh_warp(layer, motion, cell=4)
    opacity = .8+.2*math.cos(phase)
    frame.putalpha(frame.getchannel('A').point(lambda value: round(value*opacity)))
    return frame


def compose_character_frame(source, clean_base, steam_layer, folder, breath, phase, settings):
    steam_support = steam_layer.getchannel('A').point(lambda value: 255 if value else 0)
    source = Image.composite(clean_base, source, steam_support)
    frame = character_frame(source, folder, breath, phase, settings)
    return Image.alpha_composite(frame,steam_frame(steam_layer,phase,breath,folder))


def wormhole_mask():
    mask = Image.new('L', (512, 768))
    ImageDraw.Draw(mask).ellipse((126, 471, 386, 527), fill=255)
    return mask


def vanilla_frame(master, phase):
    mask = wormhole_mask()
    def motion(x, y):
        weight = ellipse(x, y, 256, 499, 130, 28)
        return 1.7*math.sin(phase)*weight, 1.2*(1-math.cos(phase))*weight
    moved = mesh_warp(master, motion, cell=4, resample=Image.Resampling.BILINEAR)
    result = Image.composite(moved, master, mask)
    # Interior is opaque in the canonical logo. Keep the entire alpha silhouette
    # and every exterior RGBA pixel untouched, including hidden background RGB.
    result.putalpha(master.getchannel('A'))
    return result


def checker(size):
    im = Image.new('RGBA', size, '#e4e5eb')
    draw = ImageDraw.Draw(im)
    for y in range(0, size[1], 16):
        for x in range(0, size[0], 16):
            if (x//16+y//16) % 2:
                draw.rectangle((x, y, x+15, y+15), fill='#c5c7ce')
    return im


def previews(identity, frames, durations):
    label=personality_name(identity);identity=review_key(identity)
    out = ROOT / 'review/idle-sequences'
    out.mkdir(parents=True, exist_ok=True)
    for tag, size in [('native', (512, 768)), ('compact', (128, 192))]:
        images = [im.copy() if im.size == size else im.resize(size, Image.Resampling.LANCZOS) for im in frames]
        images[0].save(out/f'{identity}-{tag}.png', save_all=True, append_images=images[1:],
                       duration=durations, loop=0, disposal=0, blend=0)
        # This is an animated review PNG, never an operating frame.
        with Image.open(out/f'{identity}-{tag}.png') as check:
            assert check.n_frames == len(frames)
            assert [check.seek(i) or round(check.info['duration']) for i in range(check.n_frames)] == durations
    tile_w, tile_h = 192, 332
    sheet = Image.new('RGB', (tile_w*6, tile_h*2+46), '#141923')
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default(size=14)
    draw.text((12, 10), f'{label} / IDLE / 12 frames / 5.0 s / UNAPPROVED', fill='white', font=font)
    for i, im in enumerate(frames):
        x, y = (i % 6)*tile_w, (i//6)*tile_h+46
        canvas = checker((192, 288))
        canvas.alpha_composite(im.resize(canvas.size, Image.Resampling.LANCZOS))
        sheet.paste(canvas.convert('RGB'), (x, y))
        draw.text((x+8, y+293), f'{i:04d} | {durations[i]} ms', fill='white', font=font)
    sheet.save(out/f'{identity}-all-frames.png')
    # Portable fallback for viewers that cannot display APNG or HTML visualizations.
    flat = []
    for image in frames:
        canvas = checker((256,384))
        canvas.alpha_composite(image.resize(canvas.size, Image.Resampling.LANCZOS))
        flat.append(canvas.convert('RGB'))
    palette_source = Image.new('RGB',(256*4,384*3))
    for i, image in enumerate(flat):
        palette_source.paste(image,((i%4)*256,(i//4)*384))
    palette = palette_source.quantize(colors=256)
    gif = [image.quantize(palette=palette,dither=Image.Dither.NONE) for image in flat]
    gif[0].save(out/f'{identity}-review.gif',save_all=True,append_images=gif[1:],
                duration=durations,loop=0,optimize=True,disposal=1)
    with Image.open(out/f'{identity}-review.gif') as check:
        if check.n_frames != len(frames):
            raise AssetError('GIF encoder merged authored frames; inspect timing')
        for index,expected in enumerate(gif):
            check.seek(index)
            if check.convert('RGB').tobytes() != expected.convert('RGB').tobytes():
                raise AssetError('GIF delta encoding changed a displayed frame')


def generate():
    config = read_json(ROOT/'provenance/idle-motion.json')
    rigs = read_json(ROOT/'provenance/idle-rigs.json')
    durations = [ticks*config['timebase_ms'] for ticks in config['exposure_ticks']]
    if len(durations) != 12 or sum(durations) != 5000:
        raise AssetError('initial IDLE contract requires 12 frames and a five-second loop')
    summary = {'approval': 'pending human review', 'timebase_ms': config['timebase_ms'],
               'frame_durations_ms': durations, 'loop_duration_ms': sum(durations), 'packs': {}}
    for identity, manifest in packs():
        data = load_pack(manifest, identity)
        style = data['default_style']
        info = data['styles'][style]
        if info['states']['idle']['animation']['status'] == 'approved':
            raise AssetError('refusing to replace approved animation')
        folder = manifest.parent.name
        master_path = manifest.parent/f'Generated/{style}/idle.png'
        inspect_png(master_path)
        with Image.open(master_path) as im:
            master = im.copy()
        blink, provenance = (None, {}) if identity == 'vanilla' else blink_keyframe(folder, master, rigs[identity])
        if identity != 'vanilla':
            clean_base = Image.open(manifest.parent/'Intermediate/idle-clean-base.png').copy()
            steam_layer = Image.open(manifest.parent/'Intermediate/idle-steam-layer.png').copy()
        out = manifest.parent/f'Generated/{style}/idle'
        out.mkdir(parents=True, exist_ok=True)
        frames, metadata = [], []
        for i, breath in enumerate(config['breath']):
            phase = i/11*2*math.pi
            if i in (0, 11):
                frame = master.copy()
            elif identity == 'vanilla':
                frame = vanilla_frame(master, phase)
            else:
                source = blink if i in config['blink_indices'] else master
                frame = compose_character_frame(source,clean_base,steam_layer,folder,breath,phase,rigs[identity])
            target = out/f'{i:04d}.png'
            frame.save(target)
            checked = inspect_png(target)
            frames.append(frame)
            metadata.append({'path': target.relative_to(manifest.parent).as_posix(),
                             'sha256': checked['sha256'], 'anchor': info['anchor'], 'duration_ms': durations[i]})
        info['states']['idle']['animation'] = {'status': 'validated', 'frames': metadata,
                                             'loop': True, 'priority': 0, 'interruptible': True,
                                             'cooldown_ms': 0, 'transition': 'cut'}
        write_json(manifest, data)
        previews(identity, frames, durations)
        provenance.update({'personality_id': identity, 'method': 'fixed-master 2D limited-motion compositor',
                           'approval': 'pending human review', 'master_sha256': digest(master_path),
                           'motion_config_sha256': digest(ROOT/'provenance/idle-motion.json'),
                           'rig_config_sha256': digest(ROOT/'provenance/idle-rigs.json'),
                           'compositor_sha256': digest(Path(__file__)), 'frames': metadata})
        if identity != 'vanilla':
            provenance['revision_provenance_sha256'] = digest(ROOT/f'provenance/{identity}-idle-revision.json')
            provenance['steam_motion'] = {'method':'separate alpha layer; curl, rise, fade',
                                         'max_rise_px':8,'max_wind_and_curl_px':3.8,
                                         'opacity_multiplier_range':[.6,1]}
        if identity == 'vanilla':
            mask_path = manifest.parent/'Intermediate/wormhole-motion-mask.png'
            mask_path.parent.mkdir(exist_ok=True)
            wormhole_mask().save(mask_path)
            provenance['protected_area'] = 'Every RGBA pixel outside the binary ellipse mask is exact canonical master data; whole-image alpha unchanged.'
        write_json(ROOT/f'provenance/{identity}-idle-animation.json', provenance)
        summary['packs'][identity] = validate_pack(manifest, identity)
    write_json(ROOT/'review/idle-sequences/generation-validation.json', summary)
    return summary


if __name__ == '__main__':
    result = generate()
    for identity, report in result['packs'].items():
        print(identity, 'frames=12', 'errors=', report['errors'], 'warnings=', len(report['warnings']))
    raise SystemExit(int(any(p['errors'] for p in result['packs'].values())))
