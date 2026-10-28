<template>
  <aside class="pcl-page-nav">
    <template v-for="(group, groupIndex) in groups" :key="group.label || groupIndex">
      <div v-if="group.label" class="pcl-page-nav__group">{{ group.label }}</div>
      <button
        v-for="item in group.items"
        :key="item.id"
        type="button"
        class="pcl-page-nav__item"
        :class="{ active: modelValue === item.id }"
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
import type { PclNavGroup } from '../types';
import { useI18n } from 'vue-i18n';
const { t } = useI18n();

defineProps<{
  modelValue: string;
  groups: PclNavGroup[];
}>();

const emit = defineEmits<{
  'update:modelValue': [id: string];
  refresh: [id: string];
}>();
</script>
