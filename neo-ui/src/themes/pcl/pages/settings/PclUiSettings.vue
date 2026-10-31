<template>
<div class="pcl-settings-section">
            <PclAppearancePanel />
            <PclCard :title="t('pcl.auto2.k119e538904')" collapsible>
              <PclHint theme="blue">{{ t('pcl.auto2.k4bc53f2a29') }}</PclHint>
              <div class="pcl-check-grid"><PclCheckbox :model-value="bool('MenuBarInsteadOfToolBar')" :label="t('pcl.auto2.kcfe01d2e68')" @update:model-value="save('MenuBarInsteadOfToolBar', $event)" /><PclCheckbox :model-value="bool('UseNewUI')" :label="t('pcl.auto2.kfd0f0be669')" @update:model-value="save('UseNewUI', $event)" /><PclCheckbox :model-value="bool('ShowNewsBar')" :label="t('pcl.auto2.kbb3e9b3130')" @update:model-value="save('ShowNewsBar', $event)" /><PclCheckbox :model-value="bool('StatusBarVisible')" :label="t('pcl.auto2.ka80d0756b7')" @update:model-value="save('StatusBarVisible', $event)" /><PclCheckbox :model-value="bool('ShowServerPreview')" :label="t('pcl.auto2.k7fc810e95d')" @update:model-value="save('ShowServerPreview', $event)" /><PclCheckbox :model-value="bool('ServerPreviewDynamicWidth')" :label="t('pcl.auto2.k7372f1409c')" @update:model-value="save('ServerPreviewDynamicWidth', $event)" /><PclCheckbox :model-value="bool('ShowTerracottaToolBar')" :label="t('pcl.auto2.kbf6344c295')" @update:model-value="save('ShowTerracottaToolBar', $event)" /><PclCheckbox :model-value="bool('ShowYukariConnectToolBar')" :label="t('pcl.auto2.k875aaa909c')" @update:model-value="save('ShowYukariConnectToolBar', $event)" /><PclCheckbox :model-value="bool('ShowTerracottaStatusBar')" :label="t('pcl.auto2.k13ab5bf5eb')" @update:model-value="save('ShowTerracottaStatusBar', $event)" /><PclCheckbox :model-value="bool('ShowYukariConnectStatusBar')" :label="t('pcl.auto2.ke107eb3edb')" @update:model-value="save('ShowYukariConnectStatusBar', $event)" /></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.k0ac5a748d1') }}</span><div class="pcl-form-row__control"><PclSelect :model-value="draftValue('InstSortMode', 'Name')" :options="[{ label: t('pcl.auto2.k0ea96767da'), value: 'Name' }, { label: t('pcl.auto2.kf04b9a3f81'), value: 'LastLaunch' }]" @update:model-value="save('InstSortMode', $event)" /></div></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.kaa6f9f5c04') }}</span><div class="pcl-form-row__control"><PclSelect :model-value="draftValue('InstRenamingMode', 'AskEverytime')" :options="[{ label: t('pcl.auto2.k393704c0f8'), value: 'AskEverytime' }, { label: t('pcl.auto2.ka2c67e7514'), value: 'PhysicalDir' }, { label: t('pcl.auto2.k570d7259cd'), value: 'MetadataOnly' }]" @update:model-value="save('InstRenamingMode', $event)" /></div></div>
            </PclCard>

          </div>
</template>

<script setup lang="ts">
import { inject } from 'vue';
import { PclSetupContextKey } from './PclSetupContext';
import PclAppearancePanel from './PclAppearancePanel.vue';
import PclCard from '../../components/PclCard.vue';
import PclCheckbox from '../../components/PclCheckbox.vue';
import PclHint from '../../components/PclHint.vue';
import PclSelect from '../../components/PclSelect.vue';

const context = inject(PclSetupContextKey);
if (!context) throw new Error('PclSetupContext is missing');
const { t, props, settings, scope, busy, execute, drafts, languages, languageId, useSystemLocale, languagesLoading, runtimeInfo, updateState, updateCheck, updateChecked, updateChecking, proxy, proxyTestUrl, proxyTestResult, cacheState, cacheRoutes, cacheBusy, cacheError, onlineBackends, onlineBusy, isInstance, windowMode, mirrorType, windowOptions, mirrorOptions, downloadBackendOptions, modApiMirrorOptions, proxyTypeOptions, consoleColorOptions, consoleFontOptions, assetVerificationOptions, assetVerificationDescriptions, languageOptions, pathSettings, toolPathSettings, ariaProxyDisabled, formatBytes, applicationVersion, credits, contributors, projectUrl, licenseUrl, docsUrl, bmclApiUrl, mcimUrl, maxMem, memoryMode, memoryTotalGiB, memoryUsedGiB, memoryUsedPercent, memoryAllocAfterPercent, sectionOverrideGroups, error, notice, val, num, bool, draftNumber, draftValue, formatMib, formatSpeed, setWindowMode, save, saveMirrorType, savePasteUrl, chooseGlobalDataPacks, chooseDirectory, chooseFilePath, secretConfigured, onMemoryMode, refreshSection, verifyAndTrust, loadOnlineBackends, loadCache, purgeYggdrasilCache, clearMetadataCache, loadLanguages, selectLanguage, setSystemLocale, loadRuntime, loadUpdate, configureUpdate, checkUpdate, applyUpdate, loadProxy, saveProxy, testProxy, openUrl, openPath, openLogs, openUpdateLog, section } = context;
</script>

