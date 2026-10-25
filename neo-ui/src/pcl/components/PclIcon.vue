<template>
  <span
    class="pcl-icon"
    :style="{ width: cssSize, height: cssSize }"
    :aria-label="label || undefined"
    :aria-hidden="label ? undefined : 'true'"
    v-html="source"
  />
</template>

<script setup lang="ts">
import { computed } from 'vue';

// Lucide static icons copied from PCL-CE's icon pack (ISC, v1.17.0).
const sources = import.meta.glob('../icons/*.svg', {
  eager: true,
  query: '?raw',
  import: 'default',
}) as Record<string, string>;

const props = withDefaults(defineProps<{
  name: string;
  size?: number | string;
  label?: string;
}>(), {
  size: 18,
  label: '',
});

const cssSize = computed(() => typeof props.size === 'number' ? `${props.size}px` : props.size);
const source = computed(() => {
  const requested = sources[`../icons/${props.name}.svg`];
  // Dynamic icons can come from the launcher API (loader/component metadata).
  // Keep the control visible when an upstream name has no local SVG yet.
  return requested || sources['../icons/circle-help.svg'];
});
</script>

<style scoped>
.pcl-icon {
  display: inline-flex;
  flex: 0 0 auto;
  align-items: center;
  justify-content: center;
  line-height: 1;
  vertical-align: -.125em;
}

.pcl-icon :deep(svg) {
  width: 100%;
  height: 100%;
  display: block;
}
</style>
