<template>
<div class="pcl-settings-section">
            <PclCard :title="t('pcl.auto2.k672450779d')">
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k320fb2d05d') }}</span>
                <div class="pcl-form-row__control"><PclSelect :model-value="draftNumber('AssetVerificationMode', 1)" :options="assetVerificationOptions" @update:model-value="save('AssetVerificationMode', $event)" /></div>
              </div>
              <p class="pcl-muted">{{ assetVerificationDescriptions[draftNumber('AssetVerificationMode', 1)] }}</p>
              <div v-if="draftNumber('AssetVerificationMode', 1) === 2" class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.kf61c84d95c') }}</span>
                <div class="pcl-form-row__control"><PclNumberInput :label="t('pcl.auto2.kf61c84d95c')" :model-value="draftNumber('AssetCacheExpiryDays', 7)" :min="1" :max="365" :unit="t('pcl.setup.days')" @update:model-value="save('AssetCacheExpiryDays', $event)" /></div>
              </div>
            </PclCard>
            <PclCard v-if="isInstance" :title="t('pcl.auto2.k88cd2c41dc')">
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k389f4e6753') }}</span>
                <div class="pcl-form-row__control"><strong>{{ bool('AssetIntegrityTrusted') ? t('pcl.auto2.k0a9bd11e25') : t('pcl.auto2.k67ea0c37b8') }}</strong></div>
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.kde492f2031') }}</span>
              </div>
              <div class="pcl-actions">
                <PclButton variant="highlight" icon="shield-check" :disabled="busy" @click="verifyAndTrust">{{ t('pcl.auto2.k2fd46d0f03') }}</PclButton>
                <PclButton v-if="bool('AssetIntegrityTrusted')" variant="ghost" icon="x" :disabled="busy" @click="save('AssetIntegrityTrusted', false)">{{ t('pcl.auto2.kb614311d42') }}</PclButton>
              </div>
              <PclHint theme="yellow">{{ t('pcl.auto2.kd65b7ff3b2') }}</PclHint>
            </PclCard>
            <PclCard :title="t('pcl.auto2.kd5a33bb7e7')">
              <div class="pcl-check-grid"><PclCheckbox :model-value="bool('OnlineFixes')" :label="t('pcl.auto2.k72587f73c0')" @update:model-value="save('OnlineFixes', $event)" /><PclCheckbox :model-value="bool('UseNativeGLFW')" :label="t('pcl.auto2.kb6565ef62e')" @update:model-value="save('UseNativeGLFW', $event)" /><PclCheckbox :model-value="bool('UseNativeOpenAL')" :label="t('pcl.auto2.k7377d6632f')" @update:model-value="save('UseNativeOpenAL', $event)" /><PclCheckbox :model-value="bool('UseDiscreteGpu')" :label="t('pcl.auto2.k3ddf18f61b')" @update:model-value="save('UseDiscreteGpu', $event)" /><PclCheckbox :model-value="bool('EnableFeralGamemode')" :label="t('pcl.auto2.k71a5c775a3')" @update:model-value="save('EnableFeralGamemode', $event)" /><PclCheckbox :model-value="bool('EnableMangoHud')" :label="t('pcl.auto2.k69c86767a7')" @update:model-value="save('EnableMangoHud', $event)" /><PclCheckbox :model-value="bool('UseZink')" :label="t('pcl.auto2.k5a5d182f09')" @update:model-value="save('UseZink', $event)" /></div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k57dc7ecf81') }}</span>
                <div class="pcl-form-row__control"><PclInput :disabled="!bool('UseNativeGLFW')" :model-value="draftValue('CustomGLFWPath')" :placeholder="t('pcl.auto2.k5ae521723c')" @update:model-value="drafts.CustomGLFWPath = $event" @blur="save('CustomGLFWPath', drafts.CustomGLFWPath)" /><PclIconButton icon="folder-open" :label="t('pcl.auto2.k163290badb')" :size="28" :disabled="busy || !bool('UseNativeGLFW')" @click="chooseFilePath('CustomGLFWPath', t('pcl.auto2.k57dc7ecf81'))" /></div>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k0c29921cb8') }}</span>
                <div class="pcl-form-row__control"><PclInput :disabled="!bool('UseNativeOpenAL')" :model-value="draftValue('CustomOpenALPath')" :placeholder="t('pcl.auto2.k5ae521723c')" @update:model-value="drafts.CustomOpenALPath = $event" @blur="save('CustomOpenALPath', drafts.CustomOpenALPath)" /><PclIconButton icon="folder-open" :label="t('pcl.auto2.kf372315ecd')" :size="28" :disabled="busy || !bool('UseNativeOpenAL')" @click="chooseFilePath('CustomOpenALPath', t('pcl.auto2.k0c29921cb8'))" /></div>
              </div>
            </PclCard>
            <PclCard :title="t('pcl.auto2.k467ce31a84')">
              <div class="pcl-check-grid"><PclCheckbox :model-value="bool('ShowGameTime')" :label="t('pcl.auto2.kff9e4486e9')" @update:model-value="save('ShowGameTime', $event)" /><PclCheckbox v-if="!isInstance" :model-value="bool('ShowGlobalGameTime')" :label="t('pcl.auto2.k60f26bd0b9')" @update:model-value="save('ShowGlobalGameTime', $event)" /><PclCheckbox v-if="!isInstance" :model-value="bool('ShowGameTimeWithoutDays')" :label="t('pcl.auto2.k2ee9de6d49')" @update:model-value="save('ShowGameTimeWithoutDays', $event)" /><PclCheckbox :model-value="bool('RecordGameTime')" :label="t('pcl.auto2.k19241e920e')" @update:model-value="save('RecordGameTime', $event)" /></div>
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
import PclIconButton from '../../components/PclIconButton.vue';
import PclInput from '../../components/PclInput.vue';
import PclNumberInput from '../../components/PclNumberInput.vue';
import PclSelect from '../../components/PclSelect.vue';

const context = inject(PclSetupContextKey);
if (!context) throw new Error('PclSetupContext is missing');
const { t, props, settings, scope, busy, execute, drafts, languages, languageId, useSystemLocale, languagesLoading, runtimeInfo, updateState, updateCheck, updateChecked, updateChecking, proxy, proxyTestUrl, proxyTestResult, cacheState, cacheRoutes, cacheBusy, cacheError, onlineBackends, onlineBusy, isInstance, windowMode, mirrorType, windowOptions, mirrorOptions, downloadBackendOptions, modApiMirrorOptions, proxyTypeOptions, consoleColorOptions, consoleFontOptions, assetVerificationOptions, assetVerificationDescriptions, languageOptions, pathSettings, toolPathSettings, ariaProxyDisabled, formatBytes, applicationVersion, credits, contributors, projectUrl, licenseUrl, docsUrl, bmclApiUrl, mcimUrl, maxMem, memoryMode, memoryTotalGiB, memoryUsedGiB, memoryUsedPercent, memoryAllocAfterPercent, sectionOverrideGroups, error, notice, val, num, bool, draftNumber, draftValue, formatMib, formatSpeed, setWindowMode, save, saveMirrorType, savePasteUrl, chooseGlobalDataPacks, chooseDirectory, chooseFilePath, secretConfigured, onMemoryMode, refreshSection, verifyAndTrust, loadOnlineBackends, loadCache, purgeYggdrasilCache, clearMetadataCache, loadLanguages, selectLanguage, setSystemLocale, loadRuntime, loadUpdate, configureUpdate, checkUpdate, applyUpdate, loadProxy, saveProxy, testProxy, openUrl, openPath, openLogs, openUpdateLog, section } = context;
</script>

