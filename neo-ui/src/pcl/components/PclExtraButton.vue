<template>
  <button
    type="button"
    class="pcl-extra-button"
    :style="{ width: `${size}px`, height: `${size}px` }"
    :title="label"
    :aria-label="label"
    :disabled="disabled"
    @click="emit('click', $event)"
  >
    <svg v-if="progress !== undefined" class="pcl-extra-button__ring" viewBox="0 0 40 40" aria-hidden="true">
      <circle class="pcl-extra-button__ring-track" cx="20" cy="20" r="18" />
      <circle
        class="pcl-extra-button__ring-value"
        cx="20"
        cy="20"
        r="18"
        :stroke-dasharray="`${Math.max(0, Math.min(1, progress)) * 113.1} 113.1`"
      />
    </svg>
    <PclIcon :name="icon" :size="Math.round(size * 0.42)" class="pcl-extra-button__icon" />
    <span v-if="badge" class="pcl-extra-button__badge">{{ badge }}</span>
  </button>
</template>

<script setup lang="ts">
import PclIcon from './PclIcon.vue';

withDefaults(defineProps<{
  icon: string;
  label: string;
  size?: number;
  progress?: number;
  badge?: string | number;
  disabled?: boolean;
}>(), {
  size: 40,
  progress: undefined,
  badge: '',
  disabled: false,
});

const emit = defineEmits<{ click: [event: MouseEvent] }>();
</script>

<style scoped>
.pcl-extra-button {
  position: relative;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  flex: 0 0 auto;
  padding: 0;
  border: 0;
  border-radius: 50%;
  background: var(--pcl-c3);
  color: #fff;
  cursor: pointer;
  box-shadow: 0 3px 10px rgba(0, 0, 0, .3);
  transition: transform .12s ease, background .12s ease, box-shadow .12s ease;
}

.pcl-extra-button:hover:not(:disabled) {
  background: var(--pcl-c4);
  transform: translateY(-2px);
  box-shadow: 0 5px 14px rgba(0, 0, 0, .38);
}

.pcl-extra-button:active:not(:disabled) {
  transform: scale(.94);
}

.pcl-extra-button:disabled {
  background: var(--pcl-c6);
  color: var(--pcl-muted);
  cursor: default;
  box-shadow: none;
}

.pcl-extra-button__icon {
  position: relative;
  z-index: 1;
}

.pcl-extra-button__ring {
  position: absolute;
  inset: 0;
  width: 100%;
  height: 100%;
  transform: rotate(-90deg);
  pointer-events: none;
}

.pcl-extra-button__ring-track {
  fill: none;
  stroke: rgba(255, 255, 255, .22);
  stroke-width: 2;
}

.pcl-extra-button__ring-value {
  fill: none;
  stroke: #fff;
  stroke-width: 2;
  stroke-linecap: round;
  transition: stroke-dasharray .3s ease;
}

.pcl-extra-button__badge {
  position: absolute;
  top: -3px;
  right: -3px;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  min-width: 14px;
  height: 14px;
  padding: 0 4px;
  border-radius: 7px;
  background: var(--pcl-red, #ce2111);
  color: #fff;
  font: 700 9px/1 var(--pcl-font);
}
</style>
