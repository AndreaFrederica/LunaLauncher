<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <label class="ef-search">
    <span class="material-icons" aria-hidden="true">search</span>
    <input
      :value="modelValue"
      type="search"
      :placeholder="placeholder"
      :disabled="disabled"
      spellcheck="false"
      @input="emit('update:modelValue', ($event.target as HTMLInputElement).value)"
      @keydown.enter="emit('enter')"
    />
    <button v-if="modelValue" type="button" aria-label="清除" title="清除" @click="emit('update:modelValue', '')">
      <span class="material-icons">close</span>
    </button>
  </label>
</template>

<script setup lang="ts">
withDefaults(defineProps<{ modelValue: string; placeholder?: string; disabled?: boolean }>(), { placeholder: '搜索', disabled: false });
const emit = defineEmits<{ 'update:modelValue': [value: string]; enter: [] }>();
</script>

<style scoped>
.ef-search {
  display: flex;
  align-items: center;
  gap: 6px;
  height: 30px;
  padding: 0 8px;
  border: 1px solid var(--ef-rule);
  background: var(--ef-panel);
}
.ef-search:focus-within {
  border-color: var(--ef-ink);
  box-shadow: inset 0 -2px 0 var(--ef-signal);
}
.ef-search .material-icons {
  font-size: 15px;
  color: var(--ef-ink-2);
}
.ef-search input {
  flex: 1;
  min-width: 0;
  border: 0;
  background: transparent;
  color: var(--ef-ink);
  font-family: var(--ef-font-ui);
  font-size: 12px;
  outline: none;
}
.ef-search input::placeholder { color: var(--ef-ink-3, #8a8a86); }
.ef-search button {
  display: grid;
  place-items: center;
  width: 20px;
  height: 20px;
  padding: 0;
  border: 0;
  background: transparent;
  color: var(--ef-ink-2);
  cursor: pointer;
}
.ef-search button:hover { color: var(--ef-ink); }
.ef-search button .material-icons { font-size: 14px; }
</style>
