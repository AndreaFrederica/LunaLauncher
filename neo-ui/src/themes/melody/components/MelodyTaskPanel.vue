<template>
  <aside v-if="tasks.length || error" class="melody-task-panel" aria-label="任务进度">
    <p v-if="error" class="melody-error" role="alert">{{ error }}</p>
    <div v-for="task in tasks" :key="task.taskId" class="melody-task-row">
      <span>{{ task.message || task.stage || task.operation }}{{ task.error ? '：' + task.error : '' }}</span>
      <progress v-if="!task.finished" :value="progress(task)" max="100" />
      <span v-else>{{ task.state === 'succeeded' ? '已完成' : task.state === 'cancelled' ? '已取消' : '失败' }}</span>
      <MelodyButton v-if="!task.finished" @click="cancel(task.taskId)">取消</MelodyButton>
    </div>
  </aside>
</template>
<script setup lang="ts">
import { ref } from 'vue';
import { isTauri } from '@tauri-apps/api/core';
import MelodyButton from './MelodyButton.vue';
import { melodyExecute } from '../services/backend';
import { recordRuntimeDebug } from '../../../common/runtime/runtime-debug';
import { useThemeResource } from '../../../common/runtime/theme-runtime';
const tasks = ref<any[]>([]), error = ref('');
let stopped = false, timer: ReturnType<typeof setTimeout> | undefined;
function progress(task: any) { const total = Number(task.total ?? task.totalProgress); const current = Number(task.current ?? task.progress); return total > 0 && Number.isFinite(current) ? Math.min(100, Math.max(0, current / total * 100)) : undefined; }
async function refresh() {
  try { const result = await melodyExecute('neo.task.list'); if (!stopped) tasks.value = (result.tasks || []).filter((task: any) => !task.finished).slice(-4); }
  catch (e) { if (!stopped) error.value = String(e); }
  if (!stopped) timer = setTimeout(refresh, 800);
}
async function cancel(taskId: string) { try { await melodyExecute('neo.task.cancel', { taskId }); error.value = ''; } catch (e) { error.value = String(e); } }
function stopPolling() {
  stopped = true;
  clearTimeout(timer);
  timer = undefined;
  recordRuntimeDebug('timer', 'theme:melody.task-panel', 'task polling stopped');
}
useThemeResource('melody', () => {
  stopped = false;
  if (isTauri()) {
    recordRuntimeDebug('timer', 'theme:melody.task-panel', 'task polling started', { intervalMs: 800 });
    void refresh();
  }
  return stopPolling;
});
</script>
