<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <label class="ef-radio" :class="{ disabled }">
    <input type="radio" :checked="checked" :name="name" :value="value" :disabled="disabled" @change="emit('change', value)" />
    <span class="ef-radio__box" aria-hidden="true" />
    <span class="ef-radio__label"><slot>{{ label }}</slot></span>
  </label>
</template>

<script setup lang="ts">
withDefaults(defineProps<{ checked?: boolean; name?: string; value: string; label?: string; disabled?: boolean }>(), { checked: false, name: '', label: '', disabled: false });
const emit = defineEmits<{ change: [value: string] }>();
</script>

<style scoped>
.ef-radio {
  display: inline-flex;
  align-items: center;
  gap: 7px;
  cursor: pointer;
  font-size: 12px;
}
.ef-radio input { position: absolute; opacity: 0; pointer-events: none; }
.ef-radio__box {
  width: 14px;
  height: 14px;
  border: 1px solid var(--ef-ink);
  background: var(--ef-panel);
}
.ef-radio__box::after {
  content: '';
  display: block;
  width: 6px;
  height: 6px;
  margin: 3px;
  background: transparent;
}
.ef-radio input:checked + .ef-radio__box {
  background: var(--ef-signal);
}
.ef-radio input:checked + .ef-radio__box::after { background: var(--ef-on-signal); }
.ef-radio input:focus-visible + .ef-radio__box { outline: 2px solid var(--ef-ink); outline-offset: 1px; }
.ef-radio.disabled { opacity: .5; cursor: default; }
</style>
