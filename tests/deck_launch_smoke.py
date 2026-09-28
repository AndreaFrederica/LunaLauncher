# SPDX-License-Identifier: GPL-3.0-only
"""Exercise Luna Deck's launch buttons in the installed Tauri host.

pixi run python tests/deck_launch_smoke.py --exe install-neo-ui-api/luna-neo-ui.exe
Optional real Minecraft 1.21.8 smoke: --cache-root PATH --java PATH_TO_JAVA
Copies cached game files into a temporary profile; never copies accounts/worlds.
Requires a graphical desktop. The proxy rejects all authentication traffic.
"""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import queue
import shutil
import subprocess
import threading

from webui_smoke import temporary_profile, wait_until


PROBE = r"""
window.addEventListener('load', async () => {
  const config = __CONFIG__, $ = id => document.getElementById(id);
  const report = value => fetch(config.report, {method:'POST', body:JSON.stringify(value)});
  const check = (ok, msg) => { if (!ok) throw new Error(msg); };
  const wait = async (predicate, description, timeout = 20000) => {
    const deadline = Date.now() + timeout;
    while (!predicate()) {
      if (Date.now() > deadline) throw new Error(description + ': ' + $('session-error').textContent + '\n' + $('console-log').textContent.slice(-6000));
      await new Promise(resolve => setTimeout(resolve, 80));
    }
  };
  const card = id => document.querySelector('[data-instance="' + id + '"] .instance-launch');
  try {
    await wait(() => !$('refresh').disabled && $('launch-account').options.length === 3, 'accounts loaded');
    card('auth-test').click();
    await wait(() => $('launch-input').open, 'account selection prompt');
    check($('input-choice').options.length === 2, 'Missing account choices');
    $('input-cancel').click();
    await wait(() => !$('launch-input').open && !$('console-launch').disabled, 'input cancellation');
    check(!$('session-error').hidden, 'Cancelled input left no result');
    await report({stage:'input-cancelled'});

    document.querySelector('[data-page="library"]').click();
    $('launch-account').value = [...$('launch-account').options].find(o => o.textContent.startsWith('Fixture ·')).value;
    card('auth-test').click();
    await wait(() => !$('console-cancel').hidden && !$('console-cancel').disabled, 'cancellable authentication');
    const cancelStart = Date.now(); $('console-cancel').click();
    await wait(() => !$('console-launch').disabled, 'authentication cancellation', 6000);
    check(!$('launch-input').open && !$('launch-device').open, 'Stale authentication modal');
    check(!$('session-error').hidden, 'Cancelled authentication left no result');
    await report({stage:'auth-cancelled', elapsedMs:Date.now() - cancelStart});
    if (config.minecraft) {
      document.querySelector('[data-page="library"]').click();
      $('launch-mode').value = 'offline'; $('launch-mode').dispatchEvent(new Event('change'));
      $('offline-name').value = 'DeckSmoke'; $('offline-name').dispatchEvent(new Event('input'));
      check($('launch-account').disabled, 'Temporary offline name did not override account selection');
      card('deck-minecraft').click();
      await wait(() => $('session-pid').textContent.startsWith('PID ') && $('session-state').textContent === '客户端运行中', 'real Java process', 60000);
      await wait(() => /OpenAL initialized|Sound engine started|Created:.*textures\/atlas/.test($('console-log').textContent), 'Minecraft reached resource initialization', 60000);
      const result = await luna.execute('instance.get', {instance:'deck-minecraft'});
      check(result.ok && (result.data.lifecycle?.running ?? result.data.running), 'Backend does not consider Minecraft running');
      check(!document.documentElement.querySelector('dialog[open]'), 'Launch left a modal open');
      check($('console-log').scrollHeight > $('console-log').clientHeight, 'Console lacks scrollable live output');
      check(document.documentElement.scrollWidth <= innerWidth, 'Horizontal overflow in console');
      $('log-follow').checked = false; $('console-log').scrollTop = 0;
      await new Promise(resolve => setTimeout(resolve, 300));
      check($('console-log').scrollTop === 0, 'Manual log scrolling overridden');
      const pid = $('session-pid').textContent;
      $('console-instance').value = 'auth-test'; $('console-instance').dispatchEvent(new Event('change'));
      check(!$('console-log').textContent.includes('Sound engine started'), 'Logs leaked between instances');
      $('console-instance').value = 'deck-minecraft'; $('console-instance').dispatchEvent(new Event('change'));
      await wait(() => $('session-pid').textContent === pid, 'restore running session');
      await report({stage:'minecraft-running', pid, logTail:$('console-log').textContent.slice(-1800)});
      $('console-stop').click();
      check($('stop-confirm').open, 'Stop confirmation absent');
      $('stop-yes').click();
      await wait(() => $('session-state').textContent === '客户端已退出' && !$('console-launch').disabled, 'stop real Minecraft', 15000);
      await report({stage:'minecraft-stopped'});
    }
    await report({stage:'verified'});
    await window.__TAURI__.window.getCurrentWindow().close();
  } catch (error) { await report({stage:'error', error:String(error)}); }
});
"""


def copy_cached_minecraft(cache, profile, java):
    """Copy only vanilla 1.21.8 metadata, its cached jars and asset index/objects."""
    def copy(relative):
        source = cache / relative
        target = profile / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)

    copy(Path('meta/index.json'))
    for uid, version in [('net.minecraft', '1.21.8'), ('org.lwjgl3', '3.3.3')]:
        copy(Path('meta') / uid / 'index.json')
        path = Path('meta') / uid / f'{version}.json'
        copy(path)
        meta = json.loads((cache / path).read_text(encoding='utf-8'))
        libraries = meta['libraries'] + ([meta['mainJar']] if 'mainJar' in meta else [])
        for library in libraries:
            group, name, version, *_ = library['name'].split(':')
            directory = Path('libraries') / group.replace('.', '/') / name / version
            if (cache / directory).exists():
                shutil.copytree(cache / directory, profile / directory, dirs_exist_ok=True)
    index = Path('assets/indexes/26.json')
    copy(index)
    for item in json.loads((cache / index).read_text(encoding='utf-8'))['objects'].values():
        digest = item['hash']
        copy(Path('assets/objects') / digest[:2] / digest)
    if (cache / 'assets/log_configs').exists():
        shutil.copytree(cache / 'assets/log_configs', profile / 'assets/log_configs')
    instance = profile / 'instances/deck-minecraft'
    (instance / '.minecraft').mkdir(parents=True)
    (instance / 'instance.cfg').write_text(
        '[General]\nInstanceType=OneSix\nname=Deck Minecraft 1.21.8\n'
        f'OverrideJavaLocation=true\nJavaPath={java.as_posix()}\n'
        'OverrideMemory=true\nMinMemAlloc=512\nMaxMemAlloc=2048\n'
        'OverrideWindow=true\nLaunchMaximized=false\nMinecraftWinWidth=854\nMinecraftWinHeight=480\n', encoding='utf-8')
    (instance / 'mmc-pack.json').write_text(json.dumps({'formatVersion':1, 'components':[
        {'uid':'org.lwjgl3', 'version':'3.3.3', 'dependencyOnly':True},
        {'uid':'net.minecraft', 'version':'1.21.8', 'important':True}]}), encoding='utf-8')
    # Do not create or open a world; keep a real title screen quiet during the test.
    (instance / '.minecraft/options.txt').write_text('soundCategory_master:0.0\nmaxFps:30\n', encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--deck', type=Path, default=Path(__file__).resolve().parents[1] / 'examples/webui/luna-deck')
    parser.add_argument('--cache-root', type=Path)
    parser.add_argument('--java', type=Path)
    args = parser.parse_args()
    if bool(args.cache_root) != bool(args.java):
        parser.error('--cache-root and --java must be supplied together')
    executable = args.exe.resolve(strict=True)
    cli = executable.with_name('lunalauncher-cli.exe')
    events = queue.Queue()
    release = threading.Event()
    refresh_seen = threading.Event()

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_CONNECT(self):
            refresh_seen.set()
            release.wait(20)
            self.send_error(502)

        def do_POST(self):
            events.put(json.loads(self.rfile.read(int(self.headers['Content-Length']))))
            self.send_response(200)
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(b'OK')

    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    thread = threading.Thread(target=server.serve_forever)
    thread.start()
    try:
        with temporary_profile() as temporary:
            root = Path(temporary)
            profile = root / 'profile'
            instance = profile / 'instances/auth-test'
            instance.mkdir(parents=True)
            (instance / 'instance.cfg').write_text('[General]\nInstanceType=OneSix\nname=Deck auth test\n', encoding='utf-8')
            (instance / 'mmc-pack.json').write_text('{"formatVersion":1,"components":[]}', encoding='utf-8')
            (profile / 'lunalauncher.cfg').write_text(
                '[General]\nMSAClientIDOverride=fixture-client\nProxyType=HTTP\nProxyAddr=127.0.0.1\n'
                f'ProxyPort={server.server_port}\n', encoding='utf-8')
            (profile / 'accounts.json').write_text(json.dumps({'formatVersion':3, 'accounts':[
                {'type':'MSA', 'active':False, 'msa-client-id':'fixture-client',
                 'msa':{'token':'fixture', 'refresh_token':'synthetic-refresh-token'},
                 'profile':{'id':'12345678901234567890123456789012', 'name':'Fixture',
                            'skin':{'id':'', 'url':'', 'variant':''}, 'capes':[]}},
                {'type':'Offline', 'active':False,
                 'profile':{'id':'22345678901234567890123456789012', 'name':'OfflineFixture',
                            'skin':{'id':'', 'url':'', 'variant':''}, 'capes':[]}},
            ]}), encoding='utf-8')
            if args.cache_root:
                print('Preparing isolated cached vanilla Minecraft 1.21.8…', flush=True)
                copy_cached_minecraft(args.cache_root.resolve(strict=True), profile, args.java.resolve(strict=True))
            deck = profile / 'webui/luna-deck'
            shutil.copytree(args.deck.resolve(strict=True), deck)
            index = deck / 'index.html'
            index.write_text(index.read_text(encoding='utf-8').replace('</body>', '<script src="./launch-probe.js"></script></body>'), encoding='utf-8')
            config = {'report':f'http://127.0.0.1:{server.server_port}/', 'minecraft':bool(args.cache_root)}
            (deck / 'launch-probe.js').write_text(PROBE.replace('__CONFIG__', json.dumps(config)), encoding='utf-8')
            process = subprocess.Popen([str(executable)], cwd=executable.parent, env={**os.environ,
                'LUNA_LAUNCHER_CLI':str(cli), 'LUNA_LAUNCHER_DATA':str(profile), 'LUNA_WEB_UI':'data/luna-deck',
                'WEBVIEW2_USER_DATA_FOLDER':str(root / 'webview')})
            try:
                def receive():
                    while True:
                        try:
                            event = events.get_nowait()
                        except queue.Empty:
                            return False
                        if event['stage'] == 'error':
                            raise AssertionError(event['error'])
                        print(json.dumps(event, ensure_ascii=False), flush=True)
                        if event['stage'] == 'auth-cancelled':
                            assert refresh_seen.is_set(), 'Cancellation did not reach real account refresh'
                            release.set()
                        if event['stage'] == 'verified':
                            return True
                wait_until(receive, process, 'Deck launch flow', timeout=160 if args.cache_root else 50)
                process.wait(timeout=15)
                assert process.returncode == 0, process.returncode
                print('PASS: real Deck DOM account prompt/cancel, task cancellation' +
                      (', real Minecraft startup, live logs, session switching and stop' if args.cache_root else ''), flush=True)
            finally:
                release.set()
                if process.poll() is None:
                    subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'],
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
                    process.wait(timeout=15)
    finally:
        release.set()
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)


if __name__ == '__main__':
    main()
