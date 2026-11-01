<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <div class="ef ef-root ef-standalone">
    <EfCommandBar :page="''" :section="{ label: '资源下载', code: 'RESOURCE' }" :connected="true" @tasks="() => {}" @search="() => {}" />
    <div class="ef-standalone__body">
      <header class="ef-standalone__head">
        <p class="ef-eyebrow">// RESOURCE</p>
        <h1>{{ instanceName || '资源下载' }}</h1>
        <p>选择项目与版本，安装到当前实例。</p>
      </header>
      <EndfieldDownloadPage
        :busy="busy"
        :execute="execute"
        :instances="instances"
        :selected="selected"
        :initial-mode="'resource'"
        :initial-kind="kind"
        compact
        @installed="refresh"
        @toast="showToast"
      />
    </div>
    <EfToastHost ref="toastHost" />
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, ref } from 'vue';
import { launcher } from '../../../boot/launcher-api';
import { useLauncherSessionStore } from '../../../common/launcher-session';
import EfCommandBar from '../components/layout/EfCommandBar.vue';
import EfToastHost from '../components/EfToastHost.vue';
import EndfieldDownloadPage from './EndfieldDownloadPage.vue';

const props = defineProps<{ instanceId: string; instanceName?: string; kind?: string }>();
const session = useLauncherSessionStore();
const instances = computed(() => session.instances);
const selected = computed(() => instances.value.find((item: any) => String(item.id) === String(props.instanceId)) || null);
const kind = computed(() => props.kind || 'mods');
const busy = ref(false);
const toastHost = ref<any>(null);

function showToast(type: 'success' | 'error' | 'info', title: string, message = '') {
  const host = toastHost.value;
  if (!host?.toasts) return;
  const id = `${Date.now()}-${Math.random().toString(16).slice(2)}`;
  host.toasts.push({ id, type, title, message });
  setTimeout(() => host.dismiss?.(id), 4200);
}

async function execute(op: string, parameters: Record<string, unknown> = {}) {
  busy.value = true;
  try {
    const result = await launcher.execute<any>(op, parameters);
    if (!result.ok) throw new Error(result.error || '操作失败');
    return result.data;
  } catch (error: any) {
    showToast('error', '操作失败', error?.message || String(error));
    throw error;
  } finally {
    busy.value = false;
  }
}
async function refresh() {
  await session.refresh();
  await session.refreshIcons?.();
}
onMounted(() => { void refresh(); });
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
.ef-standalone__head {
  margin-bottom: 14px;
  padding-bottom: 10px;
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
.ef-standalone__head h1 {
  margin: 0;
  font-family: var(--ef-font-ui);
  font-size: 22px;
  font-weight: 800;
}
.ef-standalone__head p:last-child {
  margin: 4px 0 0;
  color: var(--ef-ink-2);
  font-size: 11px;
}
</style>
