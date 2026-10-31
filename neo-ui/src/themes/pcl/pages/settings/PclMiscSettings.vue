<template>
<div class="pcl-settings-section">
            <PclCard v-if="!isInstance" :title="t('pcl.auto2.k0cbda6b524')">
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k6caded1529') }}</span>
                <div class="pcl-form-row__control">
                  <PclSelect v-model="proxy.type" :options="proxyTypeOptions" />
                </div>
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.ke3fde22bff') }}</span>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k67d2d7970f') }}</span>
                <div class="pcl-form-row__control">
                  <PclInput v-model="proxy.host" placeholder="127.0.0.1" />
                </div>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k6cbb7335b5') }}</span>
                <div class="pcl-form-row__control">
                  <PclInput v-model.number="proxy.port" type="number" placeholder="7890" style="width: 110px" />
                </div>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.kbd05aec3ce') }}</span>
                <div class="pcl-form-row__control">
                  <PclInput v-model="proxy.username" :placeholder="t('pcl.auto2.k53e32830a5')" />
                  <PclInput v-model="proxy.password" type="password" :placeholder="t('pcl.auto2.k53e32830a5')" />
                </div>
              </div>
              <div class="pcl-actions">
                <PclButton variant="highlight" icon="save" :disabled="busy" @click="saveProxy">{{ t('pcl.auto2.kc249ff9d56') }}</PclButton>
                <PclButton icon="network" :disabled="busy" @click="testProxy">{{ t('pcl.auto2.k10b7d8eccc') }}</PclButton>
              </div>
              <PclHint theme="yellow">{{ t('pcl.auto2.kab43ea5794') }}</PclHint>
              <div v-if="proxyTestResult" class="pcl-diagnostic">{{ proxyTestResult }}</div>
            </PclCard>

            <PclCard :title="t('pcl.auto2.k1758690108')" collapsible>
              <div class="pcl-check-grid">
                <PclCheckbox :model-value="bool('PclDownloadFallbackToQt')" :label="t('pcl.auto2.k30ad19d3a5')" @update:model-value="save('PclDownloadFallbackToQt', $event)" />
                <PclCheckbox :model-value="bool('Aria2FallbackToQt')" :label="t('pcl.auto2.ka8ffe0fe9c')" @update:model-value="save('Aria2FallbackToQt', $event)" />
                <PclCheckbox :model-value="bool('DownloadsDirWatchRecursive')" :label="t('pcl.auto2.kbdd3825058')" @update:model-value="save('DownloadsDirWatchRecursive', $event)" />
              </div>
            </PclCard>
          </div>
</template>

<script setup lang="ts">
import { inject } from 'vue';
import { PclSetupContextKey } from './PclSetupContext';
import PclButton from '../../components/PclButton.vue';
import PclCard from '../../components/PclCard.vue';
import PclCheckbox from '../../components/PclCheckbox.vue';
import PclHint from '../../components/PclHint.vue';
import PclInput from '../../components/PclInput.vue';
import PclSelect from '../../components/PclSelect.vue';

const context = inject(PclSetupContextKey);
if (!context) throw new Error('PclSetupContext is missing');
const { t, props, settings, scope, busy, execute, drafts, languages, languageId, useSystemLocale, languagesLoading, runtimeInfo, updateState, updateCheck, updateChecked, updateChecking, proxy, proxyTestUrl, proxyTestResult, cacheState, cacheRoutes, cacheBusy, cacheError, onlineBackends, onlineBusy, isInstance, windowMode, mirrorType, windowOptions, mirrorOptions, downloadBackendOptions, modApiMirrorOptions, proxyTypeOptions, consoleColorOptions, consoleFontOptions, assetVerificationOptions, assetVerificationDescriptions, languageOptions, pathSettings, toolPathSettings, ariaProxyDisabled, formatBytes, applicationVersion, credits, contributors, projectUrl, licenseUrl, docsUrl, bmclApiUrl, mcimUrl, maxMem, memoryMode, memoryTotalGiB, memoryUsedGiB, memoryUsedPercent, memoryAllocAfterPercent, sectionOverrideGroups, error, notice, val, num, bool, draftNumber, draftValue, formatMib, formatSpeed, setWindowMode, save, saveMirrorType, savePasteUrl, chooseGlobalDataPacks, chooseDirectory, chooseFilePath, secretConfigured, onMemoryMode, refreshSection, verifyAndTrust, loadOnlineBackends, loadCache, purgeYggdrasilCache, clearMetadataCache, loadLanguages, selectLanguage, setSystemLocale, loadRuntime, loadUpdate, configureUpdate, checkUpdate, applyUpdate, loadProxy, saveProxy, testProxy, openUrl, openPath, openLogs, openUpdateLog, section } = context;
</script>

