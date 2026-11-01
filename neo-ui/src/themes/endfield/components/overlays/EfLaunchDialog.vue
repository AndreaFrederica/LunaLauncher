<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <div v-if="modelValue" class="ef-modal" role="dialog" aria-modal="true" aria-label="启动">
    <div class="ef-modal__panel">
      <header>
        <p class="ef-eyebrow">// LAUNCH</p>
        <h2>{{ title }}</h2>
      </header>
      <div class="ef-modal__body">
        <EfProgress :value="progress" show-value />
        <p class="ef-modal__stage">{{ stage || '准备启动' }}</p>
        <p v-if="error" class="ef-modal__error">{{ error }}</p>
      </div>
      <footer>
        <EfButton v-if="!error" variant="ghost" @click="emit('cancel')">取消</EfButton>
        <EfButton v-else variant="accent" @click="emit('close')">关闭</EfButton>
      </footer>
    </div>
  </div>
</template>

<script setup lang="ts">
import EfButton from '../EfButton.vue';
import EfProgress from '../EfProgress.vue';
withDefaults(defineProps<{ modelValue: boolean; title?: string; progress?: number; stage?: string; error?: string }>(), { title: '启动中', progress: 0, stage: '', error: '' });
const emit = defineEmits<{ cancel: []; close: [] }>();
</script>

<style scoped>
.ef-modal {
  position: fixed;
  inset: 0;
  z-index: 9200;
  display: grid;
  place-items: center;
  padding: 20px;
  background: rgba(0, 0, 0, .42);
}
.ef-modal__panel {
  width: min(420px, 92vw);
  background: var(--ef-paper);
  border: 1px solid var(--ef-ink);
  box-shadow: 8px 8px 0 rgba(0, 0, 0, .18);
  animation: ef-launch-in .16s var(--ef-snap) both;
}
@keyframes ef-launch-in { from { opacity: 0; transform: translateY(6px); } to { opacity: 1; transform: translateY(0); } }
.ef-modal__panel header {
  padding: 14px 16px 10px;
  border-bottom: 1px solid var(--ef-rule);
}
.ef-eyebrow {
  margin: 0 0 3px;
  color: var(--ef-ink-2);
  font-family: var(--ef-font-ui);
  font-size: 9px;
  letter-spacing: .16em;
  text-transform: uppercase;
}
.ef-modal__panel h2 {
  margin: 0;
  font-family: var(--ef-font-ui);
  font-size: 20px;
  font-weight: 800;
}
.ef-modal__body { padding: 16px; }
.ef-modal__stage {
  margin: 10px 0 0;
  color: var(--ef-ink-2);
  font-size: 11px;
  line-height: 1.5;
}
.ef-modal__error {
  margin: 8px 0 0;
  padding: 7px 9px;
  border-left: 2px solid var(--ef-signal);
  background: var(--ef-panel);
  color: var(--ef-ink);
  font-size: 11px;
}
.ef-modal__panel footer {
  display: flex;
  justify-content: flex-end;
  gap: 8px;
  padding: 10px 16px 14px;
}
</style>
