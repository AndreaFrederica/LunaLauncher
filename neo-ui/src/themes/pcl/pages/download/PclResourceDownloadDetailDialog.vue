<template>
<q-dialog v-model="detailOpen">
      <q-card class="pcl-dialog pcl-resource-detail">
        <q-card-section class="pcl-resource-detail__head">
          <img v-if="detail?.iconUrl" :src="detail.iconUrl" alt="" />
          <div><div class="pcl-kicker">{{ t('resource.detail') }}</div><h3>{{ detail?.name || t('tasks.loading') }}<span v-if="detail?.mcmod?.name" class="pcl-resource-detail__localized-name"> · {{ detail.mcmod.name }}</span></h3><p>{{ detail?.authors?.map((a:any) => a.name).join(', ') || providerLabel(provider) }}</p></div>
          <PclIconButton icon="x" :label="t('window.close')" :size="28" @click="detailOpen = false" />
        </q-card-section>
        <q-card-section class="pcl-resource-detail__body">
          <div v-if="detailLoading" class="pcl-resource-detail__loading">{{ t('resource.loadingDetail') }}…</div>
          <div v-else-if="detailBody" class="pcl-resource-detail__description" v-html="detailBody" />
          <p v-else>{{ t('pcl.resourceDownload.noDescription') }}</p>
          <div class="pcl-resource-detail__links">
            <template v-for="link in platformLinks(detail)" :key="`${link.label}:${link.url}`">
              <a class="pcl-resource-detail__link" :href="link.url" target="_blank" rel="noreferrer" @click.prevent="openExternal(link.url)">{{ link.label }}</a>
            </template>
            <span v-if="!platformLinks(detail).length">{{ providerLabel(provider) }}</span>
            <a v-if="detail?.mcmod?.url || detail?.mcmod?.searchUrl" class="pcl-resource-detail__link" :href="detail.mcmod.url || detail.mcmod.searchUrl" target="_blank" rel="noreferrer" @click.prevent="openExternal(detail.mcmod.url || detail.mcmod.searchUrl)">{{ t('resource.mcmodLink') }}</a>
            <a v-if="detail?.sourceUrl" class="pcl-resource-detail__link" :href="detail.sourceUrl" target="_blank" rel="noreferrer" @click.prevent="openExternal(detail.sourceUrl)">{{ t('resource.sourceCode') }}</a>
            <a v-if="detail?.websiteUrl && detail.websiteUrl !== providerUrl(detail)" class="pcl-resource-detail__link" :href="detail.websiteUrl" target="_blank" rel="noreferrer" @click.prevent="openExternal(detail.websiteUrl)">{{ t('resource.projectPage') }}</a>
          </div>
        </q-card-section>
      </q-card>
    </q-dialog>
</template>
<script setup lang="ts">
import { inject } from 'vue'; import { PclResourceDownloadContextKey } from './PclResourceDownloadContext';
import PclButton from '../../components/PclButton.vue'; import PclCheckbox from '../../components/PclCheckbox.vue'; import PclHint from '../../components/PclHint.vue'; import PclIconButton from '../../components/PclIconButton.vue';
const context = inject(PclResourceDownloadContextKey); if (!context) throw new Error('PclResourceDownloadContext is missing');
const { t, props, provider, providerLabel, detailOpen, detail, detailBody, detailLoading, openExternal, confirmOpen, confirmEntries, confirmDownload, selectedList, closeWindow, kindLabel } = context;
function providerUrl(item: any) {
  if (item?.providerUrl) return String(item.providerUrl);
  const source = String(item?.provider || provider.value || '');
  const id = String(item?.projectId || item?.slug || '').trim();
  if (!id) return '';
  if (source === 'modrinth') return `https://modrinth.com/mod/${item?.slug || id}`;
  if (source === 'curseforge') return `https://www.curseforge.com/projects/${id}`;
  return '';
}
function platformLinks(item: any) {
  const links: Array<{ label: string; url: string }> = [];
  const add = (label: string, url: unknown) => {
    const value = String(url || '').trim();
    if (value && !links.some(link => link.url === value)) links.push({ label, url: value });
  };
  const isOfficialProjectUrl = (source: string, url: string) => {
    const value = url.toLowerCase();
    return (source === 'modrinth' && value.includes('modrinth.com/')) || (source === 'curseforge' && value.includes('curseforge.com/'));
  };
  const metadata = item?.providerMeta || {};
  for (const [source, raw] of Object.entries(metadata)) {
    const data: any = typeof raw === 'string' ? { providerUrl: raw } : raw || {};
    const ownProjectUrl = String(data.projectUrl || '').trim();
    const canonicalUrl = String(data.providerUrl || '').trim();
    const platform = ownProjectUrl && isOfficialProjectUrl(source, ownProjectUrl) ? ownProjectUrl : (canonicalUrl || ownProjectUrl);
    add(providerLabel(source), platform);
    add(t('resource.sourceCode'), data.sourceUrl);
    if (ownProjectUrl && ownProjectUrl !== platform) add(t('resource.projectPage'), ownProjectUrl);
  }
  add(providerLabel(String(item?.provider || provider.value || '')), item?.providerUrl || providerUrl(item));
  const providerLinks = item?.providerLinks || {};
  add('Modrinth', typeof providerLinks.modrinth === 'string' ? providerLinks.modrinth : providerLinks.modrinth?.url);
  add('CurseForge', typeof providerLinks.curseforge === 'string' ? providerLinks.curseforge : providerLinks.curseforge?.url);
  add('Modrinth', item?.mcmod?.modrinthId && `https://modrinth.com/mod/${item.mcmod.modrinthId}`);
  add('CurseForge', item?.mcmod?.curseForgeId && `https://www.curseforge.com/projects/${item.mcmod.curseForgeId}`);
  return links;
}
</script>
<style>
.pcl-resource-detail { width:640px; max-width:92vw; max-height:84vh; overflow:hidden; }
.pcl-resource-detail__head { display:flex; align-items:flex-start; gap:10px; padding-bottom:8px; }
.pcl-resource-detail__head img { width:48px; height:48px; flex:0 0 48px; border-radius:4px; object-fit:cover; }
.pcl-resource-detail__head > div { flex:1; min-width:0; }
.pcl-resource-detail__head h3 { margin:3px 0; color:var(--pcl-text); font-size:calc(17px * var(--pcl-font-scale, 1)); }
.pcl-resource-detail__localized-name { color:var(--pcl-muted); font-size:.72em; font-weight:400; }
.pcl-resource-detail__head p { margin:0; color:var(--pcl-muted); font-size:calc(10px * var(--pcl-font-scale, 1)); }
.pcl-resource-detail__body { max-height:calc(84vh - 100px); overflow-y:auto; color:var(--pcl-text); font-size:calc(11px * var(--pcl-font-scale, 1)); }
.pcl-resource-detail__description { line-height:1.55; }
.pcl-resource-detail__description p { margin:0 0 10px; }
.pcl-resource-detail__description a, .pcl-resource-detail__body > a { color:var(--pcl-c3); }
.pcl-resource-detail__links { display:flex; flex-wrap:wrap; align-items:center; gap:5px; margin-top:12px; color:var(--pcl-c3); }
.pcl-resource-detail__link { display:inline-flex; align-items:center; min-height:20px; padding:2px 7px; border:1px solid currentColor; border-radius:4px; color:inherit; line-height:1.2; }
.pcl-resource-detail__link:hover { background:color-mix(in srgb, currentColor 12%, transparent); text-decoration:none; }
</style>
