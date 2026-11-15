<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <div class="ef ef-root ef-contours">
    <div class="ef-tri" aria-hidden="true"><i /><i /><i /></div>
    <header class="ef-cmdbar">
      <div class="brand">
        <span class="seal">E</span>
        LunaLauncher
      </div>
      <nav aria-label="主导航">
        <a v-for="tab in tabs" :key="tab.id" :aria-current="page === tab.id ? 'page' : undefined" @click.prevent="page = tab.id">
          <small>// {{ tab.code }}</small>
          {{ tab.label }}
        </a>
      </nav>
      <div class="util">
        <span class="ef-status-dot" :class="{ connected }" />
        {{ connected ? '已连接' : '未连接' }}
      </div>
    </header>
    <main class="ef-main">
      <div class="ef-head">
        <h1>{{ pageTitle }} <small>// {{ pageCode }}</small></h1>
        <p>{{ pageDesc }}</p>
      </div>
      <EndfieldHomePage v-if="page === 'home'" v-bind="homeProps" @launch="launch" @settings="openInstanceSettings" @folder="openFolder" @select="selectInstance" @refresh="refresh" />
      <EndfieldDownloadPage v-else-if="page === 'download'" :busy="busy" :execute="execute" :instances="instances" :selected="selected" @installed="refresh" />
      <EndfieldSettingsPage v-else-if="page === 'settings'" :busy="busy" :execute="execute" :settings="settings" :selected="selected" @changed="refresh" />
      <EndfieldAccountsPage v-else-if="page === 'accounts'" :busy="busy" :execute="execute" :accounts="accounts" @changed="refresh" />
      <EndfieldInstancePage v-else-if="page === 'instance' && selected" :instance="selected" :busy="busy" :execute="execute" :console-lines="consoleLines" @back="page = 'home'" @changed="refresh" @read-log="readLog" />
    </main>
    <EfToastHost />
    <EfInteractionDialog :shown="interactionShown" :request="interactionRequest" @submit="submitInteraction" @cancel="cancelInteraction" />
    <EfDialog v-model="launchDialogShown" title="启动中" :progress="launchProgress" :stage="launchStage" />
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref, watch } from 'vue';
import { storeToRefs } from 'pinia';
import { useI18n } from 'vue-i18n';
import { launcher } from '../../../boot/launcher-api';
import { useLauncherSessionStore } from '../../../common/launcher-session';
import { useThemeRuntime } from '../../../common/runtime/theme-runtime';
import EfButton from '../components/EfButton.vue';
import EfCard from '../components/EfCard.vue';
import EfDialog from '../components/EfDialog.vue';
import EfInteractionDialog from '../components/EfInteractionDialog.vue';
import EfToastHost from '../components/EfToastHost.vue';
import EndfieldHomePage from './EndfieldHomePage.vue';
import EndfieldDownloadPage from './EndfieldDownloadPage.vue';
import EndfieldSettingsPage from './EndfieldSettingsPage.vue';
import EndfieldAccountsPage from './EndfieldAccountsPage.vue';
import EndfieldInstancePage from './EndfieldInstancePage.vue';

const { t } = useI18n();
useThemeRuntime('endfield', () => true);

const session = useLauncherSessionStore();
const { icons } = storeToRefs(session);
const instances = computed(() => session.instances);
const accounts = computed(() => session.accounts);
const selected = computed(() => instances.value.find((i: any) => i.id === session.selectedInstanceId) || null);
const busy = ref(false);
const connected = ref(true);
const page = ref('home');
const settings = ref<any[]>([]);
const consoleLines = ref<string[]>([]);
const error = ref('');
const notice = ref('');
const launchState = ref('idle');
const launchProgress = ref(0);
const launchStage = ref('');
const launchDialogShown = ref(false);
const interactionShown = ref(false);
const interactionRequest = ref<any>(null);

const tabs = [
  { id: 'home', label: '启动', code: '01' },
  { id: 'download', label: '下载', code: '02' },
  { id: 'settings', label: '设置', code: '03' },
  { id: 'accounts', label: '账户', code: '04' },
];

const pageTitles: Record<string, { title: string; code: string; desc: string }> = {
  home: { title: '启动台', code: 'LAUNCH', desc: '选择实例与账户，开始游戏。' },
  download: { title: '下载', code: 'CATALOG', desc: '浏览并安装 Minecraft 与模组。' },
  settings: { title: '设置', code: 'CONFIG', desc: '启动器与实例的配置。' },
  accounts: { title: '账户', code: 'ACCOUNTS', desc: '管理登录账户与皮肤。' },
  instance: { title: '实例', code: 'INSTANCE', desc: '实例详情与日志。' },
};
const pageInfo = computed(() => pageTitles[page.value] || pageTitles.home);
const pageTitle = computed(() => pageInfo.value.title);
const pageCode = computed(() => pageInfo.value.code);
const pageDesc = computed(() => pageInfo.value.desc);

function readLog(file: any) { consoleLines.value = []; }

const homeProps = computed(() => ({
  instances: instances.value,
  selected: selected.value,
  accounts: accounts.value,
  busy: busy.value,
  consoleLines: consoleLines.value,
  icon: selected.value ? icons.value?.[selected.value.id] : undefined,
}));

const execute = async (operation: string, parameters: Record<string, unknown> = {}) => {
  busy.value = true;
  try {
    const result: any = await launcher.execute(operation, parameters);
    if (!result.ok) throw new Error(result.error || '操作失败');
    return result.data;
  } catch (e: any) {
    error.value = e?.message || String(e);
    throw e;
  } finally {
    busy.value = false;
  }
};

async function refresh() {
  try {
    await session.refresh();
    await session.refreshIcons?.();
    const s: any = await launcher.execute('settings.list', { scope: 'launcher' });
    if (s.ok && Array.isArray(s.data?.entries)) settings.value = s.data.entries;
  } catch (e: any) {
    error.value = e?.message || String(e);
  }
}

function selectInstance(instance: any) {
  session.selectInstance(instance?.id || instance);
}

async function launch() {
  if (!selected.value) return;
  launchDialogShown.value = true;
  launchState.value = 'launching';
  launchProgress.value = 0;
  launchStage.value = '准备启动';
  try {
    const account = session.selectedAccount || session.accounts.find((a: any) => a.default) || session.accounts[0];
    if (!account) { page.value = 'accounts'; launchDialogShown.value = false; return; }
    let status: any = await execute('neo.task.start', { operation: 'instance.launch', priority: 'high', parameters: { instance: selected.value.id, account: String(account.internalId || account.id || account.profileName) } });
    const taskId = status.taskId;
    while (!status.finished) {
      launchStage.value = String(status.message || status.stage || '启动中');
      const total = Number(status.total ?? status.totalProgress);
      const current = Number(status.current ?? status.progress);
      launchProgress.value = total > 0 && Number.isFinite(current) ? Math.max(0, Math.min(100, current / total * 100)) : 0;
      await new Promise(r => setTimeout(r, 500));
      status = await execute('neo.task.status', { taskId });
    }
    if (status.state !== 'succeeded') throw new Error(status.error || '启动失败');
    launchStage.value = '启动完成';
    launchProgress.value = 100;
    setTimeout(() => { launchDialogShown.value = false; }, 800);
  } catch (e: any) {
    launchStage.value = e?.message || '启动失败';
    setTimeout(() => { launchDialogShown.value = false; }, 2000);
  }
}

function openInstanceSettings() {
  if (selected.value) page.value = 'instance';
}

async function openFolder() {
  if (!selected.value) return;
  try { await execute('instance.open-folder', { instance: selected.value.id }); } catch {}
}

async function submitInteraction(value: any) {
  if (interactionRequest.value) {
    try { await launcher.respond({ interactionId: interactionRequest.value.interactionId, value }); } catch (e: any) { error.value = e?.message || String(e); }
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

let unlistenEvent: any = null;
onMounted(async () => {
  try {
    unlistenEvent = await launcher.onEvent(async (raw: any) => {
      if (raw?.kind === 'input') {
        interactionRequest.value = raw;
        interactionShown.value = true;
      } else if (raw?.kind === 'status') {
        const messageText = raw.message || raw.data?.message || (typeof raw.data === 'string' ? raw.data : '');
        if (launchDialogShown.value && messageText) launchStage.value = messageText;
      }
    });
  } catch {}
  await refresh();
});

onUnmounted(() => {
  unlistenEvent?.();
});
</script>

<style scoped>
.ef-root { width: 100%; height: 100%; display: flex; flex-direction: column; background: var(--ef-paper); color: var(--ef-ink); font-family: var(--ef-font-ui); overflow: hidden; }
.ef-main { flex: 1; overflow: auto; padding: 24px 28px; }
.ef-status-dot { width: 7px; height: 7px; background: var(--ef-ink-2); display: inline-block; }
.ef-status-dot.connected { background: var(--ef-signal); }
</style>
