<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <div v-if="modelValue" class="ef-overlay" @click.self="emit('update:modelValue', false)">
    <section class="ef-task-center" role="dialog" aria-label="任务">
      <header>
        <div>
          <p class="ef-eyebrow">// TASKS</p>
          <h2>任务</h2>
        </div>
        <EfIconButton icon="close" label="关闭" @click="emit('update:modelValue', false)" />
      </header>
      <div class="ef-task-center__body">
        <div v-if="!tasks.length" class="ef-task-center__empty">
          <span class="material-icons">task_alt</span>
          <p>当前没有任务。</p>
        </div>
        <article v-for="task in tasks" :key="task.taskId || task.id" class="ef-task">
          <div class="ef-task__head">
            <strong>{{ task.title || task.operation || task.name || '任务' }}</strong>
            <span class="ef-task__state" :class="stateOf(task)">{{ stateLabel(task) }}</span>
          </div>
          <p v-if="task.message" class="ef-task__message">{{ task.message }}</p>
          <div v-if="progressOf(task) !== null" class="ef-task__bar">
            <i :style="{ width: `${progressOf(task)}%` }" />
          </div>
          <div class="ef-task__meta">
            <EfField label="进度" :value="progressOf(task) === null ? '—' : `${progressOf(task)}%`" />
            <EfField v-if="task.current !== undefined" label="项目" :value="`${task.current}${task.total !== undefined ? ` / ${task.total}` : ''}`" />
            <button v-if="!finished(task)" type="button" class="ef-task__cancel" @click="cancel(task)">取消</button>
          </div>
        </article>
      </div>
    </section>
  </div>
</template>

<script setup lang="ts">
import { onBeforeUnmount, ref, watch } from 'vue';
import EfIconButton from '../controls/EfIconButton.vue';
import EfField from '../data/EfField.vue';

const props = defineProps<{ modelValue: boolean; execute: (op: string, p?: Record<string, unknown>) => Promise<any> }>();
const emit = defineEmits<{ 'update:modelValue': [value: boolean] }>();

const tasks = ref<any[]>([]);
let timer: ReturnType<typeof setInterval> | undefined;

function finished(task: any) {
  return Boolean(task?.finished) || ['succeeded', 'failed', 'cancelled', 'canceled', 'aborted'].includes(String(task?.state || task?.status || '').toLowerCase());
}
function progressOf(task: any): number | null {
  const total = Number(task?.total ?? task?.totalProgress);
  const current = Number(task?.current ?? task?.progress);
  if (Number.isFinite(total) && total > 0 && Number.isFinite(current)) return Math.max(0, Math.min(100, Math.round(current / total * 100)));
  const percent = Number(task?.percent ?? task?.percentage);
  return Number.isFinite(percent) ? Math.max(0, Math.min(100, Math.round(percent))) : null;
}
function stateOf(task: any) {
  return String(task?.state || task?.status || (finished(task) ? 'succeeded' : 'running')).toLowerCase();
}
function stateLabel(task: any) {
  const state = stateOf(task);
  if (state === 'succeeded' || state === 'done' || state === 'finished') return '完成';
  if (state === 'failed' || state === 'error') return '失败';
  if (state === 'cancelled' || state === 'canceled' || state === 'aborted') return '已取消';
  return task?.message && finished(task) ? '完成' : '进行中';
}
async function load() {
  try {
    const list = await props.execute('neo.task.list');
    tasks.value = Array.isArray(list) ? list : (Array.isArray(list?.tasks) ? list.tasks : []);
  } catch { /* task center is optional */ }
}
async function cancel(task: any) {
  const id = task?.taskId || task?.id;
  if (!id) return;
  try { await props.execute('neo.task.cancel', { taskId: id }); } catch { /* ignore */ }
  await load();
}
watch(() => props.modelValue, value => {
  if (value) {
    void load();
    timer = setInterval(load, 1200);
  } else if (timer) {
    clearInterval(timer);
    timer = undefined;
  }
}, { immediate: true });
onBeforeUnmount(() => { if (timer) clearInterval(timer); });
</script>

<style scoped>
.ef-overlay {
  position: fixed;
  inset: 0;
  z-index: 9000;
  display: flex;
  justify-content: flex-end;
  background: rgba(0, 0, 0, .35);
}
.ef-task-center {
  display: flex;
  flex-direction: column;
  width: min(460px, 92vw);
  height: 100%;
  background: var(--ef-paper);
  border-left: 1px solid var(--ef-ink);
  animation: ef-task-in .18s var(--ef-mech) both;
}
@keyframes ef-task-in { from { transform: translateX(24px); opacity: 0; } to { transform: translateX(0); opacity: 1; } }
.ef-task-center header {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 12px;
  padding: 16px;
  border-bottom: 1px solid var(--ef-rule);
}
.ef-eyebrow {
  margin: 0 0 3px;
  color: var(--ef-ink-2);
  font-family: var(--ef-font-ui);
  font-size: 9px;
  letter-spacing: .16em;
  text-transform: uppercase;
}
.ef-task-center h2 {
  margin: 0;
  font-family: var(--ef-font-ui);
  font-size: 20px;
  font-weight: 800;
}
.ef-task-center__body {
  flex: 1;
  overflow: auto;
  padding: 12px 16px 24px;
}
.ef-task-center__empty {
  display: grid;
  place-items: center;
  gap: 6px;
  padding: 32px 12px;
  border: 1px dashed var(--ef-rule);
  background: var(--ef-hatch);
  color: var(--ef-ink-2);
}
.ef-task-center__empty .material-icons { font-size: 22px; }
.ef-task {
  padding: 10px 0 12px;
  border-bottom: 1px solid var(--ef-rule);
}
.ef-task__head {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  gap: 10px;
}
.ef-task__head strong {
  font-family: var(--ef-font-ui);
  font-size: 12px;
  font-weight: 700;
}
.ef-task__state {
  flex: 0 0 auto;
  padding: 1px 6px;
  border: 1px solid var(--ef-ink);
  color: var(--ef-ink);
  font-family: var(--ef-font-ui);
  font-size: 9px;
  letter-spacing: .08em;
  text-transform: uppercase;
}
.ef-task__state.succeeded, .ef-task__state.done, .ef-task__state.finished {
  border-color: transparent;
  background: var(--ef-signal);
  color: var(--ef-on-signal);
}
.ef-task__message {
  margin: 4px 0 6px;
  color: var(--ef-ink-2);
  font-size: 11px;
  line-height: 1.4;
}
.ef-task__bar {
  height: 3px;
  margin: 7px 0;
  background: var(--ef-chip);
}
.ef-task__bar i {
  display: block;
  height: 100%;
  background: var(--ef-signal);
  transition: width .2s var(--ef-mech);
}
.ef-task__meta {
  display: flex;
  align-items: center;
  gap: 10px;
}
.ef-task__cancel {
  margin-left: auto;
  padding: 1px 7px;
  border: 1px solid var(--ef-rule);
  background: transparent;
  color: var(--ef-ink-2);
  font-size: 10px;
  cursor: pointer;
}
.ef-task__cancel:hover { border-color: var(--ef-danger); color: var(--ef-danger); }
</style>
