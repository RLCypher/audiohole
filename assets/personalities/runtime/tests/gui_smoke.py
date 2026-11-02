"""Run the real wx viewer, inspect its checks, and retain a native screenshot.

Linux uses a local Xvfb if DISPLAY is absent. Windows uses its native desktop.
This is a GUI integration check of the standalone host, not the main AudioHole UI.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--viewer', type=Path, required=True)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--xvfb', default=shutil.which('Xvfb'))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    server = None
    with tempfile.TemporaryDirectory(prefix='audiohole-viewer-') as temporary:
        with open(args.output / 'gui-process.log', 'w') as log:
            try:
                if os.name != 'nt' and not environment.get('DISPLAY'):
                    if not args.xvfb:
                        raise RuntimeError('Provide Xvfb or an existing DISPLAY for the real GUI test.')
                    number = next(n for n in range(95, 120)
                                  if not Path(f'/tmp/.X11-unix/X{n}').exists())
                    environment['DISPLAY'] = f':{number}'
                    server = subprocess.Popen([args.xvfb, f':{number}', '-screen', '0',
                                               '1440x1100x24', '-ac', '-nolisten', 'tcp'],
                                              stdout=log, stderr=log, env=environment)
                    for _ in range(100):
                        if Path(f'/tmp/.X11-unix/X{number}').exists():
                            break
                        if server.poll() is not None:
                            raise RuntimeError('Xvfb failed; see gui-process.log.')
                        time.sleep(.02)
                subprocess.run([str(args.viewer.resolve()), '--root', str(args.root.resolve()),
                                '--smoke-out', str(args.output.resolve())], env=environment,
                               stdout=log, stderr=log, timeout=60, check=True)
                report = json.loads((args.output / 'viewer-smoke.json').read_text())
                assert report['success'], report
                assert len(report['checks']) >= 16, report
                assert any((args.output / f'viewer.{suffix}').is_file()
                           for suffix in ['png', 'bmp']), 'Screenshot was not produced.'
                print(json.dumps(report, indent=2))
            finally:
                if server is not None:
                    server.terminate()
                    server.wait(timeout=5)


if __name__ == '__main__':
    main()
