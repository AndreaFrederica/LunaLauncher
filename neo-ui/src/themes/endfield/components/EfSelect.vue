<template>
  <div class="ef-select">
    <button class="ef-select__control" :class="{ open }" :disabled="disabled" @click="toggle">
      <span class="ef-select__value" :class="{ placeholder: !selectedLabel }">{{ selectedLabel || placeholder }}</span>
      <span class="material-icons">expand_more</span>
    </button>
    <div v-if="open" class="ef-select__menu">
      <button v-for="opt in options" :key="String(opt.value)" class="ef-select__option" :class="{ active: opt.value === modelValue, disabled: opt.disabled }" :disabled="opt.disabled" @click="choose(opt.value)">
        {{ opt.label }}
      </button>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, ref, onMounted, onBeforeUnmount } from 'vue';
const props = withDefaults(defineProps<{ modelValue?: any; options?: { label: string; value: any; disabled?: boolean }[]; placeholder?: string; disabled?: boolean }>(), { options: () => [], placeholder: '', disabled: false });
const emit = defineEmits<{ 'update:modelValue': [value: any] }>();
const open = ref(false);
const selectedLabel = computed(() => props.options.find(o => o.value === props.modelValue)?.label);
function toggle() { if (!props.disabled) open.value = !open.value; }
function choose(v: any) { emit('update:modelValue', v); open.value = false; }
function onClickOutside(e: MouseEvent) { if (!(e.target as HTMLElement)?.closest?.('.ef-select')) open.value = false; }
onMounted(() => document.addEventListener('click', onClickOutside));
onBeforeUnmount(() => document.removeEventListener('click', onClickOutside));
</script>

<style scoped>
.ef-select { position: relative; min-width: 0; }
.ef-select__control { display: flex; align-items: center; justify-content: space-between; gap: 6px; width: 100%; height: 32px; padding: 0 8px; border: 1px solid var(--ef-rule); border-radius: 0; background: var(--ef-panel); color: var(--ef-ink); font-family: var(--ef-font-ui); font-size: 13px; font-stretch: 112%; cursor: pointer; transition: border-color .12s var(--ef-mech); }
.ef-select__control:hover { border-color: var(--ef-ink); }
.ef-select__control.open { border-color: var(--ef-ink); box-shadow: inset 0 -2px 0 var(--ef-signal); }
.ef-select__value.placeholder { color: var(--ef-ink-2); }
.ef-select__menu { position: absolute; z-index: 200; top: 100%; left: 0; right: 0; max-height: 220px; overflow: auto; border: 1px solid var(--ef-ink); border-radius: 0; background: var(--ef-panel); box-shadow: none; animation: ef-menu-in .12s var(--ef-snap) both; }
.ef-select__option { display: block; width: 100%; min-height: 28px; padding: 4px 8px; border: 0; background: transparent; color: var(--ef-ink); font-family: var(--ef-font-ui); font-size: 12px; text-align: left; cursor: pointer; }
.ef-select__option:hover, .ef-select__option.active { background: var(--ef-signal); color: var(--ef-on-signal); }
.ef-select__option.disabled { color: var(--ef-ink-2); cursor: default; }
.ef-select__control .material-icons { font-size: 16px; }
@keyframes ef-menu-in { from { opacity: 0; transform: translateY(-4px); } to { opacity: 1; transform: translateY(0); } }
</style>
