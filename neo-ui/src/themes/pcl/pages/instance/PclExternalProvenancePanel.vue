<template>
  <PclCard title="来源与修改痕迹">
    <p class="provenance-copy">{{ mixed ? '检测到多个启动器或目录格式的修改痕迹。所有来源证据会同时保留。' : '以下结果来自目录结构和版本 JSON 证据。' }}</p>
    <div v-for="provider in providers" :key="provider.providerId" class="provenance-row">
      <div class="provenance-row__title"><strong>{{ provider.displayName || provider.providerId }}</strong><span>{{ provider.confidence }}%</span></div>
      <div class="provenance-row__evidence">{{ evidence(provider).join(', ') || '标准版本 JSON' }}</div>
      <div class="provenance-row__capabilities">{{ capabilities(provider).join(' · ') || '仅检测' }}</div>
    </div>
    <div v-if="lunaChanges.actions.length" class="provenance-row luna-changes">
      <div class="provenance-row__title"><strong>Luna 修改记录</strong><span>{{ lunaChanges.substantial ? '涉及游戏文件' : '仅启动器设置' }}</span></div>
      <div class="provenance-row__evidence">{{ lunaChangeText }}</div>
      <div v-if="lunaChanges.loaderType" class="provenance-row__capabilities">加载器：{{ lunaChanges.loaderType }} {{ lunaChanges.loaderVersion || '' }} · {{ lunaChanges.loaderEnabled ? '已启用' : '已停用' }}</div>
    </div>
  </PclCard>
</template>

<script setup lang="ts">
import { computed } from 'vue';
import PclCard from '../../components/PclCard.vue';
const props = defineProps<{ instance: any }>();
const provenance = computed(() => props.instance?.provenance || {});
const providers = computed(() => Array.isArray(provenance.value.providers) ? provenance.value.providers : []);
const mixed = computed(() => Boolean(provenance.value.mixed));
const lunaChanges = computed(() => {
  const value = props.instance?.lunaChanges || {};
  return { ...value, actions: Array.isArray(value.actions) ? value.actions : [] };
});
const lunaChangeText = computed(() => lunaChanges.value.actions.map((action: string) => action === 'loader' ? '安装或管理 ModLoader' : action === 'settings' ? '保存 Luna 实例设置（名称、图标、备注或分组）' : action).join('；'));
function evidence(provider: any) { return [...(provider.matchedPaths || []), ...(provider.matchedFields || [])]; }
function capabilities(provider: any) { return [provider.canReadInstanceSettings ? '可读取设置' : '', provider.canEditCanonicalMetadata ? '可编辑规范元数据' : ''].filter(Boolean); }
</script>

<style scoped>
.provenance-copy{margin:0 0 12px;color:var(--pcl-muted);font-size:12px}.provenance-row{padding:10px 0;border-top:1px solid var(--pcl-border)}.provenance-row__title{display:flex;justify-content:space-between;color:var(--pcl-text)}.provenance-row__title span{color:var(--pcl-accent)}.provenance-row__evidence,.provenance-row__capabilities{margin-top:4px;color:var(--pcl-muted);font-size:11px}.luna-changes{border-top-color:var(--pcl-accent)}
</style>
