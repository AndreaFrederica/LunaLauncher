<template>
  <header class="pcl-window-titlebar" data-tauri-drag-region @dblclick="toggleMaximize">
    <div class="pcl-window-titlebar__brand" data-tauri-drag-region>
      <div data-tauri-drag-region>
        <strong data-tauri-drag-region>{{ title || 'Luna Launcher' }}</strong>
        <small v-if="subtitle" data-tauri-drag-region>{{ subtitle }}</small>
      </div>
    </div>
    <div class="pcl-window-titlebar__actions">
      <button type="button" :aria-label="t('window.minimize')" :title="t('window.minimize')" @click="minimize">
        <span class="material-icons">remove</span>
      </button>
      <button type="button" :aria-label="maximized ? t('window.restore') : t('window.maximize')" :title="maximized ? t('window.restore') : t('window.maximize')" @click="toggleMaximize">
        <span class="material-icons">{{ maximized ? 'filter_none' : 'crop_square' }}</span>
      </button>
      <button type="button" class="pcl-window-titlebar__close" :aria-label="t('window.close')" :title="t('window.close')" @click="close">
        <span class="material-icons">close</span>
      </button>
    </div>
  </header>
</template>

<script setup lang="ts">
import { onMounted, onUnmounted, ref } from 'vue';
import { getCurrentWindow } from '@tauri-apps/api/window';
import { useI18n } from 'vue-i18n';
const { t } = useI18n();

defineProps<{
  title?: string;
  subtitle?: string;
  mark?: string;
}>();

const maximized = ref(false);
let unlisten: (() => void) | undefined;

async function minimize() {
  try { await getCurrentWindow().minimize(); } catch { /* Browser preview. */ }
}
async function toggleMaximize() {
  try {
    const window = getCurrentWindow();
    if (await window.isMaximized()) await window.unmaximize();
    else await window.maximize();
    maximized.value = await window.isMaximized();
  } catch { /* Browser preview. */ }
}
async function close() {
  try { await getCurrentWindow().close(); } catch { /* Browser preview. */ }
}
async function syncMaximized() {
  try { maximized.value = await getCurrentWindow().isMaximized(); } catch { /* Browser preview. */ }
}
onMounted(async () => {
  await syncMaximized();
  try {
    unlisten = await getCurrentWindow().onResized(syncMaximized);
  } catch { /* Browser preview. */ }
});
onUnmounted(() => unlisten?.());
</script>

<style scoped>
.pcl-window-titlebar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  flex: 0 0 38px;
  height: 38px;
  padding: 0 4px 0 12px;
  background: var(--pcl-c3);
  color: #fff;
  user-select: none;
}

.pcl-window-titlebar__brand {
  display: flex;
  align-items: center;
  gap: 8px;
  min-width: 0;
  pointer-events: none;
}

.pcl-window-titlebar__brand strong {
  display: block;
  overflow: hidden;
  max-width: 420px;
  font-size: calc(11px * var(--pcl-font-scale, 1));
  font-weight: 500;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-window-titlebar__brand small {
  display: block;
  margin-top: 1px;
  overflow: hidden;
  max-width: 520px;
  color: rgba(255, 255, 255, .68);
  font-size: calc(8px * var(--pcl-font-scale, 1));
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-window-titlebar__actions {
  display: flex;
  align-items: center;
  gap: 1px;
  flex: 0 0 auto;
}

.pcl-window-titlebar__actions button {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 38px;
  height: 38px;
  border: 0;
  border-radius: 3px;
  background: transparent;
  color: inherit;
  cursor: pointer;
}

.pcl-window-titlebar__actions button:hover { background: rgba(255, 255, 255, .18); }
.pcl-window-titlebar__actions .pcl-window-titlebar__close:hover { background: #c42b1c; }
.material-icons { font-size: 18px; line-height: 1; }
</style>
