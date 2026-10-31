<template>
<div class="pcl-tools-grid">
            <PclCard :title="t('pcl.auto2.kff5e8b99ae')">
              <template #actions>
                <PclIconButton icon="refresh-cw" :label="t('pcl.auto2.k040f050b56')" :size="24" :disabled="busy" @click="loadTasks" />
              </template>
              <PclLoading v-if="loading" :text="t('pcl.auto2.k98ffc767ac')" inline />
              <div class="pcl-tools-list">
                <PclListItem
                  v-for="task in tasks"
                  :key="task.id || task.gid"
                  :title="task.name || task.title || task.id || t('pcl.auto2.ka69c7932c0')"
                  :info="`${task.status || task.state || t('pcl.auto2.k5942497005')} · ${task.progress ?? task.completedLength ?? 0}${task.total ? ` / ${task.total}` : ''}`"
                >
                  <template #icon><PclIcon :name="task.running === false ? 'circle-check' : 'download'" :size="16" /></template>
                  <template #actions>
                    <PclButton size="sm" variant="red" :disabled="busy || task.running === false" @click.stop="cancelTask(task)">{{ t('pcl.auto2.k4d0b4688c7') }}</PclButton>
                  </template>
                </PclListItem>
                <div v-if="!tasks.length" class="pcl-empty pcl-empty--compact">{{ t('pcl.auto2.kaa28d0a2c2') }}</div>
              </div>
            </PclCard>

            <PclCard :title="t('pcl.auto2.kcc2b1baf1b')">
              <template #actions>
                <PclButton size="sm" variant="ghost" icon="trash-2" :disabled="busy" @click="clearDownloads">{{ t('pcl.auto2.k149b3c638f') }}</PclButton>
              </template>
              <div class="pcl-tools-list">
                <PclListItem
                  v-for="download in downloads"
                  :key="download.gid"
                  :title="download.name || download.gid"
                  :info="`${download.status || download.state} · ${download.completedLength || 0} / ${download.totalLength || 0}`"
                >
                  <template #icon><PclIcon name="download" :size="16" /></template>
                  <template #actions>
                    <PclButton size="sm" variant="red" :disabled="busy" @click.stop="cancelDownload(download)">{{ t('pcl.auto2.k4d0b4688c7') }}</PclButton>
                  </template>
                </PclListItem>
                <div v-if="!downloads.length" class="pcl-empty pcl-empty--compact">{{ t('pcl.auto2.k8b449eafb9') }}</div>
              </div>
            </PclCard>
          </div>
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
