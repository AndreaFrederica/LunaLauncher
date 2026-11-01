<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <header class="ef-cmdbar" data-tauri-drag-region @dblclick="toggleMaximize">
    <div class="ef-cmdbar__brand" data-tauri-drag-region>
      <span class="ef-cmdbar__seal" aria-hidden="true">E</span>
      <span class="ef-cmdbar__wordmark" data-tauri-drag-region>LunaLauncher</span>
      <small data-tauri-drag-region>// NEO</small>
    </div>
    <div v-if="section" class="ef-cmdbar__section" data-tauri-drag-region>
      <small>// {{ section.code }}</small>
      <span>{{ section.label }}</span>
    </div>
    <div class="ef-cmdbar__util">
      <span class="ef-cmdbar__status" :class="{ connected }">
        <i aria-hidden="true" />
        {{ connected ? '已连接' : '未连接' }}
      </span>
      <button type="button" class="ef-cmdbar__icon" aria-label="任务" title="任务" @click="emit('tasks')">
        <span class="material-icons">pending_actions</span>
      </button>
      <button type="button" class="ef-cmdbar__icon" aria-label="搜索" title="搜索" @click="emit('search')">
        <span class="material-icons">search</span>
      </button>
      <div class="ef-cmdbar__window">
        <button type="button" aria-label="最小化" title="最小化" @click="minimize">
          <span class="material-icons">remove</span>
        </button>
        <button type="button" :aria-label="maximized ? '还原' : '最大化'" :title="maximized ? '还原' : '最大化'" @click="toggleMaximize">
          <span class="material-icons">{{ maximized ? 'filter_none' : 'crop_square' }}</span>
        </button>
        <button type="button" class="close" aria-label="关闭" title="关闭" @click="closeWindow">
          <span class="material-icons">close</span>
        </button>
      </div>
    </div>
  </header>
</template>

<script setup lang="ts">
import { onMounted, onUnmounted, ref } from 'vue';
import { getCurrentWindow } from '@tauri-apps/api/window';

defineProps<{
  page: string;
  section?: { label: string; code: string };
  connected?: boolean;
}>();
const emit = defineEmits<{ tasks: []; search: [] }>();

const maximized = ref(false);
let unlisten: (() => void) | undefined;

async function minimize() {
  try { await getCurrentWindow().minimize(); } catch { /* browser preview */ }
}
async function toggleMaximize() {
  try {
    const win = getCurrentWindow();
    if (await win.isMaximized()) await win.unmaximize();
    else await win.maximize();
    maximized.value = await win.isMaximized();
  } catch { /* browser preview */ }
}
async function closeWindow() {
  try { await getCurrentWindow().close(); } catch { /* browser preview */ }
}
async function syncMaximized() {
  try { maximized.value = await getCurrentWindow().isMaximized(); } catch { /* browser preview */ }
}
onMounted(async () => {
  await syncMaximized();
  try { unlisten = await getCurrentWindow().onResized(syncMaximized); } catch { /* browser preview */ }
});
onUnmounted(() => unlisten?.());
</script>

<style scoped>
.ef-cmdbar {
  display: flex;
  align-items: stretch;
  flex: 0 0 46px;
  height: 46px;
  background: #161616;
  color: #ededed;
  user-select: none;
  border-bottom: 1px solid #000;
}
.ef-cmdbar__brand {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 0 14px;
  min-width: 0;
}
.ef-cmdbar__seal {
  width: 22px;
  height: 22px;
  display: grid;
  place-items: center;
  background: var(--ef-signal, #fffa00);
  color: #161616;
  font-family: var(--ef-font-ui, sans-serif);
  font-size: 14px;
  font-weight: 800;
  font-stretch: 125%;
}
.ef-cmdbar__wordmark {
  font-family: var(--ef-font-ui, sans-serif);
  font-size: 12px;
  font-weight: 700;
  font-stretch: 125%;
  letter-spacing: .14em;
  text-transform: uppercase;
  white-space: nowrap;
}
.ef-cmdbar__brand small {
  color: #7c7c7c;
  font-family: var(--ef-font-ui, sans-serif);
  font-size: 9px;
  letter-spacing: .14em;
  white-space: nowrap;
}
.ef-cmdbar__section {
  display: flex;
  align-items: baseline;
  gap: 7px;
  min-width: 0;
  padding: 0 14px;
  border-left: 1px solid #2c2c2c;
  align-self: center;
}
.ef-cmdbar__section small {
  color: #7c7c7c;
  font-family: var(--ef-font-ui, sans-serif);
  font-size: 8px;
  letter-spacing: .14em;
  white-space: nowrap;
}
.ef-cmdbar__section span {
  overflow: hidden;
  color: #d8d8d8;
  font-family: var(--ef-font-ui, sans-serif);
  font-size: 11px;
  font-weight: 600;
  letter-spacing: .04em;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.ef-cmdbar__util {
  display: flex;
  align-items: stretch;
  margin-left: auto;
}
.ef-cmdbar__status {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 0 12px;
  border-left: 1px solid #2c2c2c;
  color: #9a9a9a;
  font-family: var(--ef-font-ui, sans-serif);
  font-size: 10px;
  letter-spacing: .08em;
  white-space: nowrap;
}
.ef-cmdbar__status i {
  width: 6px;
  height: 6px;
  background: #6e6e6e;
}
.ef-cmdbar__status.connected i { background: var(--ef-signal, #fffa00); }
.ef-cmdbar__icon {
  display: grid;
  place-items: center;
  width: 38px;
  border: 0;
  border-left: 1px solid #2c2c2c;
  background: transparent;
  color: #b9b9b9;
  cursor: pointer;
}
.ef-cmdbar__icon:hover {
  background: #242424;
  color: var(--ef-signal, #fffa00);
}
.ef-cmdbar__icon .material-icons { font-size: 17px; }
.ef-cmdbar__window {
  display: flex;
  align-items: stretch;
  border-left: 1px solid #2c2c2c;
}
.ef-cmdbar__window button {
  display: grid;
  place-items: center;
  width: 40px;
  border: 0;
  background: transparent;
  color: #b9b9b9;
  cursor: pointer;
}
.ef-cmdbar__window button:hover {
  background: #2a2a2a;
  color: #fff;
}
.ef-cmdbar__window button.close:hover {
  background: #b3261e;
  color: #fff;
}
.ef-cmdbar__window .material-icons { font-size: 16px; }
@media (max-width: 780px) {
  .ef-cmdbar__brand small,
  .ef-cmdbar__section { display: none; }
  .ef-cmdbar__status { display: none; }
}
</style>
