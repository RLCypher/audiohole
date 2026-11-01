"""Read-only audit of the three initial IDLE masters, frames and animated previews."""
from __future__ import annotations

from PIL import Image, ImageChops, ImageStat

from avatar_assets import ROOT, digest, inspect_png, load_pack, read_json, validate_pack
from pipeline import inventory, packs, write_json


def delta(a, b, mask=None):
    diff = ImageChops.difference(a, b)
    if mask is not None:
        diff = Image.merge('RGBA', [ImageChops.multiply(c, mask) for c in diff.split()])
    return max(extreme[1] for extreme in diff.getextrema())


def audit():
    reference_check = inventory()
    report = {'approval': 'pending human review', 'errors': list(reference_check['errors']),
              'operating_asset_count': 0, 'native_size': [512, 768], 'compact_preview_size': [128, 192],
              'timing': {'timebase_ms': 100, 'authored_frames': 12, 'loop_duration_ms': 5000,
                         'note': 'Integer exposure holds on a 10 Hz timebase; not 10 unique drawings per second.'},
              'packs': {}}
    durations = [n*100 for n in read_json(ROOT/'provenance/idle-motion.json')['exposure_ticks']]
    report['timing']['frame_durations_ms'] = durations
    for identity, manifest in packs():
        data = load_pack(manifest, identity)
        style = data['default_style']
        idle = data['styles'][style]['states']['idle']
        validation = validate_pack(manifest, identity)
        errors = list(validation['errors'])
        master_path = manifest.parent/idle['static']['path']
        master = Image.open(master_path).copy()
        frame_meta = idle['animation']['frames']
        paths = [manifest.parent/f['path'] for f in frame_meta]
        frames = [Image.open(p).copy() for p in paths]
        report['operating_asset_count'] += 1+len(frames)
        frame_info = [inspect_png(p) for p in paths]
        centers = [((i['bbox'][0]+i['bbox'][2])/2, (i['bbox'][1]+i['bbox'][3])/2) for i in frame_info]
        shifts = [max(abs(b[j]-a[j]) for j in [0, 1]) for a, b in zip(centers, centers[1:]+centers[:1])]
        master_match = delta(master, frames[0])
        seam = delta(frames[0], frames[-1])
        ground_box = (0, 616, 512, 768)
        ground = max(delta(master.crop(ground_box), f.crop(ground_box)) for f in frames)
        case_box = (64, 384, 264, 600) if identity == 'default' else (48, 440, 216, 616)
        case = None if identity == 'vanilla' else max(delta(master.crop(case_box), f.crop(case_box)) for f in frames)
        metrics = {'master_equals_first_frame_max_rgba_delta': master_match,
                   'loop_first_last_max_rgba_delta': seam,
                   'max_adjacent_bbox_center_shift_px': max(shifts),
                   'ground_region_max_rgba_delta': ground,
                   'protected_case_region_max_rgba_delta': case,
                   'alpha_bbox_per_frame': [i['bbox'] for i in frame_info],
                   'unique_frame_hashes': len(set(digest(p) for p in paths))}
        if identity == 'vanilla':
            mask = Image.open(manifest.parent/'Intermediate/wormhole-motion-mask.png')
            exterior = ImageChops.invert(mask)
            metrics['outside_wormhole_max_rgba_delta'] = max(delta(master, f, exterior) for f in frames)
            metrics['whole_image_alpha_max_delta'] = max(ImageChops.difference(master.getchannel('A'), f.getchannel('A')).getextrema()[1] for f in frames)
            if metrics['outside_wormhole_max_rgba_delta'] or metrics['whole_image_alpha_max_delta']:
                errors.append('Vanilla protected logo pixels or alpha changed')
        else:
            mask = Image.open(manifest.parent/'Intermediate/idle-blink-mask.png')
            blink = Image.open(manifest.parent/'Intermediate/idle-blink-keyframe.png')
            # Feathered mask pixels belong to the edit. Only zero-weight pixels
            # establish the protected exterior of the eye patch.
            exterior = ImageChops.invert(mask.point(lambda value: 255 if value else 0))
            metrics['blink_keyframe_outside_eye_mask_max_rgba_delta'] = delta(master, blink, exterior)
            if metrics['blink_keyframe_outside_eye_mask_max_rgba_delta']:
                errors.append('generated blink changed pixels outside its eye mask')
        if len(frames) != 12 or [f['duration_ms'] for f in frame_meta] != durations or sum(durations) != 5000:
            errors.append('frame count or common timing mismatch')
        if any(f['anchor'] != data['styles'][style]['anchor'] for f in frame_meta):
            errors.append('frame anchors changed')
        if master_match or seam or ground or case:
            errors.append('neutral, loop seam, ground or protected case is not stable')
        if max(shifts) > 4:
            errors.append('unexpected bounding-box displacement over four native pixels')
        # Quantify compact visibility; this is evidence, not an artistic score.
        compact = [f.resize((128, 192), Image.Resampling.LANCZOS) for f in frames]
        composited = []
        for frame in compact:
            background = Image.new('RGBA', frame.size, '#dfe2e8')
            background.alpha_composite(frame)
            composited.append(background.convert('RGB'))
        metrics['compact_mean_rgb_change_vs_master_per_frame'] = [
            round(sum(ImageStat.Stat(ImageChops.difference(composited[0], frame)).mean)/3, 5)
            for frame in composited]
        preview_checks = {}
        for tag, size in [('native', (512, 768)), ('compact', (128, 192))]:
            path = ROOT/f'review/idle-sequences/{identity}-{tag}.png'
            with Image.open(path) as apng:
                actual = []
                for index in range(apng.n_frames):
                    apng.seek(index)
                    actual.append(round(apng.info['duration']))
                    expected = frames[index] if tag == 'native' else compact[index]
                    if delta(apng.convert('RGBA'), expected):
                        errors.append(f'{tag} APNG frame {index} differs from operating sequence')
                if apng.size != size or apng.n_frames != 12 or actual != durations or apng.info.get('loop') != 0:
                    errors.append(f'{tag} APNG dimensions, frame count, timing or loop mismatch')
                preview_checks[tag] = {'size': list(apng.size), 'frames': apng.n_frames,
                                       'duration_ms': sum(actual), 'rgba_frames_match': True,
                                       'sha256': digest(path)}
        for states in data['styles'].values():
            for state, entry in states['states'].items():
                if state != 'idle' and any(entry[k]['status'] not in {'not_generated', 'unavailable'} for k in ['static', 'animation']):
                    errors.append('non-IDLE asset generated outside this milestone')
        report['packs'][identity] = {'errors': errors, 'warnings': sorted(set(validation['warnings'])),
                                     'frame_count': len(frames), 'alignment': metrics, 'previews': preview_checks,
                                     'master_sha256': digest(master_path), 'manifest_integrity': 'pass' if not validation['errors'] else 'fail'}
        report['errors'].extend(f'{identity}: {error}' for error in errors)
    return report


if __name__ == '__main__':
    result = audit()
    write_json(ROOT/'review/idle-sequences/validation.json', result)
    print('Operating assets:', result['operating_asset_count'], 'errors:', result['errors'])
    for pid, pack in result['packs'].items():
        print(pid, pack['alignment'])
    raise SystemExit(bool(result['errors']))
