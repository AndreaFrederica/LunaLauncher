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
const invoke = (...args) => window.__TAURI__.core.invoke(...args);
const ok = async (operation, parameters = {}) => {
  const result = await invoke('launcher_request', {
    method:'launcher/execute', parameters:{operation, parameters},
  });
  check(result.ok, operation + ': ' + JSON.stringify(result));
  return result.data;
};
const wait = async predicate => {
  const deadline = Date.now() + 30000;
  while (!await predicate()) {
    if (Date.now() > deadline) throw new Error('Wizard did not reach expected state: ' + predicate.toString() + '; heading=' + heading() + '; error=' + card()?.querySelector('.setup-error')?.textContent);
    await new Promise(resolve => setTimeout(resolve, 60));
  }
};
const card = () => document.querySelector('.neo-setup-card');
const heading = () => card()?.querySelector('h3')?.textContent;
const stage = () => card()?.dataset.setupStage;
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
    await wait(() => heading() === '迁移旧数据');
    while (button('跳过')) {
      await wait(() => !button('跳过').disabled);
      const count = card().querySelectorAll('.migration-list .migration-item').length;
      button('跳过').click();
      await wait(() => card().querySelectorAll('.migration-list .migration-item').length < count);
    }
    const source = card().querySelector('#setup-migration-source');
    const migrate = button('迁移');
    check(source && button('浏览') && migrate.disabled, 'Custom migration controls are missing');
    // Model a config written after the wizard first displayed the migration step.
    await ok('settings.set', {scope:'launcher', key:'Language', value:'zh_CN'});
    setInput(source, config.profile);
    await wait(() => !migrate.disabled);
    migrate.click();
    await wait(() => card().querySelector('.setup-error'));
    setInput(source, config.source);
    await wait(() => !migrate.disabled);
    migrate.click();
    await wait(() => card().textContent.includes('迁移数据。') && !card().querySelector('.setup-error'));
    check((await ok('instance.list')).some(instance => instance.id === 'migration-fixture'), 'Migrated instance is missing from the live backend');
    check(card().querySelector('#setup-migration-source').value === '', 'Source input was not cleared');
    const migratedSettings = await ok('settings.list', {scope:'launcher'});
    check(Number(migratedSettings.find(item => item.key === 'MinMemAlloc')?.value) === 768, 'Legacy setting was not imported');
    check(migratedSettings.find(item => item.key === 'Language')?.value === 'zh_CN', 'Existing Luna language was overwritten');
    await next();
    await wait(() => heading() === '常用设置');
    check(memory('最小内存')?.value === '768', 'Preferences did not refresh after migration');
    check(card().classList.contains('pcl-dialog') === (config.shell === 'pcl'), 'Wrong wizard shell');
    check(card().querySelector('img[alt="Luna Launcher"]')?.naturalWidth > 0, 'Launcher logo missing');
    if (config.shell === 'pcl') {
      check(card().querySelectorAll('.pcl-select').length >= 6 && card().querySelector('.pcl-check'), 'PCL controls missing');
      const background = getComputedStyle(card()).backgroundColor;
      const accent = getComputedStyle(card()).getPropertyValue('--setup-accent');
      await choose('preset', 'PCL');
      await wait(() => getComputedStyle(card()).getPropertyValue('--setup-accent') !== accent);
      await wait(async () => (await invoke('get_neo_ui_preferences'))?.pclLoadingAnimation === 'pcl-pickaxe');
      const darkHighlight = getComputedStyle(card()).getPropertyValue('--pcl-c2');
      await choose('color', '浅色');
      await wait(() => document.documentElement.dataset.neoTheme === 'light');
      check(getComputedStyle(card()).backgroundColor !== background, 'Wizard background did not follow light theme');
      check(getComputedStyle(card()).getPropertyValue('--pcl-c2') !== darkHighlight, 'Light theme kept the dark highlight palette');
      check(getComputedStyle(card()).color !== 'rgb(229, 231, 235)', 'Wizard kept hard-coded dark text');
      layout();
      await choose('preset', 'Luna Neo（默认）');
      await wait(() => getComputedStyle(card()).getPropertyValue('--setup-accent') === accent);
      await wait(async () => (await invoke('get_neo_ui_preferences'))?.pclLoadingAnimation === 'luna-spinner');
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
    await wait(() => stage() === 'account');
    (button('上一步') || button('Previous step')).click();
    await wait(() => stage() === 'preferences');
    check(memory('最大内存').value === '8192', 'Back navigation lost preferences');
    await next();
    await wait(() => stage() === 'account');
    await next();
    await wait(() => stage() === 'finish');
    (button('开始使用') || button('Get started')).click();
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
    os.environ['LUNA_DISABLE_BACKGROUND_DOWNLOADS'] = '1'
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
            source = root / '旧 Prism 配置'
            source.mkdir()
            external = root / 'external-instances'
            fixture = external / 'migration-fixture'
            fixture.mkdir(parents=True)
            (fixture / 'instance.cfg').write_text(
                '[General]\nConfigVersion=1.3\nInstanceType=OneSix\nname=Migration Fixture\n', encoding='utf-8'
            )
            (fixture / 'mmc-pack.json').write_text(json.dumps({
                'formatVersion': 1, 'components': [{'uid': 'net.minecraft', 'version': '1.20.1'}],
            }), encoding='utf-8')
            (source / 'prismlauncher.cfg').write_text(
                f'[General]\nConfigVersion=1.3\nInstanceDir={external.as_posix()}\nMinMemAlloc=768\nLanguage=en_US\n', encoding='utf-8'
            )
            (source / 'assets').mkdir()
            (source / 'assets' / 'existing.txt').write_text('Prism', encoding='utf-8')
            (source / 'assets' / 'new.txt').write_text('imported asset', encoding='utf-8')
            (source / 'accounts.json').write_text('{"formatVersion":3,"accounts":[]}', encoding='utf-8')
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
                (profile / 'assets').mkdir(exist_ok=True)
                (profile / 'assets' / 'existing.txt').write_text('Luna', encoding='utf-8')
                duplicate = profile / 'instances' / fixture.name
                duplicate.mkdir(parents=True)
                (duplicate / 'keep.txt').write_text('keep whole directory', encoding='utf-8')
                sidecar.ok('settings.set', scope='launcher', key='Language', value='zh_CN')
                for _ in range(2):
                    sidecar.ok('startup.migrate', source=str(source))
                assert (profile / 'assets' / 'existing.txt').read_text(encoding='utf-8') == 'Luna'
                assert (profile / 'assets' / 'new.txt').read_text(encoding='utf-8') == 'imported asset'
                assert (profile / 'accounts.json').read_bytes() == (source / 'accounts.json').read_bytes()
                assert sorted(p.name for p in duplicate.iterdir()) == ['keep.txt']
                print('PASS: idempotent merge with existing settings, asset and instance directory', flush=True)
            finally:
                sidecar.close()

            # Use the real exported SPA, native bridge and backend in both shells.
            for shell in ('pcl', 'neo'):
                profile = root / ('ui-profile-' + shell)
                profile.mkdir()
                ui = profile / 'webui/setup-test'
                shutil.copytree(args.spa, ui)
                preferences = {'shell':shell, 'theme':'dark', 'pclStylePreset':'luna-neo'}
                (ui.parent / 'neo-ui-preferences.json').write_text(json.dumps(preferences), encoding='utf-8')
                index = ui / 'index.html'
                html = index.read_text(encoding='utf-8').replace('<head>', '<head><script src="./setup-preferences.js"></script>')
                index.write_text(html.replace('</body>', '<script src="./setup-probe.js"></script></body>'), encoding='utf-8')
                for reopen, expected in [(False, 'complete'), (True, 'reopen')]:
                    config = {'report':f'http://127.0.0.1:{server.server_port}/', 'reopen':reopen,
                              'shell':shell, 'source':str(source), 'profile':str(profile)}
                    (ui / 'setup-probe.js').write_text(PROBE.replace('__CONFIG__', json.dumps(config)), encoding='utf-8')
                    (ui / 'setup-preferences.js').write_text('' if reopen else
                        'localStorage.setItem("luna-neo-ui-locale", "zh-CN");', encoding='utf-8')
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
                        assert (profile / 'instances' / fixture.name / 'instance.cfg').is_file()
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
