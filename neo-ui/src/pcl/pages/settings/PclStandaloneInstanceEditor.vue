<template>
  <div class="pcl-standalone-editor">
    <PclWindowTitleBar title="实例设置" :subtitle="instance?.name || instanceId" />
    <div class="pcl-standalone-editor__body">
    <div v-if="loading" class="pcl-standalone-editor__loading"><PclLoading /> 正在读取实例…</div>
    <PclInstancePage
      v-else-if="instance"
      :instance="instance"
      :details="details"
      :icon="icon"
      :components="components"
      :settings="settings"
      :component-error="componentError"
      :notes="notes"
      :busy="refreshing"
      :execute="execute"
      :console-lines="[]"
      :console-state="'未连接'"
      :log-files="logFiles"
      :file-log="fileLog"
      @launch="launch"
      @open-folder="openFolder"
      @settings="() => {}"
      @delete="removeInstance"
      @change-icon="changeIcon"
      @save-notes="saveNotes"
      @refresh="refresh"
      @connect-console="() => {}"
      @clear-console="() => {}"
      @read-log="readLog"
      @component-enabled="updateComponentEnabled"
      @component-version="updateComponentVersion"
      @action="(operation) => execute('instance.' + operation, { instance: instanceId })"
      @browse="() => {}"
      @export="() => {}"
      @save-setting="(key, value) => execute('settings.set', { scope: 'instance', instance: instanceId, key, value })"
      @install-start="() => {}"
      @install-progress="() => {}"
      @install-done="() => {}"
    />
    <PclHint v-else-if="error" theme="red" :closable="false">{{ error }}</PclHint>
    </div>
  </div>
</template>

<script setup lang="ts">
import { onMounted, ref } from 'vue';
import PclHint from '../../components/PclHint.vue';
import PclWindowTitleBar from '../../components/PclWindowTitleBar.vue';
import PclLoading from '../../components/PclLoading.vue';
import PclInstancePage from '../instance/PclInstancePage.vue';
import { launcher } from '../../../boot/launcher-api';
import { pickPclFile } from '../../composables/usePclFileDialog';

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
    let task: any = await execute('neo.task.start', {
      operation: 'instance.component.set-version',
      priority: 'high',
      parameters: { instance: props.instanceId, component: component.id, version, important: component.id === 'net.minecraft' },
    });
    while (task && !task.finished) {
      await new Promise(resolve => setTimeout(resolve, 250));
      task = await execute('neo.task.status', { taskId: task.taskId });
    }
    if (!task || task.state !== 'succeeded') throw new Error(task?.error || '组件安装失败');
    await refresh();
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  }
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

async function changeIcon() {
  const path = await pickPclFile({ title: '选择实例图标', filters: [{ name: '图片', extensions: ['png', 'jpg', 'jpeg', 'ico', 'webp'] }] });
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
  if (!window.confirm('确认移除这个实例？')) return;
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
    fileLog.value = String(result?.content || '') + (result?.truncated ? '\n[文件超过 1 MiB，仅显示前部分]' : '');
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  }
}

onMounted(() => {
  document.documentElement.dataset.neoShell = 'pcl';
  document.title = '实例 - Luna Launcher';
  void load();
});
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
</style>
