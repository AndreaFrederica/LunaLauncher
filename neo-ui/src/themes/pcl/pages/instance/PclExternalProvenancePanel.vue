<template>
  <PclCard title="来源与修改痕迹">
    <p class="provenance-copy">{{ mixed ? '检测到多个启动器或目录格式的修改痕迹。所有来源证据会同时保留。' : '以下结果来自目录结构和版本 JSON 证据。' }}</p>
    <div v-for="provider in providers" :key="provider.providerId" class="provenance-row">
      <div class="provenance-row__title"><strong>{{ provider.displayName || provider.providerId }}</strong><span>{{ provider.confidence }}%</span></div>
      <div class="provenance-row__evidence">{{ evidence(provider).join(', ') || '标准版本 JSON' }}</div>
      <div class="provenance-row__capabilities">{{ capabilities(provider).join(' · ') || '仅检测' }}</div>
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
function evidence(provider: any) { return [...(provider.matchedPaths || []), ...(provider.matchedFields || [])]; }
function capabilities(provider: any) { return [provider.canReadInstanceSettings ? '可读取设置' : '', provider.canEditCanonicalMetadata ? '可编辑规范元数据' : ''].filter(Boolean); }
</script>

<style scoped>
.provenance-copy{margin:0 0 12px;color:var(--pcl-muted);font-size:12px}.provenance-row{padding:10px 0;border-top:1px solid var(--pcl-border)}.provenance-row__title{display:flex;justify-content:space-between;color:var(--pcl-text)}.provenance-row__title span{color:var(--pcl-accent)}.provenance-row__evidence,.provenance-row__capabilities{margin-top:4px;color:var(--pcl-muted);font-size:11px}
</style>
