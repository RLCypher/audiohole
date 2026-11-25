"""Retrieve optional, ignored research references and verify exact Git blob bytes.

Run outside audio processing. Uses ordinary HTTPS with inherited proxy/CA settings;
never accesses credentials or changes network policy. Existing valid files are reused.
Only manifest-selected paths below the research upstream directory may be written.
"""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import urllib.request

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--probe-only', action='store_true', help='Fetch just DLL/ring/defs headers')
args = parser.parse_args()
manifest = json.loads((root / 'upstream/MANIFEST.json').read_text())
probe_headers = {'spa/include/spa/utils/dll.h', 'spa/include/spa/utils/defs.h',
                 'spa/include/spa/utils/ringbuffer.h'}


def git_blob_hash(data):
    return hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()


checked = 0
for entry in manifest['entries']:
    if args.probe_only and (entry['repository'] != 'PipeWire/pipewire' or entry['path'] not in probe_headers):
        continue
    destination = (root / 'upstream' / entry['local_path']).resolve()
    if not destination.is_relative_to(root / 'upstream'):
        raise SystemExit('Manifest path escapes research reference directory')
    if destination.exists():
        data = destination.read_bytes()
    else:
        if entry['repository'] == 'PipeWire/pipewire':
            url = ('https://raw.githubusercontent.com/PipeWire/pipewire/' +
                   manifest['pipewire_commit'] + '/' + entry['path'])
        else:
            url = entry['blob_url']
        request = urllib.request.Request(url, headers={'User-Agent': 'AudioHole-PipeWire-Research'})
        with urllib.request.urlopen(request, timeout=30) as response:
            data = response.read()
        if entry['repository'] != 'PipeWire/pipewire':
            blob = json.loads(data)
            if blob.get('encoding') != 'base64':
                raise SystemExit('Unexpected GitHub blob encoding')
            data = base64.b64decode(blob['content'])
    if git_blob_hash(data) != entry['git_blob_sha']:
        raise SystemExit('Reference hash mismatch: ' + entry['local_path'])
    if not destination.exists():
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
    checked += 1
print(f'Verified {checked} ignored reference files; no downloaded source should be committed.')
