# SPDX-License-Identifier: GPL-3.0-only
"""Validate a full GTNH directory ZIP without launching bundled code.

Run through Pixi with --source <GTNH.zip> and optionally --exe <deployed CLI>.
Checks both destination formats, live progress, status response latency and
SHA-256 of bundled runtime/content files in isolated stores. Results and test
instances remain in build-meson-debug/full-gtnh-<timestamp> for inspection.
"""
import argparse
import hashlib
import json
import shlex
from pathlib import Path, PurePosixPath
import sys
import time
import zipfile

repo = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(repo / 'tests'))
from api_sidecar_smoke import Sidecar

parser = argparse.ArgumentParser(description='Validate full GTNH directory-pack imports and live Neo progress without launching bundled code.')
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--exe', type=Path, default=repo / 'install-webui-authfix-release/lunalauncher-cli.exe')
parser.add_argument('--formats', nargs='+', choices=['prism', 'vanilla-compatible'], default=['prism', 'vanilla-compatible'])
args = parser.parse_args()
source = args.source
root = repo / 'build-meson-debug' / ('full-gtnh-' + time.strftime('%Y%m%d-%H%M%S'))
root.mkdir()
report = {'source': str(source), 'zipBytes': source.stat().st_size, 'root': str(root), 'imports': []}

def digest(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

def read(path):
    return json.loads(path.read_text(encoding='utf-8'))

print('validation root:', root, flush=True)
s = Sidecar(args.exe.resolve(), root / 'data')
def api(operation, **parameters):
    before = time.monotonic()
    message = s.response(s.send('launcher/execute', {'operation': operation, 'parameters': parameters}), timeout=10)
    elapsed = time.monotonic() - before
    if operation == 'neo.task.status':
        report['maxStatusResponseSeconds'] = round(max(report.get('maxStatusResponseSeconds',0),elapsed),2)
    assert 'error' not in message, message
    value = message['result']
    assert value.get('ok'), value
    return value['data']

try:
    for fmt, prefix in [('prism','mmc'), ('vanilla-compatible','raw')]:
        for suffix in ('a','b'):
            sid = prefix + '-' + suffix
            api('instance-store.add', id=sid, name=sid, path=str(root / sid), format=fmt, enabled=True, writable=True)
    for fmt, sid in [('prism','mmc-a'), ('vanilla-compatible','raw-a')]:
        if fmt not in args.formats:
            continue
        started = time.monotonic()
        result = s.call('launcher/execute', {'operation':'neo.task.start', 'parameters': {
            'operation':'instance.import', 'parameters': {'source': str(source), 'name':'GTNH-full-' + sid}}})
        assert result['ok'], result
        task = result['data']
        task_id = task['taskId']
        interactions = []
        answered = set()
        previous = None
        samples = []
        while not task.get('finished'):
            assert time.monotonic() - started < 1200, task
            if task.get('status') and not task.get('interaction'):
                samples.append({k: task.get(k) for k in ('status','progress','totalProgress','details')})
            request = task.get('interaction', {})
            if request.get('interactionId') and request['interactionId'] not in answered:
                choices = request.get('choices', [])
                ids = [c.get('id') if isinstance(c, dict) else c for c in choices]
                selected = fmt if fmt in ids else sid if sid in ids else None
                assert selected is not None, request
                print('Neo interaction:', request['prompt'], '->', selected, flush=True)
                interactions.append({'prompt': request['prompt'], 'choices': choices, 'selected': selected})
                api('neo.task.respond', taskId=task_id, interactionId=request['interactionId'], value=ids.index(selected))
                answered.add(request['interactionId'])
            state = (task.get('state'),task.get('status'))
            if state != previous:
                print(fmt, state, flush=True)
                previous = state
            time.sleep(0.25)
            task = api('neo.task.status', taskId=task_id)
        assert task['state'] == 'succeeded', task
        assert len(interactions) == 2, interactions
        copying = [r for r in samples if '复制' in r['status'] and 0 < (r['progress'] or 0) < (r['totalProgress'] or 0)]
        assert len(copying) >= 3, samples
        assert len({r['progress'] for r in copying}) >= 3, copying
        assert report['maxStatusResponseSeconds'] < 5, report['maxStatusResponseSeconds']
        info = api('instance.info', instance=task['createdInstanceId'])
        assert info['minecraftVersion'] == '1.7.10', info
        dest = root / sid / ('GTNH-full-' + sid) if fmt == 'prism' else root / sid / '.minecraft/versions' / ('GTNH-full-' + sid)
        record = read(dest / 'luna-imported/client-artifact.json')
        client = dest / record['path']
        assert digest(client) == record['sha256']
        runtime = read(dest / 'patches/net.minecraft.json') if fmt == 'prism' else read(dest / (dest.name + '.json'))
        assert runtime['mainClass'] == 'org.jackhuang.hmcl.HMCLMultiMCBootstrap'
        assert (runtime.get('compatibleJavaMajors') == [25]) if fmt == 'prism' else (runtime['javaVersion']['majorVersion'] == 25)
        target_game = dest / 'minecraft' if fmt == 'prism' else dest
        cache_root = dest if fmt == 'prism' else dest.parent.parent
        checked = 0
        bytes_checked = 0
        with zipfile.ZipFile(source) as z:
            profiles = [n for n in z.namelist() if PurePosixPath(n).parent.parent.name == 'versions' and PurePosixPath(n).name == PurePosixPath(n).parent.name + '.json' and '/assets/' not in n]
            assert len(profiles) == 1, profiles
            profile_root = str(PurePosixPath(profiles[0]).parent) + '/'
            original = json.loads(z.read(profiles[0]))
            assert read(dest / 'luna-imported/source-profile.json') == original
            if fmt == 'prism':
                source_jvm = original['arguments']['jvm']
                converted_jvm = runtime['+jvmArgs']
                # Compare ordered option/operand pairs. Repeated JVM switches
                # must survive even when all library/content hashes match.
                for flag in ('--add-opens', '--add-exports', '--enable-native-access'):
                    source_pairs = [(flag, source_jvm[i+1]) for i, value in enumerate(source_jvm) if value == flag]
                    target_pairs = [(flag, converted_jvm[i+1]) for i, value in enumerate(converted_jvm) if value == flag]
                    assert source_pairs == target_pairs, (flag, source_pairs, target_pairs)
                assert shlex.split(runtime['minecraftArguments']) == original['arguments']['game']
                for key in ('-Djava.system.class.loader=', '-Dhmcl.mmc.bootstrap='):
                    assert [value for value in converted_jvm if value.startswith(key)] == [value for value in source_jvm if isinstance(value, str) and value.startswith(key)]
                components = api('instance.components.list', instance=task['createdInstanceId'])['components']
                by_id = {component['id']: component for component in components}
                assert by_id['net.minecraft']['version'] == '1.7.10', components
                assert by_id['net.minecraftforge']['version'] == '10.13.4.1614-1.7.10', components
                assert all(by_id[uid]['enabled'] and by_id[uid]['custom'] and not by_id[uid]['problems'] for uid in ('net.minecraft', 'net.minecraftforge')), components
                archive_names = {name.lower(): name for name in z.namelist()}
                for library in runtime['libraries']:
                    if library.get('MMC-hint') != 'local':
                        continue
                    name = library['name']
                    parts = name.split(':')
                    artifact = parts[1] + '-' + parts[2] + ('-' + parts[3] if len(parts) > 3 else '') + '.jar'
                    source_path = library.get('downloads', {}).get('artifact', {}).get('path') or '/'.join([parts[0].replace('.', '/'), parts[1], parts[2], artifact])
                    original_library = archive_names['.minecraft/libraries/' + source_path.lower()]
                    with z.open(original_library) as f:
                        assert hashlib.file_digest(f, 'sha256').hexdigest() == digest(dest / 'libraries' / library['MMC-filename']), name
                print('PASS MMC runtime: MC 1.7.10; Forge 10.13.4.1614-1.7.10; ordered JVM/game arguments and localized libraries match source', flush=True)
            with z.open(profile_root + PurePosixPath(profile_root).name + '.jar') as f:
                assert hashlib.file_digest(f,'sha256').hexdigest() == record['sha256']
            for entry in z.infolist():
                if entry.is_dir():
                    continue
                path = entry.filename
                target = None
                if path.startswith('.minecraft/libraries/') or path.startswith('.minecraft/assets/'):
                    target = cache_root / path.removeprefix('.minecraft/')
                elif path.startswith(profile_root):
                    relative = path.removeprefix(profile_root)
                    if relative.split('/')[0] in ('mods','config','saves','resourcepacks','shaderpacks','scripts','resources'):
                        target = target_game / relative
                if target is None:
                    continue
                assert target.is_file(), str(target)
                with z.open(entry) as f:
                    expected = hashlib.file_digest(f, 'sha256').hexdigest()
                assert digest(target) == expected, str(target)
                checked += 1
                bytes_checked += entry.file_size
        if fmt == 'vanilla-compatible':
            assert not any((dest / n).exists() for n in ('instance.cfg','mmc-pack.json','patches'))
            assert read(dest / 'hmclversion.cfg')['gameDirType'] == 1
            assert len(api('vanilla-instance.list',storeId=sid)['instances']) == 1
        imported = {'format':fmt,'taskId':task_id,'instanceId':task['createdInstanceId'],'destination':str(dest),
                    'seconds':round(time.monotonic()-started,2),'checkedFiles':checked,'checkedBytes':bytes_checked,
                    'clientSha256':record['sha256'],'interactions':interactions,'minecraftVersion':info['minecraftVersion'], 'progressSamples':samples}
        report['imports'].append(imported)
        (root / 'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
        print('PASS',fmt,'full ZIP;',checked,'files checked;',imported['seconds'],'seconds',flush=True)
finally:
    s.close()
    (root / 'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
