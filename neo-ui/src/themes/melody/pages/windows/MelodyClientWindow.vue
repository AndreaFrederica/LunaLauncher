<template>
  <MelodyModernWindow kind="client" :title="(instance?.name || instanceId) + ' · 客户端管理'" @close="emit('close')">
    <div class="melody-modern-column">
    <p v-if="error" class="melody-error" role="alert">{{ error }}</p>
    <div class="melody-editor-body">
    <div v-if="loading" class="pcl-standalone-editor__loading"><PclLoading /> {{ t('pcl.standalone.reading') }}</div>
    <PclInstancePage
      v-else-if="instance"
      :instance="instance"
      :show-favorite="false"
      :details="details"
      :icon="icon"
      :components="components"
      :settings="settings"
      :component-error="componentError"
      :notes="notes"
      :busy="refreshing"
      :execute="execute"
      :console-lines="consoleLines"
      :console-state="consoleState"
      :log-files="logFiles"
      :file-log="fileLog"
      @launch="launch"
      @open-folder="openFolder"
      @delete="confirmRemove = true"
      @change-icon="changeIcon"
      @save-notes="saveNotes"
      @refresh="refresh"
      @connect-console="connectConsole"
      @clear-console="consoleLines = []"
      @read-log="readLog"
      @component-enabled="updateComponentEnabled"
      @component-version="updateComponentVersion"
      @action="workspaceAction"
      @browse="uid => emit('browse', uid)"
      @export="exportInstance"
      @save-setting="(key, value) => action('settings.set', { scope: 'instance', instance: instanceId, key, value })"
      @install-done="refresh"
    />
    </div>
    <MelodyTaskPanel />
    </div>
    <q-dialog v-model="confirmRemove" persistent>
      <q-card class="melody-modern-content melody-confirm">
        <h2>删除客户端</h2><p>确定删除“{{ instance?.name }}”及其文件？此操作无法撤销。</p>
        <div class="melody-toolbar"><MelodyButton @click="confirmRemove = false">取消</MelodyButton><MelodyButton @click="removeInstance">确认删除</MelodyButton></div>
      </q-card>
    </q-dialog>
    <q-dialog v-model="renameOpen" persistent>
      <q-card class="melody-modern-content melody-confirm">
        <h2>重命名客户端</h2>
        <MelodyInput v-model="newName" aria-label="客户端名称" @keydown.enter="rename" />
        <div class="melody-toolbar"><MelodyButton @click="renameOpen = false">取消</MelodyButton><MelodyButton :disabled="!newName.trim()" @click="rename">保存</MelodyButton></div>
      </q-card>
    </q-dialog>
  </MelodyModernWindow>
</template>
<script setup lang="ts">
import { useI18n } from 'vue-i18n';
const { t } = useI18n();
import { onMounted, onUnmounted, provide, ref } from 'vue';
import { resourceWindowHostKey } from '../../../pcl/composables/resource-window-host';
import MelodyModernWindow from '../../components/MelodyModernWindow.vue';
import MelodyButton from '../../components/MelodyButton.vue';
import MelodyInput from '../../components/MelodyInput.vue';
import MelodyTaskPanel from '../../components/MelodyTaskPanel.vue';
import { melodyExecute, notifyMelodyInstancesChanged } from '../../services/backend';
import PclLoading from '../../../pcl/components/PclLoading.vue';
import PclInstancePage from '../../../pcl/pages/instance/PclInstancePage.vue';
import { launcher } from '../../../../boot/launcher-api';
import { pickPclFile } from '../../../pcl/composables/usePclFileDialog';
import { useThemeResource } from '../../../../common/runtime/theme-runtime';

const props = defineProps<{ instanceId: string }>();
const emit = defineEmits<{ close: []; browse: [uid: string] }>();
provide(resourceWindowHostKey, (_instance, kind) => emit('browse', `resource:${kind}`));
const confirmRemove = ref(false);
const renameOpen = ref(false), newName = ref('');
let disposed = false;
const consoleLines = ref<string[]>([]), consoleState = ref('未连接');
let subscription = '', unlistenStream: (() => void) | undefined;
let connecting = false;
let runtimeActive = false;
let consoleGeneration = 0;

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

const execute = melodyExecute;
async function workspaceAction(operation: string) {
  if (operation === 'rename') { newName.value = instance.value?.name || ''; renameOpen.value = true; return; }
  if (operation === 'update') { await runTask('instance.update', { instance: props.instanceId }); return; }
  if (operation === 'open-saves' || operation === 'open-mods') { await action('instance.open-folder', { instance: props.instanceId, kind: operation.slice(5) }); return; }
  await action(`instance.${operation}`, { instance: props.instanceId });
}
async function rename() {
  if (!newName.value.trim()) return;
  try { await execute('instance.rename', { instance: props.instanceId, name: newName.value.trim() }); renameOpen.value = false; await refresh(); notifyMelodyInstancesChanged(); }
  catch (e) { error.value = String(e); renameOpen.value = false; }
}
async function action(operation: string, parameters: Record<string, unknown> = {}) {
  try { await execute(operation, parameters); await refresh(); notifyMelodyInstancesChanged(); }
  catch (e) { error.value = String(e); }
}
async function runTask(operation: string, parameters: Record<string, unknown>) {
  try {
    let task = await execute('neo.task.start', { operation, priority: 'high', parameters });
    while (!task.finished && !disposed && runtimeActive) {
      await new Promise(resolve => setTimeout(resolve, 400));
      if (disposed || !runtimeActive) return;
      task = await execute('neo.task.status', { taskId: task.taskId });
    }
    if (disposed || !runtimeActive) return;
    if (task.state !== 'succeeded') throw new Error(task.error || '操作未完成');
    await refresh(); notifyMelodyInstancesChanged();
  } catch (e) { if (!disposed) error.value = String(e); }
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
  await runTask('instance.component.set-version', { instance: props.instanceId, component: component.id, version, important: component.id === 'net.minecraft' });
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
    settings.value = Array.isArray(settingResult) ? settingResult : settingResult?.entries || [];
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
    settings.value = Array.isArray(settingResult) ? settingResult : settingResult?.entries || [];
    logFiles.value = Array.isArray(logs) ? logs : [];
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    refreshing.value = false;
  }
}

async function launch() {
  await connectConsole();
  await runTask('instance.launch', { instance: props.instanceId });
}
async function exportInstance(payload: any) {
  await runTask('instance.export', { ...payload, instance: props.instanceId });
}
async function connectConsole() {
  if (!runtimeActive || subscription || connecting) return;
  connecting = true;
  const generation = ++consoleGeneration;
  try {
    const sub = await execute('instance.console.subscribe', { instance: props.instanceId });
    if (!runtimeActive || disposed || generation !== consoleGeneration) {
      if (sub?.subscriptionId) void execute('event.unsubscribe', { subscriptionId: sub.subscriptionId }).catch(() => {});
      return;
    }
    subscription = String(sub.subscriptionId);
    const unlisten = await launcher.onStream((batch: any) => {
      if (disposed || batch.subscriptionId !== subscription) return;
      for (const event of batch.events || []) {
        if (event.kind === 'console.line') consoleLines.value.push(event.text);
        else if (event.kind === 'console.data') consoleLines.value.push(new TextDecoder().decode(Uint8Array.from(atob(event.data), c => c.charCodeAt(0))));
        else if (event.kind === 'instance.state') { consoleState.value = event.running ? '运行中' : '已停止'; if (instance.value) instance.value.running = event.running; notifyMelodyInstancesChanged(); }
        else if (event.kind === 'console.reset') consoleState.value = event.available ? '已连接' : '等待启动';
      }
      consoleLines.value.splice(0, Math.max(0, consoleLines.value.length - 3000));
    });
    if (!runtimeActive || disposed || generation !== consoleGeneration) {
      unlisten();
      const staleSubscription = subscription;
      subscription = '';
      if (staleSubscription) void execute('event.unsubscribe', { subscriptionId: staleSubscription }).catch(() => {});
      return;
    }
    unlistenStream = unlisten;
    consoleState.value = '已连接';
  } catch (e) { error.value = String(e); } finally { connecting = false; }
}
function disconnectConsole() {
  consoleGeneration++;
  unlistenStream?.();
  unlistenStream = undefined;
  const activeSubscription = subscription;
  subscription = '';
  if (activeSubscription) void execute('event.unsubscribe', { subscriptionId: activeSubscription }).catch(() => {});
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
      notifyMelodyInstancesChanged();
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
  confirmRemove.value = false;
  try {
    await execute('instance.delete', { instance: props.instanceId, confirm: true });
    notifyMelodyInstancesChanged();
    emit('close');
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

onMounted(load);
useThemeResource('melody', () => {
  runtimeActive = true;
  disposed = false;
  return () => { runtimeActive = false; disconnectConsole(); };
});

onUnmounted(() => { disposed = true; });
</script>
