<template>
<div class="pcl-settings-section">
            <PclCard :title="t('pcl.auto2.kd4cbc9e6a2')">
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k765b5418a7') }}</span>
                <div class="pcl-form-row__control pcl-check-row">
                  <PclCheckbox
                    :model-value="!bool('ModDependenciesDisabled')"
                    :label="t('pcl.auto2.k21941e05fb')"
                    @update:model-value="save('ModDependenciesDisabled', !$event)"
                  />
                  <PclCheckbox
                    :model-value="bool('GlobalDataPacksEnabled')"
                    :label="t('pcl.auto2.ke3ac026340')"
                    @update:model-value="save('GlobalDataPacksEnabled', $event)"
                  />
                </div>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.kec623c3f23') }}</span>
                <div class="pcl-form-row__control">
                  <PclInput
                    :model-value="draftValue('GlobalDataPacksPath', '')"
                    :placeholder="t('pcl.auto2.kf36e479c03')"
                    @update:model-value="drafts.GlobalDataPacksPath = $event"
                    @blur="save('GlobalDataPacksPath', drafts.GlobalDataPacksPath)"
                  />
                  <PclIconButton icon="folder-open" :label="t('pcl.auto2.k5f869fce49')" :size="28" :disabled="busy" @click="chooseGlobalDataPacks" />
                </div>
              </div>
            </PclCard>

            <PclCard :title="t('pcl.auto2.kb040e698ea')">
              <div class="pcl-check-grid">
                <PclCheckbox :model-value="bool('ModMetadataDisabled')" :label="t('pcl.auto2.k6e4b7a0c56')" @update:model-value="save('ModMetadataDisabled', $event)" />
                <PclCheckbox :model-value="bool('SkipModpackUpdatePrompt')" :label="t('pcl.auto2.kb444441195')" @update:model-value="save('SkipModpackUpdatePrompt', $event)" />
                <PclCheckbox :model-value="bool('ShowModIncompat')" :label="t('pcl.auto2.k5d8dbaa02a')" @update:model-value="save('ShowModIncompat', $event)" />
                <PclCheckbox :model-value="bool('FallbackMRBlockedMods')" :label="t('pcl.auto2.kbea2a5f6f6')" @update:model-value="save('FallbackMRBlockedMods', $event)" />
              </div>
            </PclCard>
          </div>
</template>

<script setup lang="ts">
import { inject } from 'vue';
import { PclSetupContextKey } from './PclSetupContext';
import PclCard from '../../components/PclCard.vue';
import PclCheckbox from '../../components/PclCheckbox.vue';
import PclIconButton from '../../components/PclIconButton.vue';
import PclInput from '../../components/PclInput.vue';

const context = inject(PclSetupContextKey);
if (!context) throw new Error('PclSetupContext is missing');
const { t, props, settings, scope, busy, execute, drafts, languages, languageId, useSystemLocale, languagesLoading, runtimeInfo, updateState, updateCheck, updateChecked, updateChecking, proxy, proxyTestUrl, proxyTestResult, cacheState, cacheRoutes, cacheBusy, cacheError, onlineBackends, onlineBusy, isInstance, windowMode, mirrorType, windowOptions, mirrorOptions, downloadBackendOptions, modApiMirrorOptions, proxyTypeOptions, consoleColorOptions, consoleFontOptions, assetVerificationOptions, assetVerificationDescriptions, languageOptions, pathSettings, toolPathSettings, ariaProxyDisabled, formatBytes, applicationVersion, credits, contributors, projectUrl, licenseUrl, docsUrl, bmclApiUrl, mcimUrl, maxMem, memoryMode, memoryTotalGiB, memoryUsedGiB, memoryUsedPercent, memoryAllocAfterPercent, sectionOverrideGroups, error, notice, val, num, bool, draftNumber, draftValue, formatMib, formatSpeed, setWindowMode, save, saveMirrorType, savePasteUrl, chooseGlobalDataPacks, chooseDirectory, chooseFilePath, secretConfigured, onMemoryMode, refreshSection, verifyAndTrust, loadOnlineBackends, loadCache, purgeYggdrasilCache, clearMetadataCache, loadLanguages, selectLanguage, setSystemLocale, loadRuntime, loadUpdate, configureUpdate, checkUpdate, applyUpdate, loadProxy, saveProxy, testProxy, openUrl, openPath, openLogs, openUpdateLog, section } = context;
</script>

