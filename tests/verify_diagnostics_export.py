"""Exercise diagnostic ZIP exports through the installed CLI, using temporary instances."""
import argparse
import gzip
import json
import os
from pathlib import Path
import subprocess
import tempfile
import zipfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--install', type=Path, default=Path('install-webui-release'))
    args = parser.parse_args()
    exe = args.install.resolve() / 'lunalauncher-cli.exe'
    root = Path(tempfile.mkdtemp(prefix='luna-diagnostics-test-'))
    data = root / 'data'
    native = data / 'instances' / 'diagnostic'
    native.mkdir(parents=True)
    (native / 'instance.cfg').write_text('[General]\nInstanceType=OneSix\nname=诊断测试\n', encoding='utf-8')
    (native / 'mmc-pack.json').write_text(json.dumps({'formatVersion': 1, 'components': [
        {'uid': 'net.minecraft', 'version': '1.12.2'}, {'uid': 'net.minecraftforge', 'version': '14.23.5.2860'}]}))
    env = dict(os.environ, QT_QPA_PLATFORM='offscreen')
    sequence = 0

    def api(op, params, ok=True):
        nonlocal sequence
        sequence += 1
        proc = subprocess.run([str(exe), '--cli', '--json', '--non-interactive', '--dir', str(data),
                               'api', op, json.dumps(params)], env=env, capture_output=True, timeout=120)
        (root / f'{sequence}.stderr').write_bytes(proc.stderr)
        lines = [line for line in proc.stdout.decode('utf-8', errors='replace').splitlines() if line.startswith('{')]
        assert lines, (op, proc.returncode, proc.stderr[-2000:])
        response = json.loads(lines[-1])
        assert response.get('ok') == ok, response
        return response.get('data')

    def put(path, text, mtime):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8')
        os.utime(path, (mtime, mtime))

    def export(instance='diagnostic', **extra):
        output = root / f'诊断-{sequence}.zip'
        result = api('instance.diagnostics.export', {'instance': instance, 'output': str(output), **extra})
        with zipfile.ZipFile(output) as archive:
            contents = {name: archive.read(name) for name in archive.namelist()}
        assert contents['AI-PROMPT.md'].decode('utf-8') == result['prompt']
        return output, result, contents

    game = Path(api('instance.info', {'instance': 'diagnostic'})['gameRoot'])
    no_logs = root / 'no-logs.zip'
    api('instance.diagnostics.export', {'instance': 'diagnostic', 'output': str(no_logs)}, False)
    assert not no_logs.exists()
    api('instance.diagnostics.prepare', {'instance': 'diagnostic'}, False)
    prepared = api('instance.diagnostics.prepare', {'instance': 'diagnostic', 'console': 'EARLY FAILURE'})
    assert 'EARLY FAILURE' in prepared['prompt'] and 'path' not in prepared
    _, result, contents = export(console='Launch failed before Minecraft started')
    assert contents['launcher-console.log'] == b'Launch failed before Minecraft started'
    assert 'game-log' in result['missing'] and 'crash-report' in result['missing']
    put(game / 'logs/latest.log', 'CURRENT GAME LOG\njava.lang.TestFailure\n', 1700000000)
    put(game / 'logs/debug.log', 'DEBUG IS NEWER BUT NOT THE SESSION LOG', 1700000500)
    put(game / 'crash-reports/crash-old.txt', 'OLD CRASH', 1699999900)
    put(game / 'crash-reports/crash-new.txt', 'NEW CRASH', 1700000100)
    put(game / 'hs_err_pid1.log', 'JVM CRASH', 1700000200)
    put(game / 'options.txt', 'UNRELATED PRIVATE SETTINGS', 1700000900)
    output, result, contents = export(console='Visible console output')
    assert contents['game-log-latest.log'] == (game / 'logs/latest.log').read_bytes()
    assert contents['crash-report-crash-new.txt'] == b'NEW CRASH'
    assert contents['jvm-crash-hs_err_pid1.log'] == b'JVM CRASH'
    assert not any('old' in name or 'debug' in name or 'options' in name for name in contents)
    manifest = json.loads(contents['manifest.json'])
    assert manifest['instance']['minecraftVersion'] == '1.12.2'
    assert manifest['instance']['loaderType'] == 'forge'
    assert manifest['instance']['name'] == '诊断测试'
    assert 'java.lang.TestFailure' in result['prompt'] and 'NEW CRASH' in result['prompt']
    assert '更早的一次启动' in result['prompt']
    zips_before = set(root.glob('*.zip'))
    prepared = api('instance.diagnostics.prepare', {'instance': 'diagnostic', 'console': 'CLIPBOARD CONSOLE'})
    assert 'java.lang.TestFailure' in prepared['prompt'] and 'NEW CRASH' in prepared['prompt']
    assert 'CLIPBOARD CONSOLE' in prepared['prompt'] and 'path' not in prepared
    assert set(root.glob('*.zip')) == zips_before
    assert not list(root.glob('.luna-diagnostics-*'))
    before = output.read_bytes()
    api('instance.diagnostics.export', {'instance': 'diagnostic', 'output': str(output)}, False)
    assert output.read_bytes() == before
    print('PASS: latest.log preference, latest crash/JVM report, console, context, missing files and no overwrite')

    large = 'HEAD MARKER\n' + 'x' * 400000 + '\nTAIL FAILURE MARKER'
    put(game / 'logs/latest.log', large, 1700001000)
    _, result, contents = export(language='en-US')
    assert contents['game-log-latest.log'] == (game / 'logs/latest.log').read_bytes()
    assert 'HEAD MARKER' in result['prompt'] and 'TAIL FAILURE MARKER' in result['prompt']
    assert 'Middle omitted' in result['prompt'] and len(result['prompt']) < 200000
    prepared = api('instance.diagnostics.prepare', {'instance': 'diagnostic', 'language': 'en-US'})
    assert 'HEAD MARKER' in prepared['prompt'] and 'TAIL FAILURE MARKER' in prepared['prompt']
    assert 'Middle omitted' in prepared['prompt'] and len(prepared['prompt']) < 200000
    assert 'Full log files are included in the ZIP' not in prepared['prompt']
    print('PASS: clipboard preparation without output path or ZIP, console-only and bounded long logs')
    (game / 'logs/latest.log').unlink()
    (game / 'logs/debug.log').unlink()
    compressed = gzip.compress(b'ROTATED GAME LOG')
    (game / 'logs/rotated.log.gz').write_bytes(compressed)
    _, result, contents = export()
    assert contents['game-log-rotated.log.gz'] == compressed
    assert '压缩日志' in result['prompt']
    print('PASS: full archive logs, bounded head/tail AI excerpts, compressed log fallback')

    vanilla = root / 'external/.minecraft'
    for name, isolated in [('shared', False), ('isolated', True)]:
        version = vanilla / 'versions' / name
        version.mkdir(parents=True)
        (version / f'{name}.json').write_text(json.dumps({'id': name, 'type': 'release',
            'mainClass': 'net.minecraft.client.main.Main', 'minecraftArguments': '--gameDir ${game_directory}', 'libraries': []}))
        (version / 'hmclversion.cfg').write_text(json.dumps({'gameDirType': 1 if isolated else 0}))
        target = version if isolated else vanilla
        put(target / 'logs/latest.log', name.upper() + ' LOG', 1700002000)
        put(target / 'crash-reports/crash.txt', name.upper() + ' CRASH', 1700002000)
    (data / 'instance-stores.json').write_text(json.dumps({'formatVersion': 1, 'InstanceStores': [
        {'id': 'external', 'name': 'external', 'path': str(vanilla.parent), 'format': 'vanilla-compatible',
         'origin': 'user', 'enabled': True, 'writable': True}]}))
    listing = api('instance.list', {})
    for name in ['shared', 'isolated']:
        instance = next(item for item in listing if item['name'] == name)
        _, _, contents = export(instance=instance['id'])
        assert contents['game-log-latest.log'] == (name.upper() + ' LOG').encode()
        assert contents['crash-report-crash.txt'] == (name.upper() + ' CRASH').encode()
    assert not list(root.glob('.luna-diagnostics-*'))
    print('PASS: HMCL shared and isolated game directories; temporary files cleaned')
    print('Evidence:', root)


if __name__ == '__main__':
    main()
