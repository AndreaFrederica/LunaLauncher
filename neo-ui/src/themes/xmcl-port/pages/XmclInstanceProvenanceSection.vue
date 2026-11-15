<template>
  <q-card flat class="workspace-card provenance-card">
    <q-card-section>
      <div class="card-heading"><span>来源与修改痕迹</span><q-badge v-if="provenance.mixed" color="warning" label="混合来源" /></div>
      <p class="provenance-hint">{{ provenance.mixed ? '检测到多个启动器或目录格式的修改痕迹。' : '以下结果来自目录结构和版本 JSON 证据。' }}</p>
      <div v-for="provider in (provenance.providers || [])" :key="provider.providerId" class="provenance-item">
        <div class="provenance-item__head"><strong>{{ provider.displayName || provider.providerId }}</strong><span>{{ provider.confidence }}%</span></div>
        <small>{{ [...(provider.matchedPaths || []), ...(provider.matchedFields || [])].join(', ') || '标准版本 JSON' }}</small>
      </div>
    </q-card-section>
  </q-card>
</template>
<script setup lang="ts">
import { computed } from 'vue';
const props = defineProps<{ instance: any }>();
const provenance = computed(() => props.instance?.provenance || {});
</script>
<style scoped>
.provenance-hint{margin:0 0 12px;color:rgba(255,255,255,.55)}.provenance-item{padding:10px 0;border-top:1px solid rgba(255,255,255,.1)}.provenance-item__head{display:flex;justify-content:space-between}.provenance-item__head span{color:#a5b4fc}.provenance-item small{display:block;margin-top:4px;color:rgba(255,255,255,.5)}
</style>
