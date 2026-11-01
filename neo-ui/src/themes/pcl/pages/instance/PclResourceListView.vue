<template>
<div class="pcl-resource-table">
      <div class="pcl-resource-table__head">
        <span />
        <span>{{ t('resource.name') }}</span>
        <span>{{ t('resource.version') }}</span>
        <span>{{ t('resource.size') }}</span>
        <span>{{ t('resource.status') }}</span>
        <span />
      </div>
      <div
        v-for="item in filtered"
        :key="item.id || item.path || item.name"
        class="pcl-resource-row"
        :class="{ disabled: item.enabled === false, selected: selected.has(resourceRef(item)) }"
      >
        <PclCheckbox :model-value="selected.has(resourceRef(item))" @update:model-value="toggleSelected(item)" />
        <button class="pcl-resource-row__name" type="button" @click="inspect(item)">
          <span class="pcl-resource-row__icon">
            <img v-if="item.icon" :src="item.icon" alt="" />
            <PclIcon v-else :name="icon" :size="18" />
          </span>
          <span class="pcl-resource-row__copy">
            <strong>{{ item.name || item.fileName || item.path || t('resource.unnamed') }}</strong>
            <small v-if="item.provider || item.mcmod?.name">{{ item.provider || t('resource.localResource') }}<span v-if="item.mcmod?.name"> · {{ item.mcmod.name }}</span></small>
          </span>
        </button>
        <span class="pcl-resource-row__meta">{{ item.version || item.metadata?.version || '—' }}</span>
        <span class="pcl-resource-row__meta">{{ item.size ? formatSize(item.size) : '—' }}</span>
        <span class="pcl-resource-row__status" :class="{ off: item.enabled === false }">
          {{ item.enabled === false ? t('account.disabled') : t('resource.enabled') }}
        </span>
        <span class="pcl-resource-row__actions">
          <PclIconButton
            :icon="item.enabled === false ? 'circle-check' : 'power'"
            :label="item.enabled === false ? t('resource.enable') : t('resource.disable')"
            :size="24"
            :disabled="busy || localBusy"
            @click="setEnabled(item, item.enabled === false)"
          />
          <PclIconButton icon="trash-2" :label="t('resource.delete')" :size="24" danger :disabled="busy || localBusy" @click="remove(item)" />
        </span>
      </div>
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
