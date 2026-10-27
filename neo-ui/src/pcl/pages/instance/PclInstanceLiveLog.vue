<template>
  <div class="pcl-live-log">
    <PclCard :title="t('liveLog.title')">
      <template #actions>
        <PclCheckbox v-model="neoUiPreferences.pclLaunchLogAutoScroll" :label="t('liveLog.autoScroll')" />
        <LogUploadButton :instance="instance.id" :name="t('liveLog.upload')" :content="text" :execute="execute" :disabled="!text" pcl />
        <PclButton size="sm" icon="trash-2" variant="ghost" :disabled="!lines.length" @click="lines = []">{{ t('liveLog.clear') }}</PclButton>
        <PclButton v-if="running" size="sm" variant="red" icon="power" @click="forceStop">{{ t('liveLog.forceStop') }}</PclButton>
        <PclButton v-else-if="launching" size="sm" variant="red" icon="x" @click="cancelLaunch">{{ t('liveLog.cancelLaunch') }}</PclButton>
        <PclButton v-else size="sm" variant="highlight" icon="play" @click="launch">{{ t('liveLog.launch') }}</PclButton>
      </template>

      <div class="pcl-live-log__status">
        <span class="pcl-live-log__dot" :class="{ connected, running, busy: launching }" />
        <strong>{{ stateText }}</strong>
        <span v-if="stage">{{ stage }}</span>
        <span v-if="progress > 0" class="pcl-live-log__progress-text">{{ Math.round(progress * 100) }}%</span>
      </div>
      <div v-if="launching" class="pcl-progress-track pcl-live-log__progress">
        <span :style="{ width: `${Math.round(progress * 100)}%` }" />
      </div>

      <pre ref="logElement" class="pcl-live-log__console" aria-live="polite">{{ text || t('liveLog.empty') }}</pre>
    </PclCard>

    <PclLaunchProgressDialog
      v-model="launchDialogOpen"
      :state="launchDialogState"
      :progress="progress"
      :stage="stage"
      :details="launchDetails"
      :children="launchChildren"
      :error="launchError"
      :instance-name="instance?.name || instance?.id || ''"
      :cancelling="cancelling"
      @cancel="cancelLaunch"
      @retry="launch"
    />
  </div>
</template>

<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, onMounted, ref } from 'vue';
import { useI18n } from 'vue-i18n';
import PclButton from '../../components/PclButton.vue';
import PclLaunchProgressDialog from '../../components/PclLaunchProgressDialog.vue';
import PclCheckbox from '../../components/PclCheckbox.vue';
import PclCard from '../../components/PclCard.vue';
import LogUploadButton from '../../../components/LogUploadButton.vue';
import { launcher } from '../../../boot/launcher-api';
import { waitForTask } from '../../composables/useTaskEvents';
import { neoUiPreferences } from '../../../neo-ui-preferences';
import { flattenTaskItems, taskActiveItems, taskProgressStats } from '../../composables/useTaskProgress';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
const props = defineProps<{ instance: any; execute: Execute }>();
const { t } = useI18n();

const lines = ref<string[]>([]);
const state = ref(t('liveLog.connecting'));
const connected = ref(false);
const launching = ref(false);
const launchDialogOpen = ref(false);
const launchError = ref('');
const launchDetails = ref('');
const launchChildren = ref<any[]>([]);
const launchCancelled = ref(false);
const cancelling = ref(false);
const running = ref(Boolean(props.instance?.running));
let currentTaskId = '';
const progress = ref(0);
const stage = ref('');
const logElement = ref<HTMLElement | null>(null);
let subscriptionId = '';
let unlistenStream: (() => void) | undefined;

const text = computed(() => lines.value.join(''));
const stateText = computed(() => running.value ? t('liveLog.running') : state.value);
const launchDialogState = computed(() => {
  if (launching.value) return 'launching';
  if (launchError.value) return 'failed';
  if (launchCancelled.value) return 'cancelled';
  if (running.value) return 'launched';
  return 'launching';
});

async function append(value: string) {
  lines.value.push(value);
  if (lines.value.length > 3000) lines.value.splice(0, lines.value.length - 3000);
  await nextTick();
  if (neoUiPreferences.pclLaunchLogAutoScroll && logElement.value) logElement.value.scrollTop = logElement.value.scrollHeight;
}

async function connect() {
  try {
    const result = await props.execute('instance.console.subscribe', { instance: props.instance.id });
    subscriptionId = String(result?.subscriptionId || '');
    connected.value = Boolean(subscriptionId);
    state.value = connected.value ? t('liveLog.connected') : t('liveLog.unavailable');
    unlistenStream = await launcher.onStream((batch: any) => {
      if (!batch || batch.subscriptionId !== subscriptionId) return;
      if (batch.dropped) void append(`${t('liveLog.dropped', { count: batch.dropped })}\n`);
      for (const event of batch.events || []) {
        if (event.kind === 'console.line') void append(String(event.text || '') + '\n');
        else if (event.kind === 'console.data') void append(new TextDecoder().decode(Uint8Array.from(atob(event.data), c => c.charCodeAt(0))));
        else if (event.kind === 'instance.state') {
          state.value = event.running ? t('liveLog.running') : t('instance.stopped');
          running.value = Boolean(event.running);
          if (!event.running) launching.value = false;
        } else if (event.kind === 'console.reset') {
          state.value = event.available ? t('liveLog.connected') : t('liveLog.waiting');
        }
      }
    });
  } catch (exception) {
    state.value = exception instanceof Error ? exception.message : String(exception);
  }
}

async function launch() {
  if (launching.value) return;
  launching.value = true;
  launchDialogOpen.value = true;
  launchError.value = '';
  launchCancelled.value = false;
  launchDetails.value = '';
  launchChildren.value = [];
  progress.value = 0;
  stage.value = t('liveLog.preparing');
  try {
    let task = await props.execute('neo.task.start', {
      operation: 'instance.launch',
      priority: 'high',
      parameters: { instance: props.instance.id },
    });
    currentTaskId = String(task?.taskId || '');
    const finished = await waitForTask(currentTaskId, snapshot => {
      const rows = flattenTaskItems(snapshot);
      const stats = taskProgressStats(snapshot, rows);
      const activeStep = taskActiveItems(rows, 1)[0];
      stage.value = activeStep?.label || activeStep?.status || snapshot?.details || snapshot?.status || stage.value;
      launchDetails.value = [snapshot?.details, stats.itemCount ? stats.summary : '', stats.byteSummary].filter(Boolean).join(' · ');
      launchChildren.value = rows;
      if (stats.percent !== null) progress.value = Math.max(0, Math.min(1, stats.percent / 100));
    });
    launching.value = false;
    if (finished?.state === 'succeeded') {
      progress.value = 1;
      stage.value = t('liveLog.started');
      running.value = true;
    } else if (finished?.state === 'cancelled') {
      launchCancelled.value = true;
      stage.value = t('liveLog.cancelled');
    } else {
      launchError.value = finished?.error || t('liveLog.failed');
      state.value = launchError.value;
    }
  } catch (exception) {
    launching.value = false;
    launchError.value = exception instanceof Error ? exception.message : String(exception);
    state.value = launchError.value;
  }
}

async function cancelLaunch() {
  cancelling.value = true;
  launching.value = false;
  launchCancelled.value = true;
  if (currentTaskId) {
    try { await props.execute('neo.task.cancel', { taskId: currentTaskId }); } catch { /* Task may already be terminal. */ }
  }
  stage.value = t('liveLog.cancelledDone');
  cancelling.value = false;
}

async function forceStop() {
  if (!window.confirm(t('liveLog.forceStopConfirm'))) return;
  try {
    await props.execute('instance.kill', { instance: props.instance.id });
    running.value = false;
    launching.value = false;
    state.value = t('liveLog.forceStopped');
  } catch (exception) {
    state.value = exception instanceof Error ? exception.message : String(exception);
  }
}

onMounted(() => { void connect(); });
onBeforeUnmount(async () => {
  unlistenStream?.();
  if (subscriptionId) {
    try { await props.execute('event.unsubscribe', { subscriptionId }); } catch { /* Already gone. */ }
  }
});
</script>

<style scoped>
.pcl-live-log__status { display: flex; align-items: center; gap: 8px; color: var(--pcl-text); font-size: calc(11px * var(--pcl-font-scale, 1)); }
.pcl-live-log__status > span:not(.pcl-live-log__dot):not(.pcl-live-log__progress-text) { color: var(--pcl-muted); font-size: calc(9px * var(--pcl-font-scale, 1)); }
.pcl-live-log__dot { width: 7px; height: 7px; border-radius: 50%; background: var(--pcl-c5); flex: 0 0 7px; }
.pcl-live-log__dot.connected { background: #34d399; }
.pcl-live-log__dot.running { background: var(--pcl-c3); box-shadow: 0 0 7px var(--pcl-c3); }
.pcl-live-log__dot.busy { background: var(--pcl-c2); animation: pcl-live-pulse 1s infinite; }
.pcl-live-log__progress-text { margin-left: auto; color: var(--pcl-c3); font-size: calc(11px * var(--pcl-font-scale, 1)); }
.pcl-live-log__progress { margin: 9px 0; }
.pcl-live-log__console { height: calc(100vh - 265px); min-height: 180px; margin: 8px 0 0; padding: 10px 12px; overflow: auto; border: 1px solid var(--pcl-border); border-radius: 4px; background: var(--pcl-c8); color: var(--pcl-text); font: 11px/1.55 ui-monospace, Consolas, monospace; white-space: pre-wrap; overflow-wrap: anywhere; }
@keyframes pcl-live-pulse { 50% { opacity: .35; } }
</style>
