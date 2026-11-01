<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <div class="ef-settings">
    <div class="ef-settings__toolbar">
      <EfSearchBox v-model="search" placeholder="搜索设置" />
      <EfSelect v-model="category" :options="categoryOptions" />
    </div>
    <p class="ef-settings__summary">
      {{ scope === 'instance' ? '实例' : '启动器' }} · {{ visible.length }} 项
    </p>
    <div v-if="visible.length" class="ef-settings__rows">
      <article v-for="setting in visible" :key="setting.key" class="ef-setting">
        <div class="ef-setting__copy">
          <strong>{{ metadata(setting).label }}</strong>
          <small>{{ setting.key }} | {{ typeLabel(setting) }} | {{ setting.isDefault ? '默认' : '已修改' }}</small>
        </div>
        <div class="ef-setting__control">
          <EfCheckbox
            v-if="kind(setting) === 'boolean'"
            :model-value="Boolean(draft[setting.key])"
            :disabled="busy"
            @update:model-value="value => (draft[setting.key] = value)"
          >
            {{ draft[setting.key] ? '开启' : '关闭' }}
          </EfCheckbox>
          <EfInput
            v-else-if="kind(setting) === 'number'"
            v-model="draft[setting.key]"
            type="number"
            :disabled="busy"
          />
          <EfInput
            v-else
            v-model="draft[setting.key]"
            :type="setting.redacted ? 'password' : 'text'"
            :multiline="isStructured(setting)"
            :placeholder="setting.redacted ? '已隐藏，留空表示不修改' : ''"
            :disabled="busy"
          />
        </div>
        <div class="ef-setting__actions">
          <EfButton size="sm" variant="ghost" :disabled="busy" @click="emit('reset', setting.key)">重置</EfButton>
          <EfButton size="sm" variant="accent" :disabled="busy || (setting.redacted && !String(draft[setting.key] || '').trim())" @click="save(setting)">保存</EfButton>
        </div>
      </article>
    </div>
    <EfEmpty v-else title="没有匹配的设置" description="换一个关键词或分类。" icon="search_off" />
    <p v-if="validation" class="ef-settings__error">{{ validation }}</p>
  </div>
</template>

<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import { settingMetadata } from '../../../../settings-schema';
import { settingCategoryOrder } from '../../../../settings-categories';
import EfButton from '../EfButton.vue';
import EfCheckbox from '../EfCheckbox.vue';
import EfEmpty from '../feedback/EfEmpty.vue';
import EfInput from '../EfInput.vue';
import EfSelect from '../EfSelect.vue';
import EfSearchBox from '../controls/EfSearchBox.vue';

type ApiSetting = { key: string; type?: string; value: unknown; default?: unknown; isDefault?: boolean; redacted?: boolean; surface?: string; category?: string };
const props = withDefaults(defineProps<{ settings: ApiSetting[]; busy: boolean; scope?: string }>(), { scope: 'launcher' });
const emit = defineEmits<{ save: [key: string, value: unknown]; reset: [key: string] }>();

const search = ref('');
const category = ref('all');
const validation = ref('');
const draft = ref<Record<string, any>>({});
const legacyGroups: Record<string, string> = { Proxy: 'network', Launcher: 'qt-ui', ExternalTools: 'advanced', API: 'advanced' };

function metadata(setting: ApiSetting) {
  const fallback = settingMetadata[setting.key];
  const group = setting.category || legacyGroups[fallback?.group || ''] || fallback?.group || 'advanced';
  return { label: fallback?.label || setting.key, group };
}
function categoryOf(setting: ApiSetting) {
  return setting.surface === 'qt' ? 'qt-ui' : setting.category || metadata(setting).group;
}
function kind(setting: ApiSetting): 'boolean' | 'number' | 'structured' | 'string' {
  if (typeof setting.value === 'boolean') return 'boolean';
  if (typeof setting.value === 'number') return 'number';
  if (Array.isArray(setting.value) || (setting.value !== null && typeof setting.value === 'object')) return 'structured';
  if ((setting.type || '').includes('bool')) return 'boolean';
  if ((setting.type || '').match(/int|double|float/)) return 'number';
  return 'string';
}
function isStructured(setting: ApiSetting) { return kind(setting) === 'structured'; }
function typeLabel(setting: ApiSetting) {
  return ({ boolean: '布尔', number: '数字', structured: 'JSON', string: '文本' } as const)[kind(setting)];
}
const categoryOptions = computed(() => {
  const present = new Set(props.settings.map(categoryOf));
  return [
    { label: '全部分类', value: 'all' },
    ...settingCategoryOrder.filter(id => present.has(id)).map(id => ({ label: id, value: id })),
  ];
});
const visible = computed(() => {
  const query = search.value.trim().toLowerCase();
  return props.settings.filter(setting =>
    (category.value === 'all' || categoryOf(setting) === category.value) &&
    (!query || `${setting.key} ${metadata(setting).label}`.toLowerCase().includes(query)),
  );
});
watch(() => props.settings, items => {
  draft.value = Object.fromEntries(items.map(setting => [
    setting.key,
    setting.redacted ? '' : isStructured(setting) ? JSON.stringify(setting.value, null, 2) : setting.value,
  ]));
}, { immediate: true });

function save(setting: ApiSetting) {
  validation.value = '';
  let value: unknown = draft.value[setting.key];
  try {
    if (isStructured(setting)) value = JSON.parse(String(value || ''));
    if (kind(setting) === 'number' && !Number.isFinite(Number(value))) throw new Error('请输入有效数字');
    emit('save', setting.key, value);
  } catch (error) {
    validation.value = error instanceof Error ? error.message : String(error);
  }
}
</script>

<style scoped>
.ef-settings { width: 100%; }
.ef-settings__toolbar {
  display: flex;
  gap: 8px;
  align-items: center;
  margin-bottom: 8px;
}
.ef-settings__toolbar > :first-child { flex: 1; }
.ef-settings__toolbar > :last-child { width: 190px; }
.ef-settings__summary {
  margin: 0 0 10px;
  color: var(--ef-ink-2);
  font-family: var(--ef-font-ui);
  font-size: 9px;
  letter-spacing: .12em;
  text-transform: uppercase;
}
.ef-settings__rows { display: flex; flex-direction: column; }
.ef-setting {
  display: grid;
  grid-template-columns: minmax(160px, 1fr) minmax(180px, 1.2fr) auto;
  gap: 12px;
  align-items: center;
  padding: 9px 0;
  border-bottom: 1px solid var(--ef-rule);
}
.ef-setting__copy { min-width: 0; }
.ef-setting__copy strong {
  display: block;
  overflow: hidden;
  font-family: var(--ef-font-ui);
  font-size: 12px;
  font-weight: 700;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.ef-setting__copy small {
  display: block;
  margin-top: 2px;
  overflow: hidden;
  color: var(--ef-ink-3, #8a8a86);
  font-size: 9px;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.ef-setting__control { min-width: 0; }
.ef-setting__actions { display: flex; gap: 5px; }
.ef-settings__error {
  margin: 10px 0 0;
  padding: 6px 8px;
  border-left: 2px solid var(--ef-danger);
  background: var(--ef-panel);
  color: var(--ef-ink);
  font-size: 11px;
}
@media (max-width: 760px) {
  .ef-setting { grid-template-columns: 1fr; gap: 6px; }
  .ef-setting__actions { justify-content: flex-end; }
}
</style>
