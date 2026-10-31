<template>
  <MelodyWindow :kind="kind" :title="title" show-title class="melody-modern-window" @close="$emit('close')">
    <div class="melody-modern-content"><slot /></div>
  </MelodyWindow>
</template>
<script setup lang="ts">
import MelodyWindow from './MelodyWindow.vue';
defineProps<{ kind: 'preferences' | 'games' | 'clients' | 'client'; title?: string }>();
defineEmits<{ close: [] }>();
</script>
