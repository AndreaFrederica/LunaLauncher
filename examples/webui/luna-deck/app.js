// SPDX-License-Identifier: GPL-3.0-only
(() => {
  const $ = id => document.getElementById(id);
  let instances = [], detailPath = '', notificationTimer;
  const icons = new Map();
  const running = instance => Boolean(instance.lifecycle?.running ?? instance.running);
  function notify(message, error = false) {
    clearTimeout(notificationTimer);
    $('toast').textContent = message;
    $('toast').className = 'toast' + (error ? ' error' : '');
    $('toast').hidden = false;
    notificationTimer = setTimeout(() => $('toast').hidden = true, error ? 7000 : 3500);
  }
  function fail(error) { notify(error.message || String(error), true); }
  function host() { if (!window.luna) throw new Error('请通过 Luna Neo UI 加载此文件夹；浏览器预览无法连接实例。'); return window.luna; }
  async function call(operation, parameters = {}) {
    const result = await host().execute(operation, parameters);
    if (!result.ok) throw new Error(result.error);
    return result.data;
  }
  function element(tag, text, className) {
    const node = document.createElement(tag); node.textContent = text;
    if (className) node.className = className;
    return node;
  }
  function placeholder() {
    const svg = '<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64"><rect width="64" height="64" rx="9" fill="#d6dfc8"/><path d="M12 42h13V29h13V16h14v37H12z" fill="#667f5b"/></svg>';
    return 'data:image/svg+xml,' + encodeURIComponent(svg);
  }
  async function showDetails(instance) {
    try {
      const info = await call('instance.get', { instance: instance.id });
      detailPath = info.paths?.root || '';
      $('detail-content').replaceChildren(element('h2', info.name), element('p', `分组：${info.group || '未分组'}`),
        element('p', `状态：${running(info) ? '正在运行' : '未运行'}`), element('p', `目录：${detailPath || '不可用'}`));
      if (info.notes) $('detail-content').append(element('p', info.notes));
      $('detail-folder').disabled = !detailPath;
      $('details').showModal();
    } catch (error) { fail(error); }
  }
  function renderInstances() {
    const query = $('search').value.trim().toLocaleLowerCase();
    const group = $('group').value;
    const visible = instances.filter(i => (!group || (i.group || '未分组') === group) && i.name.toLocaleLowerCase().includes(query));
    $('instances').replaceChildren();
    for (const [index, instance] of visible.entries()) {
      const card = element('article', '', 'instance-card');
      const top = element('div', '', 'instance-card-top');
      const image = document.createElement('img'); image.className = 'instance-icon'; image.alt = instance.name;
      image.src = icons.get(instance.id) || placeholder(); image.dataset.instance = instance.id;
      top.append(image, element('span', running(instance) ? '● 正在运行' : '未运行', 'instance-state' + (running(instance) ? ' running' : '')));
      const footer = element('div', '', 'card-footer');
      const button = element('button', '查看实例 ↗'); button.onclick = () => showDetails(instance);
      footer.append(element('small', `INSTANCE / ${String(index + 1).padStart(2, '0')}`), button);
      card.append(top, element('h3', instance.name), element('p', instance.group || '未分组'), footer);
      $('instances').append(card);
    }
    if (!visible.length) $('instances').append(element('div', instances.length ? '没有匹配的实例。' : '这个数据目录还没有实例。', 'empty'));
  }
  async function loadInstances() {
    $('refresh').disabled = true;
    try {
      instances = await call('instance.list');
      const groups = [...new Set(instances.map(i => i.group || '未分组'))].sort();
      $('total-count').textContent = String(instances.length);
      $('nav-count').textContent = String(instances.length);
      $('running-count').textContent = String(instances.filter(running).length);
      $('group-count').textContent = String(groups.length);
      const selected = $('group').value;
      $('group').replaceChildren(new Option('所有分组', ''), ...groups.map(g => new Option(g, g)));
      if (groups.includes(selected)) $('group').value = selected;
      $('connection-label').textContent = '已连接真实后端'; $('connection-dot').classList.add('online');
      renderInstances();
      for (const instance of instances) {
        if (!icons.has(instance.id)) {
          try { const icon = await call('instance.icon', { instance: instance.id }); icons.set(instance.id, icon.dataUrl); }
          catch { continue; }
        }
        for (const image of document.querySelectorAll('img[data-instance]')) if (image.dataset.instance === instance.id) image.src = icons.get(instance.id);
      }
    } catch (error) {
      $('connection-label').textContent = window.luna ? '后端暂不可用' : '浏览器预览';
      $('instances').replaceChildren(element('div', error.message || String(error), 'empty'));
      fail(error);
    } finally { $('refresh').disabled = false; }
  }
  async function loadSettings() {
    const [settings, catalog] = await Promise.all([host().webUiSettings(), host().webUis()]);
    const options = catalog.entries.filter(entry => !entry.error);
    $('startup').replaceChildren(...options.map(entry => new Option(entry.name, entry.id)));
    if (!options.some(entry => entry.id === settings.startupUi)) $('startup').append(new Option(settings.startupUi + '（不可用）', settings.startupUi));
    $('startup').value = settings.startupUi;
    $('remember').checked = settings.rememberLastUi; $('startup').disabled = settings.rememberLastUi;
    $('scan-install').checked = settings.scanInstallDirectory; $('scan-data').checked = settings.scanDataDirectory;
    $('additional').value = settings.additionalDirectories.join('\n'); $('show-manager').checked = settings.showManagerOnStart;
    $('switch-target').replaceChildren(...options.map(entry => new Option(entry.name, entry.id)));
    $('switch-target').value = options.some(entry => entry.id === catalog.active) ? catalog.active : 'builtin';
    $('active-ui').textContent = '当前：' + (catalog.entries.find(entry => entry.id === catalog.active)?.name || catalog.active);
    $('startup-override').textContent = catalog.startupOverride ? `LUNA_WEB_UI=${catalog.startupOverride} 优先于启动设置。清除该环境变量后使用这里的选择。` : '';
    $('roots').replaceChildren(...catalog.roots.map(root => element('p', root.path)));
    $('settings-path').textContent = catalog.settingsPath;
    if (catalog.warning) notify(catalog.warning, true);
  }
  async function saveSettings(patch) {
    const buttons = document.querySelectorAll('#loading-settings button'); buttons.forEach(button => button.disabled = true);
    $('settings-feedback').textContent = '';
    try {
      await host().updateWebUiSettings(patch);
      await loadSettings();
      $('settings-feedback').textContent = '已保存。下次启动按新设置加载；现在切换请使用右侧按钮。';
      notify('界面加载设置已保存');
    } catch (error) { $('settings-feedback').textContent = error.message || String(error); fail(error); }
    finally { buttons.forEach(button => button.disabled = false); }
  }
  async function switchTo(id) { try { await host().switchWebUi(id); } catch (error) { fail(error); } }
  for (const button of document.querySelectorAll('[data-page]')) button.onclick = () => {
    const settings = button.dataset.page === 'settings';
    $('library-page').hidden = settings; $('settings-page').hidden = !settings;
    $('breadcrumb').textContent = settings ? '界面加载设置' : '实例库';
    document.querySelectorAll('[data-page]').forEach(item => item.classList.toggle('active', item === button));
    if (settings) loadSettings().catch(fail);
  };
  $('search').oninput = renderInstances; $('group').onchange = renderInstances;
  $('refresh').onclick = loadInstances;
  $('remember').onchange = () => $('startup').disabled = $('remember').checked;
  $('loading-settings').onsubmit = event => { event.preventDefault(); void saveSettings({
    rememberLastUi: $('remember').checked, startupUi: $('startup').value,
    scanInstallDirectory: $('scan-install').checked, scanDataDirectory: $('scan-data').checked,
    additionalDirectories: $('additional').value.split('\n').map(line => line.trim()).filter(Boolean),
    showManagerOnStart: $('show-manager').checked,
  }); };
  $('reset-settings').onclick = () => saveSettings({ rememberLastUi: true, startupUi: 'builtin', scanInstallDirectory: true, scanDataDirectory: true, additionalDirectories: [], showManagerOnStart: false });
  $('switch-ui').onclick = () => switchTo($('switch-target').value);
  $('refresh-uis').onclick = () => loadSettings().catch(fail);
  $('full-ui').onclick = $('detail-full').onclick = () => switchTo('builtin');
  $('manager').onclick = () => { try { host().openWebUiManager().catch(fail); } catch (error) { fail(error); } };
  $('close-details').onclick = () => $('details').close();
  $('detail-folder').onclick = () => host().openPath(detailPath).catch(fail);
  void loadInstances();
})();
