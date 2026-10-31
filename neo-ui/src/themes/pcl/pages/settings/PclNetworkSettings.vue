<template>
<div class="pcl-settings-section">
            <PclCard v-if="isInstance" :title="t('pcl.auto2.k6aca62f330')">
              <p class="pcl-muted">{{ t('pcl.auto2.kf1bf44c5a9') }}</p>
              <PclHint theme="blue">{{ t('pcl.auto2.kbaa138bcf7') }}</PclHint>
            </PclCard>

            <template v-else>
              <PclCard :title="t('pcl.auto2.k22db8c6e46')">
                <template #actions><PclButton size="sm" icon="refresh-cw" :disabled="onlineBusy" @click="loadOnlineBackends(true)">{{ t('pcl.auto2.k38108eaa1d') }}</PclButton></template>
                <PclLoading v-if="onlineBusy" :text="t('pcl.auto2.ke4a515e814')" inline />
                <div v-if="onlineBackends.length" class="pcl-tools-list">
                  <PclListItem v-for="backend in onlineBackends" :key="backend.id" :title="backend.name || backend.id" :info="`${backend.installed ? (backend.version || t('pcl.auto2.keb88ff57c9')) : t('pcl.auto2.k6f7dc945ae')} · ${backend.running ? t('pcl.auto2.k9761f5d947') : t('pcl.auto2.k803638ec0a')}`">
                    <template #icon><PclIcon name="bubbles" :size="16" /></template>
                  </PclListItem>
                </div>
                <PclHint v-else-if="!onlineBusy" theme="yellow">{{ t('pcl.auto2.kb9e6473b23') }}</PclHint>
                <PclHint theme="blue">{{ t('pcl.auto2.k85f96ac696') }}</PclHint>
              </PclCard>
            </template>
          </div>
</template>

<script setup lang="ts">
import { inject } from 'vue';
import { PclSetupContextKey } from './PclSetupContext';
import PclButton from '../../components/PclButton.vue';
import PclCard from '../../components/PclCard.vue';
import PclHint from '../../components/PclHint.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclListItem from '../../components/PclListItem.vue';
import PclLoading from '../../components/PclLoading.vue';

const context = inject(PclSetupContextKey);
if (!context) throw new Error('PclSetupContext is missing');
const { t, props, settings, scope, busy, execute, drafts, languages, languageId, useSystemLocale, languagesLoading, runtimeInfo, updateState, updateCheck, updateChecked, updateChecking, proxy, proxyTestUrl, proxyTestResult, cacheState, cacheRoutes, cacheBusy, cacheError, onlineBackends, onlineBusy, isInstance, windowMode, mirrorType, windowOptions, mirrorOptions, downloadBackendOptions, modApiMirrorOptions, proxyTypeOptions, consoleColorOptions, consoleFontOptions, assetVerificationOptions, assetVerificationDescriptions, languageOptions, pathSettings, toolPathSettings, ariaProxyDisabled, formatBytes, applicationVersion, credits, contributors, projectUrl, licenseUrl, docsUrl, bmclApiUrl, mcimUrl, maxMem, memoryMode, memoryTotalGiB, memoryUsedGiB, memoryUsedPercent, memoryAllocAfterPercent, sectionOverrideGroups, error, notice, val, num, bool, draftNumber, draftValue, formatMib, formatSpeed, setWindowMode, save, saveMirrorType, savePasteUrl, chooseGlobalDataPacks, chooseDirectory, chooseFilePath, secretConfigured, onMemoryMode, refreshSection, verifyAndTrust, loadOnlineBackends, loadCache, purgeYggdrasilCache, clearMetadataCache, loadLanguages, selectLanguage, setSystemLocale, loadRuntime, loadUpdate, configureUpdate, checkUpdate, applyUpdate, loadProxy, saveProxy, testProxy, openUrl, openPath, openLogs, openUpdateLog, section } = context;
</script>

