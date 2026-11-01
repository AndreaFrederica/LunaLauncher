<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <div class="ef-stat" :class="{ alert }">
    <strong>{{ value }}<small v-if="unit">{{ unit }}</small></strong>
    <span>{{ label }}</span>
    <em v-if="note">{{ note }}</em>
  </div>
</template>

<script setup lang="ts">
defineProps<{ label: string; value: string | number; unit?: string; note?: string; alert?: boolean }>();
</script>

<style scoped>
.ef-stat {
  min-width: 110px;
  padding: 10px 12px 9px;
  border-left: 2px solid var(--ef-ink);
  background: var(--ef-panel);
}
.ef-stat strong {
  display: block;
  color: var(--ef-ink);
  font-family: var(--ef-font-ui);
  font-size: 26px;
  font-weight: 800;
  font-stretch: 112%;
  font-variant-numeric: tabular-nums;
  line-height: 1;
}
.ef-stat strong small {
  margin-left: 3px;
  font-size: 11px;
  font-weight: 600;
}
.ef-stat span {
  display: block;
  margin-top: 5px;
  color: var(--ef-ink-2);
  font-family: var(--ef-font-ui);
  font-size: 9px;
  letter-spacing: .12em;
  text-transform: uppercase;
}
.ef-stat em {
  display: block;
  margin-top: 3px;
  color: var(--ef-ink-3, #8a8a86);
  font-size: 10px;
  font-style: normal;
}
.ef-stat.alert { background: var(--ef-signal); }
.ef-stat.alert strong, .ef-stat.alert span, .ef-stat.alert em { color: var(--ef-on-signal); }
</style>
