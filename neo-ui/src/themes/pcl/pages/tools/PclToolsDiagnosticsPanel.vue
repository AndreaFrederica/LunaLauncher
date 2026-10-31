<template>
<div class="pcl-tools-grid">
            <PclCard :title="t('pcl.auto2.k053423283a')">
              <template #actions>
                <PclButton size="sm" variant="highlight" :disabled="busy" @click="loadApi">{{ t('pcl.auto2.kd6f4400a65') }}</PclButton>
              </template>
              <pre v-if="apiInfo" class="pcl-tools-pre">{{ JSON.stringify(apiInfo, null, 2) }}</pre>
              <p v-else class="pcl-tools-muted">{{ t('pcl.auto2.kecafcd94a3') }}</p>
            </PclCard>

            <PclCard :title="t('pcl.auto2.k073fc7dca9')">
              <div class="pcl-tools-skin-form">
                <PclInput v-model="skinUrl" :placeholder="t('pcl.auto2.k9303fdecda')" />
                <PclSelect v-model="avatarSize" :options="[64, 128, 256, 512].map(value => ({ label: `${value}px`, value }))" />
                <PclSelect v-model="skinVariant" :options="[{ label: 'Classic', value: 'classic' }, { label: 'Slim', value: 'slim' }]" />
              </div>
              <div class="pcl-tools-skin-preview" :style="skinUrl ? { backgroundImage: `url(${skinUrl})` } : undefined">
                <PclIcon v-if="!skinUrl" name="user" :size="28" />
              </div>
              <div class="pcl-actions">
                <PclButton size="sm" :disabled="!skinUrl" @click="skinPreview = skinUrl">{{ t('pcl.auto2.k757d1feec9') }}</PclButton>
                <PclButton size="sm" variant="ghost" :disabled="!skinPreview" @click="saveSkinPreview">{{ t('pcl.auto2.kfce6a29edb') }}</PclButton>
              </div>
              <img v-if="skinPreview" :src="skinPreview" class="pcl-tools-skin-image" :alt="t('pcl.auto2.kb1dc96e46a')" />
              <p class="pcl-tools-muted">{{ t('pcl.tools.previewInfo', { size: avatarSize, variant: skinVariant }) }}</p>
            </PclCard>

            <PclCard :title="t('pcl.auto2.k252d4b7132')">
              <div class="pcl-tools-server-form">
                <PclInput v-model="testServer" :placeholder="t('pcl.auto2.k2eb291f695')" />
                <PclButton size="sm" icon="search" :disabled="!testServer" @click="queryServer(testServer)">{{ t('pcl.auto2.k711363c424') }}</PclButton>
              </div>
              <p v-if="serverStatus" class="pcl-tools-muted">{{ t('pcl.tools.serverInfo', { name: serverStatus.name || testServer, players: serverStatus.players ?? 0, latency: serverStatus.latency ? ` · ${serverStatus.latency} ms` : '' }) }}</p>
            </PclCard>

            <PclCard :title="t('pcl.auto2.k4ecb8e7481')">
              <template #actions>
                <PclButton size="sm" icon="external-link" :disabled="urlTesting || !testUrl" @click="testUrlRequest">{{ t('pcl.auto2.k0b5d7ed54b') }}</PclButton>
              </template>
              <PclInput v-model="testUrl" placeholder="https://example.com" />
              <p v-if="urlResult" class="pcl-tools-muted">{{ urlResult }}</p>
            </PclCard>

            <PclCard :title="t('pcl.auto2.k329b62253a')">
              <template #actions>
                <PclButton size="sm" icon="shield-check" :disabled="!hashText" @click="hashTextValue">{{ t('pcl.auto2.kf49414f981') }}</PclButton>
              </template>
              <PclInput v-model="hashText" multiline :rows="4" :placeholder="t('pcl.auto2.k9a2fca6df1')" />
              <p v-if="hashResult" class="pcl-tools-pre pcl-tools-hash">{{ hashResult }}</p>
            </PclCard>

            <PclCard :title="t('pcl.auto2.k091937e96d')">
              <template #actions>
                <PclButton size="sm" variant="ghost" icon="save" @click="saveAchievement">{{ t('pcl.auto2.kfadf24dbc5') }}</PclButton>
              </template>
              <PclInput v-model="achievementTitle" :placeholder="t('pcl.auto2.k11ac30f788')" />
              <PclInput v-model="achievementLine" :placeholder="t('pcl.auto2.kbc7f02ea9e')" />
              <div class="pcl-tools-achievement">
                <strong>{{ achievementTitle || t('pcl.auto2.k11ac30f788') }}</strong>
                <span>{{ achievementLine || t('pcl.auto2.kf138df7b79') }}</span>
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
