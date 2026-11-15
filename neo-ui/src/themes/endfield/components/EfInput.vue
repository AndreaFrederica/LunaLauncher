<template>
  <textarea v-if="multiline" class="ef-input" :value="String(modelValue ?? '')" :placeholder="placeholder" :disabled="disabled" :rows="rows" :spellcheck="false" @input="emit('update:modelValue', ($event.target as HTMLTextAreaElement).value)" @blur="emit('blur')" />
  <input v-else class="ef-input" :type="type" :value="modelValue" :placeholder="placeholder" :disabled="disabled" :min="min" :max="max" :step="step" :spellcheck="false" @input="onInput" @blur="emit('blur')" @keydown.enter="emit('enter')" />
</template>

<script setup lang="ts">
const props = withDefaults(defineProps<{ modelValue?: string | number; type?: string; placeholder?: string; disabled?: boolean; multiline?: boolean; rows?: number; min?: string | number; max?: string | number; step?: string | number }>(), { modelValue: '', type: 'text', placeholder: '', disabled: false, multiline: false, rows: 4 });
const emit = defineEmits<{ 'update:modelValue': [value: any]; enter: []; blur: [] }>();
function onInput(event: Event) {
  const input = event.target as HTMLInputElement;
  emit('update:modelValue', props.type === 'number' ? (input.value === '' ? '' : Number(input.value)) : input.value);
}
</script>

<style scoped>
.ef-input {
  width: 100%; height: 32px; padding: 0 8px;
  border: 1px solid var(--ef-rule); border-radius: 0;
  background: var(--ef-panel); color: var(--ef-ink);
  font-family: var(--ef-font-ui); font-size: 13px; font-stretch: 112%;
  outline: none; transition: border-color .12s var(--ef-mech), background .12s var(--ef-mech);
}
.ef-input:hover { border-color: var(--ef-ink); background: var(--ef-panel); }
.ef-input:focus { border-color: var(--ef-ink); background: var(--ef-panel); box-shadow: inset 0 -2px 0 var(--ef-signal); }
.ef-input:disabled { border-color: var(--ef-rule); background: var(--ef-chip); color: var(--ef-ink-2); }
.ef-input::placeholder { color: var(--ef-ink-2); opacity: 1; }
textarea.ef-input { height: auto; min-height: 100px; padding: 6px 8px; line-height: 1.45; resize: vertical; }
</style>
