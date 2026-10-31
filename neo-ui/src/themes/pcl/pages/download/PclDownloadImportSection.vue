<template>
<div class="pcl-download-section">
          <PclCard :title="t('pcl.auto2.k6ddefa974c')">
            <PclHint theme="blue">{{ t('pcl.auto2.k963725ad7c') }}</PclHint>
            <div
              class="pcl-import-drop"
              :class="{ active: importDragOver }"
              @dragover.prevent="importDragOver = true"
              @dragleave.prevent="importDragOver = false"
              @drop.prevent="onImportDrop"
            >
              <PclIcon name="upload" :size="34" />
              <strong>{{ t('pcl.auto2.k687f79be54') }}</strong>
              <span>{{ t('pcl.auto2.k1f1c6e9963') }}</span>
            </div>
            <div class="pcl-form-row pcl-form-row--top">
              <span class="pcl-form-row__label">{{ t('pcl.auto2.kc8a69b6fd5') }}</span>
              <div class="pcl-form-row__control pcl-form-row__control--column">
                <PclInput v-model="importPath" :placeholder="t('pcl.auto2.k040e414c43')" />
                <PclIconButton icon="folder-open" :label="t('pcl.auto2.k940f61fa15')" :size="28" :disabled="importing" @click="chooseImportFile" />
              </div>
            </div>
            <div class="pcl-form-row">
              <span class="pcl-form-row__label">{{ t('pcl.auto2.k9c0531d66e') }}</span>
              <div class="pcl-form-row__control">
                <PclInput v-model="importName" :placeholder="t('pcl.auto2.kb5e46a0c9c')" />
              </div>
            </div>
            <PclHint v-if="importError" theme="red" closable @close="importError = ''">{{ importError }}</PclHint>
            <div class="pcl-actions">
              <PclButton variant="highlight" icon="file-input" :disabled="busy || importing || !importPath.trim()" @click="importInstance">
                {{ importing ? t('pcl.auto2.kc3ddf13267') : t('pcl.auto2.k3d04880897') }}
              </PclButton>
              <PclButton variant="ghost" icon="download" @click="emit('launch')">{{ t('pcl.auto2.k14e93edbca') }}</PclButton>
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
