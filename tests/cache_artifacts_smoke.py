"""Exercise legacy artifact separation and opt-in settings using an isolated real sidecar."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import time

from api_sidecar_smoke import Sidecar


def seed(profile, count=2042):
    entries = []
    for base, name in [('ATLauncherPacks', f'fixture-{i:04d}.jar') for i in range(count)] + [
        ('libraries', 'fixture-runtime.jar'), ('meta', 'fixture.json')]:
        folder = profile/('cache/ATLauncherPacks' if base == 'ATLauncherPacks' else base)
        folder.mkdir(parents=True, exist_ok=True)
        file = folder/name
        payload = b'fixture-content'
        file.write_bytes(payload)
        entries.append(dict(base=base, path=name, md5sum=hashlib.md5(payload).hexdigest(),
                            last_changed_timestamp=int(file.stat().st_mtime*1000), eternal=True))
    (profile/'metacache').write_text(json.dumps(dict(version='1', entries=entries)), encoding='utf-8')


def main():
    exe = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='luna-artifacts-') as temp:
        profile = Path(temp)
        (profile/'lunalauncher.cfg').write_text('[General]\nLanguage=en_US\nLocalServiceCache=false\n', encoding='utf-8')
        seed(profile)
        client = Sidecar(exe, profile)
        try:
            started = time.monotonic()
            state = client.ok('launcher.cache.status')
            assert state['artifactsEnabled'] is False, state
            assert state['entries'] == 1 and state['artifacts']['entries'] == 2042, state
            assert time.monotonic()-started < 10, 'Cache statistics stalled'
            items = client.ok('launcher.cache.entries')
            assert len(items) == 1 and items[0]['path'] == 'fixture.json', items
            copies = client.ok('launcher.cache.entries', kind='artifacts')
            assert len(copies) == 2042 and all(e['path'].endswith('.jar') for e in copies)
            rejected = client.execute('launcher.cache.remove', base='libraries', path='fixture-runtime.jar', confirm=True)
            assert not rejected['ok'] and (profile/'libraries/fixture-runtime.jar').is_file()
            client.ok('launcher.cache.clear', confirm=True)
            assert client.ok('launcher.cache.entries') == []
            assert len(client.ok('launcher.cache.entries', kind='artifacts')) == 2042
            client.ok('settings.set', scope='launcher', key='CacheDownloadArtifacts', value=True)
            assert client.ok('launcher.cache.status')['artifactsEnabled'] is True
            client.ok('settings.set', scope='launcher', key='CacheDownloadArtifacts', value=False)
            client.ok('settings.set', scope='launcher', key='PclDownloadThreadLimit', value=3)
            client.ok('settings.set', scope='launcher', key='PclDownloadChunkLimit', value=2)
        finally:
            client.close()
        client = Sidecar(exe, profile)
        try:
            assert client.ok('launcher.cache.status')['artifactsEnabled'] is False
            assert client.ok('settings.get', scope='launcher', key='CacheDownloadArtifacts')['value'] is False
            for key, expected in [('PclDownloadThreadLimit', 3), ('PclDownloadChunkLimit', 2)]:
                assert int(client.ok('settings.get', scope='launcher', key=key)['value']) == expected
            started = time.monotonic()
            client.ok('launcher.cache.clear', kind='artifacts', confirm=True)
            assert time.monotonic()-started < 10, 'Artifact cleanup stalled'
            assert client.ok('launcher.cache.status')['artifacts']['entries'] == 0
            assert not list((profile/'cache/ATLauncherPacks').glob('*.jar'))
            assert (profile/'libraries/fixture-runtime.jar').is_file()
        finally:
            client.close()
    print('PASS: 2042 legacy JAR copies separated; opt-in defaults off and persists; runtime files protected; independent limits persisted')


if __name__ == '__main__': main()
