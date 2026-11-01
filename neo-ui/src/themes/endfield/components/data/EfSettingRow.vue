<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <article class="ef-setting-row">
    <div class="ef-setting-row__copy">
      <strong>{{ label }}</strong>
      <small>{{ setting.key }} | {{ typeLabel }} | {{ setting.isDefault ? '默认' : '已修改' }}</small>
      <p v-if="description" class="ef-setting-row__desc">{{ description }}</p>
    </div>
    <div class="ef-setting-row__control">
      <EfCheckbox
        v-if="kind === 'boolean'"
        :model-value="Boolean(draft)"
        :disabled="busy"
        @update:model-value="value => (draft = value)"
      >
        {{ draft ? '开启' : '关闭' }}
      </EfCheckbox>
      <EfInput
        v-else-if="kind === 'number'"
        v-model="draft"
        type="number"
        :min="setting.minimum"
        :max="setting.maximum"
        :disabled="busy"
      />
      <EfSelect
        v-else-if="options.length"
        v-model="draft"
        :options="options"
        :disabled="busy"
      />
      <EfInput
        v-else
        v-model="draft"
        :type="setting.redacted ? 'password' : 'text'"
        :multiline="kind === 'structured'"
        :placeholder="setting.redacted ? '已隐藏，留空表示不修改' : ''"
        :disabled="busy"
      />
    </div>
    <div class="ef-setting-row__actions">
      <EfButton size="sm" variant="ghost" :disabled="busy" @click="emit('reset', setting.key)">重置</EfButton>
      <EfButton size="sm" variant="accent" :disabled="busy || (setting.redacted && !String(draft || '').trim())" @click="save">保存</EfButton>
    </div>
  </article>
</template>

<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import { settingMetadata } from '../../../../settings-schema';
import EfButton from '../EfButton.vue';
import EfCheckbox from '../EfCheckbox.vue';
import EfInput from '../EfInput.vue';
import EfSelect from '../EfSelect.vue';

type ApiSetting = { key: string; type?: string; value: unknown; default?: unknown; isDefault?: boolean; redacted?: boolean; surface?: string; category?: string; minimum?: number; maximum?: number };
const props = withDefaults(defineProps<{ setting: ApiSetting; busy: boolean }>(), {});
const emit = defineEmits<{ save: [key: string, value: unknown]; reset: [key: string] }>();
const { t, te } = useI18n();

const draft = ref<any>('');
const meta = computed(() => settingMetadata[props.setting.key]);
const label = computed(() => {
  const key = meta.value?.label;
  return key && te(key) ? t(key) : props.setting.key;
});
const description = computed(() => {
  const key = `${meta.value?.label || ''}.description`;
  return key && te(key) ? t(key) : '';
});
const kind = computed<'boolean' | 'number' | 'structured' | 'string'>(() => {
  if (typeof props.setting.value === 'boolean') return 'boolean';
  if (typeof props.setting.value === 'number') return 'number';
  if (Array.isArray(props.setting.value) || (props.setting.value !== null && typeof props.setting.value === 'object')) return 'structured';
  if ((props.setting.type || '').includes('bool')) return 'boolean';
  if ((props.setting.type || '').match(/int|double|float/)) return 'number';
  return 'string';
});
const typeLabel = computed(() => ({ boolean: '布尔', number: '数字', structured: 'JSON', string: '文本' } as const)[kind.value]);
const options = computed(() => {
  if (props.setting.key.toLowerCase().includes('theme')) {
    return [{ label: '跟随系统', value: 'system' }, { label: '浅色', value: 'light' }, { label: '深色', value: 'dark' }];
  }
  return [];
});

watch(() => props.setting, setting => {
  draft.value = setting.redacted ? '' : kind.value === 'structured' ? JSON.stringify(setting.value, null, 2) : setting.value;
}, { immediate: true });

function save() {
  let value: unknown = draft.value;
  if (kind.value === 'structured') {
    try { value = JSON.parse(String(value || '{}')); } catch { /* keep raw text for the backend to reject */ }
  }
  if (kind.value === 'number') value = Number(value);
  emit('save', props.setting.key, value);
}
</script>

<style scoped>
.ef-setting-row {
  display: grid;
  grid-template-columns: minmax(200px, 1.1fr) minmax(220px, 1.3fr) auto;
  gap: 14px;
  align-items: center;
  padding: 11px 0;
  border-bottom: 1px solid var(--ef-rule);
}
.ef-setting-row__copy { min-width: 0; }
.ef-setting-row__copy strong {
  display: block;
  font-family: var(--ef-font-ui);
  font-size: 12px;
  font-weight: 700;
}
.ef-setting-row__copy small {
  display: block;
  margin-top: 2px;
  overflow: hidden;
  color: var(--ef-ink-3, #8a8a86);
  font-size: 9px;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.ef-setting-row__desc {
  margin: 4px 0 0;
  color: var(--ef-ink-2);
  font-size: 10px;
  line-height: 1.45;
}
.ef-setting-row__control { min-width: 0; }
.ef-setting-row__actions { display: flex; gap: 5px; }
@media (max-width: 760px) {
  .ef-setting-row { grid-template-columns: 1fr; gap: 7px; }
  .ef-setting-row__actions { justify-content: flex-end; }
}
</style>
