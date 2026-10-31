<template>
  <section class="pcl-page pcl-tools-page">
    <aside class="pcl-page-nav pcl-tools-nav">
      <PclNavResizeHandle />
      <div class="pcl-tools-title">
        <PclIcon name="tool-case" :size="19" />
        <span>{{ t('pcl.auto2.ka72ef18d9a') }}</span>
      </div>
      <template v-for="group in itemGroups" :key="group.label">
        <div class="pcl-tools-group">{{ group.label }}</div>
        <button
          v-for="item in group.items"
          :key="item.id"
          type="button"
          class="pcl-page-nav__item"
          :class="{ active: section === item.id }"
          @click="section = item.id"
        >
          <PclIcon :name="item.icon" :size="18" />
          <span>{{ item.label }}</span>
        </button>
      </template>
      <div class="pcl-tools-hint">
        <PclIcon name="info" :size="15" />
        <span>{{ t('pcl.auto2.kaf33a4dbeb') }}</span>
      </div>
    </aside>

    <main class="pcl-page-main">
      <div class="pcl-page-view">
          <div class="pcl-tools-toolbar">
            <div>
              <h1>{{ current.label }}</h1>
              <p>{{ current.description }}</p>
            </div>
            <PclButton size="sm" icon="refresh-cw" :disabled="busy" @click="reload">{{ t('pcl.auto2.k38108eaa1d') }}</PclButton>
          </div>

          <PclHint v-if="error" theme="red" closable @close="error = ''">{{ error }}</PclHint>

                    <PclToolsMultiplayerPanel v-show="isMultiplayerSection" />

                    <PclToolsTaskPanel v-show="section === 'tasks'" />

                    <PclToolsLogPanel v-show="section === 'logs'" />

                    <PclToolsSchedulerPanel v-show="section === 'scheduler'" />

                    <PclToolsHelperPanel v-show="section === 'authlib' || section === 'nide8auth'" />

                    <PclToolsDiagnosticsPanel v-show="section === 'test'" />
      </div>
    </main>
  </section>
</template>

<script setup lang="ts">
import { useI18n } from "vue-i18n";
const { t } = useI18n();
import { computed, onMounted, onUnmounted, ref, toRef, watch } from 'vue';
import { isTauri } from '@tauri-apps/api/core';
import PclButton from '../../components/PclButton.vue';
import PclCard from '../../components/PclCard.vue';
import PclHint from '../../components/PclHint.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclNavResizeHandle from '../../components/PclNavResizeHandle.vue';
import PclIconButton from '../../components/PclIconButton.vue';
import PclInput from '../../components/PclInput.vue';
import PclListItem from '../../components/PclListItem.vue';
import PclLoading from '../../components/PclLoading.vue';
import PclSelect from '../../components/PclSelect.vue';
import PclToolsMultiplayerPanel from './PclToolsMultiplayerPanel.vue';
import PclToolsTaskPanel from './PclToolsTaskPanel.vue';
import PclToolsLogPanel from './PclToolsLogPanel.vue';
import PclToolsSchedulerPanel from './PclToolsSchedulerPanel.vue';
import PclToolsHelperPanel from './PclToolsHelperPanel.vue';
import PclToolsDiagnosticsPanel from './PclToolsDiagnosticsPanel.vue';
import { launcher } from '../../../../boot/launcher-api';
import { useRuntimeLifecycle } from '../../../../common/runtime/lifecycle-debug';
import { useThemeResource } from '../../../../common/runtime/theme-runtime';
import { provide } from 'vue';
import { PclToolsContextKey } from './PclToolsContext';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
const props = defineProps<{ busy: boolean; execute: Execute; selected?: any; instances: any[]; active?: boolean }>();
const pageActive = computed(() => props.active !== false);
useRuntimeLifecycle('theme:pcl.tools-page', pageActive);
const emit = defineEmits<{ refresh: []; 'open-instance': [instance: any] }>();
const openInstance = (instance: any) => emit('open-instance', instance);

const itemGroups = [
  { label: t('pcl.auto2.k894f1c3e8d'), items: [
    { id: 'multiplayer', label: t('pcl.auto2.k1d5e43605d'), icon: 'earth', description: t('pcl.auto2.kccf2623767') },
    { id: 'tasks', label: t('pcl.auto2.kff5e8b99ae'), icon: 'list', description: t('pcl.auto2.ke31d997706') },
    { id: 'logs', label: t('pcl.auto2.ka9706f8c7f'), icon: 'scroll-text', description: t('pcl.auto2.k6ca5b1e7ba') },
    { id: 'scheduler', label: t('pcl.auto2.kc13a522bb1'), icon: 'gauge', description: t('pcl.auto2.k3c52f688ed') },
    { id: 'test', label: t('pcl.auto2.k6c5c08c370'), icon: 'flask-conical', description: t('pcl.auto2.kc572d9f9cc') },
  ] },
  { label: t('pcl.auto2.k726090c5c4'), items: [
    { id: 'yukari', label: 'YukariConnect', icon: 'globe', description: t('pcl.auto2.k24d2a8fe1f') },
    { id: 'terracotta', label: 'Terracotta', icon: 'network', description: t('pcl.auto2.k215c9d6ce4') },
    { id: 'authlib', label: 'authlib-injector', icon: 'shield-check', description: t('pcl.auto2.keac13be806') },
    { id: 'nide8auth', label: 'Nide8Auth', icon: 'key-round', description: t('pcl.auto2.k2e152edae1') },
  ] },
];
const items = itemGroups.flatMap(group => group.items);
const section = ref('multiplayer'), tasks = ref<any[]>([]), downloads = ref<any[]>([]), loading = ref(false);
const apiInfo = ref<any>(null), servers = ref<any[]>([]), serverAddress = ref(''), serverStatus = ref<any>(null);
const skinUrl = ref(''), skinPreview = ref(''), testServer = ref(''), achievementTitle = ref(''), achievementLine = ref('');
const avatarSize = ref(128), skinVariant = ref('classic'), testUrl = ref(''), urlResult = ref(''), urlTesting = ref(false);
const hashText = ref(''), hashResult = ref(''), error = ref('');
const stats = ref<any[]>([]);
const backendScheduler = ref<any>(null);
async function loadStats() {
  stats.value = launcher.getStats();
  try { backendScheduler.value = await props.execute('scheduler.status'); }
  catch { backendScheduler.value = null; }
}
const backends = ref<any[]>([]);
const backend = ref('terracotta');
const backendStatus = ref<any>(null);
const backendBusy = ref(false);
// `backendBusy` is an operation lock.  The loading indicator is separate so
// silent status polling does not flash the panel every few seconds.
const backendLoading = ref(false);
const backendError = ref('');
const backendMode = ref<'host' | 'guest'>('host');
const playerName = ref('Player');
const roomCode = ref('');
const backendLog = ref('');
const helperStatus = ref<Record<string, any>>({});
const helperBusy = ref(false);
let refreshTimer: ReturnType<typeof setInterval> | undefined;
let refreshInFlight = false;
let themeResourceActive = false;
let logStreamUnlisten: (() => void) | undefined;
let logStreamSubscription = '';
let logStreamGeneration = 0;
const current = computed(() => items.find(item => item.id === section.value) || items[0]);
const isMultiplayerSection = computed(() => ['multiplayer', 'yukari', 'terracotta'].includes(section.value));
const selectedBackend = computed(() => backends.value.find(item => item.id === backend.value) || null);
const backendState = computed(() => backendStatus.value?.state || null);
const backendProfiles = computed(() => Array.isArray(backendState.value?.profiles) ? backendState.value.profiles : []);

/**
 * Keep the identity of backend rows and status objects stable while the
 * three-second status poll runs.  Replacing these containers on every poll
 * makes all consumers re-evaluate their subtrees and used to make the
 * multiplayer panel look as if it was being rebuilt.
 */
function patchRecord(target: Record<string, any> | null, next: Record<string, any>) {
  if (!target) return;
  for (const key of Object.keys(target)) if (!(key in next)) delete target[key];
  for (const [key, value] of Object.entries(next)) {
    const current = target[key];
    if (current && value && typeof current === 'object' && typeof value === 'object' && !Array.isArray(current) && !Array.isArray(value)) {
      patchRecord(current, value as Record<string, any>);
    } else {
      target[key] = value;
    }
  }
}

function patchBackendList(next: any[]) {
  const oldById = new Map(backends.value.map(item => [String(item.id), item]));
  const ordered = next.map(item => {
    const existing = oldById.get(String(item.id));
    if (existing) { patchRecord(existing, item); return existing; }
    return item;
  });
  backends.value.splice(0, backends.value.length, ...ordered);
}

function patchBackendStatus(next: any) {
  if (!next) {
    if (backendStatus.value !== null) backendStatus.value = null;
    return;
  }
  if (!backendStatus.value) {
    backendStatus.value = next;
    return;
  }
  patchRecord(backendStatus.value, next);
}

function message(value: unknown) {
  return value instanceof Error ? value.message : String(value);
}

function stateLabel(state: string) {
  const labels: Record<string, string> = {
    waiting: t('pcl.auto2.kbd3488d0a9'), 'host-scanning': t('pcl.auto2.k5f1f1ae10b'), 'host-starting': t('pcl.auto2.k03c625d2d1'),
    'host-ok': t('pcl.auto2.kf79b86a571'), 'guest-connecting': t('pcl.auto2.ka5c84f5771'), 'guest-starting': t('pcl.auto2.k8c20e1cda9'),
    'guest-ok': t('pcl.auto2.kf71c183251'), exception: t('pcl.auto2.k20a0c1a2c0'),
  };
  return labels[state] || state || t('pcl.auto2.k4e72a1ffe5');
}

function readPreference(key: string, fallback: string) {
  try { return localStorage.getItem(key) || fallback; } catch { return fallback; }
}

function savePreference(key: string, value: string) {
  try { localStorage.setItem(key, value); } catch { /* storage unavailable */ }
}

function selectBackend(id: string) {
  backend.value = id;
  savePreference('luna-multiplayer-backend', id);
  backendStatus.value = null;
  backendLog.value = '';
  backendError.value = '';
  void refreshBackendAndLog();
  void syncBackendLogStream();
}

async function stopBackendLogStream() {
  logStreamGeneration += 1;
  const subscription = logStreamSubscription;
  logStreamSubscription = '';
  logStreamUnlisten?.();
  logStreamUnlisten = undefined;
  if (subscription && isTauri()) {
    try { await props.execute('event.unsubscribe', { subscriptionId: subscription }); } catch { /* The sidecar may already be gone. */ }
  }
}

function appendBackendLog(text: string) {
  if (!text) return;
  // Keep the UI bounded as well as the native subscription. The native
  // stream remains the source of truth; this only prevents an unbounded DOM
  // text node during a long Yukari session.
  backendLog.value = `${backendLog.value}${backendLog.value && !backendLog.value.endsWith('\n') ? '\n' : ''}${text}`.slice(-256 * 1024);
}

async function syncBackendLogStream() {
  await stopBackendLogStream();
  if (!isTauri() || !isMultiplayerSection.value || backend.value !== 'yukari') return;
  const generation = logStreamGeneration;
  try {
    // Register the native event listener before creating the subscription so
    // later batches cannot be missed. The initial snapshot is fetched again
    // after the id is known because the sidecar may publish it immediately.
    const unlisten = await launcher.onStream((batch: any) => {
      if (generation !== logStreamGeneration || !logStreamSubscription) return;
      if (batch?.subscriptionId !== logStreamSubscription) return;
      for (const event of batch.events || []) {
        if (event.kind === 'multiplayer.log.reset') backendLog.value = String(event.text || '');
        else if (event.kind === 'multiplayer.log.data') appendBackendLog(String(event.text || ''));
      }
    });
    if (generation !== logStreamGeneration) { unlisten(); return; }
    logStreamUnlisten = unlisten;
    const result = await props.execute('multiplayer.log.subscribe', { backend: 'yukari' });
    const subscription = String(result?.subscriptionId || '');
    if (generation !== logStreamGeneration || !subscription) {
      unlisten();
      if (subscription) void props.execute('event.unsubscribe', { subscriptionId: subscription }).catch(() => {});
      return;
    }
    logStreamSubscription = subscription;
    // Read the current native snapshot once after the id is installed. All
    // subsequent changes arrive from YukariConnect's WebSocket signal. This
    // direct read deliberately does not touch the operation lock/loading
    // state, because subscription setup can race a status refresh.
    const snapshot = await props.execute('multiplayer.log', { backend: 'yukari' });
    if (generation === logStreamGeneration) backendLog.value = String(snapshot?.log || '');
  } catch (exception) {
    if (generation === logStreamGeneration) {
      logStreamUnlisten?.();
      logStreamUnlisten = undefined;
      backendError.value = message(exception);
    }
  }
}

async function loadBackends() {
  try {
    const result = await props.execute('multiplayer.backends');
    patchBackendList(Array.isArray(result?.backends) ? result.backends : []);
    if (backends.value.length && !backends.value.some(item => item.id === backend.value)) backend.value = backends.value[0].id;
    await refreshBackend();
    await loadBackendLog({ silent: true });
  } catch (exception) {
    backendError.value = message(exception);
  }
}

async function refreshBackend(options: { silent?: boolean } = {}) {
  if (!backend.value) return;
  const showLoading = !options.silent;
  backendBusy.value = true;
  if (showLoading) backendLoading.value = true;
  backendError.value = '';
  try {
    patchBackendStatus(await props.execute('multiplayer.status', { backend: backend.value, fetchState: true }));
    const currentStatus = backendStatus.value;
    const backendSummary = backends.value.find(item => item.id === backend.value);
    if (backendSummary) patchRecord(backendSummary, {
      ...backendSummary,
      installed: Boolean(currentStatus?.installed),
      version: currentStatus?.version || backendSummary.version,
      processRunning: Boolean(currentStatus?.processRunning),
    });
    if (backendStatus.value?.state?.room) roomCode.value = backendStatus.value.state.room;
  } catch (exception) {
    backendError.value = message(exception);
  } finally {
    if (showLoading) backendLoading.value = false;
    backendBusy.value = false;
  }
}

async function installBackend() {
  backendBusy.value = true;
  backendError.value = '';
  try {
    await props.execute('multiplayer.install', { backend: backend.value });
    await loadBackends();
  } catch (exception) {
    backendError.value = message(exception);
  } finally {
    backendBusy.value = false;
  }
}

async function startBackend() {
  if (!playerName.value.trim()) return;
  backendBusy.value = true;
  backendError.value = '';
  try {
    const parameters: Record<string, unknown> = { backend: backend.value, mode: backendMode.value, player: playerName.value.trim() };
    if (backendMode.value === 'guest') parameters.room = roomCode.value.trim();
    await props.execute('multiplayer.start', parameters);
    savePreference('luna-multiplayer-player', playerName.value.trim());
    await refreshBackend();
  } catch (exception) {
    backendError.value = message(exception);
  } finally {
    backendBusy.value = false;
  }
}

async function stopBackend() {
  backendBusy.value = true;
  backendError.value = '';
  try {
    await props.execute('multiplayer.stop', { backend: backend.value });
    await refreshBackend();
  } catch (exception) {
    backendError.value = message(exception);
  } finally {
    backendBusy.value = false;
  }
}

async function forceStopBackend() {
  if (!window.confirm(t('pcl.tools.forceStopBackendConfirm'))) return;
  backendBusy.value = true;
  backendError.value = '';
  try {
    await props.execute('multiplayer.stop', { backend: backend.value, force: true });
    await refreshBackend({ silent: true });
  } catch (exception) {
    backendError.value = message(exception);
  } finally {
    backendBusy.value = false;
  }
}

async function retryBackend() {
  backendBusy.value = true;
  backendError.value = '';
  try {
    await props.execute('multiplayer.retry', { backend: backend.value });
    await refreshBackend();
  } catch (exception) {
    backendError.value = message(exception);
  } finally {
    backendBusy.value = false;
  }
}

async function loadBackendLog(options: { silent?: boolean } = {}) {
  const showLoading = !options.silent;
  backendBusy.value = true;
  if (showLoading) backendLoading.value = true;
  backendError.value = '';
  try {
    const result = await props.execute('multiplayer.log', { backend: backend.value });
    backendLog.value = String(result?.log || '');
  } catch (exception) {
    backendError.value = message(exception);
  } finally {
    if (showLoading) backendLoading.value = false;
    backendBusy.value = false;
  }
}

async function refreshBackendAndLog(options: { silent?: boolean } = {}) {
  await refreshBackend(options);
  if (backend.value !== 'yukari' || !logStreamSubscription) await loadBackendLog(options);
}

async function refreshVisibleData() {
  if (refreshInFlight || props.busy || backendBusy.value || helperBusy.value) return;
  refreshInFlight = true;
  try {
    if (isMultiplayerSection.value) {
      await refreshBackendAndLog({ silent: true });
    } else if (section.value === 'tasks') {
      await loadTasks();
    } else if (section.value === 'scheduler') {
      await loadStats();
    }
  } finally {
    refreshInFlight = false;
  }
}

async function loadTasks() {
  loading.value = true;
  error.value = '';
  try {
    const result = await props.execute('task.list');
    tasks.value = Array.isArray(result) ? result : (result?.tasks || []);
  } catch (exception) {
    error.value = message(exception);
  }
  try {
    downloads.value = await props.execute('aria2.downloads');
  } catch {
    downloads.value = [];
  } finally {
    loading.value = false;
  }
}
async function reload() {
  if (isMultiplayerSection.value) await refreshVisibleData();
  else if (section.value === 'tasks') await loadTasks();
  else if (section.value === 'scheduler') await loadStats();
  else emit('refresh');
}
async function cancelTask(task: any) {
  try { await props.execute('task.cancel', task.id ? { taskId: task.id } : {}); await loadTasks(); }
  catch (exception) { error.value = message(exception); }
}
async function cancelDownload(download: any) {
  try { await props.execute('aria2.cancel', { gid: download.gid }); await loadTasks(); }
  catch (exception) { error.value = message(exception); }
}
async function clearDownloads() {
  try { await props.execute('aria2.clear-finished'); await loadTasks(); }
  catch (exception) { error.value = message(exception); }
}
async function loadApi() {
  try { apiInfo.value = await props.execute('api.describe'); }
  catch (exception) { apiInfo.value = { error: message(exception) }; }
}
async function loadServers() {
  try {
    const result = JSON.parse(localStorage.getItem('luna-server-list') || '[]');
    servers.value = Array.isArray(result) ? result : [];
  } catch { servers.value = []; }
}
async function queryServer(address = serverAddress.value) {
  if (!address) return;
  serverAddress.value = address;
  error.value = '';
  try {
    const response = await fetch(`https://api.mcsrvstat.us/2/${encodeURIComponent(address)}`);
    const data = await response.json();
    serverStatus.value = {
      name: data.hostname || address, address,
      players: data.players?.online || 0, maxPlayers: data.players?.max || 0,
      latency: data.debug?.ping || undefined, online: Boolean(data.online),
    };
    const next = [...servers.value.filter(item => item.address !== address), { address, name: data.hostname || address, players: data.players?.online || 0 }];
    servers.value = next;
    localStorage.setItem('luna-server-list', JSON.stringify(next.slice(-50)));
  } catch (exception) {
    serverStatus.value = { name: address, error: message(exception) };
  }
}
async function testUrlRequest() {
  urlTesting.value = true;
  urlResult.value = '';
  const start = performance.now();
  try {
    const response = await fetch(testUrl.value, { method: 'HEAD' });
    urlResult.value = `HTTP ${response.status} ${response.statusText} · ${Math.round(performance.now() - start)} ms`;
  } catch (exception) {
    urlResult.value = t('pcl.tools.requestFailed', { error: message(exception) });
  } finally {
    urlTesting.value = false;
  }
}
async function hashTextValue() {
  if (!hashText.value) return;
  const bytes = new TextEncoder().encode(hashText.value);
  const digest = await crypto.subtle.digest('SHA-256', bytes);
  hashResult.value = Array.from(new Uint8Array(digest)).map(value => value.toString(16).padStart(2, '0')).join('');
}
function saveSkinPreview() {
  try { localStorage.setItem('luna-tool-skin-preview', JSON.stringify({ url: skinPreview.value, size: avatarSize.value, variant: skinVariant.value })); }
  catch { /* storage unavailable */ }
}
function helperKey(kind: string) {
  return kind === 'authlib' || kind === 'nide8auth' ? (kind === 'authlib' ? 'authlib-injector' : 'nide8auth') : '';
}
async function loadHelper(helper: string) {
  if (!helper) return;
  helperBusy.value = true;
  try { helperStatus.value[helper] = await props.execute('authentication.helper.status', { helper }); }
  catch (exception) { helperStatus.value[helper] = { error: message(exception) }; }
  finally { helperBusy.value = false; }
}
async function installHelper(helper: string) {
  helperBusy.value = true;
  try { helperStatus.value[helper] = await props.execute('authentication.helper.install', { helper }); }
  catch (exception) { helperStatus.value[helper] = { error: message(exception) }; }
  finally { helperBusy.value = false; }
}
async function removeHelper(helper: string) {
  if (!window.confirm(t('pcl.tools.confirmDelete', { name: helper }))) return;
  helperBusy.value = true;
  try { helperStatus.value[helper] = await props.execute('authentication.helper.remove', { helper, confirm: true }); }
  catch (exception) { helperStatus.value[helper] = { error: message(exception) }; }
  finally { helperBusy.value = false; }
}

function saveAchievement() {
  try { localStorage.setItem('luna-tool-achievement-preview', JSON.stringify({ title: achievementTitle.value, line: achievementLine.value })); }
  catch { /* storage unavailable */ }
}
watch(section, value => {
  if (value === 'scheduler') loadStats();
  if (value === 'yukari' || value === 'terracotta') {
    backend.value = value;
    backendStatus.value = null;
    void refreshBackendAndLog();
    void syncBackendLogStream();
  } else {
    void stopBackendLogStream();
  }
  if (value === 'authlib' || value === 'nide8auth') void loadHelper(helperKey(value));
});

function startRefreshTimer() {
  if (!themeResourceActive || refreshTimer || props.active === false) return;
  refreshTimer = setInterval(() => { void refreshVisibleData(); }, 3000);
}
function stopRefreshTimer() {
  if (refreshTimer) { clearInterval(refreshTimer); refreshTimer = undefined; }
}
useThemeResource('pcl', () => {
  themeResourceActive = true;
  if (props.active !== false) startRefreshTimer();
  return () => { themeResourceActive = false; stopRefreshTimer(); };
});
onMounted(() => {
  backend.value = readPreference('luna-multiplayer-backend', 'terracotta');
  playerName.value = readPreference('luna-multiplayer-player', 'Player') || 'Player';
  if (props.active !== false) {
    void loadTasks();
    void loadBackends().then(() => { if (isMultiplayerSection.value) void syncBackendLogStream(); });
  }
});
watch(() => props.active, value => {
  if (value === false) { stopRefreshTimer(); void stopBackendLogStream(); return; }
  startRefreshTimer();
  if (!tasks.value.length && !downloads.value.length) void loadTasks();
  if (!backends.value.length) void loadBackends().then(() => { if (isMultiplayerSection.value) void syncBackendLogStream(); });
  else if (isMultiplayerSection.value) void syncBackendLogStream();
});
onUnmounted(() => { stopRefreshTimer(); void stopBackendLogStream(); });

provide(PclToolsContextKey, { t, props, busy: toRef(props, 'busy'), execute: props.execute, selected: props.selected, instances: props.instances, section, tasks, downloads, loading, apiInfo, servers, serverAddress, serverStatus, skinUrl, skinPreview, testServer, achievementTitle, achievementLine, avatarSize, skinVariant, testUrl, urlResult, urlTesting, hashText, hashResult, error, stats, backendScheduler, backends, backend, backendStatus, backendBusy, backendLoading, backendError, backendMode, playerName, roomCode, backendLog, helperStatus, helperBusy, current, isMultiplayerSection, selectedBackend, backendState, backendProfiles, stateLabel, selectBackend, refreshBackend, installBackend, startBackend, stopBackend, forceStopBackend, retryBackend, loadBackendLog, loadTasks, cancelTask, cancelDownload, clearDownloads, loadApi, queryServer, testUrlRequest, hashTextValue, saveSkinPreview, helperKey, loadHelper, installHelper, removeHelper, saveAchievement, loadStats, openInstance });
</script>

<style>
.pcl-tools-page {
  width: 100%;
  max-width: none;
  margin: 0;
}

.pcl-tools-nav {
  width: 170px;
  min-width: 170px;
  padding: 14px 0 16px;
}

.pcl-tools-title {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 0 13px 12px;
  border-bottom: 1px solid var(--pcl-border);
  color: var(--pcl-text);
  font-size: calc(13px * var(--pcl-font-scale, 1));
  font-weight: 700;
}

.pcl-tools-group {
  padding: 9px 13px 5px;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
  font-weight: 700;
  letter-spacing: .08em;
}

.pcl-tools-hint {
  display: flex;
  gap: 6px;
  margin: 16px 10px 0;
  padding: 8px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
  line-height: 1.6;
}

.pcl-tools-toolbar {
  display: flex;
  align-items: flex-end;
  justify-content: space-between;
  gap: 12px;
  padding-bottom: 14px;
  border-bottom: 1px solid var(--pcl-border);
}

.pcl-tools-toolbar h1 {
  margin: 0;
  color: var(--pcl-text);
  font-size: calc(19px * var(--pcl-font-scale, 1));
  font-weight: 700;
}

.pcl-tools-toolbar p {
  margin: 3px 0 0;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-tools-stack {
  display: grid;
  grid-template-columns: minmax(0, 1.25fr) minmax(0, .75fr);
  gap: 12px;
  align-items: start;
}

.pcl-tools-grid {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 12px;
}

.pcl-tools-grid .pcl-card {
  margin-bottom: 0;
}

.pcl-tools-server-form {
  display: flex;
  gap: 7px;
  margin-bottom: 10px;
}

.pcl-tools-server-form .pcl-input {
  flex: 1;
}

.pcl-tools-server-list,
.pcl-tools-list {
  max-height: 360px;
  overflow-y: auto;
}

.pcl-tools-muted {
  margin: 0;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
  line-height: 1.7;
}

.pcl-tools-pre {
  max-height: 240px;
  margin: 0;
  overflow: auto;
  padding: 9px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-c8);
  color: var(--pcl-text);
  white-space: pre-wrap;
  font: calc(11px * var(--pcl-font-scale, 1))/1.55 ui-monospace, Consolas, monospace;
}

.pcl-tools-stack > .pcl-card { min-width: 0; }
.pcl-tools-stack > .pcl-card .pcl-tools-pre { max-height: calc(100vh - 300px); min-height: 220px; }

.pcl-tools-hash {
  margin-top: 8px;
  overflow-wrap: anywhere;
}

.pcl-tools-skin-form {
  display: grid;
  grid-template-columns: minmax(0, 1fr) 110px;
  gap: 7px;
  margin-bottom: 8px;
}

.pcl-tools-skin-form .pcl-input {
  grid-column: 1 / -1;
}

.pcl-tools-skin-preview {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 72px;
  height: 72px;
  margin: 8px 0;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-c6) center/cover no-repeat;
  color: var(--pcl-muted);
  image-rendering: pixelated;
}

.pcl-tools-skin-image {
  display: block;
  max-width: 180px;
  max-height: 120px;
  margin-top: 8px;
  object-fit: contain;
  image-rendering: pixelated;
}

.pcl-tools-achievement {
  display: flex;
  flex-direction: column;
  gap: 4px;
  margin-top: 9px;
  padding: 12px;
  border-radius: 4px;
  background: linear-gradient(135deg, var(--pcl-c6), var(--pcl-c8));
  color: var(--pcl-text);
}

.pcl-tools-achievement span {
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-backend-tabs {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 7px;
  margin-bottom: 10px;
}

.pcl-backend-tabs button {
  display: flex;
  flex-direction: column;
  align-items: flex-start;
  gap: 2px;
  padding: 8px 10px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-half-white);
  color: var(--pcl-text);
  text-align: left;
  cursor: pointer;
}

.pcl-backend-tabs button.active {
  border-color: var(--pcl-c2);
  color: var(--pcl-c2);
}

.pcl-backend-tabs strong {
  font-size: calc(12px * var(--pcl-font-scale, 1));
}

.pcl-backend-tabs span {
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}

.pcl-backend-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
  margin-bottom: 9px;
}

.pcl-backend-head strong {
  display: block;
  color: var(--pcl-text);
  font-size: calc(13px * var(--pcl-font-scale, 1));
}

.pcl-backend-head span {
  display: block;
  margin-top: 2px;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}

.pcl-backend-state {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 8px;
  margin-bottom: 9px;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-backend-state b {
  color: var(--pcl-text);
  font-weight: 600;
}

.pcl-backend-pill {
  padding: 2px 8px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 10px;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}

.pcl-backend-pill--host-ok,
.pcl-backend-pill--guest-ok {
  border-color: rgba(52, 211, 153, .5);
  color: #34d399;
}

.pcl-backend-pill--exception {
  border-color: rgba(255, 107, 107, .5);
  color: #ff6b6b;
}

.pcl-backend-profiles {
  max-height: 150px;
  margin-bottom: 8px;
  overflow-y: auto;
}

.pcl-backend-mode {
  display: inline-flex;
  gap: 2px;
  margin-bottom: 8px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-c8);
  padding: 2px;
}

.pcl-backend-mode button {
  height: 26px;
  padding: 0 11px;
  border: 0;
  border-radius: 3px;
  background: transparent;
  color: var(--pcl-muted);
  font: 400 calc(11px * var(--pcl-font-scale, 1))/1 var(--pcl-font);
  cursor: pointer;
}

.pcl-backend-mode button.active {
  background: var(--pcl-c3);
  color: #fff;
}

.pcl-scheduler-summary {
  display: flex;
  flex-wrap: wrap;
  gap: 8px 14px;
  margin-bottom: 10px;
  padding: 8px 10px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-half-white);
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-scheduler-summary b {
  color: var(--pcl-c2);
  font-weight: 600;
}

.pcl-stats-table {
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  overflow: hidden;
}

.pcl-stats-row {
  display: grid;
  grid-template-columns: minmax(0, 1.7fr) 52px 52px 72px 72px 58px;
  gap: 6px;
  padding: 7px 9px;
  border-bottom: 1px solid var(--pcl-border);
  color: var(--pcl-text);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-stats-row:last-child {
  border-bottom: 0;
}

.pcl-stats-row--head {
  background: var(--pcl-half-white);
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
  font-weight: 700;
}

.pcl-stats-op {
  overflow: hidden;
  color: var(--pcl-c2);
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-stats-slow {
  color: #ff6b6b;
  font-weight: 700;
}

@media (max-width: 900px) {
  .pcl-tools-nav {
    width: 152px;
    min-width: 152px;
  }

  .pcl-tools-stack,
  .pcl-tools-grid {
    grid-template-columns: 1fr;
  }
}

.pcl-helper-status { display: flex; align-items: flex-start; gap: 8px; padding: 10px; margin: 8px 0; border: 1px solid var(--pcl-border); border-radius: 4px; background: var(--pcl-c8); }
.pcl-helper-status > div { display: flex; flex-direction: column; gap: 2px; min-width: 0; }
.pcl-helper-status strong { color: var(--pcl-text); font-size: calc(11px * var(--pcl-font-scale, 1)); }
.pcl-helper-status small { color: var(--pcl-muted); font-size: calc(9px * var(--pcl-font-scale, 1)); overflow-wrap: anywhere; }
.pcl-helper-error { color: #fb7185 !important; }
</style>
