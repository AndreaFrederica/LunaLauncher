<template>
  <section
    class="pcl-card"
    :class="{
      'pcl-card--collapsible': collapsible,
      'pcl-card--untitled': !title && !$slots.title,
      'pcl-card--disabled': disabled,
    }"
  >
    <div v-if="title || $slots.title || collapsible" class="pcl-card__title" @click="toggle">
      <span class="pcl-card__title-text"><slot name="title">{{ title }}</slot></span>
      <span class="pcl-card__title-actions" @click.stop><slot name="actions" /></span>
      <span v-if="collapsible" class="pcl-card__chevron" :class="{ open: isOpen }" aria-hidden="true" />
    </div>

    <div v-if="collapsible" class="pcl-card__collapse" :class="{ open: isOpen }">
      <div class="pcl-card__collapse-inner">
        <div class="pcl-card__body"><slot /></div>
      </div>
    </div>
    <div v-else class="pcl-card__body"><slot /></div>
  </section>
</template>

<script setup lang="ts">
import { ref, watch } from 'vue';

const props = withDefaults(defineProps<{
  title?: string;
  collapsible?: boolean;
  open?: boolean;
  disabled?: boolean;
}>(), {
  title: '',
  collapsible: false,
  open: false,
  disabled: false,
});

const emit = defineEmits<{ 'update:open': [value: boolean] }>();
const isOpen = ref(props.open);

watch(() => props.open, value => { isOpen.value = value; });

function toggle() {
  if (!props.collapsible || props.disabled) return;
  isOpen.value = !isOpen.value;
  emit('update:open', isOpen.value);
}
</script>
