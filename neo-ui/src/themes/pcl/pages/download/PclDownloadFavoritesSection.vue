<template>
<div class="pcl-download-section">
          <PclCard :title="t('pcl.auto2.k93a41f509d')">
            <div class="pcl-favorites">
              <PclListItem
                v-for="item in favorites"
                :key="item.key || `${item.provider}-${item.projectId}`"
                :title="item.name || item.projectId"
                :info="`${item.provider || 'modrinth'} · ${item.kind || 'mod'}`"
                @click="openFavorite(item)"
              >
                <template #icon><PclIcon name="heart-filled" :size="19" /></template>
                <template #actions><PclIcon name="chevron-right" :size="16" /></template>
              </PclListItem>
              <div v-if="!favorites.length" class="pcl-empty">
                <PclIcon name="heart" :size="34" />
                <strong>{{ t('pcl.auto2.k525b6078f6') }}</strong>
                <span>{{ t('pcl.auto2.k9fa897a526') }}</span>
              </div>
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
