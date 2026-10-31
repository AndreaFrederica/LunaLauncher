<template>
<div class="pcl-download-section">
            <PclCard :title="componentItem?.label || ''">
              <PclHint v-if="!componentItem?.uid && !componentItem?.special" theme="yellow">
                {{ t('pcl.downloadPage.noBackend', { name: componentItem?.label || '' }) }}
              </PclHint>
              <template v-else-if="componentItem?.uid || componentItem?.special">
                <div class="pcl-component-filter">
                  <PclInput v-model="componentMinecraft" :placeholder="t('pcl.auto2.k48ad119705')" @enter="loadComponentVersions" />
                  <PclButton variant="highlight" icon="refresh-cw" :disabled="busy || componentLoading" @click="loadComponentVersions">
                    {{ componentLoading ? t('pcl.auto2.k3667cb105a') : t('pcl.auto2.k68f7347528') }}
                  </PclButton>
                </div>
                <NeoErrorDetails
                  v-if="componentError"
                  :error="componentError"
                  :operation="componentItem?.special ? 'special.loader.versions' : 'component.versions'"
                  :parameters="{ uid: componentItem?.uid || '', minecraftVersion: componentMinecraft || '' }"
                  title="无法读取加载器版本列表"
                />
                <PclLoading v-if="componentLoading" :text="t('pcl.auto2.k8e5fed83c5')" inline />
                <div v-else class="pcl-component-versions">
                  <PclListItem
                    v-for="version in componentVersions"
                    :key="version.version"
                    :title="version.version"
                    :info="componentItem?.special ? `Minecraft ${version.minecraftVersion || version.version} · ${version.type || t('pcl.auto2.k989d1affa0')}` : (version.type || t('pcl.auto2.k989d1affa0'))"
                    :tags="version.recommended ? [t('pcl.auto2.k62b46f24ae')] : []"
                    @click="useComponentVersion(version)"
                  >
                    <template #icon><PclIcon :name="componentItem?.icon || 'package'" :size="20" /></template>
                    <template #actions>
                      <span class="pcl-component-use">{{ componentItem?.id === 'client' ? t('pcl.auto2.k70b208202c') : t('pcl.auto2.k4e61458b9b') }}</span>
                    </template>
                  </PclListItem>
                  <div v-if="!componentVersions.length" class="pcl-empty">{{ t('pcl.auto2.k926bc43f38') }}</div>
                </div>
              </template>
            </PclCard>
          </div>
</template>
<script setup lang="ts">
import { inject } from 'vue'; import { PclDownloadContextKey } from './PclDownloadContext';
import NeoErrorDetails from '../../../../common/components/NeoErrorDetails.vue';
import PclButton from '../../components/PclButton.vue'; import PclCard from '../../components/PclCard.vue'; import PclHint from '../../components/PclHint.vue'; import PclInput from '../../components/PclInput.vue'; import PclIcon from '../../components/PclIcon.vue'; import PclIconButton from '../../components/PclIconButton.vue'; import PclListItem from '../../components/PclListItem.vue'; import PclLoading from '../../components/PclLoading.vue'; import PclInstallView from './PclInstallView.vue'; import PclCatalogBrowser from './PclCatalogBrowser.vue'; import PclResourceCatalogView from './PclResourceCatalogView.vue';
const context = inject(PclDownloadContextKey); if (!context) throw new Error('PclDownloadContext is missing');
const { t, props, emit, busy, execute, instances, selected, section, preferences, downloadTargetInstanceId, refreshKey, preset, initialMinecraftVersion, componentItem, componentVersions, componentMinecraft, componentLoading, componentError, loadComponentVersions, useComponentVersion, isResource, resourceKind, resourceProvider, resourceTag, favorites, importPath, importName, importError, importing, importDragOver, onImportDrop, chooseImportFile, importInstance, openFavorite, onInstalled, onModpackInstalled, active, resourceKinds } = context;
</script>
