<template>
<PclCard :title="t('pcl.auto2.ka424d76c91')">
            <template #actions>
              <PclButton size="sm" icon="refresh-cw" @click="loadStats">{{ t('pcl.auto2.k38108eaa1d') }}</PclButton>
            </template>
            <div v-if="backendScheduler" class="pcl-scheduler-summary">
              <span>{{ t('pcl.auto2.k9e3cc3588b') }}<b>{{ backendScheduler.mode }}</b></span>
              <span>{{ t('pcl.auto2.k150f863ec9') }}<b>{{ backendScheduler.activeReads }} / {{ backendScheduler.readLanes }}</b></span>
              <span>{{ t('pcl.auto2.kfce40d582f') }}<b>{{ backendScheduler.writeActive ? t('pcl.auto2.k30160a21b9') : t('pcl.auto2.k8bf5c10ad9') }}</b></span>
              <span>{{ t('pcl.auto2.k3561092d01') }}<b>{{ backendScheduler.queued }}</b></span>
              <span>{{ t('pcl.auto2.k6666f97c70') }}<b>{{ t('pcl.tools.readWrite', { reads: backendScheduler.completedReads, writes: backendScheduler.completedWrites }) }}</b></span>
            </div>
            <div v-if="stats.length" class="pcl-stats-table">
              <div class="pcl-stats-row pcl-stats-row--head">
                <span>{{ t('pcl.auto2.kf3ea6d345e') }}</span><span>{{ t('pcl.auto2.kb1fb011bbf') }}</span><span>{{ t('pcl.auto2.k6cdba4aad0') }}</span><span>{{ t('pcl.auto2.kd1454ec022') }}</span><span>{{ t('pcl.auto2.ked690ebb15') }}</span><span>{{ t('pcl.auto2.k5d288b64ca') }}</span>
              </div>
              <div v-for="row in stats" :key="row.operation" class="pcl-stats-row">
                <span class="pcl-stats-op">{{ row.operation }}</span>
                <span>{{ row.calls }}</span>
                <span>{{ row.cacheHits }}</span>
                <span>{{ row.averageMs }} ms</span>
                <span>{{ row.maxMs }} ms</span>
                <span :class="{ 'pcl-stats-slow': row.slow }">{{ row.slow }}</span>
              </div>
            </div>
            <div v-else class="pcl-empty pcl-empty--compact">{{ t('pcl.auto2.kf8162c2e06') }}</div>
            <PclHint theme="blue">{{ t('pcl.auto2.kb94f225518') }}</PclHint>
          </PclCard>
</template>
<script setup lang="ts">
import { inject } from 'vue';
import { PclToolsContextKey } from './PclToolsContext';
import PclButton from '../../components/PclButton.vue';
import PclCard from '../../components/PclCard.vue';
import PclHint from '../../components/PclHint.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclIconButton from '../../components/PclIconButton.vue';
import PclInput from '../../components/PclInput.vue';
import PclListItem from '../../components/PclListItem.vue';
import PclLoading from '../../components/PclLoading.vue';
import PclSelect from '../../components/PclSelect.vue';
const context = inject(PclToolsContextKey);
if (!context) throw new Error('PclToolsContext is missing');
const { t, props, busy, execute, selected, instances, section, tasks, downloads, loading, apiInfo, servers, serverAddress, serverStatus, skinUrl, skinPreview, testServer, achievementTitle, achievementLine, avatarSize, skinVariant, testUrl, urlResult, urlTesting, hashText, hashResult, error, stats, backendScheduler, backends, backend, backendStatus, backendBusy, backendError, backendMode, playerName, roomCode, backendLog, helperStatus, helperBusy, current, isMultiplayerSection, selectedBackend, backendState, backendProfiles, stateLabel, selectBackend, refreshBackend, installBackend, startBackend, stopBackend, retryBackend, loadBackendLog, loadTasks, cancelTask, cancelDownload, clearDownloads, loadApi, queryServer, testUrlRequest, hashTextValue, saveSkinPreview, helperKey, loadHelper, installHelper, removeHelper, saveAchievement, loadStats, openInstance } = context;
</script>
