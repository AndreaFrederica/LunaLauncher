<template>
  <canvas
    v-show="loaded && !failed"
    ref="canvas"
    class="pcl-account-skin"
    width="64"
    height="64"
    :aria-label="name ? `${name} 的皮肤头像` : '皮肤头像'"
  />
  <span v-if="!loaded || failed" class="pcl-account-skin__fallback" aria-hidden="true">
    <PclIcon name="user" :size="18" />
  </span>
</template>

<script setup lang="ts">
import { onMounted, ref, watch } from 'vue';
import PclIcon from './PclIcon.vue';

const props = defineProps<{ src?: string; name?: string }>();
const canvas = ref<HTMLCanvasElement | null>(null);
const loaded = ref(false);
const failed = ref(false);
let loadToken = 0;

function draw(image: HTMLImageElement) {
  const context = canvas.value?.getContext('2d');
  if (!context) return;
  const width = image.naturalWidth || 64;
  const height = image.naturalHeight || 64;
  const scale = width / 64;
  const head = 8 * scale;
  context.imageSmoothingEnabled = false;
  context.clearRect(0, 0, 64, 64);
  context.drawImage(image, head, head, 8 * scale, 8 * scale, 0, 0, 64, 64);
  // Legacy skins are 64x32 and have no hat layer. Modern/HD skins are square.
  if (height >= width) {
    const hat = 40 * scale;
    context.drawImage(image, hat, head, 8 * scale, 8 * scale, 0, 0, 64, 64);
  }
}

function load() {
  const token = ++loadToken;
  loaded.value = false;
  failed.value = !props.src;
  if (!props.src) return;
  const image = new Image();
  // The canvas is only displayed, never exported, so it does not need CORS.
  // Loading without CORS also supports skin hosts that omit CORS headers.
  image.onload = () => {
    if (token !== loadToken) return;
    try {
      draw(image);
      loaded.value = true;
    } catch {
      failed.value = true;
    }
  };
  image.onerror = () => {
    if (token !== loadToken) return;
    failed.value = true;
  };
  image.src = String(props.src);
}

onMounted(load);
watch(() => props.src, load);
</script>

<style scoped>
.pcl-account-skin {
  display: block;
  width: 100%;
  height: 100%;
  border-radius: inherit;
  image-rendering: pixelated;
}

.pcl-account-skin__fallback {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 100%;
  height: 100%;
  color: var(--pcl-c3);
  background: var(--pcl-c6);
}
</style>
