<template>
<div class="pcl-settings-section">
            <PclCard :title="t('pcl.auto2.k6ebe1abb5d')">
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k1165baa252') }}</span>
                <div class="pcl-form-row__control">
                  <PclSelect
                    :model-value="num('DownloadBackend', 2)"
                    :options="downloadBackendOptions"
                    @update:model-value="save('DownloadBackend', $event)"
                  />
                </div>
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.k2a2e0161b1') }}</span>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k64327d356b') }}</span>
                <div class="pcl-form-row__control">
                  <PclSelect
                    :model-value="mirrorType"
                    :options="mirrorOptions"
                    @update:model-value="saveMirrorType($event)"
                  />
                </div>
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.k01b7899496') }}</span>
              </div>

              <template v-if="mirrorType === 2">
                <PclHint theme="blue" class="pcl-advanced-hint">{{ t('pcl.auto2.kccbd1ffb95') }}</PclHint>
                <div class="pcl-form-row">
                  <span class="pcl-form-row__label">{{ t('pcl.auto2.kdf4e39f071') }}</span>
                  <div class="pcl-form-row__control">
                    <PclInput
                      :model-value="draftValue('MetaURLOverride', '')"
                      :placeholder="t('pcl.auto2.k1cec1f1b79')"
                      @update:model-value="drafts.MetaURLOverride = $event"
                      @blur="save('MetaURLOverride', drafts.MetaURLOverride)"
                    />
                  </div>
                </div>
                <div class="pcl-form-row">
                  <span class="pcl-form-row__label">{{ t('pcl.auto2.k2fa65ccaca') }}</span>
                  <div class="pcl-form-row__control">
                    <PclInput
                      :model-value="draftValue('MojangDownloadsMirrorURL', '')"
                      placeholder="https://mirror.example.com/"
                      @update:model-value="drafts.MojangDownloadsMirrorURL = $event"
                      @blur="save('MojangDownloadsMirrorURL', drafts.MojangDownloadsMirrorURL)"
                    />
                  </div>
                </div>
                <div class="pcl-form-row">
                  <span class="pcl-form-row__label">{{ t('pcl.auto2.kc0ad229574') }}</span>
                  <div class="pcl-form-row__control">
                    <PclInput
                      :model-value="draftValue('LibrariesURL', '')"
                      :placeholder="t('pcl.auto2.k041dea94f8')"
                      @update:model-value="drafts.LibrariesURL = $event"
                      @blur="save('LibrariesURL', drafts.LibrariesURL)"
                    />
                  </div>
                </div>
                <div class="pcl-form-row">
                  <span class="pcl-form-row__label">{{ t('pcl.auto2.k0e1fb9c698') }}</span>
                  <div class="pcl-form-row__control">
                    <PclInput
                      :model-value="draftValue('ResourceURLOverride', '')"
                      :placeholder="t('pcl.auto2.k3fe4026953')"
                      @update:model-value="drafts.ResourceURLOverride = $event"
                      @blur="save('ResourceURLOverride', drafts.ResourceURLOverride)"
                    />
                  </div>
                </div>
                <div class="pcl-form-row">
                  <span class="pcl-form-row__label">{{ t('pcl.auto2.kc776b5ccce') }}</span>
                  <div class="pcl-form-row__control">
                    <PclInput
                      :model-value="draftValue('LegacyFMLLibsURLOverride', '')"
                      :placeholder="t('pcl.auto2.kecc137ec78')"
                      @update:model-value="drafts.LegacyFMLLibsURLOverride = $event"
                      @blur="save('LegacyFMLLibsURLOverride', drafts.LegacyFMLLibsURLOverride)"
                    />
                  </div>
                </div>
              </template>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k639b8f8d30') }}</span>
                <div class="pcl-form-row__control">
                  <PclSelect
                    :model-value="draftNumber('ModrinthMirror', 0)"
                    :options="modApiMirrorOptions"
                    @update:model-value="save('ModrinthMirror', $event)"
                  />
                  <PclInput
                    v-if="draftNumber('ModrinthMirror', 0) === 2"
                    :model-value="draftValue('ModrinthMirrorURL', '')"
                    :placeholder="t('pcl.auto2.kedac1dee52')"
                    @update:model-value="drafts.ModrinthMirrorURL = $event"
                    @blur="save('ModrinthMirrorURL', drafts.ModrinthMirrorURL)"
                  />
                </div>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.kd3e7415b66') }}</span>
                <div class="pcl-form-row__control">
                  <PclSelect
                    :model-value="draftNumber('CurseForgeMirror', 0)"
                    :options="modApiMirrorOptions"
                    @update:model-value="save('CurseForgeMirror', $event)"
                  />
                  <PclInput
                    v-if="draftNumber('CurseForgeMirror', 0) === 2"
                    :model-value="draftValue('CurseForgeMirrorURL', '')"
                    :placeholder="t('pcl.auto2.k05899d2a38')"
                    @update:model-value="drafts.CurseForgeMirrorURL = $event"
                    @blur="save('CurseForgeMirrorURL', drafts.CurseForgeMirrorURL)"
                  />
                </div>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.kf1b19d21bf') }}</span>
                <div class="pcl-form-row__control">
                  <PclSlider
                    :model-value="draftNumber('PclDownloadThreadLimit', 8)"
                    :min="1"
                    :max="64"
                    :step="1"
                    @update:model-value="drafts.PclDownloadThreadLimit = $event"
                    @change="save('PclDownloadThreadLimit', $event)"
                  />
                </div>
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.k2e3f00b939') }}</span>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k7b5aca21d4') }}</span>
                <div class="pcl-form-row__control">
                  <PclSlider
                    :model-value="draftNumber('PclDownloadSpeedLimitKBps', 0)"
                    :min="0"
                    :max="102400"
                    :step="1024"
                    :format="formatSpeed"
                    @update:model-value="drafts.PclDownloadSpeedLimitKBps = $event"
                    @change="save('PclDownloadSpeedLimitKBps', $event)"
                  />
                </div>
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.k71fd57e730') }}</span>
              </div>

              <PclCheckbox  :model-value="bool('PclDownloadFallbackToQt')" :label="t('pcl.auto2.k9d5b1f1173')" @update:model-value="save('PclDownloadFallbackToQt', $event)" />
              <PclCheckbox :model-value="bool('McModSearchEnabled')" :label="t('resource.mcmodSearchEnabled')" @update:model-value="save('McModSearchEnabled', $event)" />
              <PclHint theme="blue">{{ t('resource.mcmodSearchHint') }}</PclHint>
            </PclCard>
            <PclCard :title="t('pcl.auto2.k1ed8c078d6')">
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.ka832ab6015') }}</span>
                <div class="pcl-form-row__control"><PclNumberInput :label="t('pcl.auto2.ka832ab6015')" :model-value="draftNumber('NumberOfConcurrentTasks', 10)" :min="1" :max="99" @update:model-value="save('NumberOfConcurrentTasks', $event)" /></div>
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.k7571a8cbc9') }}</span>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.ka55f917b70') }}</span>
                <div class="pcl-form-row__control"><PclNumberInput :label="t('pcl.auto2.ka55f917b70')" :model-value="draftNumber('NumberOfConcurrentDownloads', 6)" :min="1" :max="99" @update:model-value="save('NumberOfConcurrentDownloads', $event)" /></div>
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.k15d325f0de') }}</span>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.ka9131cbf3c') }}</span>
                <div class="pcl-form-row__control"><PclNumberInput :label="t('pcl.auto2.ka9131cbf3c')" :model-value="draftNumber('NumberOfManualRetries', 1)" :min="0" :max="99" @update:model-value="save('NumberOfManualRetries', $event)" /></div>
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.kf3191dd279') }}</span>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.ke41ce24cf7') }}</span>
                <div class="pcl-form-row__control"><PclNumberInput :label="t('pcl.auto2.ke41ce24cf7')" :model-value="draftNumber('RequestTimeout', 60)" :min="0" :max="99" @update:model-value="save('RequestTimeout', $event)" /></div>
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.keb6aaba1a1') }}</span>
              </div>
            </PclCard>

            <PclCard :title="t('pcl.auto2.k09ca00a842')">
              <div class="pcl-check-grid">
                <PclCheckbox :model-value="bool('Aria2Enabled')" :label="t('pcl.auto2.kcc4cad514a')" @update:model-value="save('Aria2Enabled', $event)" />
                <PclCheckbox :model-value="bool('Aria2AutoInstall')" :label="t('pcl.auto2.k6b79c51cad')" @update:model-value="save('Aria2AutoInstall', $event)" />
                <PclCheckbox :model-value="bool('Aria2FallbackToQt')" :label="t('pcl.auto2.k303ee36411')" @update:model-value="save('Aria2FallbackToQt', $event)" />
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.kb9f4c716af') }}</span>
                <div class="pcl-form-row__control"><PclInput :model-value="draftValue('Aria2ExecutablePath', '')" :placeholder="t('pcl.auto2.kc073f7d55a')" @update:model-value="drafts.Aria2ExecutablePath = $event" @blur="save('Aria2ExecutablePath', drafts.Aria2ExecutablePath)" /><PclIconButton icon="folder-open" :label="t('pcl.auto2.k0323866759')" :size="28" :disabled="busy" @click="chooseFilePath('Aria2ExecutablePath', t('pcl.auto2.ke332de5306'))" /></div>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k32d4fcb63a') }}</span>
                <div class="pcl-form-row__control"><PclNumberInput :label="t('pcl.auto2.k26d00a6d80')" :model-value="draftNumber('Aria2MaxConcurrentDownloads', 6)" :min="1" :max="128" :disabled="bool('Aria2FollowLauncherDownloadLimits')" @update:model-value="save('Aria2MaxConcurrentDownloads', $event)" /></div>
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.k184120fd00') }}</span>
              </div>
              <PclCheckbox :model-value="bool('Aria2FollowLauncherDownloadLimits')" :label="t('pcl.auto2.k219d3abd7a')" @update:model-value="save('Aria2FollowLauncherDownloadLimits', $event)" />
            </PclCard>

            <PclCard :title="t('pcl.auto2.kd6b3cf622a')" collapsible :open="true">
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.k9ccf3415b8') }}</span><div class="pcl-form-row__control"><PclNumberInput :label="t('pcl.auto2.k9ccf3415b8')" :model-value="draftNumber('Aria2MaxConnectionPerServer', 8)" :min="1" :max="64" @update:model-value="save('Aria2MaxConnectionPerServer', $event)" /></div></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.k35a288dea7') }}</span><div class="pcl-form-row__control"><PclNumberInput :label="t('pcl.auto2.k35a288dea7')" :model-value="draftNumber('Aria2Split', 8)" :min="1" :max="64" @update:model-value="save('Aria2Split', $event)" /></div></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.kd5c81bf7ef') }}</span><div class="pcl-form-row__control"><PclInput :model-value="draftValue('Aria2MinSplitSize', '1M')" :placeholder="t('pcl.auto2.kcfafbaef6e')" @update:model-value="drafts.Aria2MinSplitSize = $event" @blur="save('Aria2MinSplitSize', drafts.Aria2MinSplitSize)" /></div></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.k5016a7bffb') }}</span><div class="pcl-form-row__control"><PclSelect :model-value="draftValue('Aria2FileAllocation', 'none')" :options="[{ label: t('pcl.auto2.ka56ac5dc51'), value: 'none' }, { label: t('pcl.auto2.k9e16271d26'), value: 'prealloc' }, { label: t('pcl.auto2.kce920fef83'), value: 'trunc' }, { label: 'falloc', value: 'falloc' }]" @update:model-value="save('Aria2FileAllocation', $event)" /></div></div>
              <PclCheckbox :model-value="bool('Aria2Continue')" :label="t('pcl.auto2.k108a36d8d6')" @update:model-value="save('Aria2Continue', $event)" />
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.k5f40f75c95') }}</span><div class="pcl-form-row__control"><PclNumberInput :label="t('pcl.auto2.k5f40f75c95')" :model-value="draftNumber('Aria2RpcPort', 0)" :min="0" :max="65535" @update:model-value="save('Aria2RpcPort', $event)" /></div><span class="pcl-form-row__hint">{{ t('pcl.auto2.k8195cb3375') }}</span></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.k3f6690e65a') }}</span><div class="pcl-form-row__control"><PclNumberInput :label="t('pcl.auto2.k3f6690e65a')" :model-value="draftNumber('Aria2PollInterval', 500)" :min="250" :max="10000" @update:model-value="save('Aria2PollInterval', $event)" /></div><span class="pcl-form-row__hint">{{ t('pcl.auto2.kf3c3d56984') }}</span></div>
            </PclCard>

            <PclCard :title="t('pcl.auto2.k319ee56426')" collapsible>
              <PclCheckbox :model-value="bool('Aria2UseLauncherProxy')" :label="t('pcl.auto2.k48264472f0')" @update:model-value="save('Aria2UseLauncherProxy', $event)" />
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.k6caded1529') }}</span><div class="pcl-form-row__control"><PclSelect :model-value="draftValue('Aria2ProxyType', 'None')" :disabled="bool('Aria2UseLauncherProxy')" :options="[{ label: t('pcl.auto2.k4f187022e0'), value: 'None' }, { label: 'HTTP', value: 'HTTP' }, { label: 'SOCKS5', value: 'SOCKS5' }]" @update:model-value="save('Aria2ProxyType', $event)" /></div></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.kdd39ea863c') }}</span><div class="pcl-form-row__control"><PclInput :model-value="draftValue('Aria2ProxyAddr', '')" :disabled="ariaProxyDisabled" placeholder="127.0.0.1" @update:model-value="drafts.Aria2ProxyAddr = $event" @blur="save('Aria2ProxyAddr', drafts.Aria2ProxyAddr)" /><PclNumberInput :label="t('pcl.auto2.kb2e8679398')" :model-value="draftNumber('Aria2ProxyPort', 8080)" :min="1" :max="65535" :disabled="ariaProxyDisabled" @update:model-value="save('Aria2ProxyPort', $event)" /></div></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.kbd05aec3ce') }}</span><div class="pcl-form-row__control"><PclInput :model-value="draftValue('Aria2ProxyUser', '')" :disabled="ariaProxyDisabled" :placeholder="t('pcl.auto2.k53e32830a5')" @update:model-value="drafts.Aria2ProxyUser = $event" @blur="save('Aria2ProxyUser', drafts.Aria2ProxyUser)" /><PclSecretInput :label="t('pcl.auto2.kbd675552e9')" :configured="secretConfigured('Aria2ProxyPass')" :disabled="ariaProxyDisabled" :save="value => save('Aria2ProxyPass', value)" /></div></div>
              <div class="pcl-form-row pcl-form-row--top"><span class="pcl-form-row__label">{{ t('pcl.auto2.k2f42122d58') }}</span><div class="pcl-form-row__control pcl-form-row__control--column"><PclInput multiline :rows="3" :model-value="draftValue('Aria2ExtraArgs', '')" :placeholder="t('pcl.auto2.k4eb33f3f06')" @update:model-value="drafts.Aria2ExtraArgs = $event" @blur="save('Aria2ExtraArgs', drafts.Aria2ExtraArgs)" /></div></div>
            </PclCard>

            <PclCard :title="t('pcl.auto2.k5cd6bf739a')">
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.kf6e39cb4d2') }}</span><div class="pcl-form-row__control"><PclSelect :model-value="draftValue('CurseForgeDownloadBrowser', 'Embedded')" :options="[{ label: t('pcl.auto2.k46a01866fe'), value: 'Embedded' }, { label: t('pcl.auto2.kcb80f053de'), value: 'System' }, { label: t('pcl.auto2.k1a818fb254'), value: 'External' }]" @update:model-value="save('CurseForgeDownloadBrowser', $event)" /></div></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('ui.modrinthToken') }}</span><div class="pcl-form-row__control"><PclSecretInput :label="t('ui.modrinthToken')" :configured="secretConfigured('ModrinthToken')" :disabled="busy" :save="value => save('ModrinthToken', value)" /></div></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('ui.curseforgeApiKey') }}</span><div class="pcl-form-row__control"><PclSecretInput :label="t('ui.curseforgeApiKey')" :configured="secretConfigured('FlameKeyOverride')" :disabled="busy" :save="value => save('FlameKeyOverride', value)" /></div></div>
            </PclCard>
          </div>
</template>

<script setup lang="ts">
import { inject } from 'vue';
import { PclSetupContextKey } from './PclSetupContext';
import PclCard from '../../components/PclCard.vue';
import PclCheckbox from '../../components/PclCheckbox.vue';
import PclHint from '../../components/PclHint.vue';
import PclIconButton from '../../components/PclIconButton.vue';
import PclInput from '../../components/PclInput.vue';
import PclNumberInput from '../../components/PclNumberInput.vue';
import PclSecretInput from '../../components/PclSecretInput.vue';
import PclSelect from '../../components/PclSelect.vue';
import PclSlider from '../../components/PclSlider.vue';

const context = inject(PclSetupContextKey);
if (!context) throw new Error('PclSetupContext is missing');
const { t, props, settings, scope, busy, execute, drafts, languages, languageId, useSystemLocale, languagesLoading, runtimeInfo, updateState, updateCheck, updateChecked, updateChecking, proxy, proxyTestUrl, proxyTestResult, cacheState, cacheRoutes, cacheBusy, cacheError, onlineBackends, onlineBusy, isInstance, windowMode, mirrorType, windowOptions, mirrorOptions, downloadBackendOptions, modApiMirrorOptions, proxyTypeOptions, consoleColorOptions, consoleFontOptions, assetVerificationOptions, assetVerificationDescriptions, languageOptions, pathSettings, toolPathSettings, ariaProxyDisabled, formatBytes, applicationVersion, credits, contributors, projectUrl, licenseUrl, docsUrl, bmclApiUrl, mcimUrl, maxMem, memoryMode, memoryTotalGiB, memoryUsedGiB, memoryUsedPercent, memoryAllocAfterPercent, sectionOverrideGroups, error, notice, val, num, bool, draftNumber, draftValue, formatMib, formatSpeed, setWindowMode, save, saveMirrorType, savePasteUrl, chooseGlobalDataPacks, chooseDirectory, chooseFilePath, secretConfigured, onMemoryMode, refreshSection, verifyAndTrust, loadOnlineBackends, loadCache, purgeYggdrasilCache, clearMetadataCache, loadLanguages, selectLanguage, setSystemLocale, loadRuntime, loadUpdate, configureUpdate, checkUpdate, applyUpdate, loadProxy, saveProxy, testProxy, openUrl, openPath, openLogs, openUpdateLog, section } = context;
</script>

