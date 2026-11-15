<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <aside class="ef-nav" :style="{ width: `${width}px` }" aria-label="分区导航">
    <div class="ef-nav__scroll">
      <section v-for="group in groups" :key="group.id" class="ef-nav__group">
        <h2>
          <small>// {{ group.code }}</small>
          {{ group.label }}
        </h2>
        <button
          v-for="item in group.items"
          :key="item.id"
          type="button"
          class="ef-nav__item"
          :class="{ current: page === item.id }"
          :aria-current="page === item.id ? 'page' : undefined"
          @click="emit('navigate', item.id)"
        >
          <span class="material-icons" aria-hidden="true">{{ item.icon }}</span>
          <span class="ef-nav__label">{{ item.label }}</span>
          <span v-if="item.badge !== undefined && item.badge !== ''" class="ef-nav__badge">{{ item.badge }}</span>
        </button>
      </section>
    </div>
    <div class="ef-nav__hint">
      <span>LunaLauncher</span>
      <small>// ENDFIELD</small>
    </div>
    <div class="ef-nav__resizer" @pointerdown="startResize" />
  </aside>
</template>

<script setup lang="ts">
import { ref } from 'vue';

export type EfNavItem = { id: string; label: string; icon: string; badge?: string | number };
export type EfNavGroup = { id: string; label: string; code: string; items: EfNavItem[] };

const props = withDefaults(defineProps<{ page: string; groups: EfNavGroup[]; width?: number }>(), { width: 214 });
const emit = defineEmits<{ navigate: [id: string]; 'update:width': [width: number] }>();

const width = ref(props.width);
let startX = 0;
let startWidth = 0;

function startResize(event: PointerEvent) {
  startX = event.clientX;
  startWidth = width.value;
  (event.target as HTMLElement).setPointerCapture?.(event.pointerId);
  window.addEventListener('pointermove', resize);
  window.addEventListener('pointerup', stopResize, { once: true });
}
function resize(event: PointerEvent) {
  const next = Math.max(160, Math.min(320, startWidth + event.clientX - startX));
  width.value = next;
  emit('update:width', next);
}
function stopResize() {
  window.removeEventListener('pointermove', resize);
}
</script>

<style scoped>
.ef-nav {
  position: relative;
  display: flex;
  flex-direction: column;
  flex: 0 0 auto;
  min-width: 160px;
  max-width: 320px;
  background: var(--ef-paper-2, #ecece9);
  border-right: 1px solid var(--ef-rule);
  color: var(--ef-ink);
  overflow: hidden;
}
.ef-nav__scroll {
  flex: 1;
  overflow: auto;
  padding: 12px 0;
}
.ef-nav__group { margin-bottom: 16px; }
.ef-nav__group h2 {
  display: flex;
  align-items: baseline;
  gap: 6px;
  margin: 0;
  padding: 0 14px 6px;
  color: var(--ef-ink-2);
  font-family: var(--ef-font-ui);
  font-size: 10px;
  font-weight: 600;
  letter-spacing: .14em;
  text-transform: uppercase;
}
.ef-nav__group h2 small {
  color: var(--ef-ink-3, #8a8a86);
  font-size: 8px;
  letter-spacing: .14em;
}
.ef-nav__item {
  position: relative;
  display: flex;
  align-items: center;
  gap: 9px;
  width: 100%;
  min-height: 32px;
  padding: 0 12px;
  border: 0;
  border-left: 2px solid transparent;
  background: transparent;
  color: var(--ef-ink-2);
  cursor: pointer;
  text-align: left;
  transition: background .12s var(--ef-mech), color .12s var(--ef-mech), border-color .12s var(--ef-mech), padding-left .12s var(--ef-mech);
}
.ef-nav__item .material-icons {
  font-size: 16px;
  opacity: .8;
}
.ef-nav__label {
  flex: 1;
  min-width: 0;
  overflow: hidden;
  font-family: var(--ef-font-ui);
  font-size: 12px;
  font-weight: 600;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.ef-nav__badge {
  min-width: 18px;
  padding: 0 4px;
  background: var(--ef-chip);
  color: var(--ef-ink-2);
  font-family: var(--ef-font-ui);
  font-size: 9px;
  line-height: 16px;
  text-align: center;
}
.ef-nav__item:hover {
  padding-left: 14px;
  border-left-color: var(--ef-signal);
  background: var(--ef-paper-3, #e2e2de);
  color: var(--ef-ink);
}
.ef-nav__item.current {
  border-left-color: var(--ef-ink);
  background: var(--ef-signal);
  color: var(--ef-on-signal);
}
.ef-nav__item.current .material-icons { opacity: 1; }
.ef-nav__item.current .ef-nav__badge {
  background: #161616;
  color: var(--ef-signal);
}
.ef-nav__hint {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  gap: 8px;
  padding: 8px 14px;
  border-top: 1px solid var(--ef-rule);
  color: var(--ef-ink-3, #8a8a86);
  font-family: var(--ef-font-ui);
  font-size: 9px;
  letter-spacing: .1em;
  text-transform: uppercase;
}
.ef-nav__hint small { font-size: 8px; letter-spacing: .14em; }
.ef-nav__resizer {
  position: absolute;
  top: 0;
  right: 0;
  bottom: 0;
  width: 5px;
  cursor: col-resize;
}
.ef-nav__resizer:hover { background: var(--ef-signal); }
@media (max-width: 900px) {
  .ef-nav { width: 54px !important; min-width: 54px; }
  .ef-nav__group h2, .ef-nav__label, .ef-nav__badge, .ef-nav__hint { display: none; }
  .ef-nav__item { justify-content: center; padding: 0; }
  .ef-nav__item:hover { padding-left: 0; }
}
</style>
