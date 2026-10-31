<template>
  <MelodyWindow kind="download" @close="$emit('close')">
    <div class="melody-download">
      <div class="melody-download__line"><span>文件名: {{ fileName }}</span></div>
      <div class="melody-download__line"><span>文件大小: {{ fileSize }}</span><MelodyButton variant="link" @click="$emit('manual')">手动下载</MelodyButton></div>
      <div class="melody-download__speed">下载速度: {{ speed }}</div>
      <MelodyProgress :model-value="progress" label="下载进度" />
    </div>
  </MelodyWindow>
</template>
<script setup lang="ts">
import MelodyButton from '../../components/MelodyButton.vue';
import MelodyProgress from '../../components/MelodyProgress.vue';
import MelodyWindow from '../../components/MelodyWindow.vue';
withDefaults(defineProps<{ fileName?: string; fileSize?: string; speed?: string; progress?: number }>(), { fileName: '', fileSize: '', speed: '0 KB/s', progress: 0 });
defineEmits<{ close: []; manual: [] }>();
</script>
