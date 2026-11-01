<template>
  <div class="pcl-result-list">
    <template v-for="item in items" :key="item.projectId">
    <button
      class="pcl-result-row"
      type="button"
      :disabled="busy"
      @click="emit('select', item)"
    >
      <span v-if="selectable" class="pcl-result-check" @click.stop>
        <PclCheckbox :model-value="Boolean(selected?.[item.projectId])" :disabled="busy" @update:model-value="emit('toggle', item, $event)" />
      </span>
      <img v-if="item.iconUrl" :src="item.iconUrl" alt="" />
      <span v-else class="pcl-result-icon"><PclIcon name="box" :size="24" /></span>
      <span class="pcl-result-copy">
        <strong>{{ item.name }}</strong>
        <span v-if="item.mcmod?.name || item.mcmod?.url || item.mcmod?.searchUrl" class="pcl-result-mcmod">
          <span v-if="item.mcmod?.name">{{ item.mcmod.name }}</span>
          <span v-if="item.mcmod?.url || item.mcmod?.searchUrl" class="pcl-result-mcmod__link" role="link" tabindex="0" @click.stop="openMcMod(item.mcmod.url || item.mcmod.searchUrl)" @keydown.enter.stop="openMcMod(item.mcmod.url || item.mcmod.searchUrl)">{{ t('resource.mcmodLink') }}</span>
        </span>
        <small>{{ item.description || fallbackDescription }}</small>
        <em>{{ providerLabel(item.provider || provider) }} · {{ item.projectId }}</em>
      </span>
      <PclIcon name="chevron-right" :size="16" />
    </button>
    <slot name="expanded" :item="item" />
    </template>
    <div v-if="searched && !items.length && !busy" class="pcl-catalog-empty">
      <PclIcon name="search" :size="30" />{{ emptyText }}
    </div>
  </div>
</template>

<script setup lang="ts">
import { useI18n } from 'vue-i18n';
import PclIcon from '../../components/PclIcon.vue';
import PclCheckbox from '../../components/PclCheckbox.vue';

const { t } = useI18n();

defineProps<{
  items: any[];
  busy: boolean;
  searched: boolean;
  provider: string;
  providerLabel: (value: string) => string;
  fallbackDescription: string;
  emptyText: string;
  selectable?: boolean;
  selected?: Record<string, any>;
}>();

const emit = defineEmits<{ select: [item: any]; toggle: [item: any, value: boolean] }>();

function openMcMod(url: string) {
  window.open(url, '_blank', 'noopener,noreferrer');
}
</script>

<style scoped>
.pcl-result-list {
  display: flex;
  flex-direction: column;
  border-top: 1px solid var(--pcl-border);
}

.pcl-result-row {
  display: flex;
  align-items: center;
  gap: 11px;
  min-height: 64px;
  padding: 9px 6px;
  border: 0;
  border-bottom: 1px solid var(--pcl-border);
  background: transparent;
  color: var(--pcl-text);
  text-align: left;
  cursor: pointer;
}

.pcl-result-row:hover { background: var(--pcl-hover); }
.pcl-result-row:disabled { cursor: default; opacity: .65; }
.pcl-result-check { display:flex; flex:0 0 auto; align-items:center; }

.pcl-catalog-inline-project {
  margin: 8px 0 12px;
}

.pcl-result-row > img,
.pcl-result-icon {
  display: flex;
  align-items: center;
  justify-content: center;
  flex: 0 0 46px;
  width: 46px;
  height: 46px;
  border-radius: 5px;
  background: var(--pcl-c6);
  color: var(--pcl-c2);
  object-fit: cover;
}

.pcl-result-copy {
  display: flex;
  flex-direction: column;
  min-width: 0;
  flex: 1;
}

.pcl-result-copy strong { color: var(--pcl-text); font-size: calc(12px * var(--pcl-font-scale, 1)); font-weight: 500; }
.pcl-result-copy small,
.pcl-result-copy em { overflow: hidden; color: var(--pcl-muted); font-size: calc(10px * var(--pcl-font-scale, 1)); font-style: normal; text-overflow: ellipsis; white-space: nowrap; }
.pcl-result-copy small { margin-top: 2px; }
.pcl-result-copy em { margin-top: 3px; color: var(--pcl-c5); font-size: calc(9px * var(--pcl-font-scale, 1)); }
.pcl-result-mcmod { display: flex; align-items: center; gap: 7px; margin-top: 2px; color: var(--pcl-muted); font-size: calc(9px * var(--pcl-font-scale, 1)); }
.pcl-result-mcmod__link { color: var(--pcl-c3); cursor: pointer; }

.pcl-catalog-empty { display: flex; flex-direction: column; align-items: center; gap: 7px; padding: 45px 20px; color: var(--pcl-muted); font-size: calc(11px * var(--pcl-font-scale, 1)); }
.pcl-catalog-empty :deep(svg) { color: var(--pcl-c5); }
</style>
