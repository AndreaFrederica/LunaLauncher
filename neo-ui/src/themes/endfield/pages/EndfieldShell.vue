<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <div class="ef ef-root">
    <EfCommandBar
      :page="page"
      :section="{ label: pageInfo.title, code: pageInfo.code }"
      :connected="connected"
      @tasks="taskCenterOpen = true"
      @search="searchOpen = true"
    />
    <div class="ef-shell">
      <EfSideNav :page="page" :groups="navGroups" @navigate="navigate" />
      <main class="ef-main">
        <EfPageHeader :eyebrow="pageInfo.code" :title="pageInfo.title" :description="pageInfo.description">
          <EfButton icon="refresh" :disabled="busy" @click="refresh">刷新</EfButton>
        </EfPageHeader>

        <EndfieldHomePage
          v-if="page === 'home'"
          v-bind="homeProps"
          @select="selectInstance"
          @launch="launch"
          @settings="openInstance"
          @folder="openFolder"
          @refresh="refresh"
          @account="selectAccount"
          @accounts="navigate('accounts')"
        />
        <EndfieldInstancesPage
          v-else-if="page === 'instances'"
          :instances="instances"
          :icons="icons"
          :selected="selected"
          :busy="busy"
          :execute="execute"
          @select="selectInstance"
          @open="openInstance"
          @launch="launch"
          @changed="refresh"
          @toast="showToast"
        />
        <EndfieldInstancePage
          v-else-if="page === 'instance' && selected"
          :instance="selected"
          :busy="busy"
          :execute="execute"
          :console-lines="consoleLines"
          :icon="selected ? icons[selected.id] : undefined"
          @launch="launch"
          @folder="openFolder"
          @changed="refresh"
          @toast="showToast"
        />
        <EndfieldDownloadPage
          v-else-if="page === 'download'"
          :busy="busy"
          :execute="execute"
          :instances="instances"
          :selected="selected"
          @installed="onInstalled"
          @toast="showToast"
        />
        <EndfieldAccountsPage
          v-else-if="page === 'accounts'"
          :accounts="accounts"
          :busy="busy"
          :execute="execute"
          :selected-account="selectedAccountId"
          @select="selectAccount"
          @changed="refresh"
          @toast="showToast"
        />
        <EndfieldSettingsPage
          v-else-if="page === 'settings'"
          :busy="busy"
          :execute="execute"
          :settings="settings"
          :selected="selected"
          :instances="instances"
          :accounts="accounts"
          @changed="refresh"
          @accounts="navigate('accounts')"
          @toast="showToast"
        />
        <EndfieldToolsPage
          v-else-if="page === 'tools'"
          :busy="busy"
          :execute="execute"
          :instances="instances"
          :selected="selected"
          @toast="showToast"
        />
        <EfEmpty
          v-else-if="page === 'instance'"
          title="还没有选择实例"
          description="先到实例页选择一个实例，再查看详情。"
          icon="inventory_2"
        >
          <EfButton variant="accent" @click="navigate('instances')">去实例页</EfButton>
        </EfEmpty>
      </main>
    </div>

    <EfTaskCenter v-model="taskCenterOpen" :execute="execute" />
    <EfLaunchDialog
      v-model="launchShown"
      :title="launchTitle"
      :progress="launchProgress"
      :stage="launchStage"
      :error="launchError"
      @cancel="cancelLaunch"
      @close="closeLaunch"
    />
    <EfInteractionDialog :shown="interactionShown" :request="interactionRequest" @submit="submitInteraction" @cancel="cancelInteraction" />
    <EfToastHost ref="toastHost" />

    <div v-if="searchOpen" class="ef-search-overlay" @click.self="searchOpen = false">
      <section class="ef-search-panel">
        <EfSearchBox v-model="searchQuery" placeholder="搜索实例或页面" />
        <div class="ef-search-results">
          <button v-for="item in searchResults" :key="item.id" type="button" @click="openSearchItem(item)">
            <span class="material-icons">{{ item.icon }}</span>
            <strong>{{ item.label }}</strong>
            <small>{{ item.kind }}</small>
          </button>
          <p v-if="!searchResults.length">没有匹配项。</p>
        </div>
      </section>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref } from 'vue';
import { storeToRefs } from 'pinia';
import { launcher } from '../../../boot/launcher-api';
import { useLauncherSessionStore } from '../../../common/launcher-session';
import { useThemeRuntime } from '../../../common/runtime/theme-runtime';
import EfCommandBar from '../components/layout/EfCommandBar.vue';
import EfSideNav from '../components/layout/EfSideNav.vue';
import EfPageHeader from '../components/layout/EfPageHeader.vue';
import EfButton from '../components/EfButton.vue';
import EfSearchBox from '../components/controls/EfSearchBox.vue';
import EfEmpty from '../components/feedback/EfEmpty.vue';
import EfTaskCenter from '../components/overlays/EfTaskCenter.vue';
import EfLaunchDialog from '../components/overlays/EfLaunchDialog.vue';
import EfInteractionDialog from '../components/EfInteractionDialog.vue';
import EfToastHost from '../components/EfToastHost.vue';
import EndfieldHomePage from './EndfieldHomePage.vue';
import EndfieldInstancesPage from './EndfieldInstancesPage.vue';
import EndfieldInstancePage from './EndfieldInstancePage.vue';
import EndfieldDownloadPage from './EndfieldDownloadPage.vue';
import EndfieldAccountsPage from './EndfieldAccountsPage.vue';
import EndfieldSettingsPage from './EndfieldSettingsPage.vue';
import EndfieldToolsPage from './EndfieldToolsPage.vue';

useThemeRuntime('endfield', () => true);

const session = useLauncherSessionStore();
const { icons, selectedAccountId } = storeToRefs(session);
const instances = computed(() => session.instances);
const accounts = computed(() => session.accounts);
const selected = computed(() => instances.value.find((item: any) => item.id === session.selectedInstanceId) || null);

const page = ref('home');
const busy = ref(false);
const connected = ref(true);
const settings = ref<any[]>([]);
const consoleLines = ref<string[]>([]);
const taskCenterOpen = ref(false);
const searchOpen = ref(false);
const searchQuery = ref('');
const toastHost = ref<any>(null);
const interactionShown = ref(false);
const interactionRequest = ref<any>(null);
const launchShown = ref(false);
const launchTitle = ref('启动中');
const launchProgress = ref(0);
const launchStage = ref('');
const launchError = ref('');
let launchTaskId = '';
let launchCancelled = false;

const tabs = [
  { id: 'home', label: '启动', code: '01' },
  { id: 'instances', label: '实例', code: '02' },
  { id: 'download', label: '下载', code: '03' },
  { id: 'accounts', label: '账户', code: '04' },
  { id: 'settings', label: '设置', code: '05' },
  { id: 'tools', label: '工具', code: '06' },
];

const pageInfoMap: Record<string, { title: string; code: string; description: string }> = {
  home: { title: '启动台', code: 'LAUNCH', description: '选择实例与账户，开始游戏。' },
  instances: { title: '实例', code: 'INSTANCES', description: '管理所有 Minecraft 实例与运行状态。' },
  instance: { title: '实例详情', code: 'INSTANCE', description: '组件、日志与实例配置。' },
  download: { title: '下载', code: 'CATALOG', description: '安装 Minecraft、整合包与资源。' },
  accounts: { title: '账户', code: 'ACCOUNTS', description: '登录、默认账户与皮肤。' },
  settings: { title: '设置', code: 'CONFIG', description: '启动器与实例配置。' },
  tools: { title: '工具', code: 'TOOLS', description: 'Java、内存、网络、日志与诊断。' },
};
const pageInfo = computed(() => pageInfoMap[page.value] || pageInfoMap.home);

const navGroups = computed(() => [
  {
    id: 'game',
    label: '游戏',
    code: 'GAME',
    items: [
      { id: 'home', label: '启动', icon: 'play_arrow' },
      { id: 'instances', label: '实例', icon: 'inventory_2', badge: instances.value.length },
      ...(selected.value ? [{ id: 'instance', label: '当前实例', icon: 'tune' }] : []),
      { id: 'download', label: '下载', icon: 'download' },
      { id: 'accounts', label: '账户', icon: 'group', badge: accounts.value.length },
    ],
  },
  {
    id: 'launcher',
    label: '启动器',
    code: 'LAUNCHER',
    items: [
      { id: 'settings', label: '设置', icon: 'settings' },
      { id: 'tools', label: '工具', icon: 'build' },
    ],
  },
]);

const homeProps = computed(() => ({
  instances: instances.value,
  selected: selected.value,
  accounts: accounts.value,
  accountId: selectedAccountId.value,
  busy: busy.value,
  consoleLines: consoleLines.value,
  icon: selected.value ? icons.value?.[selected.value.id] : undefined,
}));

function showToast(type: 'success' | 'error' | 'info', title: string, message = '') {
  const host = toastHost.value;
  if (!host?.toasts) return;
  const id = `${Date.now()}-${Math.random().toString(16).slice(2)}`;
  host.toasts.push({ id, type, title, message });
  setTimeout(() => host.dismiss?.(id), 4200);
}

async function call(op: string, parameters: Record<string, unknown> = {}) {
  const result = await launcher.execute<any>(op, parameters);
  if (!result.ok) throw new Error(result.error || '操作失败');
  connected.value = true;
  return result.data;
}

async function execute(op: string, parameters: Record<string, unknown> = {}) {
  busy.value = true;
  try {
    const data = await call(op, parameters);
    return data;
  } catch (error: any) {
    showToast('error', '操作失败', error?.message || String(error));
    throw error;
  } finally {
    busy.value = false;
  }
}

async function refresh() {
  try {
    await session.refresh();
    await session.refreshIcons?.();
    const result: any = await call('settings.list', { scope: 'launcher' });
    settings.value = Array.isArray(result?.entries) ? result.entries : (Array.isArray(result) ? result : []);
  } catch (error: any) {
    showToast('error', '刷新失败', error?.message || String(error));
  }
}

function navigate(id: string) {
  page.value = id;
  searchOpen.value = false;
  if (id === 'settings' || id === 'instance') void refresh();
}

function selectInstance(instance: any) {
  if (!instance?.id) return;
  session.selectInstance(instance.id);
  page.value = 'instance';
}

function openInstance() {
  if (selected.value) page.value = 'instance';
}

async function selectAccount(id: string) {
  session.selectAccount(id);
}

async function openFolder() {
  if (!selected.value) return;
  try {
    await execute('instance.open-folder', { instance: selected.value.id });
    showToast('success', '已打开文件夹');
  } catch { /* toast handled */ }
}

async function onInstalled() {
  await refresh();
  showToast('success', '安装完成');
}

function isFinished(task: any) {
  return Boolean(task?.finished) || ['succeeded', 'failed', 'cancelled', 'canceled', 'aborted'].includes(String(task?.state || '').toLowerCase());
}

async function launch() {
  if (!selected.value || busy.value) return;
  const account = session.selectedAccount || accounts.value.find((item: any) => item.default) || accounts.value[0];
  if (!account) {
    showToast('error', '没有可用账户', '请先到账户页登录。');
    page.value = 'accounts';
    return;
  }
  launchShown.value = true;
  launchCancelled = false;
  launchTitle.value = `启动 ${selected.value.name}`;
  launchProgress.value = 0;
  launchStage.value = '准备启动';
  launchError.value = '';
  busy.value = true;
  try {
    let task: any = await call('neo.task.start', {
      operation: 'instance.launch',
      priority: 'high',
      parameters: { instance: selected.value.id, account: String(account.internalId || account.id || account.profileName) },
    });
    launchTaskId = task?.taskId || '';
    while (task && !isFinished(task)) {
      if (launchCancelled) {
        if (task?.taskId) await call('neo.task.cancel', { taskId: task.taskId }).catch(() => {});
        break;
      }
      launchStage.value = String(task.message || task.stage || '启动中');
      const total = Number(task.total ?? task.totalProgress);
      const current = Number(task.current ?? task.progress);
      launchProgress.value = total > 0 && Number.isFinite(current)
        ? Math.max(0, Math.min(100, Math.round(current / total * 100)))
        : Number(task.percent ?? task.percentage ?? 0);
      await new Promise(resolve => setTimeout(resolve, 400));
      task = await call('neo.task.status', { taskId: task.taskId });
    }
    if (launchCancelled) {
      launchStage.value = '已取消';
      launchShown.value = false;
      return;
    }
    if (task?.state !== 'succeeded') throw new Error(task?.error || '启动失败');
    launchStage.value = '启动完成';
    launchProgress.value = 100;
    showToast('success', '已启动', selected.value.name);
    await refresh();
    setTimeout(() => { launchShown.value = false; }, 900);
  } catch (error: any) {
    launchError.value = error?.message || String(error);
    showToast('error', '启动失败', launchError.value);
  } finally {
    busy.value = false;
    launchTaskId = '';
  }
}

async function cancelLaunch() {
  launchCancelled = true;
  if (launchTaskId) {
    try { await call('neo.task.cancel', { taskId: launchTaskId }); } catch { /* ignore */ }
  }
}

function closeLaunch() {
  launchShown.value = false;
  launchError.value = '';
}

const searchResults = computed(() => {
  const query = searchQuery.value.trim().toLowerCase();
  const items = [
    ...tabs.map(tab => ({ id: `page:${tab.id}`, label: tab.label, kind: '页面', icon: 'chevron_right', page: tab.id })),
    ...instances.value.map((instance: any) => ({ id: `instance:${instance.id}`, label: instance.name, kind: '实例', icon: 'inventory_2', instance })),
    ...accounts.value.map((account: any) => ({ id: `account:${account.id || account.internalId}`, label: account.profileName || account.displayName || '账户', kind: '账户', icon: 'person', page: 'accounts' })),
  ];
  return items.filter(item => !query || item.label.toLowerCase().includes(query) || item.kind.toLowerCase().includes(query)).slice(0, 14);
});

function openSearchItem(item: any) {
  searchOpen.value = false;
  if (item.instance) selectInstance(item.instance);
  else if (item.page) page.value = item.page;
}

async function submitInteraction(value: any) {
  if (interactionRequest.value) {
    try { await launcher.respond({ interactionId: interactionRequest.value.interactionId, value }); } catch { /* ignore */ }
  }
  interactionShown.value = false;
  interactionRequest.value = null;
}

function cancelInteraction() {
  if (interactionRequest.value) {
    launcher.respond({ interactionId: interactionRequest.value.interactionId, cancel: true }).catch(() => {});
  }
  interactionShown.value = false;
  interactionRequest.value = null;
}

let unlisten: any = null;
onMounted(async () => {
  try {
    unlisten = await launcher.onEvent((raw: any) => {
      if (raw?.kind === 'input') {
        interactionRequest.value = raw;
        interactionShown.value = true;
      } else if (raw?.kind === 'status') {
        const message = raw.message || raw.data?.message || (typeof raw.data === 'string' ? raw.data : '');
        if (launchShown.value && message) launchStage.value = message;
      }
    });
  } catch { /* optional */ }
  await refresh();
});
onUnmounted(() => { unlisten?.(); });
</script>

<style scoped>
.ef-root {
  display: flex;
  flex-direction: column;
  width: 100%;
  height: 100%;
  overflow: hidden;
  background: var(--ef-paper);
  color: var(--ef-ink);
  font-family: var(--ef-font-ui);
}
.ef-shell {
  display: flex;
  flex: 1;
  min-height: 0;
}
.ef-main {
  flex: 1;
  min-width: 0;
  overflow: auto;
  padding: 18px 22px 32px;
}
.ef-search-overlay {
  position: fixed;
  inset: 0;
  z-index: 9400;
  display: flex;
  justify-content: center;
  padding: 80px 20px;
  background: rgba(0, 0, 0, .35);
}
.ef-search-panel {
  width: min(560px, 94vw);
  align-self: flex-start;
  padding: 12px;
  background: var(--ef-paper);
  border: 1px solid var(--ef-ink);
  box-shadow: 6px 6px 0 rgba(0, 0, 0, .16);
}
.ef-search-results {
  display: flex;
  flex-direction: column;
  margin-top: 8px;
  max-height: 50vh;
  overflow: auto;
}
.ef-search-results button {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 7px 6px;
  border: 0;
  border-bottom: 1px solid var(--ef-rule);
  background: transparent;
  color: var(--ef-ink);
  text-align: left;
  cursor: pointer;
}
.ef-search-results button:hover { background: var(--ef-signal); color: var(--ef-on-signal); }
.ef-search-results button strong {
  flex: 1;
  font-family: var(--ef-font-ui);
  font-size: 12px;
  font-weight: 600;
}
.ef-search-results button small {
  color: var(--ef-ink-2);
  font-family: var(--ef-font-ui);
  font-size: 9px;
  letter-spacing: .1em;
  text-transform: uppercase;
}
.ef-search-results button:hover small { color: #4a4700; }
.ef-search-results button .material-icons { font-size: 15px; }
.ef-search-results p {
  margin: 12px 0;
  color: var(--ef-ink-2);
  font-size: 11px;
}
</style>
