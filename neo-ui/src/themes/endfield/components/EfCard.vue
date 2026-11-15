<template>
  <section class="ef-panel" :class="{ 'cut-br': cut }">
    <h2 v-if="title || $slots.title">
      <slot name="title">{{ title }}</slot>
      <slot name="actions" />
    </h2>
    <slot />
  </section>
</template>

<script setup lang="ts">
defineProps<{ title?: string; cut?: boolean }>();
</script>

<style scoped>
.ef-panel { margin-bottom: 1rem; }
</style>
