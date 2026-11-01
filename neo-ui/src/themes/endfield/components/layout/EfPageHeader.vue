<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <header class="ef-page-head">
    <div class="ef-page-head__copy">
      <p class="ef-eyebrow">// {{ eyebrow }}</p>
      <h1>{{ title }}</h1>
      <p v-if="description" class="ef-page-head__desc">{{ description }}</p>
    </div>
    <div v-if="$slots.default" class="ef-page-head__actions">
      <slot />
    </div>
  </header>
</template>

<script setup lang="ts">
defineProps<{ eyebrow: string; title: string; description?: string }>();
</script>

<style scoped>
.ef-page-head {
  display: flex;
  align-items: flex-end;
  justify-content: space-between;
  gap: 18px;
  padding-bottom: 14px;
  margin-bottom: 18px;
  border-bottom: 1px solid var(--ef-rule);
}
.ef-page-head__copy { min-width: 0; }
.ef-eyebrow {
  margin: 0 0 4px;
  color: var(--ef-ink-2);
  font-family: var(--ef-font-ui);
  font-size: 9px;
  font-weight: 600;
  letter-spacing: .16em;
  text-transform: uppercase;
}
.ef-page-head h1 {
  margin: 0;
  color: var(--ef-ink);
  font-family: var(--ef-font-ui);
  font-size: 26px;
  font-weight: 800;
  font-stretch: 112%;
  letter-spacing: -.01em;
}
.ef-page-head__desc {
  margin: 6px 0 0;
  max-width: 62ch;
  color: var(--ef-ink-2);
  font-size: 12px;
  line-height: 1.5;
}
.ef-page-head__actions {
  display: flex;
  gap: 8px;
  flex-wrap: wrap;
  justify-content: flex-end;
}
</style>
