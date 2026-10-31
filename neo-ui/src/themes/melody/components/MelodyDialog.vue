<!-- Shared caption, content surface and action strip for legacy dialogs. -->
<template>
  <MelodyWindow kind="message" :title="title" :width="width" show-title @close="$emit('close')">
    <div class="melody-dialog">
      <div class="melody-dialog__content" tabindex="0" role="region" aria-label="对话框内容"><slot /></div>
      <footer class="melody-dialog__actions"><slot name="actions" /></footer>
    </div>
  </MelodyWindow>
</template>
<script setup lang="ts">
import MelodyWindow from './MelodyWindow.vue';
withDefaults(defineProps<{ title?: string; width?: number }>(), { title: '提示:' });
defineEmits<{ close: [] }>();
</script>
