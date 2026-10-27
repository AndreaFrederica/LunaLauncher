<template>
  <Teleport to="body">
    <div class="pcl-toast-host" aria-live="polite">
      <TransitionGroup name="pcl-toast">
        <article
          v-for="toast in pclToasts"
          :key="toast.id"
          class="pcl-toast"
          :class="`pcl-toast--${toast.type}`"
          role="status"
          @click="dismissPclToast(toast.id)"
        >
          <div class="pcl-toast__body">
            <strong>{{ toast.title }}</strong>
            <span>{{ toast.message }}</span>
          </div>
          <button class="pcl-toast__close" type="button" :aria-label="t('pcl.common.closeHint')" @click.stop="dismissPclToast(toast.id)">
            <PclIcon name="x" :size="13" />
          </button>
          <span
            v-if="toast.timeout > 0"
            class="pcl-toast__progress"
            :style="{ animationDuration: `${toast.timeout}ms` }"
          />
        </article>
      </TransitionGroup>
    </div>
  </Teleport>
</template>

<script setup lang="ts">
import { useI18n } from 'vue-i18n';
import PclIcon from './PclIcon.vue';
import { dismissPclToast, pclToasts } from '../composables/usePclToast';
const { t } = useI18n();
</script>

<style scoped>
.pcl-toast-host {
  position: fixed;
  top: 52px;
  left: 50%;
  z-index: 7500;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8px;
  width: min(320px, calc(100vw - 32px));
  transform: translateX(-50%);
  pointer-events: none;
}

.pcl-toast {
  position: relative;
  display: flex;
  align-items: flex-start;
  gap: 8px;
  overflow: hidden;
  padding: 10px 11px 12px;
  border: 1px solid var(--pcl-card-border, #38577e);
  border-left: 3px solid var(--pcl-c3, #2a80e3);
  border-radius: 5px;
  background: var(--pcl-c6, #18222f);
  color: var(--pcl-text, #e9f3ff);
  box-shadow: 0 4px 14px rgba(0, 0, 0, .35);
  pointer-events: auto;
  cursor: pointer;
}

.pcl-toast--success {
  border-left-color: #34d399;
}

.pcl-toast--error {
  border-left-color: #ff6b6b;
}

.pcl-toast--warning {
  border-left-color: #f5c542;
}

.pcl-toast__body {
  display: flex;
  flex-direction: column;
  gap: 2px;
  min-width: 0;
  flex: 1;
}

.pcl-toast__body strong {
  color: var(--pcl-text, #e9f3ff);
  font-size: calc(12px * var(--pcl-font-scale, 1));
  font-weight: 700;
}

.pcl-toast__body span {
  overflow-wrap: anywhere;
  color: var(--pcl-muted, #8a99a7);
  font-size: calc(11px * var(--pcl-font-scale, 1));
  line-height: 1.55;
}

.pcl-toast__close {
  display: flex;
  align-items: center;
  justify-content: center;
  flex: 0 0 20px;
  width: 20px;
  height: 20px;
  padding: 0;
  border: 0;
  border-radius: 3px;
  background: transparent;
  color: var(--pcl-muted, #8a99a7);
  cursor: pointer;
}

.pcl-toast__close:hover {
  background: var(--pcl-hover, rgba(255, 255, 255, .08));
  color: var(--pcl-text, #e9f3ff);
}

.pcl-toast__progress {
  position: absolute;
  left: 0;
  bottom: 0;
  width: 100%;
  height: 2px;
  background: var(--pcl-c3, #2a80e3);
  transform-origin: left;
  animation: pcl-toast-progress linear forwards;
}

.pcl-toast--success .pcl-toast__progress {
  background: #34d399;
}

.pcl-toast--error .pcl-toast__progress {
  background: #ff6b6b;
}

.pcl-toast--warning .pcl-toast__progress {
  background: #f5c542;
}

@keyframes pcl-toast-progress {
  from { transform: scaleX(1); }
  to { transform: scaleX(0); }
}

.pcl-toast-enter-active,
.pcl-toast-leave-active {
  transition: opacity .18s ease, transform .22s cubic-bezier(.16, 1.3, .3, 1);
}

.pcl-toast-enter-from,
.pcl-toast-leave-to {
  opacity: 0;
  transform: translateX(24px);
}
</style>
