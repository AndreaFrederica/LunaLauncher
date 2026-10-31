<template>
  <aside class="pcl-page-nav">
    <PclNavResizeHandle />
    <template v-for="(group, groupIndex) in groups" :key="group.label || groupIndex">
      <div v-if="group.label" class="pcl-page-nav__group">{{ group.label }}</div>
      <button
          v-for="(item, itemIndex) in group.items"
        :key="item.id"
        type="button"
        class="pcl-page-nav__item"
        :class="{ active: modelValue === item.id }"
          :style="{ '--pcl-nav-index': navItemIndex(groupIndex, itemIndex) }"
        @click="emit('update:modelValue', item.id)"
      >
        <PclIcon :name="item.icon" :size="18" />
        <span>{{ item.label }}</span>
        <span
          v-if="item.refresh"
          class="pcl-page-nav__refresh pcl-icon-btn"
          role="button"
          :tabindex="0"
          :title="t('common.refresh')"
          @click.stop="emit('refresh', item.id)"
          @keydown.enter.stop="emit('refresh', item.id)"
        >
          <PclIcon name="refresh-cw" :size="15" />
        </span>
      </button>
    </template>
  </aside>
</template>

<script setup lang="ts">
import PclIcon from './PclIcon.vue';
import PclNavResizeHandle from './PclNavResizeHandle.vue';
import type { PclNavGroup } from '../types';
import { useI18n } from 'vue-i18n';
const { t } = useI18n();

const props = defineProps<{
  modelValue: string;
  groups: PclNavGroup[];
}>();

function navItemIndex(groupIndex: number, itemIndex: number) {
  return props.groups.slice(0, groupIndex).reduce((total, group) => total + group.items.length, 0) + itemIndex;
}

const emit = defineEmits<{
  'update:modelValue': [id: string];
  refresh: [id: string];
}>();
</script>
