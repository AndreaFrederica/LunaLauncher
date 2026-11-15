<template>
<q-dialog v-model="confirmOpen">
      <q-card class="pcl-dialog pcl-resource-confirm">
        <q-card-section class="pcl-resource-detail__head">
          <div><div class="pcl-kicker">{{ t('pcl.resourceDownload.downloadConfirm') }}</div><h3>{{ t('pcl.resourceDownload.confirmCount', { count: confirmEntries.filter((entry:any) => entry.checked).length }) }}</h3><p>{{ t('pcl.resourceDownload.requiredDeps') }}</p></div>
          <PclIconButton icon="x" :label="t('window.close')" :size="28" @click="confirmOpen = false" />
        </q-card-section>
        <q-card-section class="pcl-resource-confirm__body">
          <PclHint theme="blue">{{ t('pcl.resourceDownload.confirmHint') }}</PclHint>
          <div class="pcl-resource-confirm__list">
            <label v-for="entry in confirmEntries" :key="`${entry.provider || provider}:${entry.projectId}`" class="pcl-resource-confirm__row">
              <PclCheckbox v-model="entry.checked" />
              <span><strong>{{ entry.name }}</strong><small>{{ entry.dependency ? t(entry.dependencyType === 'optional' ? 'pcl.resourceDownload.optionalBy' : 'pcl.resourceDownload.requiredBy', { name: entry.requiredBy || t('pcl.resourceDownload.selectedResource') }) : t('pcl.resourceDownload.selected') }}</small></span>
            </label>
          </div>
        </q-card-section>
        <q-card-actions align="right">
          <PclButton variant="ghost" @click="confirmOpen = false">{{ t('common.cancel') }}</PclButton>
          <PclButton variant="highlight" icon="download" :disabled="!confirmEntries.some((entry:any) => entry.checked && entry.versionId)" @click="confirmDownload">{{ t('pcl.resourceDownload.start') }}</PclButton>
        </q-card-actions>
      </q-card>
    </q-dialog>
</template>
<script setup lang="ts">
import { inject } from 'vue'; import { PclResourceDownloadContextKey } from './PclResourceDownloadContext';
import PclButton from '../../components/PclButton.vue'; import PclCheckbox from '../../components/PclCheckbox.vue'; import PclHint from '../../components/PclHint.vue'; import PclIconButton from '../../components/PclIconButton.vue';
const context = inject(PclResourceDownloadContextKey); if (!context) throw new Error('PclResourceDownloadContext is missing');
const { t, props, provider, providerLabel, detailOpen, detail, detailLoading, openExternal, confirmOpen, confirmEntries, confirmDownload, selectedList, closeWindow, kindLabel } = context;
</script>
<style>
.pcl-resource-confirm { width:620px; max-width:92vw; max-height:80vh; overflow:hidden; }
.pcl-resource-confirm__body { max-height:calc(80vh - 150px); overflow-y:auto; }
.pcl-resource-confirm__list { display:flex; flex-direction:column; gap:4px; margin-top:10px; }
.pcl-resource-confirm__row { display:flex; align-items:flex-start; gap:7px; padding:6px; border:1px solid var(--pcl-border); border-radius:3px; }
.pcl-resource-confirm__row > span { display:flex; flex-direction:column; min-width:0; }
.pcl-resource-confirm__row strong { overflow:hidden; color:var(--pcl-text); font-size:calc(10px * var(--pcl-font-scale, 1)); font-weight:500; text-overflow:ellipsis; white-space:nowrap; }
.pcl-resource-confirm__row small { color:var(--pcl-muted); font-size:calc(9px * var(--pcl-font-scale, 1)); }
</style>
