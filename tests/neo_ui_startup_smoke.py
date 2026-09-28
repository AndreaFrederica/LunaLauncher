# SPDX-License-Identifier: GPL-3.0-only
"""Verify native startup resolution and repeated launches in real Tauri windows.

pixi run python tests/neo_ui_startup_smoke.py --exe install-neo-ui-api/luna-neo-ui.exe
Uses only disposable data directories and closes only processes started here.
"""
import argparse
from contextlib import ExitStack
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
const config = __CONFIG__;
const report = value => fetch(config.url, {method: 'POST', body: JSON.stringify({tag: config.tag, ...value})});
const ok = async (operation, parameters = {}) => {
  const reply = await luna.execute(operation, parameters);
  if (!reply.ok) throw new Error(JSON.stringify(reply));
  return reply.data;
};
async function run() {
  const info = await ok('runtime.info');
  if (info.dataRoot.replaceAll('\\', '/').toLowerCase() !== config.profile.toLowerCase()) throw new Error('Wrong profile');
  const subscription = await ok('instance.console.subscribe', {instance:'fixture'});
  await window.__TAURI__.event.listen('neo-ui-activated', async () => {
    try {
      // A replacement backend would reject this subscription.
      await ok('event.poll', {subscriptionId: subscription.subscriptionId, after:0});
      await report({stage:'activated'});
    } catch (error) { await report({stage:'error', error:String(error)}); }
  });
  await window.__TAURI__.window.getCurrentWindow().minimize();
  await report({stage:'ready'});
  const timer = setInterval(async () => {
    const action = await (await fetch(config.url + config.tag)).text();
    if (action === 'close') {
      clearInterval(timer);
      await window.__TAURI__.window.getCurrentWindow().close();
    }
  }, 150);
}
run().catch(error => report({stage:'error', error:String(error)}));
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--cli', type=Path)
    args = parser.parse_args()
    executable = args.exe.resolve(strict=True)
    cli = (args.cli or executable.with_name('lunalauncher-cli.exe' if os.name == 'nt' else 'lunalauncher')).resolve(strict=True)
    events, commands = queue.Queue(), {}
    env = dict(os.environ, LUNA_LAUNCHER_CLI=str(cli))
    for variable in ('LUNA_LAUNCHER_DATA', 'LUNALAUNCHER_DATA_DIR', 'LUNA_WEB_UI'):
        env.pop(variable, None)

    class Receiver(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def reply(self, body):
            self.send_response(200)
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(body)

        def do_POST(self):
            events.put(json.loads(self.rfile.read(int(self.headers['Content-Length']))))
            self.reply(b'OK')

        def do_GET(self):
            self.reply(commands.pop(self.path.lstrip('/'), '').encode())

    server = ThreadingHTTPServer(('127.0.0.1', 0), Receiver)
    thread = threading.Thread(target=server.serve_forever)
    thread.start()
    try:
        with temporary_profile() as temporary, ExitStack() as cleanup:
            root = Path(temporary)

            def context(directory=None, environment=env, binary=cli):
                command = [str(binary), '--cli', '--print-startup-context']
                if directory is not None:
                    command.extend(['--dir', str(directory)])
                result = subprocess.run(command, env=environment, capture_output=True, check=True, timeout=15)
                return json.loads(result.stdout)

            profiles = {tag: root / tag for tag in ('first', 'other')}
            for tag, profile in profiles.items():
                client = profile / 'instances/fixture'
                (client / '.minecraft').mkdir(parents=True)
                (client / 'instance.cfg').write_text('[General]\nInstanceType=OneSix\nname=Fixture\n')
                (client / 'mmc-pack.json').write_text('{"formatVersion":1,"components":[]}')
                ui = profile / 'webui/probe'
                ui.mkdir(parents=True)
                (ui / 'index.html').write_text('<!doctype html><title>Startup probe</title><script src="./probe.js"></script>')
                config = {'tag':tag, 'profile':profile.as_posix(), 'url':f'http://127.0.0.1:{server.server_port}/'}
                (ui / 'probe.js').write_text(PROBE.replace('__CONFIG__', json.dumps(config)), encoding='utf-8')

            first_context = context(profiles['first'])
            alias = context(environment=dict(env, LUNALAUNCHER_DATA_DIR=str(profiles['first'])))
            assert first_context == alias
            assert first_context != context(profiles['other'])
            print('PASS: explicit/environment paths resolve to the same native single-instance ID', flush=True)

            if os.name == 'nt':
                portable = root / 'portable'
                portable.mkdir()
                portable_cli = portable / 'lunalauncher-cli.exe'
                shutil.copy2(cli, portable_cli)
                portable_env = dict(env, PATH=str(cli.parent) + os.pathsep + env.get('PATH', ''),
                                    QT_PLUGIN_PATH=str(cli.parent))
                (portable / 'UserData').mkdir()
                assert Path(context(environment=portable_env, binary=portable_cli)['dataRoot']) == portable / 'UserData'
                (portable / 'UserData').rmdir()
                (portable / 'portable.txt').touch()
                assert Path(context(environment=portable_env, binary=portable_cli)['dataRoot']) == portable
                print('PASS: native portable UserData and portable.txt resolution', flush=True)

            def start(tag, native_env=False):
                current = dict(env, LUNA_WEB_UI='data/probe', WEBVIEW2_USER_DATA_FOLDER=str(root / ('webview-' + tag)))
                current['LUNALAUNCHER_DATA_DIR' if native_env else 'LUNA_LAUNCHER_DATA'] = str(profiles[tag])
                process = subprocess.Popen([str(executable)], cwd=executable.parent, env=current)
                def stop():
                    if process.poll() is None:
                        if os.name == 'nt':
                            subprocess.run(['taskkill','/PID',str(process.pid),'/T','/F'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                        else:
                            process.terminate()
                        process.wait(timeout=15)
                cleanup.callback(stop)
                return process

            def stage(process, tag, name):
                def received():
                    try:
                        event = events.get_nowait()
                    except queue.Empty:
                        return False
                    if event['stage'] == 'error':
                        raise AssertionError(event)
                    assert event['tag'] == tag and event['stage'] == name, event
                    return True
                wait_until(received, process, f'{tag}: {name}')

            first = start('first')
            stage(first, 'first', 'ready')
            # The query must remain available while MCP owns the profile.
            assert context(profiles['first']) == first_context
            for native_env in (False, True):
                second = start('first', native_env=native_env)
                assert second.wait(timeout=20) == 0
                stage(first, 'first', 'activated')
            print('PASS: repeated launch exits successfully, activates original UI and preserves backend subscriptions', flush=True)

            other = start('other')
            stage(other, 'other', 'ready')
            assert first.poll() is None and other.poll() is None
            print('PASS: different data directories can run concurrently', flush=True)

            commands['first'] = 'close'
            assert first.wait(timeout=20) == 0
            reopened = start('first')
            stage(reopened, 'first', 'ready')
            print('PASS: closing and reopening releases both host and native backend ownership', flush=True)
            for tag, process in (('first', reopened), ('other', other)):
                commands[tag] = 'close'
                assert process.wait(timeout=20) == 0
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)


if __name__ == '__main__':
    main()
