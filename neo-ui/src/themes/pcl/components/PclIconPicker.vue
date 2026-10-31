<template>
  <q-dialog v-model="open">
    <q-card class="pcl-dialog pcl-icon-picker">
      <div class="pcl-icon-picker__head">
        <div>
          <div class="pcl-kicker">{{ t('instance.iconLibrary') }}</div>
          <h3>{{ t('instance.chooseIcon') }}</h3>
        </div>
        <button type="button" class="pcl-icon-picker__close" :aria-label="t('window.close')" @click="open = false">×</button>
      </div>

      <div class="pcl-icon-picker__toolbar">
        <input v-model="query" class="pcl-input" type="search" :placeholder="t('instance.searchIcons')" />
        <PclButton icon="folder-open" :disabled="loading" @click="importIcon">{{ t('instance.importIcon') }}</PclButton>
      </div>

      <div v-if="loading" class="pcl-icon-picker__state">{{ t('about.loadingRuntime') }}</div>
      <div v-else-if="error" class="pcl-icon-picker__state pcl-icon-picker__state--error">{{ error }}</div>
      <div v-else-if="!filteredIcons.length" class="pcl-icon-picker__state">{{ t('instance.noIcons') }}</div>
      <div v-else class="pcl-icon-picker__grid">
        <button
          v-for="item in filteredIcons"
          :key="item.key"
          type="button"
          class="pcl-icon-picker__item"
          :class="{ selected: item.key === selectedKey }"
          :title="item.name || item.key"
          @click="choose(item.key)"
        >
          <img :src="item.dataUrl" :alt="item.name || item.key" />
          <span>{{ item.name || item.key }}</span>
        </button>
      </div>
    </q-card>
  </q-dialog>
</template>

<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import PclButton from './PclButton.vue';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
type IconOption = { key: string; name?: string; dataUrl: string };

const props = defineProps<{
  modelValue: boolean;
  selectedKey?: string;
  execute: Execute;
}>();
const emit = defineEmits<{
  'update:modelValue': [value: boolean];
  select: [key: string];
  import: [];
}>();
const { t } = useI18n();

const open = computed({ get: () => props.modelValue, set: value => emit('update:modelValue', value) });
const query = ref('');
const loading = ref(false);
const error = ref('');
const icons = ref<IconOption[]>([]);
let loaded = false;

const filteredIcons = computed(() => {
  const needle = query.value.trim().toLocaleLowerCase();
  if (!needle) return icons.value;
  return icons.value.filter(item => `${item.name || ''} ${item.key}`.toLocaleLowerCase().includes(needle));
});

async function load() {
  if (loaded || loading.value) return;
  loading.value = true;
  error.value = '';
  try {
    const result = await props.execute('instance.icon-library');
    icons.value = Array.isArray(result?.icons)
      ? result.icons.filter((item: any) => item?.key && item?.dataUrl).map((item: any) => ({ key: String(item.key), name: String(item.name || item.key), dataUrl: String(item.dataUrl) }))
      : [];
    loaded = true;
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    loading.value = false;
  }
}

function choose(key: string) {
  emit('select', key);
  open.value = false;
}

function importIcon() {
  open.value = false;
  emit('import');
}

watch(() => props.modelValue, value => {
  if (value) void load();
});
</script>

<style scoped>
.pcl-icon-picker {
  width: min(720px, 94vw);
  max-width: 94vw;
  padding: 16px;
}

.pcl-icon-picker__head,
.pcl-icon-picker__toolbar {
  display: flex;
  align-items: center;
  gap: 10px;
}

.pcl-icon-picker__head {
  justify-content: space-between;
  margin-bottom: 12px;
}

.pcl-icon-picker__head h3 {
  margin: 3px 0 0;
  color: var(--pcl-text);
  font-size: calc(16px * var(--pcl-font-scale, 1));
}

.pcl-icon-picker__close {
  width: 26px;
  height: 26px;
  border: 0;
  border-radius: 3px;
  background: transparent;
  color: var(--pcl-muted);
  font-size: 20px;
  line-height: 1;
  cursor: pointer;
}

.pcl-icon-picker__close:hover { background: var(--pcl-c6); color: var(--pcl-text); }
.pcl-icon-picker__toolbar { margin-bottom: 12px; }
.pcl-icon-picker__toolbar .pcl-input { min-width: 0; flex: 1; }

.pcl-icon-picker__grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(104px, 1fr));
  gap: 8px;
  max-height: min(500px, 62vh);
  overflow-y: auto;
  padding: 2px;
}

.pcl-icon-picker__item {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 6px;
  min-width: 0;
  min-height: 96px;
  padding: 9px 6px 7px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: color-mix(in srgb, var(--pcl-c7) 72%, transparent);
  color: var(--pcl-text);
  cursor: pointer;
}

.pcl-icon-picker__item:hover,
.pcl-icon-picker__item.selected {
  border-color: var(--pcl-c3);
  background: color-mix(in srgb, var(--pcl-c3) 14%, var(--pcl-c7));
}

.pcl-icon-picker__item img {
  width: 48px;
  height: 48px;
  object-fit: contain;
  image-rendering: auto;
}

.pcl-icon-picker__item span {
  width: 100%;
  overflow: hidden;
  font-size: calc(11px * var(--pcl-font-scale, 1));
  text-overflow: ellipsis;
  white-space: nowrap;
  text-align: center;
}

.pcl-icon-picker__state {
  padding: 30px 12px;
  color: var(--pcl-muted);
  text-align: center;
}

.pcl-icon-picker__state--error { color: #f28b9d; }
</style>
