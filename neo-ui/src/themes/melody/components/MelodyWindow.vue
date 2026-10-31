<template>
  <section ref="element" class="melody-window" :class="[`melody-window--${kind}`, { 'has-title': showTitle, 'is-native': context?.native }]"
    :style="style" :data-window="context?.entry.value.id || kind" role="dialog" :aria-label="title || spec.title"
    :aria-modal="kind === 'message' || undefined" tabindex="-1" @pointerdown="context?.activate()" @keydown="keyDown">
    <header class="melody-window__titlebar" @pointerdown="startDrag">
      <span v-if="showTitle">{{ title || spec.title }}</span>
      <button class="melody-window__close" type="button" aria-label="关闭" @pointerdown.stop @click="$emit('close')">×</button>
    </header>
    <div class="melody-window__body"><slot /></div>
  </section>
</template>
<script setup lang="ts">
import { computed, inject, onMounted, ref } from 'vue';
import { getCurrentWindow } from '@tauri-apps/api/window';
import { melodyWindowKey } from '../services/window-context';
import { windows, type MelodyWindowKind } from '../types';
const props = withDefaults(defineProps<{ kind: MelodyWindowKind; title?: string; showTitle?: boolean; width?: number }>(), { showTitle: false });
const emit = defineEmits<{ close: [] }>();
const context = inject(melodyWindowKey, undefined);
const element = ref<HTMLElement>();
const spec = computed(() => windows[props.kind]);
const style = computed(() => {
  // Native Tauri windows own their size. The document must fill the actual
  // client area instead of applying the browser preview's design dimensions.
  if (context?.native) return { zIndex: context.entry.value.z || 1 };
  return {
    width: `${props.width ?? context?.entry.value.width ?? spec.value.width}px`,
    height: `${spec.value.height}px`,
    left: `${context?.entry.value.x || 0}px`,
    top: `${context?.entry.value.y || 0}px`,
    zIndex: context?.entry.value.z || 1,
  };
});
onMounted(() => element.value?.focus({ preventScroll: true }));
async function startDrag(event: PointerEvent) {
  if (event.button !== 0 || !context) return;
  context.activate();
  if (context.native) { await getCurrentWindow().startDragging(); return; }
  const target = event.currentTarget as HTMLElement;
  const startX = event.clientX, startY = event.clientY;
  const { x, y } = context.entry.value;
  target.setPointerCapture(event.pointerId);
  const move = (e: PointerEvent) => context.move(x + e.clientX - startX, y + e.clientY - startY);
  const stop = () => { target.removeEventListener('pointermove', move); target.removeEventListener('lostpointercapture', stop); };
  target.addEventListener('pointermove', move);
  target.addEventListener('lostpointercapture', stop);
}
function keyDown(event: KeyboardEvent) {
  if (event.key === 'Escape' && props.kind !== 'launcher') { event.stopPropagation(); emit('close'); }
  if (event.key !== 'Tab') return;
  const controls = [...(element.value?.querySelectorAll<HTMLElement>('button:not(:disabled),input:not(:disabled),select:not(:disabled),textarea:not(:disabled),a[href],[tabindex="0"]') || [])].filter(el => el.offsetParent !== null && !el.closest('[inert]'));
  if (!controls.length) { event.preventDefault(); return; }
  const index = controls.indexOf(document.activeElement as HTMLElement);
  if (event.shiftKey && index <= 0) { event.preventDefault(); controls[controls.length - 1]?.focus(); }
  else if (!event.shiftKey && (index === -1 || index === controls.length - 1)) { event.preventDefault(); controls[0]?.focus(); }
}
</script>
