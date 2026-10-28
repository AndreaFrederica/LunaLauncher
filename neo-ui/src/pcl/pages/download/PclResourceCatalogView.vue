<template>
  <div class="pcl-resource-catalog-view">
    <div class="resource-module-head"><div><span class="pcl-kicker">{{ t('ui.resourceCatalog') }}</span><h2>{{ title }}</h2><p>{{ description }}</p></div><span class="material-icons resource-module-icon">{{ icon }}</span></div>
    <PclCatalogBrowser :instance="instance" :kind="kind" :minecraft-version="minecraftVersion" :loader="loader" :busy="busy" :execute="execute" :default-provider="provider" :tag="tag" :auto-refresh="autoRefresh" :refresh-token="refreshToken" @installed="emit('installed')"
      @install-start="emit('install-start', $event)"
      @install-progress="emit('install-progress', $event)"
      @install-done="emit('install-done', $event)" />
  </div>
</template>
<script setup lang="ts">
import { computed } from 'vue';
import { useI18n } from 'vue-i18n';
import PclCatalogBrowser from './PclCatalogBrowser.vue';
type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
const props = defineProps<{ kind: string; instance?: string; minecraftVersion?: string; loader?: string; busy: boolean; execute: Execute; provider?: string; tag?: string; autoRefresh?: boolean; refreshToken?: number }>();
const { t } = useI18n();
const emit = defineEmits<{ installed: []; 'install-start': [info: any]; 'install-progress': [task: any]; 'install-done': [info: any] }>();
const title = computed(() => t(`resource.kinds.${props.kind}`));
const description = computed(() => t(`resource.catalog.${props.kind}`));
const icon = computed(() => ({ mods: 'extension', datapacks: 'data_object', resourcepacks: 'texture', shaderpacks: 'auto_awesome', worlds: 'public' } as Record<string, string>)[props.kind] || 'inventory_2');
const autoRefresh = computed(() => props.autoRefresh !== false);
</script>
<style scoped>
.pcl-resource-catalog-view{color:#49677e}.resource-module-head{display:flex;align-items:flex-start;justify-content:space-between;gap:12px;margin-bottom:16px;padding-bottom:14px;border-bottom:1px solid #dce6ec}.resource-module-head h2{margin:4px 0;color:#315a79;font-size: calc(20px * var(--pcl-font-scale, 1))}.resource-module-head p{margin:0;color:#82929f;font-size: calc(12px * var(--pcl-font-scale, 1))}.resource-module-icon{font-size: calc(30px * var(--pcl-font-scale, 1));color:#6ba0c3}
</style>
