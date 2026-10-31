<template>
<div class="pcl-tools-stack">
            <PclCard :title="section === 'authlib' ? 'authlib-injector' : 'Nide8Auth'">
              <template #actions>
                <PclButton size="sm" icon="refresh-cw" :disabled="helperBusy" @click="loadHelper(helperKey(section))">{{ t('pcl.auto2.k7cc7f07a2c') }}</PclButton>
              </template>
              <PclLoading v-if="helperBusy" :text="t('pcl.auto2.k442a88def5')" inline />
              <div v-if="helperStatus[helperKey(section)]" class="pcl-helper-status">
                <span class="pcl-local-status__dot" :class="{ ok: helperStatus[helperKey(section)].installed }" />
                <div>
                  <strong>{{ helperStatus[helperKey(section)].installed ? t('pcl.auto2.keb88ff57c9') : t('pcl.auto2.k6f7dc945ae') }} <template v-if="helperStatus[helperKey(section)].version">· {{ helperStatus[helperKey(section)].version }}</template></strong>
                  <small>{{ helperStatus[helperKey(section)].path || t('pcl.auto2.ka006ba4bcc') }}</small>
                  <small v-if="helperStatus[helperKey(section)].error" class="pcl-helper-error">{{ helperStatus[helperKey(section)].error }}</small>
                </div>
              </div>
              <div class="pcl-actions">
                <PclButton variant="highlight" icon="download" :disabled="helperBusy" @click="installHelper(helperKey(section))">{{ helperStatus[helperKey(section)]?.installed ? t('pcl.auto2.k11dfe8d875') : t('pcl.auto2.ke72533139c') }}</PclButton>
                <PclButton variant="red" icon="trash-2" :disabled="helperBusy || !helperStatus[helperKey(section)]?.installed" @click="removeHelper(helperKey(section))">{{ t('pcl.auto2.k42527a1700') }}</PclButton>
              </div>
              <PclHint theme="blue">{{ t('pcl.auto2.k367d7f0227') }}</PclHint>
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
