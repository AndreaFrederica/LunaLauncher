<template>
<div class="pcl-install-step">
        <PclCard class="pcl-install-header">
          <div class="pcl-install-header__row">
            <PclIconButton icon="arrow-left" :label="t('pcl.installPage.backVersions')" :size="26" @click="backToVersions" />
            <span class="pcl-install-header__logo"><PclIcon name="boxes" :size="32" /></span>
            <div class="pcl-install-header__copy">
              <div class="pcl-install-header__eyebrow">Minecraft {{ selectedVersion?.version }}</div>
              <PclInput
                v-model="instanceName"
                :placeholder="t('app.instanceName')"
                class="pcl-install-name"
                @update:model-value="nameTouched = true"
              />
            </div>
            <span class="pcl-install-header__hint">{{ selectedLoader ? selectedLoader.label : t('pcl.installPage.vanilla') }}</span>
          </div>
        </PclCard>

        <PclHint v-if="error" theme="red" closable @close="error = ''">{{ error }}</PclHint>
        <PclHint v-if="nameError" theme="red">{{ nameError }}</PclHint>
        <PclHint v-if="selectedLoader?.id === 'fabric'" theme="blue">
          {{ t('pcl.installPage.fabricHint') }}
        </PclHint>
        <PclHint v-if="selectedLoader?.id === 'forge'" theme="yellow">
          {{ t('pcl.installPage.forgeHint') }}
        </PclHint>
        <PclHint theme="blue">
          {{ t('pcl.installPage.loaderHint') }}
        </PclHint>

        <PclCard
          v-for="loader in loaderCards"
          :key="loader.id"
          collapsible
          :disabled="isLoaderDisabled(loader)"
          :open="openLoader === loader.id"
          @update:open="toggleLoader(loader, $event)"
        >
          <template #title>
            <span class="pcl-loader-title">
              <PclIcon :name="loader.icon" :size="18" />
              <span>{{ loader.label }}</span>
            </span>
          </template>
          <template #actions>
            <span v-if="selectedLoader?.id === loader.id" class="pcl-loader-selected">{{ selectedLoader.version }}</span>
            <span v-else-if="isLoaderDisabled(loader)" class="pcl-loader-unavailable">{{ loaderStateLabel(loader) }}</span>
            <span v-else class="pcl-loader-add">{{ t('pcl.installPage.addLoader') }}</span>
            <PclIconButton
              v-if="selectedLoader?.id === loader.id"
              icon="x"
              :label="t('pcl.installPage.clearLoader')"
              :size="22"
              @click.stop="clearLoader"
            />
          </template>

          <PclLoading v-if="loaderState[loader.id]?.loading" :text="t('pcl.installPage.readingVersions')" inline />
          <PclHint v-else-if="loaderState[loader.id]?.error" theme="red">{{ loaderState[loader.id].error }}</PclHint>
          <div v-else class="pcl-loader-list">
            <PclListItem
              v-for="version in loaderState[loader.id]?.versions || []"
              :key="version.version"
              :title="version.version"
              :info="versionInfo(version)"
              :selected="selectedLoader?.id === loader.id && selectedLoader.version === version.version"
              @click="selectLoader(loader, version)"
            >
              <template #icon><PclIcon :name="loader.icon" :size="19" /></template>
            </PclListItem>
            <div v-if="!(loaderState[loader.id]?.versions || []).length" class="pcl-empty">
              {{ t('pcl.installPage.noCompatible', { version: selectedVersion?.version }) }}
            </div>
          </div>
        </PclCard>

        <div class="pcl-install-footer">
          <PclButton variant="accent" size="xl" icon="download" :disabled="!canInstall || installing" @click="install">
            {{ installing ? t('pcl.installPage.creating') : t('pcl.installPage.startDownload') }}
          </PclButton>
        </div>
      </div>
</template>
<script setup lang="ts">
import { inject } from 'vue'; import { PclInstallContextKey } from './PclInstallContext';
import PclButton from '../../components/PclButton.vue'; import PclCard from '../../components/PclCard.vue'; import PclHint from '../../components/PclHint.vue'; import PclIcon from '../../components/PclIcon.vue'; import PclInput from '../../components/PclInput.vue'; import PclListItem from '../../components/PclListItem.vue'; import PclLoading from '../../components/PclLoading.vue'; import PclSelect from '../../components/PclSelect.vue';
import PclIconButton from '../../components/PclIconButton.vue';

const context = inject(PclInstallContextKey); if (!context) throw new Error('PclInstallContext is missing');
const { t, props, emit, busy, execute, instances, preset, initialMinecraftVersion, step, loading, loadError, installing, error, versions, selectedVersion, selectedLoader, openLoader, instanceName, nameTouched, categoryOpen, loaderState, loaderCards, sortedVersions, releaseVersions, snapshotVersions, oldVersions, latestVersions, versionCategories, nameError, canInstall, groupOf, versionInfo, generatedName, isLoaderCompatible, isLoaderDisabled, loaderStateLabel, clearLoaderState, loadMinecraft, selectMinecraft, backToVersions, toggleLoader, selectLoader, clearLoader, install } = context;
</script>
