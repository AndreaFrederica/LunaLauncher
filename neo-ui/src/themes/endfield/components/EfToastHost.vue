<template>
  <Teleport to="body">
    <div class="ef-toast-host" aria-live="polite">
      <TransitionGroup name="ef-toast">
        <article v-for="toast in toasts" :key="toast.id" class="ef-toast" :class="`ef-toast--${toast.type}`" role="status" @click="dismiss(toast.id)">
          <div class="ef-toast__body">
            <strong>{{ toast.title }}</strong>
            <span>{{ toast.message }}</span>
          </div>
          <button class="ef-toast__close" type="button" aria-label="关闭" @click.stop="dismiss(toast.id)">
            <span class="material-icons">close</span>
          </button>
        </article>
      </TransitionGroup>
    </div>
  </Teleport>
</template>

<script setup lang="ts">
import { ref } from 'vue';
const toasts = ref<any[]>([]);
function dismiss(id: string) { toasts.value = toasts.value.filter(t => t.id !== id); }
defineExpose({ toasts, dismiss });
</script>

<style scoped>
.ef-toast-host { position: fixed; top: 56px; left: 50%; z-index: 7500; display: flex; flex-direction: column; align-items: center; gap: 8px; width: min(360px, calc(100vw - 32px)); transform: translateX(-50%); pointer-events: none; }
.ef-toast { position: relative; display: flex; align-items: flex-start; gap: 8px; padding: 10px 12px; border: 1px solid var(--ef-ink); border-left: 4px solid var(--ef-signal); border-radius: 0; background: var(--ef-panel); color: var(--ef-ink); box-shadow: none; pointer-events: auto; cursor: pointer; animation: ef-toast-in .18s var(--ef-snap) both; }
.ef-toast--error { border-left-color: var(--ef-danger); }
.ef-toast--success { border-left-color: var(--ef-ink); }
.ef-toast__body { flex: 1 1 auto; display: flex; flex-direction: column; gap: 2px; }
.ef-toast__body strong { font-family: var(--ef-font-ui); font-size: 12px; font-stretch: 125%; font-weight: 700; letter-spacing: .06em; text-transform: uppercase; }
.ef-toast__body span { font-size: 12px; color: var(--ef-ink-2); }
.ef-toast__close { border: 0; background: transparent; color: var(--ef-ink-2); cursor: pointer; padding: 0; }
.ef-toast__close .material-icons { font-size: 14px; }
@keyframes ef-toast-in { from { opacity: 0; transform: translateY(-8px); } to { opacity: 1; transform: translateY(0); } }
</style>
