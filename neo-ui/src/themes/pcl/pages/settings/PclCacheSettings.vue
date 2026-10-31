<template>
<div class="pcl-settings-section">
            <PclCard :title="t('pcl.auto2.kab43b1565b')">
              <template #actions>
                <PclButton size="sm" icon="refresh-cw" :disabled="cacheBusy" @click="loadCache(true)">{{ t('pcl.auto2.k38108eaa1d') }}</PclButton>
              </template>
              <PclLoading v-if="cacheBusy" :text="t('pcl.auto2.k3e6f013d61')" inline />
              <PclHint v-if="cacheError" theme="red" closable @close="cacheError = ''">{{ cacheError }}</PclHint>
              <div v-if="cacheState.stats" class="pcl-about-grid">
                <span>{{ t('pcl.auto2.kd42da9e2ac') }}</span><code>{{ cacheState.running ? t('pcl.auto2.k5942497005') : t('pcl.auto2.kb2ba9634d9') }}</code>
                <span>{{ t('pcl.auto2.kc1c23347ef') }}</span><code>{{ cacheState.enabled && cacheState.configured ? t('pcl.auto2.k25d2843150') : t('pcl.auto2.k8bb38ef00c') }}</code>
                <span>{{ t('pcl.auto2.k31479ef943') }}</span><code>{{ cacheState.accountCacheEnabled ? t('pcl.auto2.k25d2843150') : t('pcl.auto2.k8bb38ef00c') }}</code>
                <span>{{ t('pcl.auto2.ke4b9d92f14') }}</span><code>{{ cacheState.stats.entries ?? 0 }}</code>
                <span>{{ t('pcl.auto2.kf05934a464') }}</span><code>{{ formatBytes(cacheState.stats.bytes) }}</code>
                <span>{{ t('pcl.auto2.kbc10822e07') }}</span><code>{{ cacheState.stats.hit ?? 0 }} / {{ cacheState.stats.miss ?? 0 }}</code>
                <span>{{ t('pcl.auto2.k121c44d74b') }}</span><code>{{ cacheState.metadataPath || '—' }}</code>
                <span>{{ t('pcl.auto2.k78acda3988') }}</span><code>{{ cacheState.texturesPath || '—' }}</code>
              </div>
              <PclHint v-else-if="!cacheBusy" theme="blue">{{ t('pcl.auto2.kd9647d9b52') }}</PclHint>
              <div class="pcl-actions">
                <PclButton variant="highlight" icon="trash-2" :disabled="cacheBusy || !cacheState.running" @click="purgeYggdrasilCache">{{ t('pcl.auto2.keb4580d4ae') }}</PclButton>
                <PclButton icon="trash-2" :disabled="busy" @click="clearMetadataCache">{{ t('pcl.auto2.kb7af0a1b98') }}</PclButton>
              </div>
            </PclCard>

            <PclCard :title="t('pcl.auto2.kbeb14fa3dd')">
              <div v-if="cacheRoutes.length" class="pcl-tools-list">
                <PclListItem v-for="route in cacheRoutes" :key="route.id" :title="route.name || route.id" :info="`${route.listen_prefix || route.listenPrefix || '—'} · ${route.enabled ? t('pcl.auto2.kd4e9ca3dd4') : t('pcl.auto2.kd989e55188')}`">
                  <template #icon><PclIcon name="layers" :size="16" /></template>
                  <template #actions><span class="pcl-muted">{{ t('pcl.setup.ttl', { value: route.ttl_seconds ?? route.ttlSeconds ?? 0 }) }}</span></template>
                </PclListItem>
              </div>
              <div v-else class="pcl-empty pcl-empty--compact">{{ t('pcl.auto2.kc6d728c26b') }}</div>
            </PclCard>

            <PclCard :title="t('pcl.auto2.k109dbaf249')">
              <div class="pcl-about-grid">
                <span>{{ t('pcl.auto2.ke4b9d92f14') }}</span><code>{{ cacheState.http?.entries ?? '—' }}</code>
                <span>{{ t('pcl.auto2.kf05934a464') }}</span><code>{{ cacheState.http ? formatBytes(cacheState.http.bytes) : '—' }}</code>
                <span>{{ t('pcl.auto2.k39287df5e0') }}</span><code>{{ cacheState.http?.bases ?? '—' }}</code>
                <span>{{ t('pcl.auto2.kfc7b7a21c5') }}</span><code>{{ cacheState.http?.indexPath || '—' }}</code>
              </div>
              <PclHint theme="blue">{{ t('pcl.auto2.k45798cf477') }}</PclHint>
            </PclCard>
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

