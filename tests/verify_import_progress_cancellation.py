# SPDX-License-Identifier: GPL-3.0-only
"""Cancel directory imports while copying, through the deployed Neo task API."""
import argparse
import json
from pathlib import Path
import tempfile
import time
import zipfile

from api_sidecar_smoke import Sidecar


def main(executable):
    with tempfile.TemporaryDirectory(prefix='luna-import-cancel-') as folder:
        root = Path(folder)
        source = root / 'pack.zip'
        profile = {'id': 'Custom', 'minecraftVersion': '1.20.1', 'mainClass': 'example.Main',
                   'javaVersion': {'majorVersion': 17}, 'libraries': []}
        with zipfile.ZipFile(source, 'w', zipfile.ZIP_STORED) as archive:
            prefix = '.minecraft/versions/Custom/'
            archive.writestr(prefix + 'Custom.json', json.dumps(profile))
            archive.writestr(prefix + 'Custom.jar', b'preserved-client')
            for index in range(5000):
                archive.writestr(prefix + f'config/file-{index}.cfg', b'keep-source=true\n' * 16)
        original = (source.stat().st_size, source.stat().st_mtime_ns)
        sidecar = Sidecar(executable.resolve(), root / 'data')
        try:
            for fmt in ('prism', 'vanilla-compatible'):
                store = root / fmt
                sidecar.ok('instance-store.add', id=fmt, name=fmt, path=str(store), format=fmt, enabled=True, writable=True)
                started = sidecar.call('launcher/execute', {'operation': 'neo.task.start', 'parameters': {
                    'operation': 'instance.import', 'parameters': {
                        'source': str(source), 'name': 'Cancelled', 'targetFormat': fmt, 'targetStoreId': fmt}}})
                assert started.get('ok'), started
                task = started['data']
                cancelled = False
                deadline = time.monotonic() + 90
                while not task.get('finished'):
                    assert time.monotonic() < deadline, task
                    if not cancelled and '复制实例文件' in task.get('status', ''):
                        started = time.monotonic()
                        sidecar.ok('neo.task.cancel', taskId=task['taskId'])
                        assert time.monotonic() - started < 2, 'Cancellation request stalled'
                        cancelled = True
                    time.sleep(0.05)
                    task = sidecar.ok('neo.task.status', taskId=task['taskId'])
                assert cancelled, 'Copy phase was not observed'
                assert task['state'] == 'cancelled', task
                destination = store / 'Cancelled' if fmt == 'prism' else store / '.minecraft/versions/Cancelled'
                assert not destination.exists(), destination
                assert not list(store.glob('.tmp/*')), store
                assert not list(store.glob('.minecraft/versions/*.tmp-*')), store
                assert (source.stat().st_size, source.stat().st_mtime_ns) == original
                print('PASS:', fmt, 'copy-phase cancellation responded, temporary target removed, source unchanged', flush=True)
        finally:
            sidecar.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', required=True, type=Path)
    main(parser.parse_args().exe)
