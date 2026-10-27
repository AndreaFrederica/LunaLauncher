<template>
  <div class="pcl-color-picker">
    <div class="pcl-color-picker__presets">
      <button
        v-for="preset in PCL_ACCENT_PRESETS"
        :key="preset.value"
        type="button"
        class="pcl-color-picker__preset"
        :class="{ active: normalized === preset.value }"
        :title="`${preset.name} ${preset.value}`"
        :aria-label="`${preset.name} ${preset.value}`"
        :style="{ '--swatch': preset.value }"
        @click="choose(preset.value)"
      >
        <PclIcon v-if="normalized === preset.value" name="circle-check" :size="14" />
      </button>
    </div>
    <div class="pcl-color-picker__custom">
      <label class="pcl-color-picker__swatch" :style="{ background: normalized }" :title="t('pcl.color.custom')">
        <input type="color" :value="normalized" @input="onNativeColor" />
      </label>
      <PclInput v-model="draft" placeholder="#2a80e3" @enter="commit" />
      <PclButton size="sm" icon="check" @click="commit">{{ t('common.save') }}</PclButton>
    </div>
    <div class="pcl-color-picker__names">
      <span v-for="preset in PCL_ACCENT_PRESETS" :key="preset.value">{{ preset.name }}</span>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import { PCL_ACCENT_PRESETS, normalizePclAccent } from '../../neo-ui-preferences';
import PclButton from './PclButton.vue';
import PclIcon from './PclIcon.vue';
import PclInput from './PclInput.vue';

const props = defineProps<{ modelValue?: string }>();
const emit = defineEmits<{ 'update:modelValue': [value: string] }>();
const { t } = useI18n();
const draft = ref(props.modelValue || '#2a80e3');
const normalized = computed(() => normalizePclAccent(props.modelValue || draft.value));

watch(() => props.modelValue, value => { draft.value = value || '#2a80e3'; });

function choose(value: string) {
  draft.value = value;
  emit('update:modelValue', value);
}

function commit() {
  const value = normalizePclAccent(draft.value);
  draft.value = value;
  emit('update:modelValue', value);
}

function onNativeColor(event: Event) {
  choose((event.target as HTMLInputElement).value);
}
</script>

<style scoped>
.pcl-color-picker__presets {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
}

.pcl-color-picker__preset {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 30px;
  height: 30px;
  padding: 0;
  border: 2px solid transparent;
  border-radius: 5px;
  background: var(--swatch);
  color: #fff;
  cursor: pointer;
  box-shadow: inset 0 0 0 1px rgba(255, 255, 255, .2);
  transition: transform .12s ease, border-color .12s ease;
}

.pcl-color-picker__preset:hover {
  transform: translateY(-1px);
}

.pcl-color-picker__preset.active {
  border-color: var(--pcl-text);
}

.pcl-color-picker__custom {
  display: flex;
  align-items: center;
  gap: 6px;
  margin-top: 9px;
}

.pcl-color-picker__custom :deep(.pcl-input) {
  width: 120px;
  flex: 0 0 120px;
}

.pcl-color-picker__swatch {
  position: relative;
  display: block;
  flex: 0 0 28px;
  width: 28px;
  height: 28px;
  overflow: hidden;
  border: 1px solid var(--pcl-border);
  border-radius: 5px;
  cursor: pointer;
}

.pcl-color-picker__swatch input {
  position: absolute;
  inset: 0;
  width: 100%;
  height: 100%;
  opacity: 0;
  cursor: pointer;
}

.pcl-color-picker__names {
  display: flex;
  flex-wrap: wrap;
  gap: 3px 11px;
  margin-top: 5px;
  color: var(--pcl-muted);
  font-size: calc(9px * var(--pcl-font-scale, 1));
}
</style>
