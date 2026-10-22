<template>
  <div class="pcl-hint" :class="`pcl-hint--${theme}`" role="note">
    <slot>{{ text }}</slot>
    <button v-if="closable" type="button" class="pcl-hint__close" aria-label="关闭提示" @click="emit('close')">
      <PclIcon name="x" :size="14" />
    </button>
  </div>
</template>

<script setup lang="ts">
import PclIcon from './PclIcon.vue';

withDefaults(defineProps<{
  text?: string;
  theme?: 'blue' | 'red' | 'yellow';
  closable?: boolean;
}>(), {
  text: '',
  theme: 'red',
  closable: false,
});

const emit = defineEmits<{ close: [] }>();
</script>
