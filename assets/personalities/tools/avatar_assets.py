"""Offline avatar contract and reference loader. Never call from audio callbacks."""
from __future__ import annotations

import hashlib
import json
import struct
from dataclasses import dataclass
from pathlib import Path, PurePosixPath

from jsonschema import Draft202012Validator, FormatChecker
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SCHEMA = json.loads((ROOT / 'schemas/manifest.schema.json').read_text())
VALIDATOR = Draft202012Validator(SCHEMA, format_checker=FormatChecker())
READY = {'validated', 'approved'}


def personality_name(identity):
    """Human names are independent of the application's compatibility IDs."""
    return {'default': 'Norm', 'uwu': 'UwU', 'vanilla': 'Vanilla'}.get(identity, identity)


def review_key(identity):
    return 'norm' if identity == 'default' else identity


class AssetError(ValueError):
    pass


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_json(path: Path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise AssetError(f'duplicate JSON key: {key}')
            result[key] = value
        return result
    try:
        return json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=unique)
    except (OSError, ValueError) as exc:
        raise AssetError(f'{path.name}: {exc}') from exc


def owned_path(pack: Path, relative: str) -> Path:
    """Treat pack paths as portable POSIX paths; reject escapes on every OS."""
    rel = PurePosixPath(relative)
    if (rel.is_absolute() or '..' in rel.parts or '\\' in relative
            or ':' in relative or not relative.startswith('Generated/')):
        raise AssetError(f'asset path must remain in this pack: {relative}')
    path = pack.joinpath(*rel.parts).resolve()
    if not path.is_relative_to((pack / 'Generated').resolve()):
        raise AssetError(f'asset path escapes Generated: {relative}')
    # A symlinked Generated directory must not point into a sibling pack either.
    if not path.is_relative_to(pack.resolve()):
        raise AssetError(f'asset path escapes personality: {relative}')
    return path


def inspect_png(path: Path) -> dict:
    """Read, never repair. The raw IHDR prevents silent palette/16-bit conversion."""
    try:
        with path.open('rb') as stream:
            header = stream.read(33)
        if (len(header) != 33 or header[:8] != b'\x89PNG\r\n\x1a\n'
                or header[12:16] != b'IHDR'):
            raise AssetError('not a readable PNG header')
        width, height, depth, color = struct.unpack('>IIBB', header[16:26])
        if (width, height, depth, color) != (512, 768, 8, 6):
            raise AssetError(f'expected 512x768 8-bit RGBA PNG; got {width}x{height}, depth={depth}, color={color}')
        with Image.open(path) as check:
            check.verify()
        with Image.open(path) as im:
            im.load()
            if im.format != 'PNG' or im.mode != 'RGBA' or getattr(im, 'n_frames', 1) != 1:
                raise AssetError('expected a single RGBA PNG frame')
            alpha = im.getchannel('A')
            histogram = alpha.histogram()
            box = alpha.getbbox()
            if histogram[0] == 0:
                raise AssetError('no fully transparent background pixels')
            if box is None or max(alpha.getextrema()) < 240:
                raise AssetError('empty or entirely translucent artwork')
            # Transparent pixels inside a hole do not make an opaque background valid.
            corners = [alpha.getpixel(p) for p in [(0, 0), (511, 0), (0, 767), (511, 767)]]
            if any(corners):
                raise AssetError('canvas corners must be fully transparent')
            warnings = []
            if histogram[255] == 0:
                warnings.append('no fully opaque foreground pixels: inspect alpha fidelity')
            if box[0] == 0 or box[1] == 0 or box[2] == 512 or box[3] == 768:
                warnings.append('artwork touches the canvas edge: inspect clipping or opaque borders')
            if histogram[0] < width * height * 0.03:
                warnings.append('very little transparent padding: inspect background')
            if sum(histogram[1:255]) > width * height * 0.15:
                warnings.append('large semitransparent region: inspect halos or background remnants')
            return {'size': [width, height], 'mode': im.mode, 'bit_depth': depth,
                    'bbox': list(box), 'transparent_pixels': histogram[0],
                    'sha256': digest(path), 'warnings': warnings}
    except (OSError, SyntaxError, ValueError, EOFError, struct.error, Image.DecompressionBombError) as exc:
        if isinstance(exc, AssetError):
            raise
        raise AssetError(f'{path.name}: {exc}') from exc


def load_pack(manifest: Path, expected_personality: str | None = None) -> dict:
    data = read_json(manifest)
    problems = sorted(VALIDATOR.iter_errors(data), key=lambda e: str(list(e.path)))
    if problems:
        raise AssetError('; '.join(f'{"/".join(map(str, e.path))}: {e.message}' for e in problems))
    if expected_personality is not None and data['personality_id'] != expected_personality:
        raise AssetError('manifest personality does not match selected personality')
    default = data['default_style']
    if default not in data['styles'] or not data['styles'][default]['enabled']:
        raise AssetError('default_style must name an enabled style')
    for style, info in data['styles'].items():
        x, y = info['anchor']
        if not (0 <= x < 512 and 0 <= y < 768):
            raise AssetError(f'{style}: anchor outside canvas')
        for state, entry in info['states'].items():
            if 'fallback' in entry and (state == 'idle' or entry['fallback'] != 'idle'):
                raise AssetError(f'{style}/{state}: v1 fallback must resolve directly to idle')
            for kind in ['static', 'animation']:
                asset = entry[kind]
                frames = [asset] if kind == 'static' and 'path' in asset else asset.get('frames', [])
                for index, frame in enumerate(frames):
                    expected = (f'Generated/{style}/{state}.png' if kind == 'static'
                                else f'Generated/{style}/{state}/{index:04d}.png')
                    if frame['path'] != expected:
                        raise AssetError(f'{style}/{state}: noncanonical or mismatched {kind} path')
                    owned_path(manifest.parent, frame['path'])
                    if frame['anchor'] != info['anchor']:
                        raise AssetError(f'{style}/{state}: anchor differs from master')
    return data


def checked_frames(pack: Path, asset: dict, approved_only: bool = False) -> list[tuple[Path, dict]]:
    if asset['status'] not in ({'approved'} if approved_only else READY):
        raise AssetError(f'asset status is {asset["status"]}')
    frames = asset.get('frames', [asset])
    result = []
    for frame in frames:
        path = owned_path(pack, frame['path'])
        info = inspect_png(path)
        if info['sha256'] != frame['sha256']:
            raise AssetError(f'{frame["path"]}: content changed since validation')
        result.append((path, info))
    return result


def validate_pack(manifest: Path, expected_personality=None, approved_only=False) -> dict:
    report = {'manifest': str(manifest), 'errors': [], 'warnings': [], 'assets': []}
    try:
        data = load_pack(manifest, expected_personality)
    except AssetError as exc:
        report['errors'].append(str(exc))
        return report
    for style, info in data['styles'].items():
        for state, entry in info['states'].items():
            for kind in ['static', 'animation']:
                asset = entry[kind]
                label = f'{style}/{state}/{kind}'
                if asset['status'] in {'not_generated', 'unavailable'}:
                    continue
                try:
                    # Generated outputs can be inspected, but are not runtime-ready.
                    trial = dict(asset)
                    if trial['status'] == 'generated' and not approved_only:
                        trial['status'] = 'validated'
                        report['warnings'].append(f'{label}: generated, not marked validated')
                    frames = checked_frames(manifest.parent, trial, approved_only)
                    previous = None
                    for path, image in frames:
                        report['assets'].append({'path': str(path.relative_to(manifest.parent)), **image})
                        report['warnings'].extend(f'{label}: {w}' for w in image['warnings'])
                        box = image['bbox']
                        if previous:
                            center_shift = max(abs((box[i] + box[i+2] - previous[i] - previous[i+2]) / 2) for i in [0, 1])
                            if center_shift > 24:
                                report['warnings'].append(f'{label}: bounding-box displacement exceeds 24px; inspect motion')
                            old_area = (previous[2]-previous[0])*(previous[3]-previous[1])
                            area = (box[2]-box[0])*(box[3]-box[1])
                            if abs(area / old_area - 1) > 0.2:
                                report['warnings'].append(f'{label}: occupied area changed over 20%; inspect scale')
                        previous = box
                except AssetError as exc:
                    report['errors'].append(f'{label}: {exc}')
        if info['enabled']:
            # Idle can be a static or fully valid animation, and is mandatory per style.
            idle_errors = []
            for kind in ['animation','static']:
                asset=info['states']['idle'][kind]
                if isinstance(asset, dict):
                    try:
                        checked_frames(manifest.parent, asset, approved_only)
                        break
                    except AssetError as exc:
                        idle_errors.append(str(exc))
            else:
                report['errors'].append(f'{style}: missing/invalid mandatory idle: {"; ".join(idle_errors)}')
    return report


@dataclass(frozen=True)
class Selection:
    personality: str
    style: str
    state: str
    source: str
    paths: tuple[Path, ...] = ()
    durations_ms: tuple[int, ...] = ()
    loop: bool = False
    diagnostics: tuple[str, ...] = ()


class Resolver:
    """Reference fallback semantics for later C++ integration, with no engine access.

    Production defaults to approved assets only. Review tools must explicitly opt
    into technically validated, unapproved candidates. All frames are preflighted.
    """

    def __init__(self, approved_only=True):
        self.approved_only = approved_only
        self.key = None
        self.last = None

    def resolve(self, manifest: Path, personality: str, style: str, state: str) -> Selection:
        key = (manifest.resolve(), personality, style)
        if self.key != key:
            self.last = None
            self.key = key
        diagnostics = []
        try:
            data = load_pack(manifest, personality)
            if style not in data['styles'] or not data['styles'][style]['enabled']:
                raise AssetError('style unavailable or disabled')
            states = data['styles'][style]['states']
        except AssetError as exc:
            # A structurally invalid manifest cannot establish ownership of a cache.
            self.last = None
            return Selection(personality, style, state, 'hide', diagnostics=(str(exc),))
        candidates = [(state, 'animation'), (state, 'static')]
        if state != 'idle':
            candidates += [('idle', 'animation'), ('idle', 'static')]
        for target, kind in candidates:
            if target not in states:
                diagnostics.append(f'{target}: unknown state')
                continue
            asset = states[target][kind]
            try:
                frames = checked_frames(manifest.parent, asset, self.approved_only)
            except AssetError as exc:
                diagnostics.append(f'{target}/{kind}: {exc}')
                continue
            paths = tuple(p for p, _ in frames)
            durations = tuple(f['duration_ms'] for f in asset['frames']) if kind == 'animation' else ()
            self.last = paths[0], frames[0][1]['sha256']
            return Selection(personality, style, target,
                             kind if target == state else f'idle_{kind}', paths,
                             durations, asset.get('loop', False), tuple(diagnostics))
        if self.last:
            try:
                path, sha = self.last
                # Revalidate cached ownership/content, even after files are deleted.
                owned_path(manifest.parent, path.relative_to(manifest.parent.resolve()).as_posix())
                if inspect_png(path)['sha256'] == sha:
                    return Selection(personality, style, state, 'last_valid_frame', (path,), diagnostics=tuple(diagnostics))
            except (AssetError, ValueError):
                self.last = None
        return Selection(personality, style, state, 'hide', diagnostics=tuple(diagnostics))

    def remember_presented(self, selection: Selection, index: int = 0):
        """A renderer reports the last frame actually presented during playback."""
        if self.key is None or (selection.personality, selection.style) != self.key[1:]:
            raise AssetError('cannot remember a frame from a different personality/style')
        if not 0 <= index < len(selection.paths):
            raise AssetError('presented frame index outside selection')
        path = selection.paths[index]
        pack = self.key[0].parent
        try:
            owned_path(pack, path.relative_to(pack).as_posix())
        except ValueError as exc:
            raise AssetError('presented frame outside selected pack') from exc
        self.last = path, inspect_png(path)['sha256']
