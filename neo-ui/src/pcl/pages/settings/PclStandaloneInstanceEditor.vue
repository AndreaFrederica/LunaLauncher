<template>
  <div class="pcl-standalone-editor">
    <PclWindowTitleBar title="实例设置" :subtitle="instance?.name || instanceId" mark="L" />
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
      :busy="false"
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
      @refresh="load"
      @connect-console="() => {}"
      @clear-console="() => {}"
      @read-log="readLog"
      @component-enabled="(component, enabled) => execute('instance.component.set-enabled', { instance: instanceId, component: component.id, enabled })"
      @component-version="(component, version) => execute('instance.component.set-version', { instance: instanceId, component: component.id, version })"
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

const props = defineProps<{ instanceId: string }>();

const loading = ref(true);
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

async function load() {
  loading.value = true;
  error.value = '';
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
  const path = window.prompt('图标文件完整路径（PNG / ICO 等）');
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
