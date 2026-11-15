<template>
  <span class="ef-loading" :class="{ inline }" aria-label="加载中">
    <i /><i /><i />
  </span>
</template>

<script setup lang="ts">
defineProps<{ inline?: boolean }>();
</script>

<style scoped>
.ef-loading { display: inline-flex; align-items: center; gap: 4px; }
.ef-loading i { width: 6px; height: 6px; background: var(--ef-ink); animation: ef-blink 1s var(--ef-mech) infinite; }
.ef-loading i:nth-child(2) { animation-delay: .14s; }
.ef-loading i:nth-child(3) { animation-delay: .28s; }
.ef-loading.inline { gap: 3px; }
.ef-loading.inline i { width: 4px; height: 4px; }
@keyframes ef-blink { 0%, 80%, 100% { opacity: .25; transform: scale(.8); } 40% { opacity: 1; transform: scale(1); } }
</style>
