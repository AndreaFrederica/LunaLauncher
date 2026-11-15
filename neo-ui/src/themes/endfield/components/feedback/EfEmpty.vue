<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <div class="ef-empty">
    <span class="material-icons" aria-hidden="true">{{ icon }}</span>
    <strong>{{ title }}</strong>
    <p>{{ description }}</p>
    <slot />
  </div>
</template>

<script setup lang="ts">
withDefaults(defineProps<{ icon?: string; title: string; description?: string }>(), { icon: 'inbox', description: '' });
</script>

<style scoped>
.ef-empty {
  display: grid;
  place-items: center;
  gap: 6px;
  padding: 30px 20px;
  border: 1px dashed var(--ef-rule);
  background: var(--ef-hatch);
  text-align: center;
}
.ef-empty .material-icons {
  font-size: 24px;
  color: var(--ef-ink-2);
}
.ef-empty strong {
  font-family: var(--ef-font-ui);
  font-size: 13px;
  font-weight: 700;
}
.ef-empty p {
  margin: 0 0 8px;
  max-width: 44ch;
  color: var(--ef-ink-2);
  font-size: 11px;
  line-height: 1.5;
}
</style>
