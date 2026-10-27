<template>
  <div class="pcl-search-box">
    <PclIcon name="search" :size="14" class="pcl-search-box__icon" />
    <input
      class="pcl-search-box__input"
      type="text"
      :value="modelValue"
      :placeholder="placeholder || t('search.placeholder')"
      :disabled="disabled"
      @input="emit('update:modelValue', ($event.target as HTMLInputElement).value)"
      @keydown.enter="emit('search', modelValue)"
    />
    <button v-if="modelValue" type="button" class="pcl-search-box__clear" :aria-label="t('search.clear')" @click="clear">
      <PclIcon name="x" :size="14" />
    </button>
  </div>
</template>

<script setup lang="ts">
import PclIcon from './PclIcon.vue';
import { useI18n } from 'vue-i18n';
const { t } = useI18n();

withDefaults(defineProps<{
  modelValue?: string;
  placeholder?: string;
  disabled?: boolean;
}>(), {
  modelValue: '',
  placeholder: '',
  disabled: false,
});

const emit = defineEmits<{
  'update:modelValue': [value: string];
  search: [value: string];
}>();

function clear() {
  emit('update:modelValue', '');
}
</script>

<style scoped>
.pcl-search-box {
  position: relative;
  display: flex;
  align-items: center;
  height: 40px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 5px;
  background: var(--pcl-card);
  box-shadow: var(--pcl-shadow);
}

.pcl-search-box__icon {
  position: absolute;
  left: 14px;
  color: var(--pcl-text);
  pointer-events: none;
}

.pcl-search-box__input {
  width: 100%;
  height: 100%;
  padding: 0 40px 0 32px;
  border: 0;
  background: transparent;
  color: var(--pcl-text);
  font: 400 calc(13.5px * var(--pcl-font-scale, 1))/1 var(--pcl-font);
  outline: none;
}

.pcl-search-box__input::placeholder {
  color: var(--pcl-gray4);
}

.pcl-search-box__clear {
  position: absolute;
  right: 8px;
  display: flex;
  align-items: center;
  justify-content: center;
  width: 24px;
  height: 24px;
  padding: 0;
  border: 0;
  border-radius: 50%;
  background: transparent;
  color: var(--pcl-muted);
  cursor: pointer;
  transition: background .12s ease, color .12s ease;
}

.pcl-search-box__clear:hover {
  background: var(--pcl-hover);
  color: var(--pcl-c2);
}
</style>
