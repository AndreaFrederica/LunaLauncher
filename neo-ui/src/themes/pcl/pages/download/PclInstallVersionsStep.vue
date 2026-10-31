<template>
<div key="versions" class="pcl-install-step">
        <PclCard v-if="loading">
          <PclLoading :text="t('pcl.installPage.loadingVersions')" />
        </PclCard>

        <PclCard v-else-if="loadError">
          <NeoErrorDetails :error="loadError" operation="component.versions" :parameters="{ uid: 'net.minecraft' }" :context="loadErrorContext" title="无法读取 Minecraft 版本列表">
            <template #actions>
              <PclButton variant="highlight" icon="refresh-cw" @click="loadMinecraft">{{ t('common.retry') }}</PclButton>
            </template>
          </NeoErrorDetails>
        </PclCard>

        <PclCard v-else-if="!versions.length">
          <NeoErrorDetails
            :error="t('pcl.installPage.noVersions')"
            operation="component.versions"
            :parameters="{ uid: 'net.minecraft' }"
            title="Minecraft 版本列表为空"
          >
            <template #actions>
              <PclButton variant="highlight" icon="refresh-cw" @click="loadMinecraft">{{ t('common.retry') }}</PclButton>
            </template>
          </NeoErrorDetails>
        </PclCard>

        <template v-else>
          <PclCard v-model:open="categoryOpen.latest" :title="t('pcl.installPage.latest')">
            <div class="pcl-version-list">
              <PclListItem
                v-for="version in latestVersions"
                :key="version.version"
                :title="version.version"
                :info="versionInfo(version)"
                :tags="version.recommended ? [t('download.recommended')] : []"
                @click="selectMinecraft(version)"
              >
                <template #icon><PclIcon name="boxes" :size="21" /></template>
              </PclListItem>
            </div>
          </PclCard>

          <PclCard
            v-for="category in versionCategories"
            :key="category.id"
            v-model:open="categoryOpen[category.id]"
            collapsible
            :title="`${category.label} (${category.versions.length})`"
          >
            <div class="pcl-version-list">
              <PclListItem
                v-for="version in category.versions"
                :key="version.version"
                :title="version.version"
                :info="versionInfo(version)"
                :tags="version.recommended ? [t('download.recommended')] : []"
                @click="selectMinecraft(version)"
              >
                <template #icon><PclIcon name="boxes" :size="21" /></template>
              </PclListItem>
            </div>
          </PclCard>
        </template>
      </div>
</template>
<script setup lang="ts">
import { inject } from 'vue'; import { PclInstallContextKey } from './PclInstallContext';
import NeoErrorDetails from '../../../../common/components/NeoErrorDetails.vue';
import PclButton from '../../components/PclButton.vue'; import PclCard from '../../components/PclCard.vue'; import PclIcon from '../../components/PclIcon.vue'; import PclInput from '../../components/PclInput.vue'; import PclListItem from '../../components/PclListItem.vue'; import PclLoading from '../../components/PclLoading.vue'; import PclSelect from '../../components/PclSelect.vue';
const context = inject(PclInstallContextKey); if (!context) throw new Error('PclInstallContext is missing');
const { t, props, emit, busy, execute, instances, preset, initialMinecraftVersion, step, loading, loadError, loadErrorContext, installing, error, versions, selectedVersion, selectedLoader, openLoader, instanceName, nameTouched, categoryOpen, loaderState, loaderCards, sortedVersions, releaseVersions, snapshotVersions, oldVersions, latestVersions, versionCategories, nameError, canInstall, groupOf, versionInfo, generatedName, isLoaderCompatible, isLoaderDisabled, loaderStateLabel, clearLoaderState, loadMinecraft, selectMinecraft, backToVersions, toggleLoader, selectLoader, clearLoader, install } = context;
</script>
