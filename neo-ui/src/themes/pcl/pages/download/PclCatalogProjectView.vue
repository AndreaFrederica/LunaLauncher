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
            <h2>{{ project.name }}</h2>
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
</script>
