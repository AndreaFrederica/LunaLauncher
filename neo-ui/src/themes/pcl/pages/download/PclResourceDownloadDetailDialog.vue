<template>
<q-dialog v-model="detailOpen">
      <q-card class="pcl-dialog pcl-resource-detail">
        <q-card-section class="pcl-resource-detail__head">
          <img v-if="detail?.iconUrl" :src="detail.iconUrl" alt="" />
          <div><div class="pcl-kicker">{{ t('resource.detail') }}</div><h3>{{ detail?.name || t('tasks.loading') }}</h3><p>{{ detail?.authors?.map((a:any) => a.name).join(', ') || providerLabel(provider) }}</p></div>
          <PclIconButton icon="x" :label="t('window.close')" :size="28" @click="detailOpen = false" />
        </q-card-section>
        <q-card-section class="pcl-resource-detail__body">
          <div v-if="detailLoading" class="pcl-resource-detail__loading">{{ t('resource.loadingDetail') }}…</div>
          <div v-else-if="detailBody" class="pcl-resource-detail__description" v-html="detailBody" />
          <p v-else>{{ t('pcl.resourceDownload.noDescription') }}</p>
          <div v-if="detail?.mcmod?.name || detail?.mcmod?.url || detail?.mcmod?.searchUrl" class="pcl-resource-detail__mcmod">
            <span v-if="detail.mcmod.name">{{ detail.mcmod.name }}</span>
            <a v-if="detail.mcmod.url || detail.mcmod.searchUrl" :href="detail.mcmod.url || detail.mcmod.searchUrl" target="_blank" rel="noreferrer" @click.prevent="openExternal(detail.mcmod.url || detail.mcmod.searchUrl)">{{ t('resource.mcmodLink') }}</a>
          </div>
          <a v-if="detail?.websiteUrl" :href="detail.websiteUrl" target="_blank" rel="noreferrer" @click.prevent="openExternal(detail.websiteUrl)">{{ t('pcl.resourceDownload.openProject') }}</a>
        </q-card-section>
      </q-card>
    </q-dialog>
</template>
<script setup lang="ts">
import { inject } from 'vue'; import { PclResourceDownloadContextKey } from './PclResourceDownloadContext';
import PclButton from '../../components/PclButton.vue'; import PclCheckbox from '../../components/PclCheckbox.vue'; import PclHint from '../../components/PclHint.vue'; import PclIconButton from '../../components/PclIconButton.vue';
const context = inject(PclResourceDownloadContextKey); if (!context) throw new Error('PclResourceDownloadContext is missing');
const { t, props, provider, providerLabel, detailOpen, detail, detailBody, detailLoading, openExternal, confirmOpen, confirmEntries, confirmDownload, selectedList, closeWindow, kindLabel } = context;
</script>
<style>
.pcl-resource-detail { width:640px; max-width:92vw; max-height:84vh; overflow:hidden; }
.pcl-resource-detail__head { display:flex; align-items:flex-start; gap:10px; padding-bottom:8px; }
.pcl-resource-detail__head img { width:48px; height:48px; flex:0 0 48px; border-radius:4px; object-fit:cover; }
.pcl-resource-detail__head > div { flex:1; min-width:0; }
.pcl-resource-detail__head h3 { margin:3px 0; color:var(--pcl-text); font-size:calc(17px * var(--pcl-font-scale, 1)); }
.pcl-resource-detail__head p { margin:0; color:var(--pcl-muted); font-size:calc(10px * var(--pcl-font-scale, 1)); }
.pcl-resource-detail__body { max-height:calc(84vh - 100px); overflow-y:auto; color:var(--pcl-text); font-size:calc(11px * var(--pcl-font-scale, 1)); }
.pcl-resource-detail__description { line-height:1.55; }
.pcl-resource-detail__description p { margin:0 0 10px; }
.pcl-resource-detail__description a, .pcl-resource-detail__body > a { color:var(--pcl-c3); }
.pcl-resource-detail__mcmod { display:flex; align-items:center; gap:8px; margin-top:12px; color:var(--pcl-muted); }
.pcl-resource-detail__mcmod a { color:var(--pcl-c3); }
</style>
