<template>
  <label class="ef-check" :class="{ disabled }">
    <input type="checkbox" :checked="modelValue" :disabled="disabled" @change="emit('update:modelValue', ($event.target as HTMLInputElement).checked)" />
    <span class="ef-check__box" />
    <span class="ef-check__label"><slot /></span>
  </label>
</template>

<script setup lang="ts">
const props = withDefaults(defineProps<{ modelValue?: boolean; disabled?: boolean }>(), { modelValue: false, disabled: false });
const emit = defineEmits<{ 'update:modelValue': [value: boolean] }>();
</script>

<style scoped>
.ef-check { display: inline-flex; align-items: center; gap: 8px; cursor: pointer; color: var(--ef-ink); font-family: var(--ef-font-ui); font-size: 13px; }
.ef-check.disabled { opacity: .45; cursor: default; }
.ef-check input { position: absolute; opacity: 0; width: 0; height: 0; }
.ef-check__box { width: 16px; height: 16px; border: 1px solid var(--ef-ink); border-radius: 0; background: var(--ef-panel); position: relative; flex: 0 0 auto; }
.ef-check input:checked + .ef-check__box { background: var(--ef-signal); }
.ef-check input:checked + .ef-check__box::after { content: ''; position: absolute; left: 4px; top: 1px; width: 5px; height: 9px; border: solid var(--ef-on-signal); border-width: 0 2px 2px 0; transform: rotate(45deg); }
.ef-check:hover .ef-check__box { border-color: var(--ef-ink); }
</style>
