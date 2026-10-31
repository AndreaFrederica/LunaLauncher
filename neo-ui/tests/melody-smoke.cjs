// Run against a built frontend; PLAYWRIGHT_MODULE may point to a shared runtime.
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const base = process.env.MELODY_URL || 'http://127.0.0.1:4179/';
const output = path.resolve(__dirname, '../../temp/melody-verification');
fs.mkdirSync(output, { recursive: true });

(async () => {
  const browser = await chromium.launch({ headless: true, executablePath: process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE });
  try {
    const page = await browser.newPage({ viewport: { width: 1440, height: 1000 }, deviceScaleFactor: 1 });
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    await page.goto(base + '?theme=melody');
    const launcher = page.locator('[data-window="launcher"]');
    await launcher.waitFor();
    await launcher.screenshot({ path: path.join(output, 'launcher.png') });
    assert.equal((await launcher.boundingBox()).width, 720);
    await page.mouse.click(8, 8, { button: 'right' });
    await page.getByRole('navigation', { name: '窗口与主题' }).getByRole('button', { name: '设置', exact: true }).click();
    const settings = page.locator('[data-window="settings"]');
    await settings.waitFor();
    assert.equal(await launcher.count(), 1, 'Opening settings must keep the launcher mounted');
    await settings.screenshot({ path: path.join(output, 'settings.png') });
    await settings.getByLabel('游戏名字', { exact: true }).fill('Melody Test');
    await page.mouse.click(8, 8, { button: 'right' });
    await page.getByRole('navigation', { name: '窗口与主题' }).getByRole('button', { name: '设置', exact: true }).click();
    assert.equal(await settings.count(), 1, 'Reopening a window must reuse it');
    assert.equal(await settings.getByLabel('游戏名字', { exact: true }).inputValue(), 'Melody Test', 'Reopening must retain the draft');
    const memory = settings.getByRole('radiogroup', { name: '内存设置' });
    await memory.getByLabel('手动', { exact: true }).check();
    assert.equal(await memory.locator('input:checked').count(), 1);
    await memory.getByLabel('自动设置', { exact: true }).check();
    assert.equal(await memory.locator('input:checked').count(), 1);
    const skin = settings.getByRole('radiogroup', { name: '启动器皮肤' });
    await skin.getByLabel('默认皮肤').check();
    assert.equal(await skin.locator('input:checked').count(), 1);
    const directory = settings.getByRole('radiogroup', { name: '游戏目录' });
    await directory.locator('input').nth(1).check();
    assert.equal(await directory.locator('input:checked').count(), 1);
    const box = await settings.boundingBox();
    await page.mouse.move(box.x + 150, box.y + 10);
    await page.mouse.down();
    await page.mouse.move(box.x + 230, box.y + 40, { steps: 5 });
    await page.mouse.up();
    assert.equal(Math.round((await settings.boundingBox()).x - box.x), 80, 'Window can move independently');
    await settings.getByRole('button', { name: '保存设置' }).click();
    const message = page.locator('[data-window="message-settings"]');
    await message.waitFor();
    assert.equal(await settings.getAttribute('inert'), '');
    await page.screenshot({ path: path.join(output, 'independent-windows.png') });
    await message.getByRole('button', { name: '确认' }).click();
    assert.equal(await settings.getAttribute('inert'), null);
    assert.equal(await page.evaluate(() => document.activeElement?.textContent), '保存设置');
    await settings.getByRole('button', { name: '关闭', exact: true }).click();
    await page.mouse.click(8, 8, { button: 'right' });
    await page.getByRole('navigation', { name: '窗口与主题' }).getByRole('button', { name: '设置', exact: true }).click();
    assert.equal(await settings.getByLabel('游戏名字', { exact: true }).inputValue(), 'Melody Test');
    assert.equal(await settings.getByLabel('默认皮肤').isChecked(), true);
    await settings.getByRole('button', { name: '关闭', exact: true }).click();
    await page.mouse.click(8, 8, { button: 'right' });
    await page.getByRole('navigation', { name: '窗口与主题' }).getByRole('button', { name: '版本管理', exact: true }).click();
    const manager = page.locator('[data-window="manager"]');
    await manager.getByLabel('官方原版').check();
    assert.equal(await manager.getByRole('button', { name: '更新游戏', exact: true }).isDisabled(), true);
    await manager.getByLabel('简单整合').check();
    assert.equal(await manager.getByRole('button', { name: '更新游戏', exact: true }).isDisabled(), false);
    await manager.screenshot({ path: path.join(output, 'manager.png') });
    for (const kind of ['message', 'download', 'setup', 'components']) {
      await page.goto(base + '?theme=melody&melodyWindow=' + kind + '&message=' + encodeURIComponent('没有选择游戏,请选择或点击更新按钮更新游戏'));
      const window = page.locator('[data-window="' + kind + '"]');
      await window.waitFor();
      await window.screenshot({ path: path.join(output, kind + '.png') });
      const overflow = await window.evaluate(root => [...root.querySelectorAll('button,input,select,fieldset')].filter(el => {
        const r = el.getBoundingClientRect(), p = root.getBoundingClientRect();
        return r.width && (r.right > p.right + 1 || r.bottom > p.bottom + 1);
      }).map(el => el.outerHTML));
      assert.deepEqual(overflow, [], kind + ' must not clip controls');
    }
    assert(!/离线主题预览|不会启动旧|每个页面都是独立窗口/.test(await page.locator('body').innerText()));
    // The original message window grows for long text, then wraps/scrolls at
    // its maximum width; actions must stay reachable in both cases.
    const dialogWidths = [];
    const dialogCases = [
      ['short', '设置已保存'],
      ['java', '没有找到java路径,请手动指定javaw.exe'],
      ['long', '开启调试模式将以java.exe启动minecraft,并且在启动器目录生成输出日志.txt'],
      ['overflow', '长消息需要保留换行和完整内容。\n'.repeat(40)],
      ['wide', 'C:/a-very-long-path-without-any-spaces/'.repeat(80)],
    ];
    for (const [name, text] of dialogCases) {
      await page.goto(base + '?theme=melody&melodyWindow=message&message=' + encodeURIComponent(text));
      const dialog = page.locator('[data-window="message"]');
      await dialog.waitFor();
      dialogWidths.push((await dialog.boundingBox()).width);
      await dialog.screenshot({ path: path.join(output, 'dialog-' + name + '.png') });
      const body = dialog.locator('.melody-dialog__content');
      assert.equal(await body.evaluate(el => el.scrollWidth <= el.clientWidth), true, 'Message must not scroll sideways');
      const confirm = await dialog.getByRole('button', { name: '确认' }).boundingBox();
      const frame = await dialog.boundingBox();
      assert(confirm.y + confirm.height < frame.y + frame.height);
      if (name === 'overflow') assert(await body.evaluate(el => el.scrollHeight > el.clientHeight), 'Long message scrolls without hiding actions');
      for (let step = 0; step < 4; step++) {
        await page.keyboard.press('Tab');
        if (await page.evaluate(() => document.activeElement?.textContent === '确认')) break;
      }
      assert.equal(await page.evaluate(() => document.activeElement?.textContent), '确认');
      await page.keyboard.press('Enter');
      await dialog.waitFor({ state: 'detached' });
    }
    assert(dialogWidths[2] > dialogWidths[0], 'Long prompts should widen instead of using the short-message box');
    assert.equal(dialogWidths[4], 900, 'Unbounded text must not create an unbounded native window');
    await page.goto(base + '#/melody');
    await page.locator('[data-window="launcher"]').waitFor();
    assert.equal(await page.locator('.xmcl-app').count(), 0, 'Melody must not mount the default shell');
    assert.deepEqual(errors, []);
    console.log('PASS: independent windows, drag, radio groups, modal focus, persistence, all seven windows, no clipped controls or runtime errors.');
    console.log(output);
  } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
