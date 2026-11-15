"""Verify local diagnostic rules and real launch lifecycle events with disposable data."""
import argparse
import datetime
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
import zipfile
from api_sidecar_smoke import Sidecar


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--install', type=Path, default=Path('install-webui-release'))
    parser.add_argument('--jdk', type=Path, required=True, help='JDK used only to build and run a small test fixture')
    args = parser.parse_args()
    root = Path(tempfile.mkdtemp(prefix='luna-crash-diagnostics-'))
    data = root / 'data'
    native = data / 'instances/rules'
    (native / '.minecraft/logs').mkdir(parents=True)
    (native / 'instance.cfg').write_text('[General]\nInstanceType=OneSix\nname=Rules fixture\n')
    (native / 'mmc-pack.json').write_text('{"formatVersion":1,"components":[]}')
    version = root / 'external/.minecraft/versions/lifecycle'
    version.mkdir(parents=True)
    source = root / 'CrashFixture.java'
    source.write_text('public class CrashFixture { public static void main(String[] args) throws Exception { String mode = java.nio.file.Files.readString(java.nio.file.Path.of("mode.txt")).trim(); if(mode.equals("wait")) Thread.sleep(120000); if(mode.equals("crash")) throw new ClassCastException("class jdk.internal.loader.ClassLoaders$AppClassLoader cannot be cast to class java.net.URLClassLoader"); } }')
    subprocess.run([str(args.jdk / 'bin/javac.exe'), str(source)], check=True, capture_output=True)
    with zipfile.ZipFile(version / 'lifecycle.jar', 'w') as archive:
        archive.write(root / 'CrashFixture.class', 'CrashFixture.class')
    (version / 'lifecycle.json').write_text(json.dumps({'id':'lifecycle','type':'release','mainClass':'CrashFixture',
        'minecraftArguments':'','libraries':[], 'javaVersion':{'majorVersion':21}}))
    (data / 'instance-stores.json').write_text(json.dumps({'formatVersion':1,'InstanceStores':[
        {'id':'external','name':'external','path':str(version.parents[2]),'format':'vanilla-compatible',
         'origin':'user','enabled':True,'writable':True}]}))
    (data / 'lunalauncher.cfg').write_text('[General]\nJavaPath='+str(args.jdk / 'bin/java.exe').replace('\\','/')+'\nAutomaticJava=false\nAutoUpdate=false\n')
    sidecar = Sidecar((args.install / 'lunalauncher-cli.exe').resolve(), data)
    try:
        def analyze(text='', **extra):
            return sidecar.ok('instance.diagnostics.analyze', instance='rules', console=text, **extra)
        cases = {
            'legacy-java': 'java.lang.ClassCastException: class jdk.internal.loader.ClassLoaders$AppClassLoader cannot be cast to class java.net.URLClassLoader',
            'java-too-old': 'java.lang.UnsupportedClassVersionError: Example has been compiled by a more recent version of the Java Runtime (class file version 65.0), this version only recognizes class file versions up to 52.0',
            'java-heap': 'java.lang.OutOfMemoryError: Java heap space',
            'native-memory': 'Could not reserve enough space for 4194304KB object heap',
            'invalid-jvm-option': 'Unrecognized VM option UseConcMarkSweepGC',
            'graphics': 'GLFW error 65542: WGL: The driver does not appear to support OpenGL',
            'corrupt-archive': 'java.util.zip.ZipException: zip END header not found',
            'duplicate-mods': 'DuplicateModsFoundException: two versions of example',
            'mod-dependencies': 'Incompatible mods found! Missing mandatory dependencies: example',
            'mixin': 'org.spongepowered.asm.mixin.throwables.MixinApplyError: mod.example.json failed',
            'missing-class': 'java.lang.ClassNotFoundException: net.example.Required',
            'native-library': 'java.lang.UnsatisfiedLinkError: Cannot load 64-bit .dll on a IA 32-bit platform',
            'disk-full': 'java.io.IOException: No space left on device',
            'access-denied': 'java.nio.file.AccessDeniedException: locked-file.jar',
        }
        for code, text in cases.items():
            result = analyze(text)
            assert code in [f['id'] for f in result['findings']], (code, result)
            assert all(f['evidence'] and f['advice'] for f in result['findings'])
        assert not analyze('[WARN] Skipping bad option: lastServer:\nStopping!')['findings']
        assert not analyze('Process exited with code 2')['findings']
        assert not analyze()['findings']
        assert 'Legacy launcher' in analyze(cases['legacy-java'], language='en-US')['findings'][0]['title']
        logs = native / '.minecraft/logs/latest.log'
        logs.write_text(cases['java-heap'])
        os.utime(logs, (1691238855,1691238855))
        interval = {'since':'2026-11-15T00:00:00Z', 'until':'2026-11-16T00:00:00Z'}
        result = analyze(cases['legacy-java'], **interval)
        assert [f['id'] for f in result['findings']] == ['legacy-java'], result
        assert result['ignoredFiles'] and result['ignoredFiles'][0]['file'] == 'latest.log'
        future = datetime.datetime(2030,1,1,tzinfo=datetime.timezone.utc).timestamp()
        os.utime(logs, (future,future))
        assert not analyze(**interval)['findings']
        assert not sidecar.execute('instance.diagnostics.analyze',instance='rules',since='not-a-date')['ok']
        print('PASS: 14 diagnostic rules, evidence, unknown errors, language, stale/future log exclusion')

        listing = sidecar.ok('instance.list')
        instance = next(i for i in listing if i['name']=='lifecycle')
        instance_id = instance['id']
        game = Path(sidecar.ok('instance.info', instance=instance_id)['gameRoot'])
        sub = sidecar.ok('instance.lifecycle.subscribe')['subscriptionId']
        cursor = 0
        def completion():
            nonlocal cursor
            deadline = time.monotonic()+35
            while time.monotonic()<deadline:
                batch = sidecar.ok('event.poll',subscriptionId=sub,after=cursor)
                cursor = batch['nextCursor']
                for event in batch['events']:
                    if event['kind']=='instance.finished':
                        (root / ('event-'+event['data']['launchId'].strip('{}')+'.json')).write_text(json.dumps(event['data'],ensure_ascii=False,indent=2),encoding='utf-8')
                        return event['data']
                time.sleep(.1)
            raise AssertionError('Missing lifecycle event')
        previous_ids = set()
        for mode in ['crash','normal','wait']:
            (game / 'mode.txt').write_text(mode)
            launched = sidecar.execute('instance.launch',instance=instance_id,mode='offline',offlineName='DiagTest')
            if mode=='wait':
                assert launched['ok'], launched
                sidecar.ok('instance.kill',instance=instance_id)
            event = completion()
            assert event['launchId'] not in previous_ids
            previous_ids.add(event['launchId'])
            assert event['gameStarted'], (launched,event)
            assert event['instance']==instance_id and event['startedAt'] and event['finishedAt']
            if mode=='crash':
                assert not event['succeeded'] and not event['aborted'] and event['exitCode']!=0, event
                findings=sidecar.ok('instance.diagnostics.analyze',instance=instance_id,console=event['console'],since=event['startedAt'],until=event['finishedAt'])
                assert 'legacy-java' in [f['id'] for f in findings['findings']], (event,findings)
            elif mode=='normal': assert event['succeeded'] and event['exitCode']==0, event
            else: assert event['aborted'], event
        later=sidecar.ok('instance.lifecycle.subscribe')['subscriptionId']
        assert sidecar.ok('event.poll',subscriptionId=later)['events']==[], 'Historical failure replayed'
        sidecar.ok('event.unsubscribe',subscriptionId=later)
        print('PASS: real Java crash, normal exit, intentional stop, unique launch IDs, no historical replay')
        print('Evidence:',root)
    finally:
        sidecar.close()

if __name__=='__main__': main()
