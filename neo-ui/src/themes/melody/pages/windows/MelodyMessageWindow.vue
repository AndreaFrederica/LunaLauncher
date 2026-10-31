<template>
  <MelodyDialog :title="title" :width="width" @close="$emit('close')">
    <p class="melody-message__text">{{ message }}</p>
    <template #actions><MelodyButton variant="dialog" @click="$emit('close')">确认</MelodyButton></template>
  </MelodyDialog>
</template>
<script setup lang="ts">
import { computed } from 'vue';
import MelodyButton from '../../components/MelodyButton.vue';
import MelodyDialog from '../../components/MelodyDialog.vue';
import { messageWidth } from '../../services/message-layout';
const props = withDefaults(defineProps<{ title?: string; message: string }>(), { title: '提示:' });
const width = computed(() => messageWidth(props.message, props.title));
defineEmits<{ close: [] }>();
</script>
