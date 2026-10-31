// Browser storage and native bridge contracts, without running installs or deleting files.
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const assert = require('node:assert/strict');
const path = require('node:path');
const fs = require('node:fs');
const base = process.env.MELODY_URL || 'http://127.0.0.1:4179/';
const output = path.resolve(__dirname, '../../temp/melody-verification');
fs.mkdirSync(output, { recursive: true });

function nativeMock(backgrounds = {}) {
  window.isTauri = true;
  window.__TAURI_EVENT_PLUGIN_INTERNALS__ = { unregisterListener() {} };
  localStorage.setItem('luna-neo-ui-locale', 'zh-CN');
  window.calls = [];
  window.backgroundNames = Object.keys(backgrounds);
  const callbacks = new Map();
  let next = 1;
  const clients = ['alpha', 'beta'].map(id => ({ id, name: `客户端 ${id}`, type: 'Minecraft', notes: '', runtime: { minecraftVersion: '1.21.1' } }));
  window.__TAURI_INTERNALS__ = {
    metadata: { currentWindow: { label: 'melody-test' }, currentWebview: { label: 'melody-test' } },
    transformCallback: cb => { const id = next++; callbacks.set(id, cb); return id; },
    unregisterCallback: id => callbacks.delete(id),
    invoke: async (command, args) => {
      window.calls.push({ command, args });
      if (command === 'plugin:dialog|open') return 'D:/背景图片';
      if (command === 'list_melody_backgrounds') return window.backgroundNames;
      if (command === 'read_melody_background') return Uint8Array.from(atob(backgrounds[args.name] || ''), c => c.charCodeAt(0)).buffer;
      if (command === 'plugin:event|listen') return next++;
      if (command !== 'launcher_request') return null;
      const { operation: op, parameters: p = {} } = args.parameters;
      let data = {};
      if (op === 'instance.list') data = clients;
      else if (op === 'instance.info') data = { ...clients.find(item => item.id === p.instance), root: 'D:/Games/' + p.instance, gameRoot: 'D:/Games/' + p.instance + '/.minecraft' };
      else if (op === 'instance.components.list') data = { components: [{ id: 'net.minecraft', name: 'Minecraft', version: '1.21.1', enabled: true }] };
      else if (op === 'instance.log.list' || op === 'account.list' || op === 'instance.resources.list') data = [];
      else if (op === 'settings.list') data = { entries: [{ key: 'MaxMemAlloc', type: 'int', value: 4096, default: 4096 }, { key: 'MinMemAlloc', type: 'int', value: 512, default: 512 }, { key: 'ShowConsole', type: 'bool', value: false, default: false }] };
      else if (op === 'runtime.memory') data = { total: 17179869184, available: 8589934592 };
      else if (op === 'neo.task.list') data = { tasks: [] };
      else if (op === 'component.versions') data = { versions: [{ version: '1.21.1', type: 'release' }, { version: '1.20.1', type: 'release' }] };
      else if (op === 'neo.task.start') data = { taskId: 'mock-task', state: 'succeeded', finished: true, createdInstanceId: 'new-game' };
      else if (op === 'instance.rename') clients.find(item => item.id === p.instance).name = p.name;
      else if (op === 'instance.set-notes') clients.find(item => item.id === p.instance).notes = p.notes;
      else if (op.endsWith('.subscribe')) data = { subscriptionId: 'sub-' + next++ };
      else if (op === 'language.list') data = { languages: [] };
      return { ok: true, data };
    },
  };
}

(async () => {
  const browser = await chromium.launch({ headless: true, executablePath: process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE });
  try {
    const context = await browser.newContext({ viewport: { width: 1440, height: 1000 }, locale: 'zh-CN' });
    const page = await context.newPage();
    const errors = [];
    function record(page) {
      page.on('pageerror', e => errors.push(e.message));
      page.on('console', m => { if (m.type() === 'error' && !m.text().includes('Failed to load resource')) errors.push(m.text()); });
    }
    record(page);
    await page.goto(base + '?theme=melody');
    const launcher = page.locator('[data-window="launcher"]');
    await launcher.getByRole('button', { name: '设置', exact: true }).click();
    const settings = page.locator('[data-window="preferences"]');
    await settings.getByLabel('Luna Launcher Neo UI', { exact: true }).check();
    await page.waitForFunction(() => document.querySelector('.melody-launcher__footer span')?.textContent === 'Luna Launcher Neo UI');
    await settings.getByLabel('自定义背景图片').setInputFiles(path.resolve(__dirname, '../src/melody/assets/sky.jpg'));
    await page.waitForFunction(() => document.querySelector('.melody-launcher__background')?.getAttribute('src')?.startsWith('blob:'));
    await settings.screenshot({ path: path.join(output, 'modern-settings.png') });
    const other = await context.newPage(); record(other);
    await other.goto(base + '?theme=melody');
    await other.waitForFunction(() => document.querySelector('.melody-launcher__background')?.getAttribute('src')?.startsWith('blob:'));
    assert.equal(await other.locator('.melody-launcher__footer span').innerText(), 'Luna Launcher Neo UI');
    await settings.getByLabel('图片适配').selectOption('contain');
    await other.waitForFunction(() => document.querySelector('.melody-launcher__background')?.style.objectFit === 'contain');
    await settings.getByRole('button', { name: '恢复原版背景' }).click();
    await other.waitForFunction(() => !document.querySelector('.melody-launcher__background')?.getAttribute('src')?.startsWith('blob:'));
    await page.reload();
    await launcher.getByRole('button', { name: '设置', exact: true }).click();
    assert(await settings.getByLabel('Luna Launcher Neo UI', { exact: true }).isChecked());
    await settings.getByLabel('自定义背景图片').setInputFiles({ name: 'broken.png', mimeType: 'image/png', buffer: Buffer.from('not an image') });
    await settings.getByRole('alert').waitFor();
    assert(!(await launcher.locator('img').getAttribute('src')).startsWith('blob:'));
    await other.close();

    // Appearance controls remain live across windows and after a restart.
    assert.deepEqual(await settings.getByLabel('主题', { exact: true }).locator('option').evaluateAll(nodes => nodes.map(n => n.value)), ['neo', 'pcl', 'melody']);
    await settings.getByLabel('自定义文字', { exact: true }).check();
    await settings.getByLabel('文字内容').fill('我的旋律 · Minecraft');
    await settings.getByLabel('字号', { exact: false }).fill('18');
    await settings.getByLabel('颜色', { exact: true }).fill('#e1f4ff');
    await page.waitForFunction(() => document.querySelector('.melody-launcher__footer span')?.textContent === '我的旋律 · Minecraft');
    assert.equal(await launcher.locator('footer span').evaluate(el => getComputedStyle(el).fontSize), '18px');
    const fixture = fs.mkdtempSync(path.join(output, 'backgrounds-'));
    try {
      fs.copyFileSync(path.resolve(__dirname, '../src/melody/assets/sky.jpg'), path.join(fixture, '天空.jpg'));
      fs.copyFileSync(path.resolve(__dirname, '../src/melody/assets/minecraft-water.jpg'), path.join(fixture, '水面.jpg'));
      fs.writeFileSync(path.join(fixture, '坏图.png'), 'broken');
      fs.writeFileSync(path.join(fixture, 'notes.txt'), 'not an image');
      await settings.getByLabel('背景图片目录').setInputFiles(fixture);
      await page.waitForFunction(() => JSON.parse(localStorage.getItem('luna-melody.appearance')).backgroundMode === 'random' && document.querySelector('.melody-launcher__background')?.getAttribute('src')?.startsWith('blob:'));
      await settings.getByText(/2 张图片/).waitFor();
      const current = await page.evaluate(() => JSON.parse(localStorage.getItem('luna-melody.background-selection')).name);
      await settings.getByRole('button', { name: '换一张', exact: true }).click();
      await page.waitForFunction(previous => JSON.parse(localStorage.getItem('luna-melody.background-selection')).name !== previous, current);
      const beforeRestart = await page.evaluate(() => JSON.parse(localStorage.getItem('luna-melody.background-selection')).name);
      await page.reload();
      await page.waitForFunction(previous => JSON.parse(localStorage.getItem('luna-melody.background-selection')).name !== previous && document.querySelector('.melody-launcher__background')?.getAttribute('src')?.startsWith('blob:'), beforeRestart);
      assert.equal(await launcher.locator('footer span').innerText(), '我的旋律 · Minecraft');
      await launcher.getByRole('button', { name: '设置', exact: true }).click();
      const preview = await context.newPage(); record(preview);
      await preview.goto(base + '?theme=melody&melodyWindow=preferences');
      await preview.getByText(/2 张图片/).waitFor();
      const choice = await page.evaluate(() => localStorage.getItem('luna-melody.background-selection'));
      await preview.getByRole('button', { name: '换一张', exact: true }).click();
      await page.waitForFunction(old => localStorage.getItem('luna-melody.background-selection') !== old, choice);
      const shownName = await preview.evaluate(() => JSON.parse(localStorage.getItem('luna-melody.background-selection')).name);
      await settings.getByText(shownName, { exact: true }).waitFor();
      await settings.getByLabel('主题', { exact: true }).scrollIntoViewIfNeeded();
      await settings.screenshot({ path: path.join(output, 'appearance-directory.png') });
      await settings.getByLabel('文字内容').scrollIntoViewIfNeeded();
      await settings.screenshot({ path: path.join(output, 'appearance-footer.png') });
      await preview.close();
      await settings.getByRole('button', { name: '恢复原版背景' }).click();
      await page.waitForFunction(() => !document.querySelector('.melody-launcher__background')?.getAttribute('src')?.startsWith('blob:'));
    } finally { fs.rmSync(fixture, { recursive: true, force: true }); }

    // Both theme options persist and leave the standalone Melody URL.
    for (const shell of ['neo', 'pcl']) {
      const switcher = await context.newPage();
      await switcher.goto(base + '?theme=melody&melodyWindow=preferences');
      await switcher.getByLabel('主题', { exact: true }).selectOption(shell);
      await switcher.waitForURL(url => !url.searchParams.has('theme') && !url.searchParams.has('melodyWindow'));
      assert.equal(await switcher.evaluate(() => JSON.parse(localStorage.getItem('luna-neo-ui.preferences')).shell), shell);
      await switcher.close();
    }

    const native = await browser.newContext({ viewport: { width: 1100, height: 720 }, locale: 'zh-CN' });
    await native.addInitScript(nativeMock, {
      '水面.jpg': fs.readFileSync(path.resolve(__dirname, '../src/melody/assets/minecraft-water.jpg')).toString('base64'),
      '天空.jpg': fs.readFileSync(path.resolve(__dirname, '../src/melody/assets/sky.jpg')).toString('base64'),
    });
    const app = await native.newPage(); record(app);
    await app.goto(base + '?theme=melody&melodyWindow=preferences');
    await app.getByRole('button', { name: '选择目录', exact: true }).click();
    await app.getByText(/2 张图片/).waitFor();
    await app.waitForFunction(() => document.querySelector('.melody-background-preview')?.getAttribute('src')?.startsWith('blob:'));
    const nativeFirst = await app.evaluate(() => JSON.parse(localStorage.getItem('luna-melody.background-selection')).name);
    await app.getByRole('button', { name: '换一张', exact: true }).click();
    await app.waitForFunction(name => JSON.parse(localStorage.getItem('luna-melody.background-selection')).name !== name, nativeFirst);
    await app.evaluate(() => { window.backgroundNames = ['天空.jpg', '损坏.png']; });
    await app.getByRole('button', { name: '换一张', exact: true }).click();
    await app.waitForFunction(() => JSON.parse(localStorage.getItem('luna-melody.background-selection')).name === '天空.jpg');
    await app.evaluate(() => { window.backgroundNames = []; });
    await app.getByRole('button', { name: '换一张', exact: true }).click();
    await app.getByRole('alert').filter({ hasText: '目录中没有可用' }).waitFor();
    assert(!(await app.locator('.melody-background-preview').getAttribute('src')).startsWith('blob:'));
    await app.getByRole('button', { name: '恢复原版背景' }).click();
    const nav = app.getByRole('navigation', { name: '设置分类' });
    for (const label of ['账户', '游戏启动', '内存', 'Java', '游戏设置', '高级启动', '联机服务', '下载设置', '文件与路径', '缓存', '控制台', '启动器与代理', '高级设置', '语言', '关于', '检查更新', '反馈', '启动器日志']) {
      await nav.getByRole('button', { name: label, exact: true }).click();
      await app.locator('.pcl-setup-page').waitFor();
      assert((await app.locator('.melody-modern-main').innerText()).trim().length > 2, label + ' must render content');
    }
    await nav.getByRole('button', { name: '内存', exact: true }).click();
    await app.locator('[data-window="preferences"]').screenshot({ path: path.join(output, 'modern-memory.png') });
    // Persist a real setting through the adapted page.
    await nav.getByRole('button', { name: '游戏启动', exact: true }).click();
    const check = app.locator('.pcl-settings-section:visible input[type="checkbox"]').first();
    await check.setChecked(!(await check.isChecked()), { force: true });
    await app.waitForFunction(() => window.calls.some(call => call.args?.parameters?.operation === 'settings.set'));

    await app.goto(base + '?theme=melody&melodyWindow=clients');
    await app.getByRole('button', { name: '管理客户端' }).first().click();
    await app.getByRole('button', { name: '管理客户端' }).nth(1).click();
    const opens = await app.evaluate(() => window.calls.filter(c => c.command === 'open_melody_window'));
    assert.deepEqual(opens.map(c => c.args.instanceId), ['alpha', 'beta']);
    await app.locator('[data-window="clients"]').screenshot({ path: path.join(output, 'modern-clients.png') });
    await app.goto(base + '?theme=melody&melodyWindow=client&instance=alpha');
    await app.locator('.pcl-instance-page').waitFor();
    await app.locator('[data-window="client"]').screenshot({ path: path.join(output, 'modern-client.png') });
    const notes = app.locator('.pcl-overview textarea');
    await notes.fill('Melody 测试备注');
    await app.getByRole('button', { name: '保存笔记', exact: true }).click();
    await app.waitForFunction(() => window.calls.some(c => c.args?.parameters?.operation === 'instance.set-notes' && c.args.parameters.parameters.instance === 'alpha'));
    await app.getByRole('button', { name: '重命名', exact: true }).click();
    await app.getByLabel('客户端名称').fill('新的客户端名称');
    await app.getByRole('button', { name: '保存', exact: true }).click();
    await app.waitForFunction(() => document.querySelector('.melody-window__titlebar')?.textContent?.includes('新的客户端名称'));

    await app.goto(base + '?theme=melody&melodyWindow=games');
    await app.getByText('1.21.1', { exact: true }).first().waitFor();
    await app.locator('[data-window="games"]').screenshot({ path: path.join(output, 'modern-games.png') });
    await app.getByText('1.21.1', { exact: true }).first().click();
    await app.getByRole('button', { name: '开始下载', exact: true }).waitFor();
    await app.locator('[data-window="games"]').screenshot({ path: path.join(output, 'modern-install.png') });
    await app.getByRole('button', { name: '开始下载', exact: true }).click();
    await app.waitForFunction(() => window.calls.some(c => c.args?.parameters?.operation === 'neo.task.start' && c.args.parameters.parameters.operation === 'instance.create'));
    const install = await app.evaluate(() => window.calls.find(c => c.args?.parameters?.operation === 'neo.task.start').args.parameters.parameters);
    assert.equal(install.parameters.version, '1.21.1');
    await app.goto(base + '?theme=melody&melodyWindow=games&instance=alpha&component=resource%3Amods');
    await app.locator('.pcl-resource-catalog-view').waitFor();
    assert.equal(await app.locator('.pcl-page-nav__item.active').innerText(), '模组');
    assert.deepEqual(errors, [], 'No render exceptions');
    console.log('Melody modern: theme switching, editable footer, directory shuffle/restart/sync, native directory bridge and fallback, settings, clients and downloads passed.');
  } finally { await browser.close(); }
})().catch(e => { console.error(e); process.exitCode = 1; });
