"""Check publication scope, instruction provenance, references, and research links.

Read-only; no Git index or branch mutations. Optional downloaded source is ignored
for publication and checked whenever present. --require-reference-snapshots requires
the complete examined 49-file cache; ordinary fresh checkouts report absent caches.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--require-reference-snapshots', action='store_true')
parser.add_argument('--publication-files', action='store_true', help='Print JSON allowlist after checking')
args = parser.parse_args()

instruction_bytes = (root / 'AGENTS.md').read_bytes()
assert hashlib.sha256(instruction_bytes[:18840]).hexdigest() == (
    '22ff9505a242ffb8db3a4e66487c4f4fa228a0b3835e615434deeb8963186c51')
assert b'## Additional Standing Instructions' in instruction_bytes[18840:]
assert b'GitHub publication is authorized.' in instruction_bytes[18840:]
manifest = json.loads((root / 'upstream/MANIFEST.json').read_text())
assert manifest['pipewire_commit'] == 'fd60e04525f3a04d90bf50085222e0cc9139b4a4'
assert len(manifest['entries']) == 49
reference_count = 0
for entry in manifest['entries']:
    path = (root / 'upstream' / entry['local_path']).resolve()
    assert path.is_relative_to(root / 'upstream')
    if not path.exists():
        assert not args.require_reference_snapshots, str(path)
        continue
    data = path.read_bytes()
    blob_hash = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
    assert blob_hash == entry['git_blob_sha'], str(path)
    reference_count += 1

publication_files = [p.relative_to(root).as_posix() for p in sorted(root.glob('*.md'))]
publication_files += ['.gitignore', 'upstream/MANIFEST.json']
publication_files += ['experiments/' + name for name in (
    'dll_probe.c', 'dll_results.csv', 'dll_results_sanitized.csv', 'numerics.py',
    'numerics.json', 'fetch_references.py', 'verify_research.py')]
allowed = set(publication_files)
for name in publication_files:
    path = root / name
    assert path.is_file() and not path.is_symlink(), name
    assert path.stat().st_size < 100000, name
    assert b'\0' not in path.read_bytes(), name
    assert not name.startswith(('work/', 'upstream/pipewire/', 'upstream/windows/'))

link_count = 0
for name in publication_files:
    if not name.endswith('.md') or name == 'AGENTS.md':
        continue
    document = root / name
    for target in re.findall(r'\[[^\]]*\]\(([^)]+)\)', document.read_text()):
        if '://' in target or target.startswith('mailto:'):
            continue
        path_part, _, anchor = target.partition('#')
        local = (document.parent / path_part).resolve() if path_part else document
        assert local.is_relative_to(root), (name, target)
        assert local.relative_to(root).as_posix() in allowed, (name, target)
        if anchor and local.name == 'SOURCE_REFERENCES.md':
            assert f'id="{anchor}"' in local.read_text(), (name, target)
        link_count += 1
assert (root / 'experiments/dll_results.csv').read_bytes() == (
    root / 'experiments/dll_results_sanitized.csv').read_bytes()
assert len((root / 'experiments/dll_results.csv').read_text().splitlines()) == 10
if args.publication_files:
    print(json.dumps(publication_files, indent=2))
else:
    print(f'PASS: original assignment prefix and publication policy; {reference_count}/49 '
          f'cached blobs checked; {len(publication_files)} text-only publication files; '
          f'{link_count} published local links; nine consistent model scenarios.')
