<template>
  <CrashDiagnosticDialog v-if="queue.length" :key="queue[0]!.launchId" :failure="queue[0]!" :pcl="pcl" :execute="execute" @close="queue.shift()" />
</template>
<script setup lang="ts">
import { computed, onMounted, onBeforeUnmount, ref } from 'vue';
import { isTauri } from '@tauri-apps/api/core';
import { getCurrentWindow } from '@tauri-apps/api/window';
import { launcher } from '../boot/launcher-api';
import { neoUiPreferences } from '../neo-ui-preferences';
import { LaunchFailureQueue, type LaunchCompletion } from '../common/launch-diagnostics';
import CrashDiagnosticDialog from './CrashDiagnosticDialog.vue';
const queue = ref<LaunchCompletion[]>([]);
const failures = new LaunchFailureQueue();
const pcl = computed(() => neoUiPreferences.shell === 'pcl' || neoUiPreferences.shell === 'melody');
let stopped = false, subscription = '', subscribing = false;
let unlisten: (() => void) | undefined;
let unlistenExit: (() => void) | undefined;
let retry: ReturnType<typeof setTimeout> | undefined;
async function execute(operation: string, parameters: Record<string, unknown> = {}): Promise<any> {
  const response = await launcher.execute(operation, parameters);
  if (!response.ok) throw new Error(response.error || operation);
  return response.data;
}
function receive(batch: any) {
  if (stopped || !subscription || batch.subscriptionId !== subscription) return;
  for (const event of batch.events || []) {
    if (event.kind !== 'instance.finished') continue;
    const failure = event.data as LaunchCompletion;
    if (!failures.accept(failure)) continue;
    queue.value.push(failure);
    // The launcher may have been hidden/minimized while the game ran.
    void getCurrentWindow().show().then(() => getCurrentWindow().unminimize()).catch(() => {});
  }
}
async function subscribe() {
  if (stopped || subscribing) return;
  clearTimeout(retry);
  subscribing = true;
  try {
    const result = await execute('instance.lifecycle.subscribe');
    subscription = result.subscriptionId;
    if (stopped) { await execute('event.unsubscribe', { subscriptionId: subscription }); return; }
    // Recover completions pushed between the subscription response and its
    // assignment above. Launch IDs deduplicate overlap with pushed batches.
    receive(await execute('event.poll', { subscriptionId: subscription }));
  } catch (exception) {
    console.warn('Crash diagnostics subscription unavailable', exception);
    if (subscription) {
      const previous = subscription;
      subscription = '';
      void execute('event.unsubscribe', { subscriptionId: previous }).catch(() => {});
    }
    if (!stopped) retry = setTimeout(subscribe, 5000);
  } finally { subscribing = false; }
}
onMounted(async () => {
  if (!isTauri()) return;
  try {
    unlisten = await launcher.onStream(receive);
    unlistenExit = await launcher.onExit(() => {
      subscription = '';
      clearTimeout(retry);
      if (!stopped) retry = setTimeout(subscribe, 2000);
    });
    if (stopped) { unlisten(); unlistenExit(); return; }
    await subscribe();
  } catch (exception) { console.warn('Crash diagnostics subscription unavailable', exception); }
});
onBeforeUnmount(() => {
  stopped = true; clearTimeout(retry); unlisten?.(); unlistenExit?.();
  if (subscription) void execute('event.unsubscribe', { subscriptionId: subscription }).catch(() => {});
});
</script>
