<template>
  <div class="pcl-standalone-editor">
    <PclWindowTitleBar :title="t('instanceSections.settings')" :subtitle="instance?.name || instanceId" />
    <div class="pcl-standalone-editor__body">
    <div v-if="loading" class="pcl-standalone-editor__loading"><PclLoading /> {{ t('pcl.standalone.reading') }}</div>
    <PclInstancePage
      v-else-if="instance"
      :instance="instance"
      :details="details"
      :icon="icon"
      :components="components"
      :settings="settings"
      :component-error="componentError"
      :notes="notes"
      :busy="refreshing || taskRunning || browseLoading"
      :execute="execute"
      :console-lines="consoleLines"
      :console-state="consoleState"
      :log-files="logFiles"
      :file-log="fileLog"
      @launch="launch"
      @open-folder="openFolder"
      @settings="() => {}"
      @delete="removeInstance"
      @change-icon="changeIcon"
      @save-notes="saveNotes"
      @refresh="refresh"
      @connect-console="connectConsole"
      @clear-console="clearConsole"
      @read-log="readLog"
      @component-enabled="updateComponentEnabled"
      @component-version="updateComponentVersion"
      @action="workspaceAction"
      @browse="browseComponent"
      @export="exportInstance"
      @save-setting="(key, value) => execute('settings.set', { scope: 'instance', instance: instanceId, key, value })"
      @install-start="installStarted"
      @install-progress="installProgressed"
      @install-done="installFinished"
    />
    <PclHint v-else-if="error" theme="red" :closable="false">{{ error }}</PclHint>
    </div>

    <div v-if="taskMessage" class="pcl-standalone-editor__task" role="status">
      <PclLoading v-if="taskRunning" />
      <span>{{ taskMessage }}</span>
    </div>

    <q-dialog v-model="browseOpen">
      <q-card class="pcl-standalone-editor__browse">
        <q-card-section class="pcl-standalone-editor__browse-head">
          <div>
            <div class="pcl-kicker">// COMPONENT VERSIONS</div>
            <h3>{{ browseLabel }}</h3>
          </div>
          <PclButton icon="x" variant="ghost" @click="browseOpen = false">{{ t('window.close') }}</PclButton>
        </q-card-section>
        <q-card-section>
          <div class="pcl-standalone-editor__browse-filter">
            <PclInput v-model="browseMinecraft" :placeholder="t('pcl.auto2.k48ad119705')" @enter="loadBrowseVersions" />
            <PclButton variant="highlight" icon="refresh-cw" :disabled="browseLoading" @click="loadBrowseVersions">
              {{ browseLoading ? t('pcl.auto2.k3667cb105a') : t('pcl.auto2.k68f7347528') }}
            </PclButton>
          </div>
          <PclHint v-if="browseError" theme="red">{{ browseError }}</PclHint>
          <PclLoading v-if="browseLoading" :text="t('pcl.auto2.k8e5fed83c5')" inline />
          <div v-else-if="!browseVersions.length" class="pcl-empty">{{ t('pcl.auto2.k926bc43f38') }}</div>
          <div v-else class="pcl-standalone-editor__versions">
            <button v-for="version in browseVersions" :key="String(version.version || version.id)" type="button" @click="applyBrowseVersion(version)">
              <span>{{ version.version || version.id }}</span>
              <small>{{ version.type || version.channel || '' }}</small>
            </button>
          </div>
        </q-card-section>
      </q-card>
    </q-dialog>
  </div>
</template>

<script setup lang="ts">
import { useI18n } from 'vue-i18n';
const { t } = useI18n();
import { onMounted, onUnmounted, ref } from 'vue';
import PclHint from '../../components/PclHint.vue';
import PclWindowTitleBar from '../../components/PclWindowTitleBar.vue';
import PclLoading from '../../components/PclLoading.vue';
import PclButton from '../../components/PclButton.vue';
import PclInput from '../../components/PclInput.vue';
import PclInstancePage from '../instance/PclInstancePage.vue';
import { launcher } from '../../../../boot/launcher-api';
import { pickPclFile } from '../../composables/usePclFileDialog';
import { isTaskItemFinished } from '../../composables/useTaskProgress';

const props = defineProps<{ instanceId: string }>();

const loading = ref(true);
const refreshing = ref(false);
const error = ref('');
const instance = ref<any>(null);
const details = ref<any>(null);
const icon = ref('');
const components = ref<any[]>([]);
const settings = ref<any[]>([]);
const componentError = ref('');
const notes = ref('');
const logFiles = ref<any[]>([]);
const fileLog = ref('');
const consoleLines = ref<string[]>([]);
const consoleState = ref(t('pcl.standalone.disconnected'));
const taskMessage = ref('');
const taskRunning = ref(false);
const browseOpen = ref(false);
const browseUid = ref('');
const browseLabel = ref('');
const browseMinecraft = ref('');
const browseVersions = ref<any[]>([]);
const browseLoading = ref(false);
const browseError = ref('');
let disposed = false;
let consoleSubscription = '';
let unlistenConsole: (() => void) | undefined;

async function execute(operation: string, parameters: Record<string, unknown> = {}) {
  const result = await launcher.execute(operation, parameters);
  if (result && result.ok === false) throw new Error(result.error || operation);
  return result?.data ?? result;
}

async function updateComponentEnabled(component: any, enabled: boolean) {
  try {
    await execute('instance.component.set-enabled', { instance: props.instanceId, component: component.id, enabled });
    await refresh();
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  }
}

async function updateComponentVersion(component: any, version: string) {
  try {
    await runTask('instance.component.set-version', {
      instance: props.instanceId,
      component: component.id,
      version,
      important: component.id === 'net.minecraft',
    }, t('app.installingComponent', { name: component.name || component.id }));
    await refresh();
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  }
}

async function runTask(operation: string, parameters: Record<string, unknown>, message: string) {
  taskRunning.value = true;
  taskMessage.value = message;
  try {
    let task: any = await execute('neo.task.start', { operation, priority: 'high', parameters });
    while (task && !isTaskItemFinished(task)) {
      if (disposed) return;
      await new Promise(resolve => setTimeout(resolve, 350));
      task = await execute('neo.task.status', { taskId: task.taskId });
    }
    if (!task || task.state !== 'succeeded') throw new Error(task?.error || message);
    return task;
  } finally {
    taskRunning.value = false;
    taskMessage.value = '';
  }
}

async function workspaceAction(operation: string) {
  try {
    if (operation === 'rename') {
      const name = window.prompt(t('instance.rename'), instance.value?.name || '');
      if (name?.trim()) { await execute('instance.rename', { instance: props.instanceId, name: name.trim() }); await refresh(); }
      return;
    }
    if (operation === 'open-saves' || operation === 'open-mods') {
      await execute('instance.open-folder', { instance: props.instanceId, kind: operation.slice(5) });
      return;
    }
    if (operation === 'update') {
      await runTask('instance.update', { instance: props.instanceId }, t('instance.update'));
      await refresh();
      return;
    }
    await execute(`instance.${operation}`, { instance: props.instanceId });
    await refresh();
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  }
}

async function browseComponent(uid: string) {
  browseUid.value = String(uid || '');
  const component = components.value.find(item => String(item?.id || item?.uid || '').toLowerCase() === browseUid.value.toLowerCase());
  browseLabel.value = component?.name || browseUid.value;
  browseMinecraft.value = String(components.value.find(item => String(item?.id || item?.uid || '').toLowerCase() === 'net.minecraft')?.version || details.value?.minecraftVersion || '');
  browseVersions.value = [];
  browseError.value = '';
  browseOpen.value = true;
  await loadBrowseVersions();
}

async function loadBrowseVersions() {
  if (!browseUid.value) return;
  browseLoading.value = true;
  browseError.value = '';
  try {
    const result: any = await execute('component.versions', {
      uid: browseUid.value,
      ...(browseMinecraft.value.trim() ? { minecraftVersion: browseMinecraft.value.trim() } : {}),
    });
    browseVersions.value = Array.isArray(result) ? result : (result?.versions || []);
  } catch (exception) {
    browseError.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    browseLoading.value = false;
  }
}

async function applyBrowseVersion(version: any) {
  const value = String(version?.version || version?.id || '').trim();
  if (!value) return;
  browseOpen.value = false;
  await updateComponentVersion({ id: browseUid.value }, value);
}

async function exportInstance(payload: any) {
  try {
    await runTask('instance.export', {
      instance: props.instanceId,
      output: payload.output,
      format: payload.format || 'zip',
      name: payload.name,
      version: payload.version,
      summary: payload.summary,
      author: payload.author,
      optionalFiles: Boolean(payload.optionalFiles),
      overwrite: payload.overwrite !== false,
    }, t('app.exportingInstance'));
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  }
}

function installStarted(info: any) { taskRunning.value = true; taskMessage.value = String(info?.title || t('resource.installing')); }
function installProgressed(task: any) { if (task?.status || task?.message) taskMessage.value = String(task.status || task.message); }
function installFinished(info: any) { taskRunning.value = false; taskMessage.value = info?.error ? String(info.error) : ''; void refresh(); }

async function connectConsole() {
  if (consoleSubscription || disposed) return;
  try {
    const result: any = await execute('instance.console.subscribe', { instance: props.instanceId });
    consoleSubscription = String(result?.subscriptionId || '');
    if (!consoleSubscription) return;
    unlistenConsole = await launcher.onStream((batch: any) => {
      if (disposed || batch?.subscriptionId !== consoleSubscription) return;
      for (const event of batch.events || []) {
        if (event.kind === 'console.line') consoleLines.value.push(String(event.text || ''));
        else if (event.kind === 'console.data' && event.data) consoleLines.value.push(new TextDecoder().decode(Uint8Array.from(atob(event.data), c => c.charCodeAt(0))));
        else if (event.kind === 'instance.state') consoleState.value = event.running ? t('instance.running') : t('instance.stopped');
      }
      consoleLines.value.splice(0, Math.max(0, consoleLines.value.length - 3000));
    });
    consoleState.value = t('app.consoleConnected');
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  }
}

function clearConsole() { consoleLines.value = []; }
function disconnectConsole() {
  unlistenConsole?.();
  unlistenConsole = undefined;
  const subscription = consoleSubscription;
  consoleSubscription = '';
  if (subscription) void execute('event.unsubscribe', { subscriptionId: subscription }).catch(() => {});
}

async function load() {
  loading.value = true;
  error.value = '';
  componentError.value = '';
  try {
    const info: any = await execute('instance.info', { instance: props.instanceId });
    instance.value = { ...info, id: info?.id || props.instanceId };
    details.value = info;
    notes.value = String(info?.notes || '');
    const [iconResult, componentResult, settingResult, logs]: any[] = await Promise.all([
      execute('instance.icon', { instance: props.instanceId }).catch(() => null),
      execute('instance.components.list', { instance: props.instanceId }).catch((exception: any) => {
        componentError.value = exception instanceof Error ? exception.message : String(exception);
        return { components: [] };
      }),
      execute('settings.list', { scope: 'instance', instance: props.instanceId }).catch(() => []),
      execute('instance.log.list', { instance: props.instanceId }).catch(() => []),
    ]);
    icon.value = String(iconResult?.dataUrl || '');
    components.value = Array.isArray(componentResult?.components) ? componentResult.components : [];
    settings.value = Array.isArray(settingResult) ? settingResult : [];
    logFiles.value = Array.isArray(logs) ? logs : [];
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    loading.value = false;
  }
}

// Refresh the data for the open editor window in place.  Re-running load()
// toggles the top-level loading branch, unmounts PclInstancePage, resets its
// selected section and makes the whole editor visibly flash.  A refresh only
// needs the instance summary, component profile, settings and log index; the
// window and the current section stay mounted while those requests complete.
async function refresh() {
  if (loading.value || refreshing.value) return;
  refreshing.value = true;
  error.value = '';
  componentError.value = '';
  try {
    const [info, componentResult, settingResult, logs]: any[] = await Promise.all([
      execute('instance.info', { instance: props.instanceId }),
      execute('instance.components.list', { instance: props.instanceId }).catch((exception: any) => {
        componentError.value = exception instanceof Error ? exception.message : String(exception);
        return { components: [] };
      }),
      execute('settings.list', { scope: 'instance', instance: props.instanceId }).catch(() => []),
      execute('instance.log.list', { instance: props.instanceId }).catch(() => []),
    ]);
    instance.value = { ...instance.value, ...info, id: info?.id || props.instanceId };
    details.value = info;
    notes.value = String(info?.notes || '');
    components.value = Array.isArray(componentResult?.components) ? componentResult.components : [];
    settings.value = Array.isArray(settingResult) ? settingResult : [];
    logFiles.value = Array.isArray(logs) ? logs : [];
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    refreshing.value = false;
  }
}

async function launch() {
  try {
    await execute('neo.task.start', { operation: 'instance.launch', priority: 'high', parameters: { instance: props.instanceId } });
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  }
}

async function openFolder() {
  try { await execute('instance.open-folder', { instance: props.instanceId }); }
  catch (exception) { error.value = exception instanceof Error ? exception.message : String(exception); }
}

async function saveNotes(value: string) {
  try { await execute('instance.set-notes', { instance: props.instanceId, notes: value }); notes.value = value; }
  catch (exception) { error.value = exception instanceof Error ? exception.message : String(exception); }
}

async function changeIcon(iconKey?: string) {
  if (iconKey?.trim()) {
    try {
      await execute('instance.set-icon', { instance: props.instanceId, icon: iconKey.trim() });
      const result: any = await execute('instance.icon', { instance: props.instanceId }).catch(() => null);
      icon.value = String(result?.dataUrl || '');
    } catch (exception) {
      error.value = exception instanceof Error ? exception.message : String(exception);
    }
    return;
  }
  const path = await pickPclFile({ title: t('pcl.standalone.chooseIcon'), filters: [{ name: t('pcl.standalone.image'), extensions: ['png', 'jpg', 'jpeg', 'ico', 'webp'] }] });
  if (!path?.trim()) return;
  try {
    await execute('instance.set-icon', { instance: props.instanceId, path: path.trim() });
    const result: any = await execute('instance.icon', { instance: props.instanceId }).catch(() => null);
    icon.value = String(result?.dataUrl || '');
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  }
}

async function removeInstance() {
  if (!window.confirm(t('pcl.standalone.confirmRemove'))) return;
  try {
    await execute('instance.delete', { instance: props.instanceId, confirm: true });
    window.close();
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  }
}

async function readLog(file: string) {
  try {
    const result: any = await execute('instance.log.read', { instance: props.instanceId, file, maxBytes: 1048576 });
    fileLog.value = String(result?.content || '') + (result?.truncated ? `\n[${t('app.logTruncated')}]` : '');
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  }
}

onMounted(() => {
  disposed = false;
  document.documentElement.dataset.neoShell = 'pcl';
  document.title = `${t('instance.client')} - Luna Launcher`;
  void load();
});
onUnmounted(() => { disposed = true; disconnectConsole(); });
</script>

<style scoped>
.pcl-standalone-editor {
  display: flex;
  flex-direction: column;
  width: 100vw;
  height: 100vh;
  overflow: hidden;
  background: var(--pcl-page-bg);
}

.pcl-standalone-editor__body {
  flex: 1 1 auto;
  min-height: 0;
  overflow: hidden;
}

.pcl-standalone-editor :deep(.pcl-instance-page) {
  width: 100%;
  height: 100%;
  max-width: none;
  margin: 0;
}

.pcl-standalone-editor :deep(.pcl-page-main) {
  height: 100%;
  overflow-y: auto;
}

.pcl-standalone-editor__loading {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 8px;
  height: 100vh;
  color: var(--pcl-muted);
  font-size: 12px;
}

.pcl-standalone-editor__task {
  position: fixed;
  right: 16px;
  bottom: 16px;
  z-index: 20;
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 9px 12px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 5px;
  background: var(--pcl-card);
  color: var(--pcl-text);
  box-shadow: var(--pcl-shadow);
  font-size: 12px;
}

.pcl-standalone-editor__browse {
  width: 620px;
  max-width: 94vw;
  max-height: 82vh;
  overflow: hidden;
}

.pcl-standalone-editor__browse-head {
  display: flex;
  justify-content: space-between;
  align-items: flex-start;
  border-bottom: 1px solid var(--pcl-border);
}

.pcl-standalone-editor__browse-head h3 {
  margin: 4px 0 0;
  color: var(--pcl-text);
  font-size: 17px;
}

.pcl-standalone-editor__browse-filter {
  display: flex;
  gap: 8px;
  margin-bottom: 10px;
}

.pcl-standalone-editor__browse-filter .pcl-input {
  flex: 1;
}

.pcl-standalone-editor__versions {
  max-height: 52vh;
  overflow: auto;
  display: grid;
  gap: 5px;
}

.pcl-standalone-editor__versions button {
  display: flex;
  justify-content: space-between;
  gap: 10px;
  width: 100%;
  padding: 9px 10px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-half-white);
  color: var(--pcl-text);
  text-align: left;
  cursor: pointer;
}

.pcl-standalone-editor__versions button:hover {
  border-color: var(--pcl-c3);
  background: var(--pcl-hover);
}

.pcl-standalone-editor__versions small {
  color: var(--pcl-muted);
}
</style>
