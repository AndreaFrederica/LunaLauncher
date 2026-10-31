<template>
<div class="pcl-download-section">
          <PclCard :title="t('pcl.auto2.kcf1656101e')">
            <PclHint theme="yellow">{{ t('pcl.auto2.k07cc85beed') }}</PclHint>
            <div class="pcl-actions">
              <PclButton icon="folder-open" @click="emit('launch')">{{ t('pcl.auto2.k14e93edbca') }}</PclButton>
            </div>
          </PclCard>
          </div>
</template>
<script setup lang="ts">
import { inject } from 'vue'; import { PclDownloadContextKey } from './PclDownloadContext';
import PclButton from '../../components/PclButton.vue'; import PclCard from '../../components/PclCard.vue'; import PclHint from '../../components/PclHint.vue'; import PclInput from '../../components/PclInput.vue'; import PclIcon from '../../components/PclIcon.vue'; import PclIconButton from '../../components/PclIconButton.vue'; import PclListItem from '../../components/PclListItem.vue'; import PclLoading from '../../components/PclLoading.vue'; import PclInstallView from './PclInstallView.vue'; import PclCatalogBrowser from './PclCatalogBrowser.vue'; import PclResourceCatalogView from './PclResourceCatalogView.vue';
const context = inject(PclDownloadContextKey); if (!context) throw new Error('PclDownloadContext is missing');
const { t, props, emit, busy, execute, instances, selected, section, preferences, downloadTargetInstanceId, refreshKey, preset, initialMinecraftVersion, componentItem, componentVersions, componentMinecraft, componentLoading, componentError, loadComponentVersions, useComponentVersion, isResource, resourceKind, resourceProvider, resourceTag, favorites, importPath, importName, importError, importing, importDragOver, onImportDrop, chooseImportFile, importInstance, openFavorite, onInstalled, onModpackInstalled, active, resourceKinds } = context;
</script>
