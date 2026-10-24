<template>
  <div class="pcl-edit-window" :style="{ left: `${position.x}px`, top: `${position.y}px`, zIndex: 120 + index }">
    <div class="pcl-edit-window__head" @pointerdown="startDrag">
      <img v-if="icon" :src="icon" alt="" />
      <span v-else class="pcl-edit-window__fallback"><PclIcon name="box" :size="18" /></span>
      <div class="pcl-edit-window__title">
        <strong>{{ form.name || instance?.name || '编辑实例' }}</strong>
        <small>{{ instance?.id || '' }}</small>
      </div>
      <button type="button" class="pcl-edit-window__close" aria-label="关闭" @click="emit('close')">×</button>
    </div>

    <div class="pcl-edit-window__body">
      <PclHint v-if="error" theme="red" closable @close="error = ''">{{ error }}</PclHint>
      <div v-if="loading" class="pcl-edit-window__loading"><PclLoading inline /> 正在读取实例设置…</div>
      <template v-else>
        <div class="pcl-edit-window__grid">
          <label><span>实例名称</span><PclInput v-model="form.name" placeholder="实例名称" /></label>
          <label><span>分组</span><PclInput v-model="form.group" placeholder="留空表示常规实例" /></label>
          <label><span>最小内存 (MiB)</span><PclInput v-model="form.minMemory" type="number" /></label>
          <label><span>最大内存 (MiB)</span><PclInput v-model="form.maxMemory" type="number" /></label>
          <label class="pcl-edit-window__wide"><span>Java 路径</span><PclInput v-model="form.javaPath" placeholder="留空使用自动选择" /></label>
          <label class="pcl-edit-window__wide"><span>JVM 参数</span><PclInput v-model="form.jvmArgs" placeholder="实例专用 JVM 参数" /></label>
          <label class="pcl-edit-window__wide"><span>实例备注</span><PclInput v-model="form.notes" multiline :rows="4" placeholder="实例备注" /></label>
        </div>
        <div class="pcl-edit-window__actions">
          <span v-if="saved" class="pcl-edit-window__saved">已保存</span>
          <PclButton variant="highlight" icon="save" :disabled="saving" @click="save">{{ saving ? '保存中…' : '保存修改' }}</PclButton>
        </div>
      </template>
    </div>
  </div>
</template>

<script setup lang="ts">
import { onBeforeUnmount, reactive, ref } from 'vue';
import PclButton from './PclButton.vue';
import PclHint from './PclHint.vue';
import PclIcon from './PclIcon.vue';
import PclInput from './PclInput.vue';
import PclLoading from './PclLoading.vue';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;

const props = defineProps<{
  instance: any;
  index: number;
  icon?: string;
  execute: Execute;
}>();

const emit = defineEmits<{ close: []; changed: [] }>();

const loading = ref(true);
const saving = ref(false);
const saved = ref(false);
const error = ref('');
const position = reactive({ x: 190 + Math.max(0, props.index) * 34, y: 92 + Math.max(0, props.index) * 28 });
const original = reactive({ name: '', group: '', notes: '', minMemory: '512', maxMemory: '4096', javaPath: '', jvmArgs: '' });
const form = reactive({ ...original });

function settingValue(settings: any[], key: string, fallback: any) {
  const item = settings.find(value => String(value?.key || '') === key);
  return item?.value ?? fallback;
}

async function load() {
  loading.value = true;
  error.value = '';
  try {
    const info = await props.execute('instance.info', { instance: props.instance.id });
    const settings = await props.execute('settings.list', { scope: 'instance', instance: props.instance.id });
    Object.assign(form, {
      name: String(info?.name || props.instance.name || ''),
      group: String(info?.group || props.instance.group || ''),
      notes: String(info?.notes || props.instance.notes || ''),
      minMemory: String(settingValue(settings, 'MinMemAlloc', 512)),
      maxMemory: String(settingValue(settings, 'MaxMemAlloc', 4096)),
      javaPath: String(settingValue(settings, 'JavaPath', '')),
      jvmArgs: String(settingValue(settings, 'JvmArgs', '')),
    });
    Object.assign(original, form);
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    loading.value = false;
  }
}

async function save() {
  saving.value = true;
  saved.value = false;
  error.value = '';
  try {
    const id = props.instance.id;
    if (form.name.trim() && form.name !== original.name) {
      await props.execute('instance.rename', { instance: id, name: form.name.trim() });
      original.name = form.name.trim();
    }
    if (form.group !== original.group) {
      await props.execute('instance.group', { instance: id, group: form.group });
      original.group = form.group;
    }
    if (form.notes !== original.notes) {
      await props.execute('instance.set-notes', { instance: id, notes: form.notes });
      original.notes = form.notes;
    }
    const changes: Array<[string, any]> = [
      ['MinMemAlloc', Number(form.minMemory) || 512],
      ['MaxMemAlloc', Number(form.maxMemory) || 4096],
      ['JavaPath', form.javaPath],
      ['JvmArgs', form.jvmArgs],
    ];
    for (const [key, value] of changes) {
      const before = String(original[key === 'MinMemAlloc' ? 'minMemory' : key === 'MaxMemAlloc' ? 'maxMemory' : key === 'JavaPath' ? 'javaPath' : 'jvmArgs'] ?? '');
      if (String(value ?? '') === before) continue;
      await props.execute('settings.set', { scope: 'instance', instance: id, key, value });
    }
    Object.assign(original, { minMemory: form.minMemory, maxMemory: form.maxMemory, javaPath: form.javaPath, jvmArgs: form.jvmArgs });
    saved.value = true;
    emit('changed');
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    saving.value = false;
  }
}

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
  position.x = Math.max(0, Math.min(window.innerWidth - 380, event.clientX - dragX));
  position.y = Math.max(0, Math.min(window.innerHeight - 120, event.clientY - dragY));
}
function stopDrag() {
  dragging = false;
  window.removeEventListener('pointermove', onDrag);
  window.removeEventListener('pointerup', stopDrag);
}
onBeforeUnmount(stopDrag);
load();
</script>

<style scoped>
.pcl-edit-window {
  position: fixed;
  width: 430px;
  max-width: calc(100vw - 20px);
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
.pcl-edit-window__body { max-height: min(680px, calc(100vh - 150px)); overflow-y: auto; padding: 12px; }
.pcl-edit-window__loading { display: flex; align-items: center; gap: 7px; min-height: 90px; color: var(--pcl-muted); font-size: calc(10px * var(--pcl-font-scale, 1)); }
.pcl-edit-window__grid { display: grid; grid-template-columns: 1fr 1fr; gap: 9px 12px; }
.pcl-edit-window__grid label { display: flex; flex-direction: column; gap: 4px; min-width: 0; color: var(--pcl-muted); font-size: calc(9px * var(--pcl-font-scale, 1)); }
.pcl-edit-window__wide { grid-column: 1 / -1; }
.pcl-edit-window__actions { display: flex; align-items: center; justify-content: flex-end; gap: 9px; margin-top: 12px; }
.pcl-edit-window__saved { color: var(--pcl-c3); font-size: calc(9px * var(--pcl-font-scale, 1)); }
</style>
