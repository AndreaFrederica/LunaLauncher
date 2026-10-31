<template>
<div class="pcl-settings-section">
            <PclCard :title="t('pcl.auto2.kcd99b225a0')">
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.kd5d8edd12c') }}</span>
                <div class="pcl-form-row__control">
                  <PclSelect
                    :model-value="languageId"
                    :options="languageOptions"
                    :placeholder="t('pcl.auto2.kf4bbd91f79')"
                    @update:model-value="selectLanguage"
                  />
                </div>
                <PclButton icon="refresh-cw" :disabled="busy || languagesLoading" @click="loadLanguages(true)">{{ t('pcl.auto2.k38108eaa1d') }}</PclButton>
              </div>
              <PclCheckbox :model-value="useSystemLocale" :label="t('pcl.auto2.k726bce844f')" @update:model-value="setSystemLocale" />
              <PclLoading v-if="languagesLoading" :text="t('pcl.auto2.kffd7a06887')" inline />
            </PclCard>

            <div class="pcl-language-banner">
              <PclIcon name="earth" :size="190" class="pcl-language-banner__earth" />
              <div class="pcl-language-banner__copy">
                <div class="pcl-language-banner__title">{{ t('pcl.auto2.k12628589c6') }}<b>Luna</b>{{ t('pcl.auto2.k256bab05d0') }}</div>
                <p>{{ t('pcl.auto2.k23e1871996') }}</p>
                <p>{{ t('pcl.auto2.k2fd54e48a1') }}</p>
                <p>{{ t('pcl.auto2.kf3d356c728') }}</p>
                <div class="pcl-actions">
                  <PclButton variant="highlight" icon="upload" @click="openUrl(projectUrl + '/pulls')">{{ t('pcl.auto2.k5927da4a31') }}</PclButton>
                  <PclButton icon="book-marked" @click="openUrl(docsUrl)">{{ t('pcl.auto2.k550fd49891') }}</PclButton>
                </div>
              </div>
            </div>
          </div>
</template>

<script setup lang="ts">
import { inject } from 'vue';
import { PclSetupContextKey } from './PclSetupContext';
import PclButton from '../../components/PclButton.vue';
import PclCard from '../../components/PclCard.vue';
import PclCheckbox from '../../components/PclCheckbox.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclLoading from '../../components/PclLoading.vue';
import PclSelect from '../../components/PclSelect.vue';

const context = inject(PclSetupContextKey);
if (!context) throw new Error('PclSetupContext is missing');
const { t, props, settings, scope, busy, execute, drafts, languages, languageId, useSystemLocale, languagesLoading, runtimeInfo, updateState, updateCheck, updateChecked, updateChecking, proxy, proxyTestUrl, proxyTestResult, cacheState, cacheRoutes, cacheBusy, cacheError, onlineBackends, onlineBusy, isInstance, windowMode, mirrorType, windowOptions, mirrorOptions, downloadBackendOptions, modApiMirrorOptions, proxyTypeOptions, consoleColorOptions, consoleFontOptions, assetVerificationOptions, assetVerificationDescriptions, languageOptions, pathSettings, toolPathSettings, ariaProxyDisabled, formatBytes, applicationVersion, credits, contributors, projectUrl, licenseUrl, docsUrl, bmclApiUrl, mcimUrl, maxMem, memoryMode, memoryTotalGiB, memoryUsedGiB, memoryUsedPercent, memoryAllocAfterPercent, sectionOverrideGroups, error, notice, val, num, bool, draftNumber, draftValue, formatMib, formatSpeed, setWindowMode, save, saveMirrorType, savePasteUrl, chooseGlobalDataPacks, chooseDirectory, chooseFilePath, secretConfigured, onMemoryMode, refreshSection, verifyAndTrust, loadOnlineBackends, loadCache, purgeYggdrasilCache, clearMetadataCache, loadLanguages, selectLanguage, setSystemLocale, loadRuntime, loadUpdate, configureUpdate, checkUpdate, applyUpdate, loadProxy, saveProxy, testProxy, openUrl, openPath, openLogs, openUpdateLog, section } = context;
</script>

