# SPDX-License-Identifier: GPL-3.0-only
"""Exercise external bundles in the real installed Tauri/WebView host.

pixi run python tests/webui_smoke.py --exe install-neo-ui-api/luna-neo-ui.exe
Uses a disposable launcher profile. Requires a graphical desktop/WebView runtime.
"""
import argparse
from contextlib import contextmanager
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import queue
import shutil
import subprocess
import tempfile
import threading
import time


PROBE = r"""
const config = __CONFIG__;
const report = value => fetch(config.report, {method: 'POST', body: JSON.stringify(value)});
const check = (condition, message) => { if (!condition) throw new Error(message); };
const ok = async (operation, parameters = {}) => {
  const result = await luna.execute(operation, parameters);
  check(result.ok, operation + ': ' + JSON.stringify(result));
  return result.data;
};
// Start the API call before the document's load event, just like SPA setup code.
const runtime = ok('runtime.info');
async function run() {
  await report({stage: 'loaded', bundle: config.bundle});
  check(luna.apiVersion === 1, 'Missing injected bridge');
  const info = await runtime;
  check(info.dataRoot.replaceAll('\\', '/').toLowerCase() === config.profile.toLowerCase(), 'Wrong data root');
  const catalog = await luna.webUis();
  check(catalog.active === 'data/' + config.bundle, 'Wrong active UI');
  check(catalog.roots.some(r => r.source === 'install'), 'Missing install root');
  check(catalog.roots.some(r => r.source === 'data'), 'Missing data root');
  check(catalog.entries.some(e => e.id === 'data/broken' && e.error), 'Broken bundle was not reported');
  check((await ok('instance.list')).some(i => i.id === 'neo-client'), 'Real instance API unavailable');
  check((await fetch('./assets/marker.txt')).status === 200, 'Relative assets unavailable');
  check((await fetch('/assets/%E5%9B%BE%E6%A0%87.txt')).status === 200, 'Unicode asset unavailable');
  check((await fetch('/missing.js')).status === 404, 'Missing assets must return 404');
  check((await fetch('/..%2foutside.txt')).status === 404, 'Path traversal was accepted');
  check(getComputedStyle(document.body).color === 'rgb(12, 34, 56)', 'Stylesheet not loaded');
  if (config.mode === 'persist') {
    const settings = await luna.webUiSettings();
    check(!settings.rememberLastUi && settings.startupUi === 'data/probe-a', 'Fixed startup settings did not persist');
    check(settings.additionalDirectories.includes(config.extra), 'Custom directory did not persist');
    await luna.openWebUiManager();
    await report({stage: 'persisted'});
    await window.__TAURI__.window.getCurrentWindow().close();
    return;
  }
  if (config.bundle === 'probe-a') {
    const defaults = await luna.webUiSettings();
    check(defaults.rememberLastUi && defaults.startupUi === 'builtin', 'Incorrect settings defaults');
    const settings = await luna.updateWebUiSettings({rememberLastUi: false, startupUi: 'data/probe-a', additionalDirectories: [config.extra], showManagerOnStart: true});
    check(!settings.rememberLastUi && settings.scanInstallDirectory, 'Settings patch lost defaults');
    const discovered = await luna.webUis();
    check(discovered.entries.some(e => e.source.startsWith('custom:') && e.name === 'Extra bundle'), 'Extra folder not scanned');
    for (const patch of [{additionalDirectories: ['relative/path']}, {rememberLastUi: 'true'}, {typo: true}, {startupUi: 'data/broken'}]) {
      let invalid = false;
      try { await luna.updateWebUiSettings(patch); } catch { invalid = true; }
      check(invalid, 'Invalid settings accepted: ' + JSON.stringify(patch));
    }
    check(JSON.stringify(await luna.webUiSettings()) === JSON.stringify(settings), 'Rejected patch changed settings');
    await luna.updateWebUiSettings({scanInstallDirectory: false});
    check(!(await luna.webUis()).roots.some(r => r.source === 'install'), 'Disabled root is still scanned');
    await luna.updateWebUiSettings({scanInstallDirectory: true});
    await luna.catalog();
    await Promise.all([ok('runtime.info'), ok('instance.list')]);
    let resolveInput;
    const input = new Promise(resolve => { resolveInput = resolve; });
    const unlisten = await luna.onEvent(event => { if (event.kind === 'input') resolveInput(event); });
    const unstream = await luna.onStream(() => {});
    const login = luna.execute('account.login', {type: 'offline'});
    const event = await input;
    await ok('task.list', {runningOnly: true});
    let rejected = false;
    try { await luna.switchWebUi('builtin'); } catch { rejected = true; }
    check(rejected, 'UI switch must not abandon pending login');
    await luna.respond({interactionId: event.interactionId, cancel: true});
    check(!(await login).ok, 'Login cancellation failed');
    unlisten(); unstream();
    const subscription = await ok('instance.console.subscribe', {instance: 'neo-client'});
    sessionStorage.setItem('smoke.subscription', subscription.subscriptionId);
    await luna.openWebUiManager();
    await report({stage: 'switching'});
    await luna.switchWebUi('data/probe-b');
  } else {
    check(!(await luna.webUiSettings()).rememberLastUi, 'Switching discarded loading settings');
    const subscriptionId = sessionStorage.getItem('smoke.subscription');
    check(!!subscriptionId, 'Navigation lost session state');
    // Subscription belongs to the original sidecar; a restarted backend rejects it.
    await ok('event.poll', {subscriptionId, after: 0});
    check((await ok('event.unsubscribe', {subscriptionId})).removed, 'Backend state was lost');
    await report({stage: 'switched'});
    await luna.switchWebUi('builtin');
  }
}
run().catch(error => report({stage: 'error', error: error.message || String(error)}));
"""


DECK_PROBE = r"""
window.addEventListener('load', async () => {
  const report = value => fetch(__REPORT__, {method: 'POST', body: JSON.stringify(value)});
  const check = (condition, message) => { if (!condition) throw new Error(message); };
  const wait = async predicate => {
    const deadline = Date.now() + 15000;
    while (!predicate()) {
      if (Date.now() > deadline) throw new Error('Deck DOM did not reach expected state');
      await new Promise(resolve => setTimeout(resolve, 50));
    }
  };
  const $ = id => document.getElementById(id);
  try {
    await wait(() => !$('refresh').disabled && document.querySelector('.instance-card'));
    check(document.querySelector('.instance-card h3').textContent === 'Web UI fixture', 'Deck did not render real instance');
    check(getComputedStyle(document.querySelector('.sidebar')).backgroundColor === 'rgb(35, 46, 43)', 'Deck stylesheet missing');
    check(document.documentElement.scrollWidth <= innerWidth, 'Deck has horizontal overflow');
    const icon = document.querySelector('.instance-icon');
    check(icon.src.startsWith('data:image/'), 'Deck did not load instance icon');
    document.querySelector('.instance-details').click();
    await wait(() => $('details').open);
    check($('detail-content').textContent.includes('neo-client'), 'Deck detail did not render real directory');
    check(!$('detail-folder').disabled, 'Deck folder action missing');
    $('close-details').click();
    await wait(() => !document.querySelector('.instance-launch').disabled);
    document.querySelector('.instance-launch').click();
    await wait(() => !$('session-error').hidden && $('console-cancel').hidden);
    check(!$('console-page').hidden, 'Deck launch did not open console');
    check($('session-error').textContent.includes('No account'), 'Deck did not retain backend launch error');
    check(!$('console-launch').disabled, 'Deck launch stayed pending after failure');
    check($('console-log').textContent.includes('No account'), 'Deck console omitted launch failure');
    check(document.documentElement.scrollWidth <= innerWidth, 'Deck console has horizontal overflow');
    // A retry must reuse the console subscription and finish cleanly too.
    $('console-launch').click();
    await wait(() => !$('session-error').hidden && !$('console-launch').disabled);
    check((await luna.execute('event.subscriptions')).data.length === 1, 'Deck retry leaked subscriptions');
    document.querySelector('[data-page="library"]').click();
    $('search').value = 'does-not-match';
    $('search').dispatchEvent(new Event('input'));
    check(!document.querySelector('.instance-card'), 'Deck search did not filter instances');
    document.querySelector('[data-page="settings"]').click();
    await wait(() => $('startup').options.length > 0);
    check(!$('settings-page').hidden && $('library-page').hidden, 'Deck settings navigation failed');
    check(document.documentElement.scrollWidth <= innerWidth, 'Deck settings have horizontal overflow');
    $('remember').checked = false;
    $('remember').dispatchEvent(new Event('change'));
    check(!$('startup').disabled, 'Deck fixed startup selection is disabled');
    $('startup').value = 'builtin';
    $('scan-data').checked = false;
    $('loading-settings').requestSubmit();
    await wait(() => $('settings-feedback').textContent.startsWith('已保存'));
    const settings = await luna.webUiSettings();
    check(!settings.rememberLastUi && !settings.scanDataDirectory, 'Deck form did not save settings');
    check((await fetch('./styles.css')).ok, 'Settings change invalidated the active page');
    $('reset-settings').click();
    await wait(() => $('settings-feedback').textContent.startsWith('已保存') && $('remember').checked);
    check((await luna.webUiSettings()).scanDataDirectory, 'Deck reset did not restore discovery');
    await report({stage: 'deck-verified'});
    $('full-ui').click();
  } catch (error) { await report({stage: 'error', error: error.message || String(error)}); }
});
"""


@contextmanager
def temporary_profile():
    temporary = tempfile.TemporaryDirectory(prefix='luna-webui-smoke-')
    try:
        yield temporary.name
    finally:
        # Windows can briefly retain WebView/Qt file handles after taskkill.
        for attempt in range(30):
            try:
                temporary.cleanup()
                break
            except PermissionError:
                if attempt == 29:
                    raise
                time.sleep(0.2)


def wait_until(predicate, process, description, timeout=45):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return
        if process.poll() is not None:
            raise AssertionError(f"Host exited ({process.returncode}) while {description}")
        time.sleep(0.1)
    raise AssertionError(f"Timed out: {description}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--cli', type=Path)
    args = parser.parse_args()
    executable = args.exe.resolve(strict=True)
    cli = (args.cli or executable.with_name('lunalauncher-cli.exe' if os.name == 'nt' else 'lunalauncher')).resolve(strict=True)
    events = queue.Queue()

    class Receiver(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_POST(self):
            events.put(json.loads(self.rfile.read(int(self.headers['Content-Length']))))
            self.send_response(200)
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(b'OK')

    server = ThreadingHTTPServer(('127.0.0.1', 0), Receiver)
    thread = threading.Thread(target=server.serve_forever)
    thread.start()
    try:
        with temporary_profile() as temporary:
            root = Path(temporary)
            profile = root / 'profile'
            client = profile / 'instances/neo-client'
            (client / '.minecraft').mkdir(parents=True)
            (client / 'instance.cfg').write_text('[General]\nInstanceType=OneSix\nname=Web UI fixture\n', encoding='utf-8')
            (client / 'mmc-pack.json').write_text('{"formatVersion":1,"components":[]}', encoding='utf-8')
            webui = profile / 'webui'
            (webui / 'broken').mkdir(parents=True)
            (webui / 'outside.txt').write_text('Must not be served', encoding='utf-8')
            preference = webui / 'selection.json'
            extra = profile / 'extra-uis'
            (extra / 'example').mkdir(parents=True)
            (extra / 'example/index.html').write_text('<h1>Custom root</h1>', encoding='utf-8')
            (extra / 'example/webui.json').write_text('{"name":"Extra bundle"}', encoding='utf-8')

            def bundle(name, mode):
                directory = webui / name
                (directory / 'assets').mkdir(parents=True, exist_ok=True)
                (directory / 'index.html').write_text('<!doctype html><meta charset="utf-8">'
                    '<link rel="stylesheet" href="/assets/style.css">'
                    '<body>External UI integration test<script src="/assets/probe.js"></script></body>', encoding='utf-8')
                (directory / 'assets/style.css').write_text('body { color: rgb(12,34,56); }', encoding='utf-8')
                for name in ('marker.txt', '图标.txt'):
                    (directory / 'assets' / name).write_text('fixture', encoding='utf-8')
                config = {'report': f'http://127.0.0.1:{server.server_port}/', 'profile': profile.as_posix(),
                          'bundle': directory.name, 'mode': mode, 'extra': str(extra)}
                (directory / 'assets/probe.js').write_text(PROBE.replace('__CONFIG__', json.dumps(config)), encoding='utf-8')

            def start(selection=None):
                env = dict(os.environ, LUNA_LAUNCHER_CLI=str(cli), LUNA_LAUNCHER_DATA=str(profile),
                           WEBVIEW2_USER_DATA_FOLDER=str(root / 'webview'))
                env.pop('LUNA_WEB_UI', None)
                if selection:
                    env['LUNA_WEB_UI'] = selection
                return subprocess.Popen([str(executable)], cwd=executable.parent, env=env)

            def stop(process):
                if process.poll() is None:
                    if os.name == 'nt':
                        subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'],
                                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
                    else:
                        process.terminate()
                    process.wait(timeout=15)

            def selected():
                try:
                    return json.loads(preference.read_text(encoding='utf-8'))['selected']
                except (OSError, ValueError, KeyError):
                    return None

            def await_stage(process, expected):
                def received():
                    while True:
                        try:
                            event = events.get_nowait()
                        except queue.Empty:
                            return False
                        if event['stage'] == 'error':
                            raise AssertionError(event['error'])
                        print(event, flush=True)
                        if event['stage'] == expected:
                            return True
                wait_until(received, process, expected)

            bundle('probe-a', 'switch')
            bundle('probe-b', 'switch')
            process = start('data/probe-a')
            try:
                await_stage(process, 'switched')
                wait_until(lambda: selected() == 'builtin', process, 'return to embedded UI')
                print('PASS: external assets, API/events, pending-login guard, manager, live switching and backend continuity', flush=True)
            finally:
                stop(process)

            bundle('probe-a', 'persist')
            # Last UI is builtin, but fixed startup is probe-a: only the saved
            # loading policy should decide the next document (no env override).
            saved = json.loads(preference.read_text(encoding='utf-8'))
            assert saved['selected'] == 'builtin' and not saved['settings']['rememberLastUi']
            process = start()
            try:
                await_stage(process, 'persisted')
                process.wait(timeout=15)
                if process.returncode != 0:
                    raise AssertionError(f'Graceful close failed: {process.returncode}')
                print('PASS: persisted selection and graceful exit with manager open', flush=True)
            finally:
                stop(process)

            for requested, override in [('data/missing', None), ('data/broken', None), ('data/probe-a', 'builtin')]:
                preference.write_text(json.dumps({'selected': requested}), encoding='utf-8')
                process = start(override)
                try:
                    wait_until(lambda: selected() == 'builtin', process, f'recovery from {requested}')
                    print(f'PASS: recovery from {requested} (override={override})', flush=True)
                finally:
                    stop(process)

            deck = webui / 'luna-deck'
            shutil.copytree(Path(__file__).resolve().parents[1] / 'examples/webui/luna-deck', deck)
            index = deck / 'index.html'
            index.write_text(index.read_text(encoding='utf-8').replace('</body>',
                '<script src="./smoke-probe.js"></script></body>'), encoding='utf-8')
            (deck / 'smoke-probe.js').write_text(DECK_PROBE.replace('__REPORT__',
                json.dumps(f'http://127.0.0.1:{server.server_port}/')), encoding='utf-8')
            process = start('data/luna-deck')
            try:
                await_stage(process, 'deck-verified')
                wait_until(lambda: selected() == 'builtin', process, 'Deck return to embedded UI')
                print('PASS: Luna Deck cards, launch failure/retry, console subscription, filtering, settings and switching', flush=True)
            finally:
                stop(process)
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)


if __name__ == '__main__':
    main()
