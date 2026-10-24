<template>
  <q-dialog :model-value="modelValue" @update:model-value="emit('update:modelValue', $event)">
    <q-card class="pcl-dialog pcl-task-center">
      <q-card-section class="pcl-task-center__head">
        <div>
          <div class="pcl-kicker">TASKS</div>
          <h3>任务中心</h3>
          <p>查看安装、下载和启动任务，取消不需要的任务。</p>
        </div>
        <PclButton size="sm" icon="refresh-cw" :disabled="loading" @click="load">刷新</PclButton>
      </q-card-section>

      <q-card-section class="pcl-task-center__body">
        <div v-if="activeDownloads.length" class="pcl-task-downloads">
          <div class="pcl-task-downloads__head">正在下载 · {{ activeDownloads.length }} 项</div>
          <div v-for="row in activeDownloads" :key="row.id" class="pcl-task-downloads__row">
            <span class="pcl-task-downloads__name">{{ row.label }}</span>
            <span class="pcl-task-downloads__meta">{{ row.percent }}<template v-if="row.rate"> · {{ formatRate(row.rate) }}</template></span>
            <div class="pcl-progress-track pcl-task-downloads__track"><span :style="{ width: `${row.value}%` }" /></div>
          </div>
        </div>
        <div v-if="tasks.length" class="pcl-task-list">
          <div v-for="task in tasks" :key="task.taskId" class="pcl-task-row">
            <div class="pcl-task-row__head">
              <strong>{{ operationLabel(task.operation) }}</strong>
              <span class="pcl-task-row__state" :class="task.state">{{ stateLabel(task.state) }}</span>
            </div>
            <div class="pcl-progress-track">
              <span :style="{ width: `${percent(task)}%` }" />
            </div>
            <div class="pcl-task-row__meta">
              <span>{{ task.status || task.details || task.error || '等待任务状态…' }}</span>
              <span v-if="task.transferRate">{{ formatRate(task.transferRate) }}</span>
            </div>
            <div class="pcl-task-row__meta">
              <span>{{ percent(task) }}% <template v-if="task.total > 0"> · {{ formatSize(task.progress) }} / {{ formatSize(task.total) }}</template></span>
              <PclButton v-if="!task.finished" size="sm" variant="ghost" icon="x" @click="cancel(task)">取消</PclButton>
            </div>
          </div>
        </div>
        <div v-else class="pcl-empty">
          <PclIcon name="assignment" :size="34" />
          <strong>{{ loading ? '正在读取任务' : '没有正在执行的任务' }}</strong>
          <span>安装和启动任务会显示在这里。</span>
        </div>
      </q-card-section>
    </q-card>
  </q-dialog>
</template>

<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref, watch } from 'vue';
import PclButton from './PclButton.vue';
import PclIcon from './PclIcon.vue';
import { subscribeTaskSnapshots } from '../composables/useTaskEvents';

const props = defineProps<{
  modelValue: boolean;
  execute: (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
}>();
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

const activeDownloads = computed(() => tasks.value.flatMap((task: any) => (Array.isArray(task.children) ? task.children : [])
  .filter((row: any) => row && row.state === 'running' && (Number(row.total) > 0 || row.details || row.label))
  .map((row: any) => {
    const total = Number(row.total || 0), current = Number(row.current || 0);
    const value = total > 0 ? Math.max(0, Math.min(100, Math.round(current / total * 100))) : 0;
    return { id: row.id || `${task.taskId}:${row.label}`, label: row.label || row.details || '子任务', value, percent: total > 0 ? `${value}%` : '进行中', rate: Number(row.transferRate || 0) };
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
  const total = Number(task?.totalProgress || task?.total || 0);
  const progress = Number(task?.progress || 0);
  if (total > 0) return Math.max(0, Math.min(100, Math.round(progress / total * 100)));
  return task?.finished ? 100 : 0;
}
function formatSize(value: unknown) {
  const bytes = Number(value || 0);
  if (!bytes) return '0 B';
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KiB`;
  if (bytes < 1024 * 1024 * 1024) return `${(bytes / 1024 / 1024).toFixed(1)} MiB`;
  return `${(bytes / 1024 / 1024 / 1024).toFixed(2)} GiB`;
}
function formatRate(value: unknown) {
  const bytes = Number(value || 0);
  return bytes ? `${formatSize(bytes)}/s` : '';
}
function operationLabel(operation: string) {
  const labels: Record<string, string> = {
    'modpack.install': '整合包安装',
    'instance.import': '实例导入',
    'instance.create': '创建实例',
    'instance.update': '实例更新',
    'instance.special-loader.install': '特殊加载器安装',
    'resource.install': '资源安装',
    'resource.install-version': '资源安装',
    'resource.install-with-dependencies': '资源及依赖安装',
    'instance.component.set-version': '组件安装',
    'java.install': 'Java 安装',
    'account.login': '账户登录',
    'account.refresh': '账户刷新',
    'instance.launch': '启动游戏',
    'instance.export': '导出实例',
    'server.start': '启动服务端',
  };
  return labels[operation] || operation;
}
function stateLabel(state: string) {
  return (({ queued: '排队中', running: '进行中', succeeded: '已完成', failed: '失败', cancelled: '已取消', canceled: '已取消', aborted: '已中止', completed: '已完成', interrupted: '已中断' } as Record<string, string>)[state] || state || '未知');
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
