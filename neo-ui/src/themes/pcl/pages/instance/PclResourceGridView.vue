<template>
<div class="pcl-resource-grid">
      <article
        v-for="item in filtered"
        :key="item.id || item.path || item.name"
        class="pcl-resource-card"
        :class="{ disabled: item.enabled === false, selected: selected.has(resourceRef(item)) }"
      >
        <PclCheckbox
          class="pcl-resource-check"
          :model-value="selected.has(resourceRef(item))"
          @update:model-value="toggleSelected(item)"
        />
        <button class="pcl-resource-icon" :title="t('resource.inspect', { name: item.name || item.fileName || item.path })" @click="inspect(item)">
          <img v-if="item.icon" :src="item.icon" alt="" />
          <PclIcon v-else :name="icon" :size="24" />
        </button>
        <div class="pcl-resource-copy" @click="inspect(item)">
          <strong>{{ item.name || item.fileName || item.path || t('resource.unnamed') }}</strong>
          <small v-if="item.version || item.metadata?.version">{{ item.version || item.metadata?.version }}</small>
          <small>{{ item.enabled === false ? t('resource.disabled') : item.size ? formatSize(item.size) : statusLabel(item) }}</small>
        </div>
        <div class="pcl-resource-actions">
          <PclIconButton
            :icon="item.enabled === false ? 'circle-check' : 'power'"
            :label="item.enabled === false ? t('resource.enable') : t('resource.disable')"
            :size="26"
            :disabled="busy || localBusy"
            @click="setEnabled(item, item.enabled === false)"
          />
          <PclIconButton icon="trash-2" :label="t('resource.delete')" :size="26" danger :disabled="busy || localBusy" @click="remove(item)" />
        </div>
      </article>

      <div v-if="!filtered.length && !loading" class="pcl-empty pcl-resource-empty">
        <PclIcon name="package" :size="38" />
        <strong>{{ query ? t('resource.noMatch') : t('resource.noResource') }}</strong>
        <span>{{ query ? t('resource.adjustFilter') : t('resource.installHint') }}</span>
      </div>
    </div>
</template>
<script setup lang="ts">
import { inject } from 'vue'; import { PclResourceContextKey } from './PclResourceContext';
import PclButton from '../../components/PclButton.vue'; import PclCheckbox from '../../components/PclCheckbox.vue'; import PclIcon from '../../components/PclIcon.vue'; import PclIconButton from '../../components/PclIconButton.vue';
const context = inject(PclResourceContextKey); if (!context) throw new Error('PclResourceContext is missing');
const { t, props, busy, viewMode, filtered, selected, selectedCount, query, loading, localBusy, icon, resourceRef, formatSize, statusLabel, toggleSelected, inspect, setEnabled, remove, clearSelected, setSelected, removeSelected } = context;
</script>
