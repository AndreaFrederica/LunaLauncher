# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the first-run wizard and its native persistence with disposable data.

pixi run python tests/neo_ui_setup_smoke.py --exe install-neo-ui/luna-neo-ui.exe
No existing launcher data is migrated or modified, and no account is logged in.
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

from api_sidecar_smoke import Sidecar
from webui_smoke import temporary_profile, wait_until


PROBE = r"""
const config = __CONFIG__;
const report = value => fetch(config.report, {method:'POST', body:JSON.stringify(value)});
const check = (value, message) => { if (!value) throw new Error(message); };
const ok = async (operation, parameters = {}) => {
  const result = await luna.execute(operation, parameters);
  check(result.ok, operation + ': ' + JSON.stringify(result));
  return result.data;
};
const wait = async predicate => {
  const deadline = Date.now() + 30000;
  while (!predicate()) {
    if (Date.now() > deadline) throw new Error('Wizard did not reach expected state: ' + predicate.toString() + '; heading=' + heading() + '; error=' + card()?.querySelector('.setup-error')?.textContent);
    await new Promise(resolve => setTimeout(resolve, 60));
  }
};
const card = () => document.querySelector('.neo-setup-card');
const heading = () => card()?.querySelector('h3')?.textContent;
const button = label => [...card().querySelectorAll('button')].find(item => item.textContent.trim() === label);
const next = async () => {
  await wait(() => card()?.querySelector('[data-setup-next]') && !card().querySelector('[data-setup-next]').disabled);
  card().querySelector('[data-setup-next]').click();
};
const toggle = label => [...card().querySelectorAll('.pcl-check, [role="checkbox"], [role="switch"]')].find(item => item.textContent.includes(label));
const checked = item => item.matches('.pcl-check') ? item.querySelector('input[type="checkbox"]').checked : item.getAttribute('aria-checked') === 'true';
const memory = label => card().querySelector(label === '最小内存' ? '#setup-min-memory' : '#setup-max-memory');
const choose = async (field, label) => {
  const control = card().querySelector(`[data-setup-field="${field}"] .pcl-select__control`);
  control.scrollIntoView({block:'nearest'});
  control.click();
  await wait(() => [...document.querySelectorAll('.pcl-select__menu [role="option"]')].some(item => item.textContent.trim() === label));
  const option = [...document.querySelectorAll('.pcl-select__menu [role="option"]')].find(item => item.textContent.trim() === label);
  const rect = option.getBoundingClientRect();
  check(rect.width > 0 && rect.top >= 0 && rect.bottom <= innerHeight, 'Select menu outside viewport');
  check(option.contains(document.elementFromPoint(rect.left + rect.width / 2, rect.top + rect.height / 2)), 'Select menu hidden behind dialog');
  option.click();
  await wait(() => !document.querySelector('.pcl-select__menu'));
};
const layout = () => {
  const actions = card().querySelector('.neo-setup-actions').getBoundingClientRect();
  check(actions.bottom <= innerHeight && actions.top >= 0, 'Wizard actions overflow viewport');
  const body = card().querySelector('.neo-setup-body');
  check(body.scrollWidth <= body.clientWidth, 'Wizard has horizontal overflow');
};
const setInput = (input, value) => { input.value = value; input.dispatchEvent(new Event('input', {bubbles:true})); };
async function run() {
  await wait(() => document.querySelector('#q-app')?.textContent.trim());
  if (config.reopen) {
    await new Promise(resolve => setTimeout(resolve, 1500));
    check(!card(), 'Completed setup reopened');
    check(!(await ok('startup.status')).needsSetup, 'Native completion was lost');
    await report({stage:'reopen'});
  } else {
    await wait(() => heading() === '欢迎使用 NeoUI');
    await next();
    await wait(() => ['迁移旧数据', '常用设置'].includes(heading()));
    if (heading() === '迁移旧数据') {
      while (button('跳过')) {
        await wait(() => !button('跳过').disabled);
        const count = card().querySelectorAll('.migration-list .migration-item').length;
        button('跳过').click();
        await wait(() => card().querySelectorAll('.migration-list .migration-item').length < count);
      }
      await next();
    }
    await wait(() => heading() === '常用设置');
    check(card().classList.contains('pcl-dialog') === (config.shell === 'pcl'), 'Wrong wizard shell');
    check(card().querySelector('img[alt="Luna Launcher"]')?.naturalWidth > 0, 'Launcher logo missing');
    if (config.shell === 'pcl') {
      check(card().querySelectorAll('.pcl-select').length >= 6 && card().querySelector('.pcl-check'), 'PCL controls missing');
      const background = getComputedStyle(card()).backgroundColor;
      const accent = getComputedStyle(card()).getPropertyValue('--setup-accent');
      await choose('preset', 'PCL');
      await wait(() => getComputedStyle(card()).getPropertyValue('--setup-accent') !== accent);
      check(JSON.parse(localStorage.getItem('luna-neo-ui.preferences')).pclLoadingAnimation === 'pcl-pickaxe', 'PCL preset was not applied');
      const darkHighlight = getComputedStyle(card()).getPropertyValue('--pcl-c2');
      await choose('color', '浅色');
      await wait(() => document.documentElement.dataset.neoTheme === 'light');
      check(getComputedStyle(card()).backgroundColor !== background, 'Wizard background did not follow light theme');
      check(getComputedStyle(card()).getPropertyValue('--pcl-c2') !== darkHighlight, 'Light theme kept the dark highlight palette');
      check(getComputedStyle(card()).color !== 'rgb(229, 231, 235)', 'Wizard kept hard-coded dark text');
      layout();
      await choose('preset', 'Luna Neo（默认）');
      await wait(() => getComputedStyle(card()).getPropertyValue('--setup-accent') === accent);
      check(JSON.parse(localStorage.getItem('luna-neo-ui.preferences')).pclLoadingAnimation === 'luna-spinner', 'Luna preset was not applied');
      await choose('color', '深色');
      await wait(() => document.documentElement.dataset.neoTheme === 'dark');
      check(getComputedStyle(card()).backgroundColor === background, 'Wizard background did not return to dark theme');
      await choose('color', '跟随系统');
      await wait(() => document.documentElement.dataset.neoTheme === (matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light'));
      layout();
    }
    const autoDownload = toggle('需要时自动下载 Java');
    if (checked(autoDownload)) autoDownload.click();
    await wait(() => toggle('稍后配置 Java'));
    if (!checked(toggle('稍后配置 Java'))) toggle('稍后配置 Java').click();
    setInput(memory('最小内存'), '2048');
    setInput(memory('最大内存'), '1024');
    await next();
    await wait(() => card().textContent.includes('最大内存不能小于最小内存'));
    check(heading() === '常用设置', 'Invalid memory advanced the wizard');
    setInput(memory('最大内存'), '8192');
    layout();
    await next();
    await wait(() => heading() === '添加账户');
    button('上一步').click();
    await wait(() => heading() === '常用设置');
    check(memory('最大内存').value === '8192', 'Back navigation lost preferences');
    await next();
    await wait(() => heading() === '添加账户');
    await next();
    await wait(() => heading() === '设置完成');
    button('开始使用').click();
    await wait(() => !card());
    const status = await ok('startup.status');
    check(status.setupComplete && !status.needsSetup && !status.migrations.length, 'Setup did not finish');
    const settings = await ok('settings.list', {scope:'launcher'});
    check(settings.find(item => item.key === 'MaxMemAlloc')?.value === 8192, 'Memory was not saved');
    check(settings.find(item => item.key === 'IgnoreJavaWizard')?.value === true, 'Deferred Java was not saved');
    await report({stage:'complete'});
  }
  await window.__TAURI__.window.getCurrentWindow().close();
}
run().catch(error => report({stage:'error', error:String(error)}));
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--cli', type=Path)
    parser.add_argument('--spa', type=Path, default=Path(__file__).resolve().parents[1] / 'neo-ui/dist/spa')
    args = parser.parse_args()
    executable = args.exe.resolve(strict=True)
    cli = (args.cli or executable.with_name('lunalauncher-cli.exe')).resolve(strict=True)
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
        with temporary_profile(executable.parent) as temporary:
            root = Path(temporary)
            profile = root / 'profile'
            profile.mkdir()
            relative = Path(os.path.relpath(profile, cli.parent))
            sidecar = Sidecar(cli, relative)
            try:
                status = sidecar.ok('startup.status')
                assert Path(status['dataRoot']) == profile
                assert status['firstRun'] and status['needsSetup'] and not status['setupComplete']
                assert all(candidate['canMigrate'] for candidate in status['migrations'])
                assert not sidecar.execute('startup.migrate', name='not-a-source')['ok']
                if status['migrations']:
                    assert not sidecar.execute('startup.complete')['ok']
                for candidate in status['migrations']:
                    sidecar.ok('startup.dismiss-migration', name=candidate['name'])
                    assert (profile / (candidate['name'] + '_nomigrate.txt')).is_file()
                assert not sidecar.ok('startup.status')['migrations']
                print('PASS: native startup status, relative data path, pending-migration guard and skip markers', flush=True)
            finally:
                sidecar.close()

            # Use the real exported SPA, native bridge and backend in both shells.
            for shell in ('pcl', 'neo'):
                profile = root / ('ui-profile-' + shell)
                profile.mkdir()
                ui = profile / 'webui/setup-test'
                shutil.copytree(args.spa, ui)
                index = ui / 'index.html'
                html = index.read_text(encoding='utf-8').replace('<head>', '<head><script src="./setup-preferences.js"></script>')
                index.write_text(html.replace('</body>', '<script src="./setup-probe.js"></script></body>'), encoding='utf-8')
                for reopen, expected in [(False, 'complete'), (True, 'reopen')]:
                    config = {'report':f'http://127.0.0.1:{server.server_port}/', 'reopen':reopen, 'shell':shell}
                    (ui / 'setup-probe.js').write_text(PROBE.replace('__CONFIG__', json.dumps(config)), encoding='utf-8')
                    preferences = {'shell':shell, 'theme':'dark', 'pclStylePreset':'luna-neo'}
                    (ui / 'setup-preferences.js').write_text('' if reopen else
                        'localStorage.setItem("luna-neo-ui.preferences", ' + json.dumps(json.dumps(preferences)) + ');', encoding='utf-8')
                    environment = dict(os.environ, LUNA_LAUNCHER_CLI=str(cli), LUNA_LAUNCHER_DATA=str(profile),
                                       LUNA_WEB_UI='data/setup-test', WEBVIEW2_USER_DATA_FOLDER=str(root / ('webview-' + shell)))
                    environment.pop('LUNALAUNCHER_DATA_DIR', None)
                    process = subprocess.Popen([str(executable)], cwd=executable.parent, env=environment,
                                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                    try:
                        def received():
                            try:
                                event = events.get_nowait()
                            except queue.Empty:
                                return False
                            assert event['stage'] == expected, event
                            return True
                        wait_until(received, process, expected, timeout=90)
                        assert process.wait(timeout=15) == 0
                        print(f'PASS: {shell} wizard {expected}, theme/controls, Java deferral, memory validation, navigation and persistence', flush=True)
                    finally:
                        if process.poll() is None:
                            subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                            process.wait(timeout=15)
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)


if __name__ == '__main__':
    main()
