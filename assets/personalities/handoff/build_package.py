"""Build a verified runtime/source bundle and a separate full generation archive.

Run from this Git checkout after committing source. No publishing, remote writes,
application integration, asset regeneration, or system installation occurs.
"""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
REPOSITORY = ROOT.parents[1]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path)
    parser.add_argument('--skip-checkpoint-archive', action='store_true')
    args = parser.parse_args()
    output = ROOT / 'handoff'
    source_commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=REPOSITORY,
                                            text=True).strip()
    baseline_commit = '0b41b8e'
    files = {}
    for directory in ['runtime', 'integration']:
        for file in (ROOT / directory).rglob('*'):
            relative = file.relative_to(ROOT)
            if not file.is_file() or any(part.startswith(('build', '.deps', '__pycache__', 'dist'))
                                        for part in relative.parts):
                continue
            files[relative.as_posix()] = file.read_bytes()
    for pack in ['Norm', 'UwU', 'Vanilla']:
        for directory in ['Generated', 'References']:
            for file in (ROOT / pack / directory).rglob('*'):
                if file.is_file():
                    files[file.relative_to(ROOT).as_posix()] = file.read_bytes()
        for name in ['manifest.json', 'CHARACTER.md']:
            files[f'{pack}/{name}'] = (ROOT / pack / name).read_bytes()
    for relative in ['registry.json', 'states.json', 'AVATAR_ASSETS.md',
                     'provenance/accepted-assets.json', 'provenance/idle-approval.json',
                     'provenance/generation-acceptance/checkpoint.json',
                     'provenance/generation-acceptance/README.md',
                     'provenance/authorizations/accepted-generation-engine-viewer.md']:
        files[relative] = (ROOT / relative).read_bytes()
    for file in (ROOT / 'schemas').rglob('*.json'):
        files[file.relative_to(ROOT).as_posix()] = file.read_bytes()
    files['README.md'] = (output / 'PACKAGE_README.md').read_bytes()
    binary_name = None
    if args.binary:
        binary_name = 'runtime/bin/audiohole_avatar_viewer' + ('.exe' if platform.system() == 'Windows' else '')
        files[binary_name] = args.binary.read_bytes()
    approved = json.loads((ROOT / 'provenance/accepted-assets.json').read_text())['assets']
    assert len(approved) == 796
    for name, expected in approved.items():
        assert name in files and digest(files[name]) == expected, name
    package_manifest = {'source_commit': source_commit, 'accepted_checkpoint_commit': baseline_commit,
                        'binary_platform': platform.platform() if binary_name else None,
                        'operating_pngs': len(approved), 'art_unchanged': True,
                        'files': {name: digest(data) for name, data in sorted(files.items())}}
    files['PACKAGE.json'] = (json.dumps(package_manifest, indent=2) + '\n').encode()
    runtime_zip = output / 'audiohole-avatar-runtime.zip'
    with zipfile.ZipFile(runtime_zip, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for name, data in sorted(files.items()):
            entry = zipfile.ZipInfo(name, date_time=(2026, 11, 2, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.external_attr = (0o100755 if name == binary_name else 0o100644) << 16
            archive.writestr(entry, data)
    with zipfile.ZipFile(runtime_zip) as archive:
        assert archive.testzip() is None
        for name, expected in package_manifest['files'].items():
            assert digest(archive.read(name)) == expected, name
    packages = [{'path': runtime_zip.name, 'sha256': digest(runtime_zip.read_bytes()),
                 'size_bytes': runtime_zip.stat().st_size,
                 'purpose': 'Approved operating/canonical assets, native module/viewer source, integration contract, tests and validation.'}]
    if not args.skip_checkpoint_archive:
        checkpoint = output / 'audiohole-avatar-generation-checkpoint.zip'
        subprocess.run(['git', 'archive', '--format=zip', f'--output={checkpoint}', baseline_commit,
                        'assets/personalities'], cwd=REPOSITORY, check=True)
        with zipfile.ZipFile(checkpoint) as archive:
            assert archive.testzip() is None
            for name, expected in approved.items():
                assert digest(archive.read('assets/personalities/' + name)) == expected, name
        packages.append({'path': checkpoint.name, 'sha256': digest(checkpoint.read_bytes()),
                         'size_bytes': checkpoint.stat().st_size,
                         'purpose': 'Complete accepted generation workspace including intermediate/source assets, scripts/settings and all prior previews.'})
    manifest = {'branch': 'avatar/asset-pipeline', 'source_commit': source_commit,
                'accepted_checkpoint_commit': baseline_commit, 'packages': packages,
                'scope': 'No out-of-scope application files modified; no main-GUI integration, push or merge performed.'}
    (output / 'packages.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
