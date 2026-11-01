<template>
  <div class="ef-progress">
    <div class="ef-progress__track">
      <div class="ef-progress__fill" :style="{ width: `${Math.max(0, Math.min(100, value))}%` }" />
    </div>
    <span v-if="showValue" class="ef-progress__value">{{ Math.round(value) }}%</span>
  </div>
</template>

<script setup lang="ts">
defineProps<{ value?: number; showValue?: boolean }>();
</script>

<style scoped>
.ef-progress { display: flex; align-items: center; gap: 8px; }
.ef-progress__track { flex: 1 1 auto; height: 6px; background: var(--ef-chip); border: 1px solid var(--ef-rule); overflow: hidden; }
.ef-progress__fill { height: 100%; background: var(--ef-signal); transition: width .2s var(--ef-mech); }
.ef-progress__value { flex: 0 0 auto; font-family: var(--ef-font-ui); font-size: 11px; font-stretch: 125%; font-weight: 700; color: var(--ef-ink-2); }
</style>
