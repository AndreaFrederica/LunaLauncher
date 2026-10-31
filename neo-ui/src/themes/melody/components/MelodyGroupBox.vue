<template>
  <fieldset class="melody-group-box">
    <legend>{{ title }}</legend>
    <slot />
  </fieldset>
</template>

<script setup lang="ts">
defineProps<{ title: string }>();
</script>
