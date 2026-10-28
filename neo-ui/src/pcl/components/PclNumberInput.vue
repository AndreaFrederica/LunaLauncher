<template>
  <div class="pcl-number-input">
    <PclInput
      v-model="draft" type="number" :aria-label="label" :min="min" :max="max" :step="step"
      :disabled="disabled" :aria-invalid="!!error" @blur="commit" @enter="commit"
    />
    <span v-if="unit" class="pcl-muted">{{ unit }}</span>
    <span v-if="error" class="pcl-number-input__error" role="alert">{{ error }}</span>
  </div>
</template>

<script setup lang="ts">
import { ref, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import PclInput from './PclInput.vue';

const props = withDefaults(defineProps<{
  modelValue: number; min: number; max: number; step?: number; unit?: string; label?: string; disabled?: boolean;
}>(), { step: 1, unit: '', label: '', disabled: false });
const emit = defineEmits<{ 'update:modelValue': [value: number] }>();
const { t } = useI18n();
const draft = ref<string | number>(props.modelValue);
const error = ref('');
watch(() => props.modelValue, value => { draft.value = value; error.value = ''; });
function commit() {
  if (props.disabled) return;
  const value = Number(draft.value);
  if (draft.value === '' || !Number.isInteger(value) || value < props.min || value > props.max) {
    error.value = t('pcl.number.integerRange', { min: props.min, max: props.max });
    return;
  }
  error.value = '';
  if (value !== props.modelValue) emit('update:modelValue', value);
}
</script>

<style scoped>
.pcl-number-input { display: flex; align-items: center; flex-wrap: wrap; gap: 8px; }
.pcl-number-input .pcl-input { width: 130px; flex: none; }
.pcl-number-input__error { color: #dc4545; font-size: 12px; }
</style>
