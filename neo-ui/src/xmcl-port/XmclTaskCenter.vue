<template>
  <q-dialog v-model="shown" persistent :maximized="false" transition-show="slide-up" transition-hide="slide-down">
    <q-card class="task-card text-white">
      <q-card-section class="task-header"><div><div class="eyebrow">TASK CENTER</div><div class="text-h6">{{ task?.name || title || '正在执行' }}</div><div class="text-caption text-grey-5">{{ task?.status || status || '正在准备…' }}</div></div><q-btn v-if="task?.canAbort || busy" flat round icon="close" color="orange" @click="emit('cancel')" /></q-card-section>
      <q-linear-progress :value="progress" color="primary" track-color="grey-8" rounded class="q-mx-lg" />
      <q-card-section class="task-body"><div class="task-progress-row"><strong>{{ progressLabel }}</strong><span v-if="task?.transferRate">{{ formatRate(task.transferRate) }}</span></div><div v-if="task?.details" class="task-details">{{ task.details }}</div><q-list v-if="steps.length" dense class="task-steps"><q-item v-for="step in steps" :key="step.id"><q-item-section avatar><q-icon :name="step.finished ? 'check_circle' : step.state === 'running' ? 'sync' : 'radio_button_unchecked'" :color="step.finished ? 'positive' : step.state === 'running' ? 'primary' : 'grey'" /></q-item-section><q-item-section>{{ step.status || step.id }}</q-item-section><q-item-section side>{{ step.current != null && step.total ? `${step.current} / ${step.total}` : '' }}</q-item-section></q-item></q-list></q-card-section>
      <q-card-actions align="right"><q-btn flat label="后台运行" :disable="!canBackground" @click="shown=false" /><q-btn v-if="task?.finished" color="primary" label="完成" @click="shown=false" /><q-btn v-else flat color="orange" label="取消任务" @click="emit('cancel')" /></q-card-actions>
    </q-card>
  </q-dialog>
</template>
<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref, watch } from 'vue';
const props = defineProps<{ busy: boolean; execute: (op: string, p?: Record<string, unknown>) => Promise<any>; title?: string; status?: string; snapshot?: any }>();
const emit = defineEmits<{ cancel: []; updated: [task: any] }>();
const shown = ref(false); const polled = ref<any>(null); let timer: ReturnType<typeof setInterval> | undefined;
const task = computed(() => props.snapshot || polled.value); const steps = computed(() => Array.isArray(task.value?.steps) ? task.value.steps : []); const progress = computed(() => { const t = Number(task.value?.totalProgress || task.value?.total || 0); const p = Number(task.value?.progress || task.value?.current || 0); return t > 0 ? Math.max(0, Math.min(1, p / t)) : task.value?.finished ? 1 : 0; }); const progressLabel = computed(() => progress.value ? `${Math.round(progress.value * 100)}%` : (task.value?.finished ? '已完成' : '进行中')); const canBackground = computed(() => !props.busy || Boolean(task.value?.finished));
function formatRate(value: any) { const n = Number(value); return Number.isFinite(n) && n > 0 ? `${(n / 1024 / 1024).toFixed(1)} MB/s` : ''; }
async function poll() { if (!props.busy && !shown.value) return; try { const value = await props.execute('task.status'); if (value) { polled.value = value; emit('updated', value); shown.value = !value.finished || props.busy; } } catch { /* There may be no active task between operations. */ } }
watch(() => props.busy, (busy) => { if (busy) { shown.value = true; void poll(); } else if (!task.value || task.value.finished) shown.value = Boolean(task.value?.finished); });
watch(() => props.snapshot, (value) => { if (value) { shown.value = !value.finished || props.busy; polled.value = value; } });
onMounted(() => { timer = setInterval(() => void poll(), 450); }); onUnmounted(() => { if (timer) clearInterval(timer); });
</script>
<style scoped>
.task-card{width:620px;max-width:94vw;background:rgba(29,29,38,.96)!important;border:1px solid rgba(255,255,255,.12);border-radius:20px}.task-header{display:flex;align-items:flex-start;justify-content:space-between}.eyebrow{font-size:10px;letter-spacing:.16em;color:#a5b4fc;font-weight:700}.task-body{min-height:100px}.task-progress-row{display:flex;justify-content:space-between;margin:12px 0;color:rgba(255,255,255,.75)}.task-details{margin-bottom:10px;color:rgba(255,255,255,.55);font-size:12px}.task-steps{max-height:220px;overflow:auto;background:rgba(255,255,255,.035);border-radius:10px}.task-card :deep(.q-btn){text-transform:none}
</style>
