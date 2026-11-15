<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <div class="ef ef-root ef-standalone">
    <EfCommandBar :page="''" :section="{ label: '实例编辑器', code: 'INSTANCE' }" :connected="true" @tasks="taskCenterOpen = true" @search="() => {}" />
    <div class="ef-standalone__body">
      <EndfieldInstancePage
        v-if="instance"
        :instance="instance"
        :busy="busy"
        :execute="execute"
        :console-lines="[]"
        :icon="icons[instance.id]"
        @launch="launch"
        @folder="openFolder"
        @changed="refresh"
        @toast="showToast"
      />
      <EfEmpty v-else title="实例不存在" description="这个实例可能已经被删除。" icon="inventory_2" />
    </div>
    <EfTaskCenter v-model="taskCenterOpen" :execute="execute" />
    <EfToastHost ref="toastHost" />
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, ref } from 'vue';
import { storeToRefs } from 'pinia';
import { launcher } from '../../../boot/launcher-api';
import { useLauncherSessionStore } from '../../../common/launcher-session';
import EfCommandBar from '../components/layout/EfCommandBar.vue';
import EfEmpty from '../components/feedback/EfEmpty.vue';
import EfTaskCenter from '../components/overlays/EfTaskCenter.vue';
import EfToastHost from '../components/EfToastHost.vue';
import EndfieldInstancePage from './EndfieldInstancePage.vue';

const props = defineProps<{ instanceId: string }>();
const session = useLauncherSessionStore();
const { icons } = storeToRefs(session);
const instances = computed(() => session.instances);
const instance = computed(() => instances.value.find((item: any) => String(item.id) === String(props.instanceId)) || null);
const busy = ref(false);
const taskCenterOpen = ref(false);
const toastHost = ref<any>(null);

function showToast(type: 'success' | 'error' | 'info', title: string, message = '') {
  const host = toastHost.value;
  if (!host?.toasts) return;
  const id = `${Date.now()}-${Math.random().toString(16).slice(2)}`;
  host.toasts.push({ id, type, title, message });
  setTimeout(() => host.dismiss?.(id), 4200);
}

async function call(op: string, parameters: Record<string, unknown> = {}) {
  const result = await launcher.execute<any>(op, parameters);
  if (!result.ok) throw new Error(result.error || '操作失败');
  return result.data;
}
async function execute(op: string, parameters: Record<string, unknown> = {}) {
  busy.value = true;
  try { return await call(op, parameters); }
  catch (error: any) { showToast('error', '操作失败', error?.message || String(error)); throw error; }
  finally { busy.value = false; }
}
async function refresh() {
  await session.refresh();
  await session.refreshIcons?.();
}
async function launch() {
  if (!instance.value) return;
  const account = session.selectedAccount || session.accounts.find((item: any) => item.default) || session.accounts[0];
  if (!account) { showToast('error', '没有可用账户'); return; }
  try {
    await call('neo.task.start', {
      operation: 'instance.launch',
      priority: 'high',
      parameters: { instance: instance.value.id, account: String(account.internalId || account.id || account.profileName) },
    });
    showToast('success', '已启动', instance.value.name);
  } catch (error: any) {
    showToast('error', '启动失败', error?.message || String(error));
  }
}
async function openFolder() {
  if (!instance.value) return;
  try { await execute('instance.open-folder', { instance: instance.value.id }); } catch { /* toast */ }
}

onMounted(refresh);
</script>

<style scoped>
.ef-standalone {
  display: flex;
  flex-direction: column;
  width: 100%;
  height: 100vh;
  overflow: hidden;
  background: var(--ef-paper);
  color: var(--ef-ink);
  font-family: var(--ef-font-ui);
}
.ef-standalone__body {
  flex: 1;
  overflow: auto;
  padding: 16px 18px 26px;
}
</style>
