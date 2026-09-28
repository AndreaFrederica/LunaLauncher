// SPDX-License-Identifier: GPL-3.0-only
// A small consumer of the public Web UI bridge; no Neo UI or backend imports.
window.createDeckLauncher = ({ call, notify, navigate, changed }) => {
  const $ = id => document.getElementById(id);
  const sessions = new Map(), subscriptions = new Map();
  let selected = '', active = null, input = null, deviceUrl = '', stopTarget = null;
  let ready = false, renderTimer, lastLog = '', lastSession = '', accounts = [], pollTimer;
  const ownedKey = 'luna-deck.console-subscriptions';
  const labels = { idle: '尚未启动', starting: '正在启动', running: '客户端运行中', ended: '客户端已退出', failed: '启动失败', removed: '实例已移除', disconnected: '后端连接已断开' };
  const message = error => error?.message || String(error);
  const remember = () => sessionStorage.setItem(ownedKey, JSON.stringify([...subscriptions.keys()]));
  const current = () => sessions.get(selected);
  const busy = () => !ready || !!active;
  const schedule = () => { if (!renderTimer) renderTimer = setTimeout(() => { renderTimer = null; render(); }, 80); };
  function addLog(session, text) {
    session.logs.push(text);
    if (session.logs.length > 2000) session.logs.splice(0, session.logs.length - 2000);
    schedule();
  }
  function render() {
    const session = current();
    $('session-count').textContent = String([...sessions.values()].filter(s => s.running).length);
    $('console-launch').disabled = busy() || !session || session.running || session.removed;
    $('console-stop').disabled = busy() || !session?.running || session.stopping;
    $('console-cancel').hidden = active !== session || !active;
    $('console-cancel').disabled = !active || (!input && !active.task?.canAbort) || active.cancelling;
    $('device-cancel').disabled = !active?.task?.canAbort || active?.cancelling;
    $('session-state').textContent = session ? labels[session.phase] : '尚未选择实例';
    $('session-pid').textContent = session?.pid ? `PID ${session.pid}` : '';
    $('session-status').textContent = session?.status || '从实例库启动，或选择已运行的实例查看输出。';
    $('session-error').textContent = session?.error || ''; $('session-error').hidden = !session?.error;
    $('stream-warning').textContent = session?.warning || ''; $('stream-warning').hidden = !session?.warning;
    const progress = $('launch-progress'), task = session === active ? session?.task : null;
    progress.hidden = !active || session !== active;
    if (task?.totalProgress > 0) { progress.max = task.totalProgress; progress.value = Math.min(task.progress, task.totalProgress); }
    else progress.removeAttribute('value');
    const output = session?.logs.join('\n') || '尚无日志。';
    if (lastLog !== output || lastSession !== selected) {
      const log = $('console-log');
      const scroll = log.scrollTop;
      log.textContent = output;
      log.scrollTop = $('log-follow').checked ? log.scrollHeight : scroll;
      lastLog = output; lastSession = selected;
    }
  }
  function stream(batch) {
    const session = subscriptions.get(batch.subscriptionId);
    if (!session) return; // Initial events are recovered by event.poll after subscribing.
    if (batch.dropped) session.warning = '输出量较大，部分早期日志已不在后端缓冲区中。完整日志可在实例目录查看。';
    for (const event of batch.events || []) {
      if (event.sequence <= session.cursor) continue;
      session.cursor = event.sequence;
      if (event.kind === 'console.line') addLog(session, event.text + (event.truncated ? ' …（此行已截断）' : ''));
      if (event.kind === 'instance.state') {
        session.running = event.running;
        session.stateSeen = true;
        if (event.running) session.phase = 'running';
        else if (session !== active) { session.phase = 'ended'; session.status = '客户端已结束，请查看日志中的退出信息。'; }
        changed(session.id, event.running);
      }
      if (event.kind === 'instance.removed') {
        session.removed = true; session.running = false; session.phase = 'removed';
        changed(session.id, false);
        void unsubscribe(session).catch(error => notify(message(error), true));
      }
      // available:false also occurs before the first launch. It is not an exit event.
    }
    schedule();
  }
  async function unsubscribe(session) {
    if (!session.subscription) return;
    await call('event.unsubscribe', { subscriptionId: session.subscription });
    subscriptions.delete(session.subscription); session.subscription = ''; remember();
  }
  async function subscribe(session) {
    if (session.subscription) return;
    if (session.subscribing) return session.subscribing;
    session.subscribing = (async () => {
      const available = await call('event.subscriptions');
      if (available.length >= 16) {
        const idle = [...subscriptions.values()].find(s => !s.running && s !== active && s !== session);
        if (idle) await unsubscribe(idle);
      }
      const sub = await call('instance.console.subscribe', { instance: session.id });
      session.subscription = sub.subscriptionId; session.cursor = 0;
      subscriptions.set(sub.subscriptionId, session); remember();
      // The native stream may already have delivered the initial snapshot. Pause
      // this subscription's live batches until we have replayed it in order.
      session.replaying = true; session.buffered = [];
      let batch;
      do {
        batch = await call('event.poll', { subscriptionId: sub.subscriptionId, after: session.cursor, limit: 256 });
        stream(batch);
      } while (batch.hasMore);
    })().finally(() => {
      session.replaying = false; session.subscribing = null;
      for (const batch of session.buffered || []) stream(batch);
      session.buffered = [];
    });
    return session.subscribing;
  }
  async function show(id) {
    selected = id; $('console-instance').value = id; navigate('console'); render();
    const session = current();
    if (session) {
      try { await subscribe(session); }
      catch (error) { session.error = message(error); notify(session.error, true); render(); }
    }
  }
  async function loadAccounts() {
    $('refresh-accounts').disabled = true;
    try {
      accounts = await call('account.list');
      const previous = $('launch-account').value;
      $('launch-account').replaceChildren(new Option('自动选择实例 / 默认账户', ''), ...accounts.map(a =>
        new Option(`${a.profileName || a.displayName || a.id} · ${a.type}${a.default ? ' · 默认' : ''}`, a.internalId || a.id)));
      if (accounts.some(a => (a.internalId || a.id) === previous)) $('launch-account').value = previous;
      $('account-hint').textContent = accounts.length ? '支持已有 Microsoft、离线及第三方认证账户。新增或管理账户请打开完整 Neo UI。' : '没有已保存账户。可在完整 Neo UI 登录，或选择离线模式并填写临时玩家名（需已下载游戏）。';
      modeChanged();
    } catch (error) { $('account-hint').textContent = '账户读取失败：' + message(error); notify(message(error), true); }
    finally { $('refresh-accounts').disabled = !!active; }
  }
  function modeChanged() {
    const offline = $('launch-mode').value === 'offline';
    $('offline-name-field').hidden = !offline;
    $('launch-account').disabled = !!active || $('launch-mode').value === 'demo' || (offline && !!$('offline-name').value.trim());
  }
  function closePrompts() {
    input = null; deviceUrl = '';
    $('input-text').value = ''; $('device-code').textContent = ''; $('device-url').textContent = '';
    $('launch-input').close(); $('launch-device').close();
  }
  async function reply(cancel = false) {
    if (!input) return;
    const pending = input;
    const buttons = $('launch-input-form').querySelectorAll('button'); buttons.forEach(b => b.disabled = true);
    try {
      await window.luna.respond({ interactionId: pending.interactionId, ...(cancel ? { cancel: true } : {
        value: Array.isArray(pending.choices) ? Number($('input-choice').value) : $('input-text').value,
      }) });
      // A follow-up password/profile prompt may arrive before respond resolves.
      if (input === pending) { input = null; $('input-text').value = ''; $('launch-input').close(); }
    } catch (error) { $('input-error').textContent = message(error); }
    finally { buttons.forEach(b => b.disabled = false); render(); }
  }
  function onEvent(event) {
    if (!active) return;
    if (event.kind === 'status') { active.status = event.message || ''; schedule(); }
    if (event.kind === 'task') { active.task = event.data; if (event.data.status) active.status = event.data.status; schedule(); }
    if (event.kind === 'input') {
      input = event; $('input-prompt').textContent = event.prompt; $('input-error').textContent = '';
      $('input-text').value = ''; $('input-text').type = event.secret ? 'password' : 'text';
      const choice = Array.isArray(event.choices);
      $('input-text-field').hidden = choice; $('input-choice-field').hidden = !choice;
      $('input-choice').replaceChildren(...(event.choices || []).map((item, i) => new Option(typeof item === 'string' ? item : item.name || item.displayName || item.id || String(i + 1), String(i))));
      if (!$('launch-input').open) $('launch-input').showModal();
      $(choice ? 'input-choice' : 'input-text').focus(); render();
    }
    if (event.kind === 'device_code') {
      const data = event.data || event;
      deviceUrl = data.url || ''; $('device-url').textContent = deviceUrl; $('device-code').textContent = data.code || '';
      $('device-error').textContent = '';
      if (!$('launch-device').open) $('launch-device').showModal(); render();
    }
  }
  async function launch(id) {
    const session = sessions.get(id);
    if (!session || session.removed) return;
    if (session.running) return show(id);
    if (busy()) return notify('另一个启动正在准备，请等待完成或先取消。', true);
    const parameters = { instance: id, mode: $('launch-mode').value, wait: false };
    const name = $('offline-name').value.trim();
    if (parameters.mode === 'offline' && name) {
      if (!/^[A-Za-z0-9_]{1,16}$/.test(name)) return notify('离线玩家名需为 1–16 位英文字母、数字或下划线。', true);
      parameters.offlineName = name;
    } else if (parameters.mode !== 'demo' && $('launch-account').value) parameters.account = $('launch-account').value;
    active = session; selected = id; session.phase = 'starting'; session.error = ''; session.warning = '';
    session.status = '正在连接实例控制台…'; session.task = null; session.pid = null; session.stateSeen = false;
    $('console-instance').value = id; $('details').close(); navigate('console');
    $('launch-mode').disabled = $('offline-name').disabled = $('refresh-accounts').disabled = true;
    modeChanged(); changed(); render();
    try {
      await subscribe(session);
      addLog(session, `\n—— ${new Date().toLocaleTimeString()} 启动 ${session.name} ——`);
      session.status = '正在准备账户和游戏文件…'; render();
      // task.status/task.cancel and respond bypass the bridge's request queue.
      pollTimer = setInterval(() => {
        if (!session.polling && active === session) {
          session.polling = true;
          call('task.status').then(task => { if (active === session && task?.id) { session.task = task; schedule(); } })
            .catch(() => {}).finally(() => session.polling = false);
        }
      }, 750);
      const result = await call('instance.launch', parameters);
      session.pid = result.pid;
      if (!session.stateSeen) session.running = true;
      session.phase = session.running ? 'running' : 'ended';
      session.status = session.running ? 'Minecraft 进程已启动，实时输出见下方。' : 'Minecraft 进程已结束，请查看退出日志。';
      notify(session.running ? `${session.name} 已启动` : `${session.name} 已结束`);
    } catch (error) {
      session.phase = 'failed'; session.error = message(error); session.status = '启动未完成，可修正问题后重试。';
      addLog(session, '[启动结果] ' + session.error); notify(session.error, true);
    } finally {
      clearInterval(pollTimer); session.task = null; session.cancelling = false; active = null; closePrompts();
      $('launch-mode').disabled = $('offline-name').disabled = $('refresh-accounts').disabled = false;
      modeChanged(); changed(session.id, session.running); render();
    }
  }
  async function cancel() {
    const session = active;
    if (!session || session.cancelling) return;
    if (input) return reply(true);
    if (!session.task?.canAbort) return;
    session.cancelling = true; render();
    try { await call('task.cancel', { taskId: session.task.id }); }
    catch (error) { notify(message(error), true); $('device-error').textContent = message(error); }
    finally { session.cancelling = false; render(); }
  }
  async function initialize() {
    if (!window.luna) return;
    try {
      await window.luna.onStream(batch => {
        const session = subscriptions.get(batch.subscriptionId);
        if (session?.replaying) session.buffered.push(batch);
        else stream(batch);
      });
      await window.luna.onEvent(onEvent);
      await window.luna.onExit(() => {
        ready = false; closePrompts();
        for (const session of sessions.values()) { session.phase = 'disconnected'; session.error = '无法确认客户端状态。请重新打开启动器连接后端。'; }
        changed(); render(); notify('启动器后端连接已断开', true);
      });
      // A live UI switch retains the sidecar. Release only this frontend's old
      // subscriptions; subscriptions owned by other frontends must be preserved.
      let previous = [];
      try { previous = JSON.parse(sessionStorage.getItem(ownedKey) || '[]'); } catch { /* New UI session. */ }
      const existing = await call('event.subscriptions');
      for (const sub of existing) if (previous.includes(sub.subscriptionId)) await call('event.unsubscribe', { subscriptionId: sub.subscriptionId });
      remember(); ready = true; changed(); render();
      await loadAccounts();
    } catch (error) { notify(message(error), true); }
  }
  async function setInstances(instances) {
    const ids = new Set(instances.map(i => i.id));
    for (const session of sessions.values()) if (!ids.has(session.id)) { session.removed = true; session.phase = 'removed'; await unsubscribe(session); }
    for (const instance of instances) {
      const isRunning = Boolean(instance.lifecycle?.running ?? instance.running);
      let session = sessions.get(instance.id);
      if (!session) {
        session = { id: instance.id, name: instance.name, logs: [], running: isRunning, phase: isRunning ? 'running' : 'idle', cursor: 0 };
        sessions.set(instance.id, session);
      } else {
        session.name = instance.name; session.running = isRunning; session.removed = false;
        if (isRunning) session.phase = 'running';
        else if (session.phase === 'running') session.phase = 'ended';
      }
    }
    $('console-instance').replaceChildren(new Option('选择实例', ''), ...instances.map(i => new Option(i.name, i.id)));
    if (ids.has(selected)) $('console-instance').value = selected;
    render();
    await initialized;
    for (const session of sessions.values()) if (session.running && !session.removed) {
      try { await subscribe(session); } catch (error) { session.warning = '日志订阅失败：' + message(error); }
    }
    render();
  }
  $('launch-mode').onchange = modeChanged; $('offline-name').oninput = modeChanged;
  $('refresh-accounts').onclick = loadAccounts;
  $('console-instance').onchange = () => void show($('console-instance').value);
  $('console-launch').onclick = () => void launch(selected);
  $('console-cancel').onclick = $('device-cancel').onclick = () => void cancel();
  $('clear-log').onclick = () => { if (current()) current().logs = []; render(); };
  $('log-follow').onchange = () => { if ($('log-follow').checked) $('console-log').scrollTop = $('console-log').scrollHeight; };
  $('launch-input-form').onsubmit = event => { event.preventDefault(); void reply(); };
  $('input-cancel').onclick = () => void reply(true);
  $('launch-input').oncancel = event => { event.preventDefault(); void reply(true); };
  $('launch-device').oncancel = event => { event.preventDefault(); void cancel(); };
  $('device-open').onclick = async () => {
    try {
      const url = new URL(deviceUrl); if (!['https:', 'http:'].includes(url.protocol)) throw new Error('无效的授权地址');
      await window.__TAURI__.core.invoke('plugin:opener|open_url', { url: url.href });
    } catch (error) { $('device-error').textContent = message(error); }
  };
  $('console-stop').onclick = () => { stopTarget = current(); $('stop-name').textContent = stopTarget.name; $('stop-confirm').showModal(); };
  $('stop-no').onclick = () => $('stop-confirm').close();
  $('stop-yes').onclick = async () => {
    const session = stopTarget; $('stop-confirm').close();
    if (!session?.running || busy()) return;
    session.stopping = true; render();
    try { await call('instance.stop', { instance: session.id }); session.status = '已请求停止，正在等待客户端退出。'; }
    catch (error) { session.error = message(error); notify(session.error, true); }
    finally { session.stopping = false; render(); }
  };
  const initialized = initialize();
  return { launch, show, setInstances, busy, isRunning: id => !!sessions.get(id)?.running,
    async release() {
      if (active) throw new Error('启动仍在准备中，请完成或取消后再切换界面。');
      for (const session of [...subscriptions.values()]) await unsubscribe(session);
    },
  };
};
