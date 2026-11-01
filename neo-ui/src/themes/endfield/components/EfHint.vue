<template>
  <p class="ef-notice" v-bind="{ 'data-flag': flag }">
    <slot />
  </p>
</template>

<script setup lang="ts">
withDefaults(defineProps<{ flag?: string }>(), { flag: '注意' });
</script>
