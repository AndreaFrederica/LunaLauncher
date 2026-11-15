<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <button type="button" class="ef-icon-btn" :class="{ danger }" :disabled="disabled" :aria-label="label" :title="label">
    <span class="material-icons">{{ icon }}</span>
  </button>
</template>

<script setup lang="ts">
withDefaults(defineProps<{ icon: string; label: string; disabled?: boolean; danger?: boolean }>(), { disabled: false, danger: false });
</script>

<style scoped>
.ef-icon-btn {
  display: inline-grid;
  place-items: center;
  width: 28px;
  height: 28px;
  padding: 0;
  border: 1px solid transparent;
  border-radius: 0;
  background: transparent;
  color: var(--ef-ink-2);
  cursor: pointer;
  transition: background .12s var(--ef-mech), color .12s var(--ef-mech), border-color .12s var(--ef-mech);
}
.ef-icon-btn:hover {
  border-color: var(--ef-ink);
  background: var(--ef-signal);
  color: var(--ef-on-signal);
}
.ef-icon-btn.danger:hover {
  border-color: var(--ef-danger);
  background: var(--ef-danger);
  color: #fff;
}
.ef-icon-btn:disabled { opacity: .45; cursor: default; }
.ef-icon-btn .material-icons { font-size: 16px; }
</style>
