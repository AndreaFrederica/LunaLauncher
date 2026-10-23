<template>
  <div class="pcl-resource-catalog-view">
    <div class="resource-module-head"><div><span class="pcl-kicker">PCL RESOURCE CATALOG</span><h2>{{ title }}</h2><p>{{ description }}</p></div><span class="material-icons resource-module-icon">{{ icon }}</span></div>
    <PclCatalogBrowser :instance="instance" :kind="kind" :minecraft-version="minecraftVersion" :loader="loader" :busy="busy" :execute="execute" :default-provider="provider" :tag="tag" @installed="emit('installed')"
      @install-start="emit('install-start', $event)"
      @install-progress="emit('install-progress', $event)"
      @install-done="emit('install-done', $event)" />
  </div>
</template>
<script setup lang="ts">
import { computed } from 'vue';
import PclCatalogBrowser from './PclCatalogBrowser.vue';
type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
const props = defineProps<{ kind: string; instance?: string; minecraftVersion?: string; loader?: string; busy: boolean; execute: Execute; provider?: string; tag?: string }>();
const emit = defineEmits<{ installed: []; 'install-start': [info: any]; 'install-progress': [task: any]; 'install-done': [info: any] }>();
const title = computed(() => ({ mods: '模组', datapacks: '数据包', resourcepacks: '资源包', shaderpacks: '光影', worlds: '世界' } as Record<string, string>)[props.kind] || props.kind);
const description = computed(() => ({ mods: '搜索、查看详情并安装模组，同时保留可选依赖。', datapacks: '为当前实例查找和安装数据包。', resourcepacks: '管理实例使用的资源包。', shaderpacks: '浏览并安装光影包。', worlds: '查找并安装世界存档。' } as Record<string, string>)[props.kind] || '搜索远程资源并安装到当前实例。');
const icon = computed(() => ({ mods: 'extension', datapacks: 'data_object', resourcepacks: 'texture', shaderpacks: 'auto_awesome', worlds: 'public' } as Record<string, string>)[props.kind] || 'inventory_2');
</script>
<style scoped>
.pcl-resource-catalog-view{color:#49677e}.resource-module-head{display:flex;align-items:flex-start;justify-content:space-between;gap:12px;margin-bottom:16px;padding-bottom:14px;border-bottom:1px solid #dce6ec}.resource-module-head h2{margin:4px 0;color:#315a79;font-size:20px}.resource-module-head p{margin:0;color:#82929f;font-size:12px}.resource-module-icon{font-size:30px;color:#6ba0c3}
</style>
