<template>
<PclCard :title="t('pcl.auto2.ka8ce402665')">
            <template #actions>
              <span class="pcl-tools-muted">{{ t('pcl.auto2.ka0360c967e') }}</span>
            </template>
            <div class="pcl-tools-list">
              <PclListItem
                v-for="instance in instances"
                :key="instance.id"
                :title="instance.name"
                :info="instance.running ? t('pcl.auto2.k1ca6d09614') : t('pcl.auto2.kb2ba9634d9')"
                @click="openInstance(instance)"
              >
                <template #icon><PclIcon name="scroll-text" :size="16" /></template>
                <template #actions><PclIcon name="chevron-right" :size="14" /></template>
              </PclListItem>
              <div v-if="!instances.length" class="pcl-empty pcl-empty--compact">{{ t('pcl.auto2.k641554a729') }}</div>
            </div>
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
