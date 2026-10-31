<template>
  <div
    class="pcl-nav-resize-handle"
    role="separator"
    aria-orientation="vertical"
    :aria-valuenow="preferences.pclPageNavWidth"
    aria-valuemin="140"
    aria-valuemax="360"
    tabindex="0"
    @pointerdown="startResize"
    @keydown.left.prevent="adjust(-4)"
    @keydown.right.prevent="adjust(4)"
  />
</template>

<script setup lang="ts">
import { neoUiPreferences, persistNeoUiPreferences } from '../../../neo-ui-preferences';

const preferences = neoUiPreferences;
const MIN_WIDTH = 140;
const MAX_WIDTH = 360;
let resizing = false;
let navLeft = 0;

function clamp(value: number) {
  return Math.max(MIN_WIDTH, Math.min(MAX_WIDTH, Math.round(value)));
}

function setPreviewWidth(value: number) {
  document.documentElement.style.setProperty('--pcl-page-nav-width', `${clamp(value)}px`);
}

function adjust(delta: number) {
  const next = clamp(preferences.pclPageNavWidth + delta);
  preferences.pclPageNavWidth = next;
  persistNeoUiPreferences();
}

function onPointerMove(event: PointerEvent) {
  if (!resizing) return;
  setPreviewWidth(event.clientX - navLeft);
}

function stopResize() {
  if (!resizing) return;
  resizing = false;
  const value = Number.parseFloat(getComputedStyle(document.documentElement).getPropertyValue('--pcl-page-nav-width'));
  if (Number.isFinite(value)) preferences.pclPageNavWidth = clamp(value);
  document.body.style.cursor = '';
  document.body.style.userSelect = '';
  window.removeEventListener('pointermove', onPointerMove);
  window.removeEventListener('pointerup', stopResize);
  window.removeEventListener('pointercancel', stopResize);
  persistNeoUiPreferences();
}

function startResize(event: PointerEvent) {
  if (event.button !== 0) return;
  const handle = event.currentTarget as HTMLElement;
  const nav = handle.parentElement;
  if (!nav) return;
  event.preventDefault();
  resizing = true;
  navLeft = nav.getBoundingClientRect().left;
  document.body.style.cursor = 'col-resize';
  document.body.style.userSelect = 'none';
  window.addEventListener('pointermove', onPointerMove);
  window.addEventListener('pointerup', stopResize, { once: true });
  window.addEventListener('pointercancel', stopResize, { once: true });
}
</script>

<style scoped>
.pcl-nav-resize-handle {
  position: absolute;
  z-index: 5;
  top: 0;
  right: 0;
  bottom: 0;
  width: 8px;
  cursor: col-resize;
  touch-action: none;
}

.pcl-nav-resize-handle:focus-visible,
.pcl-nav-resize-handle:hover {
  background: color-mix(in srgb, var(--pcl-c3) 35%, transparent);
}
</style>
