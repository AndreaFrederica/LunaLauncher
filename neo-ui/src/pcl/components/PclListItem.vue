<template>
  <div
    class="pcl-list-item"
    :class="{ selected, disabled }"
    :role="clickable ? 'button' : undefined"
    :tabindex="clickable && !disabled ? 0 : undefined"
    :aria-disabled="disabled || undefined"
    @click="onClick"
    @keydown.enter="onClick"
    @keydown.space.prevent="onClick"
  >
    <span v-if="icon || $slots.icon" class="pcl-list-item__icon">
      <slot name="icon"><PclIcon :name="icon" :size="20" /></slot>
    </span>
    <span class="pcl-list-item__copy">
      <span class="pcl-list-item__title"><slot name="title">{{ title }}</slot></span>
      <span v-if="info || $slots.info" class="pcl-list-item__info"><slot name="info">{{ info }}</slot></span>
      <span v-if="tags?.length" class="pcl-list-item__tags">
        <span v-for="tag in tags" :key="tag" class="pcl-tag">{{ tag }}</span>
      </span>
    </span>
    <span v-if="$slots.actions" class="pcl-list-item__actions" @click.stop><slot name="actions" /></span>
  </div>
</template>

<script setup lang="ts">
import PclIcon from './PclIcon.vue';

const props = withDefaults(defineProps<{
  title?: string;
  info?: string;
  icon?: string;
  tags?: string[];
  selected?: boolean;
  disabled?: boolean;
  clickable?: boolean;
}>(), {
  title: '',
  info: '',
  icon: '',
  tags: () => [],
  selected: false,
  disabled: false,
  clickable: true,
});

const emit = defineEmits<{ click: [event: MouseEvent | KeyboardEvent] }>();

function onClick(event: MouseEvent | KeyboardEvent) {
  if (!props.clickable || props.disabled) return;
  emit('click', event);
}
</script>

<style scoped>
.pcl-list-item.disabled {
  opacity: .45;
  cursor: default;
}
.pcl-list-item__tags {
  display: flex;
  flex-wrap: wrap;
  gap: 3px;
  margin-top: 2px;
}
</style>
