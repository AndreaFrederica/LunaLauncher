<template>
  <span class="ef-tag" :class="{ 'is-current': current }">
    <slot />
  </span>
</template>

<script setup lang="ts">
defineProps<{ current?: boolean }>();
</script>
