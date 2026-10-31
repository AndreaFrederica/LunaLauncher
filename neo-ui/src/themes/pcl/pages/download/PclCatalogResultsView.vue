<template>
      <PclCatalogResultList
        :items="results"
        :busy="busy || searching || cancelling"
        :searched="searched"
        :provider="provider"
        :provider-label="providerLabel"
        :fallback-description="t('pcl.auto2.k4b64e784af')"
        :empty-text="t('pcl.auto2.kfd34ebffa6')"
        :selectable="!packs"
        :selected="selectedProjects"
        @select="select"
        @toggle="toggleSelected"
      >
        <template #expanded="{ item }">
          <div v-if="project?.projectId === item.projectId" class="pcl-catalog-inline-project">
            <PclCatalogProjectView />
          </div>
        </template>
      </PclCatalogResultList>
      <div v-if="searched" class="pcl-pagination">
        <PclButton size="sm" :disabled="busy || cancelling || offset === 0" @click="search(Math.max(0, offset - 25))">{{ t('pcl.auto2.kb41561d807') }}</PclButton>
        <span>{{ t('pcl.catalog.pageInfo', { page: Math.floor(offset / 25) + 1 }) }}</span>
        <PclButton size="sm" :disabled="busy || cancelling || !results.length" @click="search(offset + 25)">{{ t('pcl.auto2.k67a246a344') }}</PclButton>
      </div>
      <div v-if="selectedProjectsList.length" class="pcl-catalog-batch-bar">
        <span>{{ t('resource.selected', { count: selectedProjectsList.length }) }}</span>
        <PclButton size="sm" variant="highlight" icon="download" :disabled="busy || selectedProjectsList.some((entry:any) => entry.loadingVersions || !entry.versionId)" @click="prepareBatchDownload">
          {{ t('pcl.resourceDownload.checkDownload') }}
        </PclButton>
        <PclButton size="sm" variant="ghost" @click="clearSelectedProjects">{{ t('resource.clearSelection') }}</PclButton>
      </div>
      <q-dialog v-model="batchConfirmOpen">
        <q-card class="pcl-dialog pcl-batch-confirm">
          <q-card-section class="pcl-resource-detail__head">
            <div><div class="pcl-kicker">{{ t('pcl.resourceDownload.downloadConfirm') }}</div><h3>{{ t('pcl.resourceDownload.confirmCount', { count: selectedProjectsList.filter((entry:any) => entry.checked).length }) }}</h3><p>{{ t('pcl.resourceDownload.requiredDeps') }}</p></div>
            <PclIconButton icon="x" :label="t('window.close')" :size="28" @click="batchConfirmOpen = false" />
          </q-card-section>
          <q-card-section class="pcl-batch-confirm__body">
            <PclHint theme="blue">{{ t('pcl.resourceDownload.confirmHint') }}</PclHint>
            <label v-for="entry in selectedProjectsList" :key="entry.projectId" class="pcl-batch-confirm__row">
              <PclCheckbox v-model="entry.checked" />
              <span><strong>{{ entry.name }}</strong><PclSelect v-model="entry.versionId" :options="entry.versionOptions" /><small>{{ entry.versionLabel }}</small></span>
            </label>
          </q-card-section>
          <q-card-actions align="right">
            <PclButton variant="ghost" @click="batchConfirmOpen = false">{{ t('common.cancel') }}</PclButton>
            <PclButton variant="highlight" icon="download" :disabled="!selectedProjectsList.some((entry:any) => entry.checked && entry.versionId)" @click="installSelected">{{ t('pcl.resourceDownload.start') }}</PclButton>
          </q-card-actions>
        </q-card>
      </q-dialog>
</template>
<script setup lang="ts">
import { inject } from 'vue'; import { PclCatalogContextKey } from './PclCatalogContext';
import PclButton from '../../components/PclButton.vue'; import PclCheckbox from '../../components/PclCheckbox.vue'; import PclHint from '../../components/PclHint.vue'; import PclIcon from '../../components/PclIcon.vue'; import PclIconButton from '../../components/PclIconButton.vue'; import PclInput from '../../components/PclInput.vue'; import PclSelect from '../../components/PclSelect.vue'; import PclCatalogProjectView from './PclCatalogProjectView.vue'; import PclCatalogResultList from './PclCatalogResultList.vue';
const context = inject(PclCatalogContextKey); if (!context) throw new Error('PclCatalogContext is missing');
const { t, props, emit, busy, searching, detailLoading, installing, cancelling, packs, provider, query, privateCode, results, project, versions, version, error, gameVersion, loader, sort, optional, instanceName, offset, searched, projectTab, favorite, providerLabel, providerOptions, sortOptions, loaderOptions, versionOptions, renderedBody, resetFilters, changeProvider, search, select, install, toggleFavorite, versionLabel, catalogRoot, scrollCatalogToTop, selectedProjects, selectedProjectsList, batchConfirmOpen, toggleSelected, prepareBatchDownload, installSelected, clearSelectedProjects } = context;
</script>
<style>
.pcl-catalog-batch-bar { display:flex; align-items:center; gap:8px; margin-top:10px; padding:8px 10px; border:1px solid var(--pcl-card-border); border-radius:4px; background:var(--pcl-c7); color:var(--pcl-muted); }
.pcl-catalog-batch-bar span { margin-right:auto; }
.pcl-batch-confirm { width:620px; max-width:92vw; max-height:80vh; overflow:hidden; }
.pcl-batch-confirm__body { max-height:calc(80vh - 150px); overflow-y:auto; }
.pcl-batch-confirm__row { display:flex; align-items:flex-start; gap:7px; padding:6px; margin-top:5px; border:1px solid var(--pcl-border); border-radius:3px; }
.pcl-batch-confirm__row span { display:flex; flex-direction:column; min-width:0; }
.pcl-batch-confirm__row strong { overflow:hidden; color:var(--pcl-text); font-size:calc(10px * var(--pcl-font-scale, 1)); font-weight:500; text-overflow:ellipsis; white-space:nowrap; }
.pcl-batch-confirm__row .pcl-select { min-width:240px; margin-top:4px; }
.pcl-batch-confirm__row small { color:var(--pcl-muted); font-size:calc(9px * var(--pcl-font-scale, 1)); }
</style>
