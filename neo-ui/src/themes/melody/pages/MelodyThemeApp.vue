<template>
  <main class="melody-theme-root" :class="{ 'is-native': native }" @contextmenu.prevent="menuOpen = true" @keydown.alt.t.prevent="menuOpen = true">
    <section class="melody-desktop" aria-label="忘却的旋律">
      <MelodyWindowHost v-for="entry in entries" :key="entry.id" :entry="entry" :native="native"
        @activate="activate(entry.id)" @move="(x, y) => move(entry, x, y)" @close="close(entry.id)">
        <MelodyLauncherWindow v-if="entry.kind === 'launcher'" v-model="selected" :background="appearance.backgroundMode === 'builtin' ? background : backgroundUrl" :background-fit="appearance.backgroundMode === 'builtin' ? 'fill' : appearance.fit" :footer="footer" :footer-style="footerStyle" :versions="versions" :accounts="accountOptions" :account-id="selectedAccountId" :busy="launchBusy" :inert="blocked(entry.id)" :on-open="kind => openWindow(kind, entry.id)" @account-change="session.selectAccount" @launch="launchGame(entry.id)" @folder="openFolder(entry.id)" @close="closeLauncher" />
        <MelodyPreferencesWindow v-else-if="entry.kind === 'preferences'" :inert="blocked(entry.id)" @close="close(entry.id)" @legacy="openWindow('settings', entry.id)" @theme="leaveTheme" />
        <MelodyGamesWindow v-else-if="entry.kind === 'games'" :instance-id="entry.instanceId" :component-uid="entry.componentUid" :inert="blocked(entry.id)" @close="close(entry.id)" @clients="openWindow('clients', entry.id)" />
        <MelodyClientsWindow v-else-if="entry.kind === 'clients'" :inert="blocked(entry.id)" :on-games="() => openWindow('games', entry.id)" :on-edit="id => openWindow('client', entry.id, id)" @close="close(entry.id)" />
        <MelodyClientWindow v-else-if="entry.kind === 'client' && entry.instanceId" :instance-id="entry.instanceId" :inert="blocked(entry.id)" @close="close(entry.id)" @browse="uid => openWindow('games', entry.id, entry.instanceId, uid)" />
        <MelodySettingsWindow v-else-if="entry.kind === 'settings' && settingsReady" :model-value="{ ...settings, game: selected }" :games="versions" :busy="saving" :inert="blocked(entry.id)" :execute="call" @close="close(entry.id)" @message="text => message(text, entry.id)" @save="value => saveSettings(value, entry.id)" @action="(action, value) => settingsAction(action, value, entry.id)" />
        <MelodyManagerWindow v-else-if="entry.kind === 'manager'" :versions="versions" :inert="blocked(entry.id)" @close="close(entry.id)" @install="() => message('没有可用的整合列表。', entry.id)" @update-sounds="version => message(version ? '请选择游戏并启动，NeoUI 会检查缺失的声音文件。' : '请选择想要更新的游戏版本。', entry.id)" />
        <MelodyMessageWindow v-else-if="entry.kind === 'message'" :message="entry.message || ''" :title="entry.title" @close="close(entry.id)" />
        <MelodyDownloadWindow v-else-if="entry.kind === 'download'" :file-name="task.stage" :progress="task.progress" :inert="blocked(entry.id)" @close="close(entry.id)" @manual="message('当前没有可用的下载链接。', entry.id)" />
        <MelodySetupWindow v-else-if="entry.kind === 'setup'" :inert="blocked(entry.id)" @close="close(entry.id)" @message="text => message(text, entry.id)" @complete="value => saveSettings({ ...settings, ...value }, entry.id)" />
        <MelodyComponentGallery v-else-if="entry.kind === 'components'" @close="close(entry.id)" />
      </MelodyWindowHost>
    </section>
    <TaskInteractionDialog v-if="native && initialKind === 'launcher'" :execute="call" pcl />
    <div v-if="menuOpen" class="melody-menu-backdrop" @click.self="menuOpen = false" @keydown.esc="menuOpen = false">
      <nav ref="menuElement" class="melody-menu" aria-label="窗口与主题">
        <button v-for="kind in menuKinds" :key="kind" @click="menuOpen = false; openWindow(kind)">{{ windows[kind].title }}</button>
        <button @click="menuOpen = false; openRuntimeDebug()">打开 Runtime Debugger</button>
        <button @click="leaveTheme()">返回启动器</button>
      </nav>
    </div>
  </main>
</template>
<script setup lang="ts">
import { computed, nextTick, onMounted, onUnmounted, reactive, ref, watch } from 'vue';
import { storeToRefs } from 'pinia';
import { invoke } from '@tauri-apps/api/core';
import { launcher, type LauncherInstance } from '../../../boot/launcher-api';
import { neoUiPreferences, persistNeoUiPreferences, applyNeoUiPreferences, type NeoShell } from '../../../neo-ui-preferences';
import MelodyWindowHost from '../components/MelodyWindowHost.vue';
import MelodyLauncherWindow from './windows/MelodyLauncherWindow.vue';
import MelodySettingsWindow from './windows/MelodySettingsWindow.vue';
import MelodyManagerWindow from './windows/MelodyManagerWindow.vue';
import MelodyMessageWindow from './windows/MelodyMessageWindow.vue';
import MelodyDownloadWindow from './windows/MelodyDownloadWindow.vue';
import MelodySetupWindow from './windows/MelodySetupWindow.vue';
import MelodyComponentGallery from './windows/MelodyComponentGallery.vue';
import MelodyPreferencesWindow from './windows/MelodyPreferencesWindow.vue';
import MelodyGamesWindow from './windows/MelodyGamesWindow.vue';
import MelodyClientsWindow from './windows/MelodyClientsWindow.vue';
import MelodyClientWindow from './windows/MelodyClientWindow.vue';
import TaskInteractionDialog from '../../../components/TaskInteractionDialog.vue';
import { useMelodyAppearance } from '../composables/preferences';
import { useLauncherSessionStore } from '../../../common/launcher-session';
import { useMelodyWindows } from '../composables/useMelodyWindows';
import { chooseMelodyBackground } from '../services/backgrounds';
import { useRuntimeLifecycle } from '../../../common/runtime/lifecycle-debug';
import { useThemeRuntime } from '../../../common/runtime/theme-runtime';
import { windows, defaultSettings, type MelodySettings, type MelodyWindowKind } from '../types';

const { native, entries, initialKind, open, close, activate, move, blocked } = useMelodyWindows();
useRuntimeLifecycle('theme:melody.app');
useThemeRuntime('melody', () => true);
const { appearance, backgroundUrl, footer, footerStyle } = useMelodyAppearance({ randomizeOnMount: initialKind === 'launcher' });
const settingsReady = ref(!native || initialKind !== 'settings');
const menuOpen = ref(false), launchBusy = ref(false), saving = ref(false);
const menuKinds: MelodyWindowKind[] = native ? ['preferences', 'games', 'clients', 'settings', 'manager'] : ['preferences', 'games', 'clients', 'launcher', 'settings', 'manager', 'download', 'setup', 'components'];
const storageKey = 'luna-melody.settings';
const menuElement = ref<HTMLElement>();
let menuReturnFocus: HTMLElement | null = null;
watch(menuOpen, async shown => {
  if (shown) {
    menuReturnFocus = document.activeElement instanceof HTMLElement ? document.activeElement : null;
    await nextTick();
    menuElement.value?.querySelector('button')?.focus();
  } else if (menuReturnFocus?.isConnected) menuReturnFocus.focus({ preventScroll: true });
});
const settings = reactive<MelodySettings>({ ...defaultSettings });
if (!native) try {
  const stored = JSON.parse(localStorage.getItem(storageKey) || '{}');
  for (const key of Object.keys(defaultSettings) as (keyof MelodySettings)[]) {
    if (key !== 'password' && typeof stored[key] === typeof defaultSettings[key]) Object.assign(settings, { [key]: stored[key] });
  }
} catch { /* An unavailable/corrupt browser store falls back to theme defaults. */ }
const session = useLauncherSessionStore();
const { instances, selectedInstanceId, selectedAccountId, selectedAccount } = storeToRefs(session);
const selected = selectedInstanceId;
const background = ref(chooseMelodyBackground(settings.skinMode));
if (!selected.value) session.selectInstance(settings.game);
watch(instances, values => { if (!values.some(item => item.id === selected.value)) session.selectInstance(values[0]?.id || ''); });
const versions = computed(() => instances.value.map(item => ({ value: item.id, label: item.name })));
const accountOptions = computed(() => session.accounts.map(account => ({
  value: String(account.internalId || account.id || account.profileName || ''),
  label: String(account.profileName || account.displayName || '未命名账户'),
})).filter(option => option.value));
const task = reactive({ stage: '', progress: 0 });
let disposed = false;
let taskId = '';
async function openDevTools() {
  if (!native) return;
  try { await invoke('open_devtools'); }
  catch (error) { console.error('Unable to open DevTools', error); }
}
async function openRuntimeDebug() {
  if (!native) {
    const url = new URL(location.href);
    url.searchParams.set('runtimeDebug', '1');
    window.open(url.href, '_blank', 'noopener,noreferrer');
    return;
  }
  try { await invoke('open_runtime_debug'); }
  catch (error) { await message(`无法打开 Runtime Debugger：${String(error)}`); }
}
function handleDebugShortcut(event: KeyboardEvent) {
  if (event.ctrlKey && event.shiftKey && event.key.toLowerCase() === 'i') {
    event.preventDefault();
    void openDevTools();
  }
}
async function call<T = any>(operation: string, parameters: Record<string, unknown> = {}): Promise<T> {
  const result = await launcher.execute<T>(operation, parameters);
  if (!result.ok) throw new Error(result.error || '操作失败');
  return result.data as T;
}
async function openWindow(kind: MelodyWindowKind, parent?: string, instanceId?: string, componentUid?: string) {
  try {
    await open(kind, parent, undefined, undefined, instanceId, componentUid);
  } catch (error) {
    console.error('Unable to open Melody window', kind, error);
    // Do not navigate the main native window as a fallback. That turns a
    // failed child-window request into an embedded page and breaks Melody's
    // one-window-per-screen contract. The native error remains in the Tauri
    // console; browser preview continues to use its in-page window host.
    if (native) {
      window.alert(`无法创建独立窗口「${kind}」：${String(error)}`);
      return;
    }
    await message(String(error), parent);
  }
}
async function message(text: string, parent = entries.value[0]?.id || 'launcher') {
  try { await open('message', parent, text); }
  catch (error) {
    // Keep failure visible even if the native host cannot create a child window.
    entries.value.push({ id: 'message-error', kind: 'message', parent, message: String(error), x: 0, y: 0, z: 9999 });
  }
}
watch(selected, value => {
  settings.game = value;
  if (native) void invoke('set_melody_settings', { settings: { ...settings, password: '' } }).catch(error => console.error('Unable to persist Melody settings', error));
  session.selectInstance(value);
});
async function loadStoredSettings() {
  if (!native) return;
  const stored = await invoke<Partial<MelodySettings> | null>('get_melody_settings');
  if (!stored) return;
  Object.assign(settings, stored, { password: '' });
  session.selectInstance(settings.game);
  background.value = chooseMelodyBackground(settings.skinMode);
}
function syncStorage(event: StorageEvent) {
  if (event.key === storageKey && event.newValue) {
    try {
      const saved = JSON.parse(event.newValue);
      Object.assign(settings, saved, { password: '' });
      session.selectInstance(settings.game);
      background.value = chooseMelodyBackground(settings.skinMode);
    } catch { /* Keep the current state if another window writes malformed data. */ }
  }
  if (event.key === 'luna-melody.task' && event.newValue) {
    try { Object.assign(task, JSON.parse(event.newValue)); } catch { /* Keep last progress. */ }
  }
}
onMounted(async () => {
  document.documentElement.classList.add('melody-document');
  document.documentElement.dataset.neoShell = 'melody';
  window.addEventListener('keydown', handleDebugShortcut);
  if (!native) window.addEventListener('storage', syncStorage);
  if (!native) return;
  try {
    await loadStoredSettings();
    if (initialKind === 'launcher') await invoke('configure_melody_host', { enabled: true });
    if (['launcher', 'settings', 'manager'].includes(initialKind)) {
      await session.refresh();
      if (!instances.value.some(item => item.id === selected.value)) session.selectInstance(instances.value[0]?.id || '');
    }
    if (initialKind === 'settings') {
      const result = await call('settings.list', { scope: 'launcher' });
      for (const entry of result.entries || []) {
        if (entry.key === 'JavaPath') settings.javaPath = String(entry.value || '');
        if (entry.key === 'MaxMemAlloc') settings.memory = String(entry.value);
      }
    }
  } catch (error) { if (!disposed) await message(String(error)); }
  finally { settingsReady.value = true; }
});
onUnmounted(() => {
  disposed = true;
  document.documentElement.classList.remove('melody-document');
  applyNeoUiPreferences();
  window.removeEventListener('storage', syncStorage);
  window.removeEventListener('keydown', handleDebugShortcut);
  if (native && initialKind === 'launcher') void invoke('configure_melody_host', { enabled: false }).catch(() => {});
});
async function saveSettings(value: MelodySettings, parent: string) {
  if (saving.value) return;
  const memory = Number(value.memory);
  if (!Number.isInteger(memory) || memory < 128) { await message('最大内存必须是大于或等于 128 的整数。', parent); return; }
  saving.value = true;
  try {
    if (native) {
      if (value.memoryMode === 'manual') await call('settings.set', { scope: 'launcher', key: 'MaxMemAlloc', value: memory });
      if (value.javaPath.trim()) await call('java.select', { scope: 'launcher', path: value.javaPath.trim() });
    }
    const saved = { ...value, password: '' };
    if (native) await invoke('set_melody_settings', { settings: saved });
    else localStorage.setItem(storageKey, JSON.stringify(saved));
    Object.assign(settings, saved);
    session.selectInstance(saved.game);
    background.value = chooseMelodyBackground(saved.skinMode);
    await message('设置已保存', parent);
  } catch (error) { await message(String(error), parent); }
  finally { saving.value = false; }
}
async function settingsAction(action: string, value: MelodySettings, parent: string) {
  if (action === 'find-java' && native) {
    try {
      const result = await call('java.refresh');
      const runtimes = Array.isArray(result) ? result : result?.runtimes || [];
      await message(runtimes.length ? runtimes.map((runtime: any) => runtime.path || runtime.executable).filter(Boolean).join('\n') : '没有找到 Java，请手动指定 javaw.exe。', parent);
    } catch (error) { await message(String(error), parent); }
    return;
  }
  const messages: Record<string, string> = {
    'find-java': '请使用浏览按钮选择 Java，或填写 javaw.exe 的完整路径。',
    'login-test': '请返回 NeoUI 账户管理，使用 Microsoft 账户登录。',
    'delete-login': '此处不保存正版密码。请在 NeoUI 账户管理中删除账户。',
    'rename': value.game ? '请在 NeoUI 的实例管理中重命名游戏。' : '请先选择游戏。',
    'delete-game': value.game ? '请在 NeoUI 的实例管理中删除游戏。' : '请先选择游戏。',
  };
  await message(messages[action] || '当前操作不可用。', parent);
}
async function openFolder(parent: string) {
  if (!selected.value) { await message('请先选择游戏。', parent); return; }
  try {
    await call('instance.open-folder', { instance: selected.value });
  } catch (error) { await message(String(error), parent); }
}
async function launchGame(parent: string) {
  if (!selected.value) { await message('没有选择游戏,请选择或点击更新按钮更新游戏', parent); return; }
  if (launchBusy.value) return;
  launchBusy.value = true;
  try {
    // Use the shared runtime selection so switching between PCL, XMCL and
    // Melody keeps the account chosen by the user. `refresh()` already
    // reconciles the selection with the backend account list.
    await session.refresh();
    const account = selectedAccount.value || session.accounts.find(item => item.default) || session.accounts[0];
    if (!account) { await openWindow('preferences', parent); throw new Error('请在启动器设置的“账户”分类中添加账户。'); }
    let status = await call('neo.task.start', { operation: 'instance.launch', priority: 'high', parameters: { instance: selected.value, account: String(account.internalId || account.id || account.profileName) } });
    taskId = status.taskId;
    while (!status.finished && !disposed) {
      task.stage = String(status.message || status.stage || '启动游戏');
      const total = Number(status.total ?? status.totalProgress);
      const current = Number(status.current ?? status.progress);
      task.progress = total > 0 && Number.isFinite(current) ? Math.max(0, Math.min(100, current / total * 100)) : 0;
      if (!native) localStorage.setItem('luna-melody.task', JSON.stringify(task));
      await new Promise(resolve => setTimeout(resolve, 500));
      status = await call('neo.task.status', { taskId });
    }
    if (!disposed && status.state !== 'succeeded') throw new Error(status.error || '启动失败');
  } catch (error) { if (!disposed) await message(String(error), parent); }
  finally { launchBusy.value = false; taskId = ''; }
}
async function closeLauncher() {
  if (native) await close('launcher'); else await leaveTheme();
}
async function leaveTheme(shell: NeoShell = 'pcl') {
  // Selecting Melody from its own settings is a valid choice. Keep the
  // current native document in place; the root shell is already Melody.
  if (shell === 'melody') {
    neoUiPreferences.shell = 'melody';
    persistNeoUiPreferences();
    return;
  }
  if (!['neo', 'pcl', 'endfield'].includes(shell)) return;
  neoUiPreferences.shell = shell;
  persistNeoUiPreferences();
  const url = new URL(location.href);
  for (const name of ['theme', 'melodyWindow', 'message', 'title', 'instance', 'component']) url.searchParams.delete(name);
  url.hash = '/';
  if (native) { await invoke('exit_melody_theme'); return; }
  location.replace(url.href);
}
</script>
<style src="../styles/melody-theme.scss"></style>
<style src="../styles/modern.scss"></style>
