<template>
  <q-dialog :model-value="modelValue" @update:model-value="emit('update:modelValue', $event)">
    <q-card class="pcl-dialog pcl-task-center">
      <q-card-section class="pcl-task-center__head">
        <div>
          <div class="pcl-kicker">{{ t('ui.tasks') }}</div>
          <h3>{{ t('tasks.title') }}</h3>
          <p>{{ t('tasks.description') }}</p>
        </div>
        <PclButton size="sm" icon="refresh-cw" :disabled="loading" @click="load">{{ t('common.refresh') }}</PclButton>
      </q-card-section>

      <q-card-section class="pcl-task-center__body">
        <div v-if="activeDownloads.length" class="pcl-task-downloads">
          <div class="pcl-task-downloads__head">{{ t('tasks.runningCount', { count: activeDownloadCount }) }}<template v-if="activeDownloadCount > activeDownloads.length">{{ t('tasks.showingFirst', { count: activeDownloads.length }) }}</template></div>
          <div v-for="row in activeDownloads" :key="row.id" class="pcl-task-downloads__row">
            <span class="pcl-task-downloads__name">{{ row.label }}</span>
            <span class="pcl-task-downloads__meta">{{ row.percent }}<template v-if="row.rate"> · {{ formatRate(row.rate) }}</template></span>
            <div class="pcl-progress-track pcl-task-downloads__track"><span :style="{ width: `${row.value}%` }" /></div>
          </div>
        </div>
        <div v-show="tasks.length" class="pcl-task-list">
          <div v-for="task in tasks" :key="task.taskId" class="pcl-task-row">
            <div class="pcl-task-row__head">
              <strong>{{ operationLabel(task.operation) }}</strong>
              <span class="pcl-task-row__state" :class="task.state">{{ stateLabel(task.state) }}</span>
            </div>
            <div class="pcl-progress-track">
              <span :style="{ width: `${percent(task)}%` }" />
            </div>
            <div class="pcl-task-row__meta">
              <span>{{ task.status || task.details || task.error || t('tasks.waiting') }}</span>
              <span v-if="task.transferRate">{{ formatRate(task.transferRate) }}</span>
            </div>
            <div class="pcl-task-row__meta">
              <span>{{ taskStats(task).summary }}</span>
              <PclButton v-if="!task.finished" size="sm" variant="ghost" icon="x" @click="cancel(task)">{{ t('common.cancel') }}</PclButton>
            </div>
          </div>
        </div>
        <div v-show="!tasks.length" class="pcl-empty">
          <PclIcon name="assignment" :size="34" />
          <strong>{{ loading ? t('tasks.loading') : t('tasks.empty') }}</strong>
          <span>{{ t('tasks.emptyHint') }}</span>
        </div>
      </q-card-section>
    </q-card>
  </q-dialog>
</template>

<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import PclButton from './PclButton.vue';
import PclIcon from './PclIcon.vue';
import { subscribeTaskSnapshots } from '../composables/useTaskEvents';
import { formatTaskRate, taskActiveItems, isTaskItemActive, taskItemProgressText, taskItemCurrent, taskItemLabel, taskItemTotal, taskProgressStats } from '../composables/useTaskProgress';

const props = defineProps<{
  modelValue: boolean;
  execute: (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
}>();
const { t } = useI18n();
const emit = defineEmits<{ 'update:modelValue': [value: boolean] }>();
const tasks = ref<any[]>([]);
const loading = ref(false);
let unsubscribeTaskSnapshots: (() => void) | undefined;

async function load() {
  if (!props.modelValue) return;
  loading.value = true;
  try {
    const result = await props.execute('neo.task.list');
    tasks.value = Array.isArray(result?.tasks) ? result.tasks : Array.isArray(result) ? result : [];
  } catch {
    tasks.value = [];
  } finally {
    loading.value = false;
  }
}
function applyTaskEvent(task: any) {
  if (!task?.taskId) return;
  const index = tasks.value.findIndex(entry => entry.taskId === task.taskId);
  if (index >= 0) tasks.value[index] = { ...tasks.value[index], ...task };
  else tasks.value.unshift(task);
}

const activeDownloadCount = computed(() => tasks.value.filter(task => !task.finished).reduce((sum, task) => sum + taskProgressStats(task).active, 0));
const activeDownloads = computed(() => tasks.value.filter(task => !task.finished).flatMap((task: any) => taskActiveItems(task, 64)
  .filter(isTaskItemActive)
  .map((row: any) => {
    const total = taskItemTotal(row), current = taskItemCurrent(row);
    const value = total > 0 ? Math.max(0, Math.min(100, Math.round(current / total * 100))) : 0;
    return { id: `${task.taskId}:${row.id}`, label: taskItemLabel(row), value, percent: taskItemProgressText(row), rate: Number(row.transferRate || 0) };
  })).slice(0, 12));

async function cancel(task: any) {
  try {
    await props.execute('neo.task.cancel', { taskId: task.taskId });
    await load();
  } catch {
    // The task may already be terminal.
  }
}
function percent(task: any) {
  return taskStats(task).percent ?? 0;
}
function taskStats(task: any) { return taskProgressStats(task); }
function formatRate(value: unknown) {
  return formatTaskRate(value);
}
function operationLabel(operation: string) {
  const labels: Record<string, string> = {
    'modpack.install': t('tasks.operations.modpackInstall'),
    'instance.import': t('tasks.operations.instanceImport'),
    'instance.create': t('tasks.operations.instanceCreate'),
    'instance.update': t('tasks.operations.instanceUpdate'),
    'instance.special-loader.install': t('tasks.operations.specialLoader'),
    'resource.install': t('tasks.operations.resourceInstall'),
    'resource.install-version': t('tasks.operations.resourceInstall'),
    'resource.install-with-dependencies': t('tasks.operations.resourceWithDependencies'),
    'instance.component.set-version': t('tasks.operations.componentInstall'),
    'java.install': t('tasks.operations.javaInstall'),
    'account.login': t('tasks.operations.accountLogin'),
    'account.refresh': t('tasks.operations.accountRefresh'),
    'instance.launch': t('tasks.operations.launch'),
    'instance.export': t('tasks.operations.export'),
    'server.start': t('tasks.operations.serverStart'),
  };
  return labels[operation] || operation;
}
function stateLabel(state: string) {
  return (({ queued: t('tasks.states.queued'), running: t('tasks.states.running'), succeeded: t('tasks.states.succeeded'), failed: t('tasks.states.failed'), cancelled: t('tasks.states.cancelled'), canceled: t('tasks.states.cancelled'), aborted: t('tasks.states.aborted'), completed: t('tasks.states.succeeded'), interrupted: t('tasks.states.interrupted') } as Record<string, string>)[state] || state || t('tasks.states.unknown'));
}

watch(() => props.modelValue, value => {
  if (value) void load();
}, { immediate: true });
onMounted(async () => {
  try {
    unsubscribeTaskSnapshots = await subscribeTaskSnapshots((snapshot: any) => {
      if (props.modelValue) applyTaskEvent(snapshot);
    });
  } catch {
    // The manual refresh button remains available.
  }
});
onUnmounted(() => unsubscribeTaskSnapshots?.());
</script>

<style scoped>
.pcl-task-center { width: 700px; max-width: 94vw; }
.pcl-task-center__head { display: flex; align-items: flex-start; gap: 14px; padding-bottom: 8px; }
.pcl-task-center__head > div { flex: 1; min-width: 0; }
.pcl-task-center__head h3 { margin: 3px 0; font-size: calc(18px * var(--pcl-font-scale, 1)); }
.pcl-task-center__head p { margin: 0; color: var(--pcl-muted); font-size: calc(11px * var(--pcl-font-scale, 1)); }
.pcl-task-center__body { max-height: 62vh; overflow-y: auto; padding-top: 0; }
.pcl-task-list { display: flex; flex-direction: column; gap: 8px; }
.pcl-task-row { padding: 10px; border: 1px solid var(--pcl-border); border-radius: 4px; background: var(--pcl-c8); }
.pcl-task-row__head, .pcl-task-row__meta { display: flex; align-items: center; justify-content: space-between; gap: 8px; }
.pcl-task-row__head strong { color: var(--pcl-text); font-size: calc(12px * var(--pcl-font-scale, 1)); }
.pcl-task-row__state { color: var(--pcl-muted); font-size: calc(10px * var(--pcl-font-scale, 1)); }
.pcl-task-row__state.running { color: var(--pcl-c3); }
.pcl-task-row__state.succeeded { color: #34d399; }
.pcl-task-row__state.failed { color: #fb7185; }
.pcl-task-row__meta { margin-top: 5px; color: var(--pcl-muted); font-size: calc(10px * var(--pcl-font-scale, 1)); }
.pcl-task-row__meta span:first-child { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.pcl-task-row .pcl-progress-track { margin: 7px 0 0; }

.pcl-task-downloads { margin-bottom: 10px; padding: 8px; border: 1px solid var(--pcl-border); border-radius: 4px; background: var(--pcl-c8); }
.pcl-task-downloads__head { margin-bottom: 4px; color: var(--pcl-c3); font-size: calc(9px * var(--pcl-font-scale, 1)); }
.pcl-task-downloads__row { display: grid; grid-template-columns: minmax(0, 1fr) auto; gap: 2px 8px; padding: 4px 0; border-top: 1px solid var(--pcl-border); }
.pcl-task-downloads__row:first-of-type { border-top: 0; }
.pcl-task-downloads__name { overflow: hidden; color: var(--pcl-text); font-size: calc(10px * var(--pcl-font-scale, 1)); text-overflow: ellipsis; white-space: nowrap; }
.pcl-task-downloads__meta { color: var(--pcl-muted); font-size: calc(9px * var(--pcl-font-scale, 1)); }
.pcl-task-downloads__track { grid-column: 1 / -1; margin-top: 2px; }
</style>
