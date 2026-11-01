<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <span class="ef-field">
    <span v-if="label" class="ef-field__label">{{ label }}</span>
    <strong>{{ value }}</strong>
  </span>
</template>

<script setup lang="ts">
defineProps<{ label?: string; value: string | number }>();
</script>

<style scoped>
.ef-field {
  display: inline-flex;
  align-items: baseline;
  gap: 6px;
  min-width: 0;
  padding-left: 8px;
  border-left: 1px solid var(--ef-rule);
  font-family: var(--ef-font-ui);
  font-size: 10px;
  font-variant-numeric: tabular-nums;
}
.ef-field__label {
  color: var(--ef-ink-3, #8a8a86);
  letter-spacing: .1em;
  text-transform: uppercase;
}
.ef-field strong {
  color: var(--ef-ink);
  font-size: 11px;
  font-weight: 600;
}
</style>
