<template>
  <div class="pcl-edit-window" :style="{ left: `${position.x}px`, top: `${position.y}px`, zIndex: 120 + index }">
    <div class="pcl-edit-window__head" @pointerdown="startDrag">
      <img v-if="icon" :src="icon" alt="" />
      <span v-else class="pcl-edit-window__fallback"><PclIcon name="box" :size="18" /></span>
      <div class="pcl-edit-window__title">
        <strong>{{ instance?.name || t('instanceSections.settings') }}</strong>
        <small>{{ t('instanceSections.settings') }} · {{ instance?.id || '' }}</small>
      </div>
      <button type="button" class="pcl-edit-window__close" :aria-label="t('window.close')" @click="emit('close')">×</button>
    </div>

    <div class="pcl-edit-window__body">
      <PclSetupPage
        :settings="settings"
        :busy="false"
        :scope="{ scope: 'instance', instance: instance.id }"
        :execute="execute"
        :accounts="accounts || []"
        @changed="emit('changed')"
      />
    </div>
  </div>
</template>

<script setup lang="ts">
import { useI18n } from 'vue-i18n';
const { t } = useI18n();
import { onBeforeUnmount, onMounted, reactive, ref } from 'vue';
import PclIcon from './PclIcon.vue';
import PclSetupPage from '../pages/settings/PclSetupPage.vue';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;

const props = defineProps<{
  instance: any;
  index: number;
  icon?: string;
  accounts?: any[];
  execute: Execute;
}>();

const emit = defineEmits<{ close: []; changed: [] }>();
const settings = ref<any[]>([]);

const position = reactive({ x: 150 + Math.max(0, props.index) * 34, y: 82 + Math.max(0, props.index) * 28 });

let dragging = false;
let dragX = 0;
let dragY = 0;
function startDrag(event: PointerEvent) {
  if ((event.target as HTMLElement)?.closest('button')) return;
  dragging = true;
  dragX = event.clientX - position.x;
  dragY = event.clientY - position.y;
  window.addEventListener('pointermove', onDrag);
  window.addEventListener('pointerup', stopDrag);
}
function onDrag(event: PointerEvent) {
  if (!dragging) return;
  position.x = Math.max(0, Math.min(window.innerWidth - 420, event.clientX - dragX));
  position.y = Math.max(0, Math.min(window.innerHeight - 120, event.clientY - dragY));
}
function stopDrag() {
  dragging = false;
  window.removeEventListener('pointermove', onDrag);
  window.removeEventListener('pointerup', stopDrag);
}
onBeforeUnmount(stopDrag);
onMounted(async () => {
  try {
    const result = await props.execute('settings.list', { scope: 'instance', instance: props.instance.id });
    settings.value = Array.isArray(result) ? result : [];
  } catch {
    settings.value = [];
  }
});
</script>

<style scoped>
.pcl-edit-window {
  position: fixed;
  display: flex;
  flex-direction: column;
  width: 920px;
  max-width: calc(100vw - 16px);
  max-height: calc(100vh - 80px);
  overflow: hidden;
  border: 1px solid var(--pcl-card-border);
  border-radius: 5px;
  background: var(--pcl-c8);
  color: var(--pcl-text);
  box-shadow: 0 14px 38px rgba(0, 0, 0, .42);
}

.pcl-edit-window__head {
  display: flex;
  align-items: center;
  gap: 8px;
  flex: 0 0 auto;
  min-height: 42px;
  padding: 7px 9px;
  border-bottom: 1px solid var(--pcl-border);
  background: var(--pcl-c8);
  cursor: move;
  touch-action: none;
}

.pcl-edit-window__head img,
.pcl-edit-window__fallback {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 28px;
  height: 28px;
  flex: 0 0 28px;
  border-radius: 3px;
  object-fit: contain;
}

.pcl-edit-window__fallback { background: var(--pcl-c7); color: var(--pcl-muted); }
.pcl-edit-window__title { display: flex; flex-direction: column; min-width: 0; flex: 1; }
.pcl-edit-window__title strong { overflow: hidden; font-size: calc(11px * var(--pcl-font-scale, 1)); font-weight: 500; text-overflow: ellipsis; white-space: nowrap; }
.pcl-edit-window__title small { overflow: hidden; color: var(--pcl-muted); font-size: calc(9px * var(--pcl-font-scale, 1)); text-overflow: ellipsis; white-space: nowrap; }
.pcl-edit-window__close { width: 24px; height: 24px; border: 0; border-radius: 3px; background: transparent; color: var(--pcl-muted); font-size: 18px; line-height: 1; cursor: pointer; }
.pcl-edit-window__close:hover { background: var(--pcl-c6); color: var(--pcl-text); }

.pcl-edit-window__body {
  flex: 1 1 auto;
  min-height: 0;
  height: min(680px, calc(100vh - 150px));
  overflow: hidden;
}

.pcl-edit-window__body :deep(.pcl-setup-page) {
  height: 100%;
  min-height: 0;
}

.pcl-edit-window__body :deep(.pcl-page-main) {
  height: 100%;
  padding: 18px 20px;
  overflow-y: auto;
}

.pcl-edit-window__body :deep(.pcl-page-nav) {
  height: 100%;
  padding-top: 14px;
}
</style>
