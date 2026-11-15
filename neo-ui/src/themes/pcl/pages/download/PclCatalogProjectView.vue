<template>
<template v-if="project">
      <button class="pcl-catalog-back" type="button" @click="project = null">
        <PclIcon name="arrow-left" :size="14" />{{ t('pcl.auto2.ked6bcdc619') }}</button>
      <article class="pcl-project">
        <div class="pcl-project-head">
          <img v-if="project.iconUrl" :src="project.iconUrl" alt="" />
          <span v-else class="pcl-project-icon"><PclIcon name="box" :size="34" /></span>
          <div class="pcl-project-copy">
            <small>{{ providerLabel(provider) }} · {{ project.projectId }}</small>
            <h2>{{ project.name }}<span v-if="project.mcmod?.name" class="pcl-project-localized-name"> · {{ project.mcmod.name }}</span></h2>
            <div class="pcl-project-links">
              <template v-for="link in platformLinks(project)" :key="`${link.label}:${link.url}`">
                <a class="pcl-project-link" :href="link.url" target="_blank" rel="noreferrer" @click.stop.prevent="openExternal(link.url)">{{ link.label }}</a>
              </template>
              <span v-if="!platformLinks(project).length">{{ providerLabel(provider) }}</span>
              <a v-if="project.mcmod?.url || project.mcmod?.searchUrl" class="pcl-project-link" :href="project.mcmod.url || project.mcmod.searchUrl" target="_blank" rel="noreferrer" @click.stop.prevent="openExternal(project.mcmod.url || project.mcmod.searchUrl)">{{ t('resource.mcmodLink') }}</a>
              <a v-if="project.sourceUrl" class="pcl-project-link" :href="project.sourceUrl" target="_blank" rel="noreferrer" @click.stop.prevent="openExternal(project.sourceUrl)">{{ t('resource.sourceCode') }}</a>
              <a v-if="project.websiteUrl && project.websiteUrl !== providerUrl(project)" class="pcl-project-link" :href="project.websiteUrl" target="_blank" rel="noreferrer" @click.stop.prevent="openExternal(project.websiteUrl)">{{ t('resource.projectPage') }}</a>
            </div>
            <p>{{ project.description }}</p>
          </div>
          <PclIconButton
            :icon="favorite ? 'heart-filled' : 'heart'"
            :label="favorite ? t('pcl.auto2.k26acae04cf') : t('pcl.auto2.k0e7c7c1ee1')"
            :size="28"
            :class="{ 'pcl-favorite--active': favorite }"
            @click="toggleFavorite"
          />
        </div>

        <div class="pcl-project-tabs">
          <button type="button" :class="{ active: projectTab === 'overview' }" @click="projectTab = 'overview'">{{ t('pcl.auto2.k42b249cd84') }}</button>
          <button type="button" :class="{ active: projectTab === 'versions' }" @click="projectTab = 'versions'">{{ t('pcl.catalog.versionsCount', { count: versions.length }) }}</button>
        </div>

        <div v-if="projectTab === 'overview'" class="pcl-project-body" v-html="renderedBody" />
        <div v-else class="pcl-version-list">
          <div v-if="!versions.length && !detailLoading" class="pcl-catalog-empty">{{ t('pcl.auto2.k30a123c455') }}</div>
          <label v-for="v in versions" :key="v.versionId" class="pcl-version-option">
            <input v-model="version" type="radio" :value="v.versionId" />
            <span>
              <strong>{{ versionLabel(v) }}</strong>
              <small>{{ v.releaseType || v.type || 'release' }} · {{ v.datePublished || v.created || t('pcl.auto2.kbe19659105') }}</small>
            </span>
          </label>
        </div>

        <div v-if="packs" class="pcl-project-field">
          <label>{{ t('pcl.auto2.kd1829a3f57') }}</label>
          <PclInput v-model="instanceName" :placeholder="t('pcl.auto2.k9c0531d66e')" />
        </div>
        <div class="pcl-project-field">
          <label>{{ t('pcl.auto2.k989d1affa0') }}</label>
          <PclSelect v-model="version" :options="versionOptions" :placeholder="t('pcl.auto2.kf7dc468fc8')" />
        </div>
        <PclCheckbox v-if="!packs" v-model="optional" :label="t('pcl.auto2.k4fdbdee574')" class="pcl-optional" />
        <div class="pcl-project-actions">
          <PclButton
            variant="highlight"
            icon="download"
            :disabled="busy || installing || !version || (packs && !instanceName.trim())"
            @click="install"
          >
            {{ installing ? t('pcl.auto2.ke0ce99ef5b') : packs ? t('pcl.auto2.ke6cc1d4499') : t('pcl.auto2.kffa40ad820') }}
          </PclButton>
        </div>
      </article>
    </template>
</template>
<script setup lang="ts">
import { inject } from 'vue'; import { PclCatalogContextKey } from './PclCatalogContext';
import PclButton from '../../components/PclButton.vue'; import PclCheckbox from '../../components/PclCheckbox.vue'; import PclIcon from '../../components/PclIcon.vue'; import PclIconButton from '../../components/PclIconButton.vue'; import PclInput from '../../components/PclInput.vue'; import PclSelect from '../../components/PclSelect.vue'; import PclCatalogResultList from './PclCatalogResultList.vue';
const context = inject(PclCatalogContextKey); if (!context) throw new Error('PclCatalogContext is missing');
const { t, props, emit, busy, searching, detailLoading, installing, packs, provider, query, privateCode, results, project, versions, version, error, gameVersion, loader, sort, optional, instanceName, offset, searched, projectTab, favorite, providerLabel, providerOptions, sortOptions, loaderOptions, versionOptions, renderedBody, resetFilters, changeProvider, search, select, install, toggleFavorite, versionLabel, catalogRoot, scrollCatalogToTop } = context;
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
async function openExternal(url: string) {
  const value = String(url || '').trim();
  if (!value) return;
  try {
    await props.execute('desktop.open-url', { url: value });
  } catch {
    window.open(value, '_blank', 'noopener,noreferrer');
  }
}
</script>
<style scoped>
.pcl-project-links { display: flex; flex-wrap: wrap; align-items: center; gap: 5px; color: var(--pcl-c3); font-size: calc(10px * var(--pcl-font-scale, 1)); }
.pcl-project-link { display: inline-flex; align-items: center; min-height: 20px; padding: 2px 7px; border: 1px solid currentColor; border-radius: 4px; color: inherit; line-height: 1.2; }
.pcl-project-link:hover { background: color-mix(in srgb, currentColor 12%, transparent); text-decoration: none; }
.pcl-project-localized-name { color: var(--pcl-muted); font-size: .72em; font-weight: 400; }
</style>
