"""Offline CLI: inventory, strict validation, explicit idle export, review artifacts."""
from __future__ import annotations

import argparse
import base64
from io import BytesIO
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

from avatar_assets import (ROOT, AssetError, Resolver, digest, inspect_png,
                           load_pack, owned_path, read_json, validate_pack)


def write_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_suffix(path.suffix + '.tmp')
    temp.write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')
    temp.replace(path)


def packs():
    registry = read_json(ROOT / 'registry.json')
    if registry.get('schema_version') != 1 or not isinstance(registry.get('packs'), dict):
        raise AssetError('unsupported registry')
    for identity, relative in registry['packs'].items():
        path = (ROOT / relative).resolve()
        if not path.is_relative_to(ROOT) or path.name != 'manifest.json' or path.parent == ROOT:
            raise AssetError('registry path must be a pack manifest inside the workspace')
        yield identity, path


def inventory():
    baseline = read_json(ROOT / 'provenance/reference-inventory.json')
    errors = []
    for entry in baseline['references']:
        p = ROOT / entry['path']
        if not p.is_file() or digest(p) != entry['sha256']:
            errors.append(f'canonical reference changed/missing: {entry["path"]}')
    for rel, expected in baseline['supplied_documents'].items():
        p = ROOT / rel
        revisions_path = ROOT/'provenance/document-revisions.json'
        revisions = read_json(revisions_path) if revisions_path.exists() else {}
        if rel in revisions:
            revision = revisions[rel]
            import hashlib
            raw = p.read_bytes()
            original = hashlib.sha256(raw[:revision['original_prefix_bytes']]).hexdigest()
            authority = ROOT/revision['authorization']
            if (original != expected or digest(p) != revision['current_sha256']
                    or digest(authority) != revision['authorization_sha256']):
                errors.append(f'authorized document revision integrity failed: {rel}')
            continue
        if not p.is_file() or digest(p) != expected:
            errors.append(f'supplied document changed/missing: {rel}')
    return {'references': baseline['references'], 'errors': errors}


def export_idle(identity, source: Path, replace=False):
    """Explicit format export, separate from the read-only validator.

    The model backend cannot enforce dimensions. Preserve its source, then crop
    transparent padding, fit without distortion, and ground at the style anchor.
    Vanilla is pasted pixel-for-pixel without cropping or resampling.
    """
    registry = dict(packs())
    if identity not in registry:
        raise AssetError('unknown personality')
    manifest = registry[identity]
    data = load_pack(manifest, identity)
    style = data['default_style']
    info = data['styles'][style]
    old = info['states']['idle']['static']
    if old['status'] == 'approved':
        raise AssetError('approved artwork requires a separately reviewed revision')
    target = owned_path(manifest.parent, f'Generated/{style}/idle.png')
    source = source.resolve()
    if not source.is_relative_to(manifest.parent.resolve()):
        raise AssetError('source must belong to the selected personality')
    if source == target:
        raise AssetError('source must be distinct from operating output')
    if target.exists() and not replace:
        raise AssetError('idle output exists; use --replace for an unapproved revision')
    with Image.open(source) as im:
        im.load()
        if im.mode != 'RGBA' or im.format != 'PNG':
            raise AssetError('source must already be RGBA PNG; no implicit background removal')
        original_size = im.size
        output = Image.new('RGBA', (512, 768))
        if identity == 'vanilla':
            canonical = manifest.parent / 'References/Vanilla Reference.png'
            if digest(source) != digest(canonical) or im.size != (512, 512):
                raise AssetError('Vanilla idle must use the exact canonical 512-square logo')
            box, size, offset = [0, 0, 512, 512], im.size, (0, 128)
            output.paste(im, offset)  # no mask: preserve all RGBA values exactly
            operation = 'pixel-preserving canvas placement; no resampling'
        else:
            # Ignore nearly invisible backend speckles when establishing framing.
            # This is an explicit export policy, not a validator-side repair.
            box = im.getchannel('A').point(lambda v: 255 if v >= 4 else 0).getbbox()
            if box is None:
                raise AssetError('source is entirely transparent')
            sprite = im.crop(box)
            ratio = min(448 / sprite.width, 640 / sprite.height)
            size = (max(1, round(sprite.width * ratio)), max(1, round(sprite.height * ratio)))
            sprite = sprite.resize(size, Image.Resampling.LANCZOS)
            offset = (info['anchor'][0] - size[0] // 2, info['anchor'][1] - size[1])
            output.paste(sprite, offset)
            operation = 'explicit alpha>=4 bounds crop, proportional Lanczos fit to 448x640, anchor placement'
        target.parent.mkdir(parents=True, exist_ok=True)
        temporary = target.with_suffix('.tmp.png')
        try:
            output.save(temporary)
            checked = inspect_png(temporary)
            temporary.replace(target)
        finally:
            temporary.unlink(missing_ok=True)
    info['states']['idle']['static'] = {
        'status': 'validated', 'path': target.relative_to(manifest.parent).as_posix(),
        'sha256': checked['sha256'], 'anchor': info['anchor']}
    write_json(manifest, data)
    provenance = {'personality_id': identity, 'style': style, 'state': 'idle',
                  'approval': 'pending human review', 'source': source.relative_to(ROOT).as_posix(),
                  'source_sha256': digest(source), 'source_size': list(original_size),
                  'operation': operation, 'crop_box': list(box), 'resampled_size': list(size),
                  'offset': list(offset), 'output_sha256': checked['sha256'],
                  'output': target.relative_to(ROOT).as_posix(), 'technical_validation': checked}
    write_json(ROOT / 'provenance' / f'{identity}-idle-export.json', provenance)
    return provenance


def review():
    """Self-contained offline snapshot, using the same validated resolver as tests."""
    sheets, snapshot, reports = [], {}, []
    image_urls = {}
    for identity, manifest in packs():
        data = load_pack(manifest, identity)
        reports.append(validate_pack(manifest, identity))
        snapshot[identity] = {}
        for style, info in data['styles'].items():
            snapshot[identity][style] = {}
            for state in [*info['states'], '__unknown__']:
                def resolved_entry(approved_only):
                    selection = Resolver(approved_only=approved_only).resolve(manifest, identity, style, state)
                    entry = {'source': selection.source, 'resolved_state': selection.state,
                             'diagnostics': list(selection.diagnostics), 'frames': [],
                             'durations_ms': list(selection.durations_ms), 'loop': selection.loop,
                             'status': info['states'].get(selection.state, {}).get(
                                 'animation' if selection.durations_ms else 'static', {}).get('status', 'unavailable')}
                    for path in selection.paths:
                        if path not in image_urls:
                            # Display-only compression keeps the self-contained
                            # inspector small enough for constrained HTML viewers.
                            encoded = BytesIO()
                            with Image.open(path) as display_image:
                                display_image.save(encoded,format='WEBP',quality=85,method=4)
                            image_urls[path] = 'data:image/webp;base64,' + base64.b64encode(encoded.getvalue()).decode()
                        entry['frames'].append(image_urls[path])
                    return entry
                entry = resolved_entry(False)
                entry['production'] = resolved_entry(True)
                snapshot[identity][style][state] = entry
            if style == data['default_style']:
                idle = snapshot[identity][style]['idle']
                path = manifest.parent / f'Generated/{style}/idle.png'
                sheets.append((identity, style, path if idle['frames'] else None, idle['status']))
    # Data URLs are deduplicated so state fallbacks don't inflate the HTML.
    urls = {}
    for styles in snapshot.values():
        for states in styles.values():
            for entry in states.values():
                for selection in [entry, entry['production']]:
                    indices = []
                    for url in selection['frames']:
                        if url not in urls:
                            urls[url] = len(urls)
                        indices.append(urls[url])
                    selection['frames'] = indices
    payload = json.dumps({'packs': snapshot, 'images': list(urls)}, separators=(',', ':')).replace('<', '\\u003c')
    template = (ROOT / 'tools/preview.html').read_text(encoding='utf-8')
    (ROOT / 'review/preview.html').write_text(template.replace('/*__SNAPSHOT__*/', payload), encoding='utf-8')
    make_sheet(sheets)
    result = {'approval': 'pending human review', 'packs': reports,
              'references': inventory(), 'visual_review_required': [
                  'character identity, accessories, anatomy, and lettering',
                  'dark/light background halos and alignment',
                  'Vanilla logo geometry and exact colors']}
    write_json(ROOT / 'review/validation.json', result)
    return result


def make_sheet(entries):
    width, height = 320, 560
    out = Image.new('RGB', (width * len(entries), height), '#151923')
    draw = ImageDraw.Draw(out)
    title = ImageFont.load_default(size=22)
    small = ImageFont.load_default(size=14)
    for index, (identity, style, path, status) in enumerate(entries):
        left = index * width
        draw.text((left + 16, 14), f'{identity} / {style} / idle', font=title, fill='white')
        draw.text((left + 16, 43), f'{status} - UNAPPROVED', font=small, fill='#f0bf65')
        for x in range(0, 288, 16):
            for y in range(0, 432, 16):
                fill = '#e8e8ed' if (x // 16 + y // 16) % 2 == 0 else '#c5c7ce'
                draw.rectangle((left + 16 + x, 76 + y, left + min(x + 31, 303), min(y + 91, 507)), fill=fill)
        if path:
            with Image.open(path) as im:
                sprite = im.resize((288, 432), Image.Resampling.LANCZOS)
                out.paste(sprite, (left + 16, 76), sprite)
        else:
            draw.text((left + 30, 270), 'Missing idle artwork', fill='black', font=small)
        draw.text((left + 16, 523), '512 x 768 RGBA | human review pending', fill='#b8c0cd', font=small)
    out.save(ROOT / 'review/idle-contact-sheet.png')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    val = sub.add_parser('validate', help='read-only validation; errors return exit 1')
    val.add_argument('--approved-only', action='store_true')
    sub.add_parser('inventory', help='check immutable supplied references and documents')
    sub.add_parser('review', help='build offline HTML preview, contact sheet, validation report')
    exp = sub.add_parser('export-idle', help='explicitly export one unapproved master')
    exp.add_argument('personality')
    exp.add_argument('source', type=Path)
    exp.add_argument('--replace', action='store_true')
    args = parser.parse_args()
    try:
        if args.command == 'validate':
            result = {'packs': [validate_pack(path, pid, args.approved_only) for pid, path in packs()],
                      'references': inventory()}
            failed = any(r['errors'] for r in result['packs']) or bool(result['references']['errors'])
        elif args.command == 'inventory':
            result = inventory()
            failed = bool(result['errors'])
        elif args.command == 'export-idle':
            result = export_idle(args.personality, args.source, args.replace)
            failed = False
        else:
            result = review()
            failed = any(p['errors'] for p in result['packs']) or bool(result['references']['errors'])
        print(json.dumps(result, indent=2))
        return int(failed)
    except (AssetError, OSError) as exc:
        print(json.dumps({'errors': [str(exc)]}, indent=2))
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
