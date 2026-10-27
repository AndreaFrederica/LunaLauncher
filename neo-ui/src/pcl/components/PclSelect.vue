<template>
  <div ref="root" class="pcl-select">
    <button
      type="button"
      class="pcl-select__control"
      :class="{ open }"
      :disabled="disabled"
      :aria-label="ariaLabel || undefined"
      :aria-expanded="open"
      @click="toggle"
      @keydown.esc="open = false"
      @keydown.down.prevent="move(1)"
      @keydown.up.prevent="move(-1)"
      @keydown.enter.prevent="chooseActive"
    >
      <span class="pcl-select__value" :class="{ placeholder: !selectedOption }">{{ selectedOption?.label || effectivePlaceholder }}</span>
      <PclIcon name="chevron-left" :size="12" class="pcl-select__chevron" :class="{ open }" />
    </button>
    <Teleport to="body">
      <Transition name="pcl-select">
        <div v-if="open" ref="menu" class="pcl-select__menu" :style="menuStyle" role="listbox" @pointerdown.stop>
          <button
            v-for="(option, index) in normalizedOptions"
            :key="String(option.value)"
            type="button"
            class="pcl-select__option"
            :class="{ active: option.value === modelValue, disabled: option.disabled }"
            :disabled="option.disabled"
            role="option"
            :aria-selected="option.value === modelValue"
            @click="choose(option)"
            @mouseenter="activeIndex = index"
          >
            {{ option.label }}
          </button>
        </div>
      </Transition>
    </Teleport>
  </div>
</template>

<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import PclIcon from './PclIcon.vue';

type RawOption = string | number | { label: string; value: string | number; disabled?: boolean };
type Option = { label: string; value: string | number; disabled?: boolean };

const props = withDefaults(defineProps<{
  modelValue?: string | number | null;
  options?: RawOption[];
  placeholder?: string;
  disabled?: boolean;
  ariaLabel?: string;
}>(), {
  modelValue: '',
  options: () => [],
  placeholder: '',
  disabled: false,
  ariaLabel: '',
});

const emit = defineEmits<{ 'update:modelValue': [value: string | number] }>();
const { t } = useI18n();
const effectivePlaceholder = computed(() => props.placeholder || t('dialog.choose'));
const root = ref<HTMLElement | null>(null);
const menu = ref<HTMLElement | null>(null);
const open = ref(false);
const activeIndex = ref(-1);
const menuStyle = ref<Record<string, string>>({ position: 'fixed', left: '0px', top: '0px', width: '0px' });

const normalizedOptions = computed<Option[]>(() => props.options.map(option => {
  if (typeof option === 'string' || typeof option === 'number') return { label: String(option), value: option };
  return option;
}));
const selectedOption = computed(() => normalizedOptions.value.find(option => option.value === props.modelValue));

function toggle() {
  if (props.disabled) return;
  open.value = !open.value;
  if (open.value) activeIndex.value = Math.max(0, normalizedOptions.value.findIndex(option => option.value === props.modelValue));
}

function choose(option: Option) {
  if (option.disabled) return;
  emit('update:modelValue', option.value);
  open.value = false;
}

function move(step: number) {
  if (!open.value) {
    open.value = true;
    activeIndex.value = 0;
    return;
  }
  if (!normalizedOptions.value.length) return;
  activeIndex.value = (activeIndex.value + step + normalizedOptions.value.length) % normalizedOptions.value.length;
}

function chooseActive() {
  const option = normalizedOptions.value[activeIndex.value];
  if (option) choose(option);
}

function onPointerDown(event: PointerEvent) {
  const target = event.target as Node;
  if (!root.value?.contains(target) && !menu.value?.contains(target)) open.value = false;
}

function updatePosition() {
  const rect = root.value?.getBoundingClientRect();
  if (!rect) return;
  const count = normalizedOptions.value.length;
  const menuHeight = Math.min(220, count * 26 + 8);
  const openUp = window.innerHeight - rect.bottom < menuHeight + 8 && rect.top > menuHeight + 8;
  menuStyle.value = {
    position: 'fixed',
    left: `${rect.left}px`,
    top: `${openUp ? rect.top - menuHeight - 1 : rect.bottom + 1}px`,
    width: `${rect.width}px`,
  };
}

function bindPosition() {
  window.addEventListener('scroll', updatePosition, true);
  window.addEventListener('resize', updatePosition);
}

function unbindPosition() {
  window.removeEventListener('scroll', updatePosition, true);
  window.removeEventListener('resize', updatePosition);
}

watch(open, value => {
  if (value) {
    void nextTick(() => { updatePosition(); bindPosition(); });
  } else {
    unbindPosition();
  }
});

onMounted(() => document.addEventListener('pointerdown', onPointerDown));
onBeforeUnmount(() => {
  document.removeEventListener('pointerdown', onPointerDown);
  unbindPosition();
});
</script>

<style scoped>
.pcl-select__chevron {
  transform: rotate(-90deg);
  transition: transform .12s ease;
  opacity: .75;
}
.pcl-select__chevron.open { transform: rotate(90deg); }
.pcl-select-enter-active, .pcl-select-leave-active { transition: opacity .12s ease, transform .12s ease; }
.pcl-select-enter-from, .pcl-select-leave-to { opacity: 0; transform: translateY(-3px); }
</style>
