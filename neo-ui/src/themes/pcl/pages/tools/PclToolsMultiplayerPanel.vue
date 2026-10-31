<template>
<div class="pcl-tools-stack">
            <PclCard :title="t('pcl.auto2.k85bca71c6c')">
              <template #actions>
                <PclButton size="sm" icon="refresh-cw" :disabled="backendBusy" @click="refreshBackend">{{ t('pcl.auto2.k38108eaa1d') }}</PclButton>
              </template>

              <div v-show="section === 'multiplayer'" class="pcl-backend-tabs">
                <button
                  v-for="item in backends"
                  :key="item.id"
                  type="button"
                  :class="{ active: backend === item.id }"
                  @click="selectBackend(item.id)"
                >
                  <strong>{{ item.name }}</strong>
                  <span>{{ item.installed ? (item.version || t('pcl.auto2.keb88ff57c9')) : t('pcl.auto2.k6f7dc945ae') }} · {{ item.processRunning ? t('pcl.auto2.k9761f5d947') : t('pcl.auto2.k803638ec0a') }}</span>
                </button>
              </div>
              <div v-show="!backends.length" class="pcl-tools-muted">{{ t('pcl.auto2.ka5bc709136') }}</div>

              <PclLoading v-show="backendLoading" :text="t('pcl.auto2.k8fde84e9ec')" inline />
              <PclHint v-show="backendError" theme="red" closable @close="backendError = ''">{{ backendError }}</PclHint>

              <div v-show="backendStatus" class="pcl-backend-status">
                <div class="pcl-backend-head">
                  <div>
                    <strong>{{ selectedBackend?.name || backendStatus?.name || backend }}</strong>
                    <span>{{ backendStatus?.version || t('pcl.auto2.k59b35caf64') }} · {{ backendStatus?.available ? t('pcl.auto2.k4dd55b4dad') : t('pcl.auto2.k8a036450ac') }}</span>
                  </div>
                  <div class="pcl-actions">
                    <PclButton size="sm" icon="download" :disabled="backendBusy" @click="installBackend">
                      {{ backendStatus?.installed ? t('pcl.auto2.ka6df38586d') : t('pcl.auto2.k4246ff8817') }}
                    </PclButton>
                    <PclButton size="sm" variant="ghost" icon="refresh-cw" :disabled="backendBusy" @click="refreshBackend">{{ t('pcl.auto2.k7cc7f07a2c') }}</PclButton>
                  </div>
                </div>

                <div v-show="backendState" class="pcl-backend-state">
                  <span class="pcl-backend-pill" :class="`pcl-backend-pill--${backendState?.state || 'unknown'}`">{{ stateLabel(backendState?.state || '') }}</span>
                  <span v-if="backendState?.room">{{ t('pcl.auto2.ke007bf989b') }}<b>{{ backendState.room }}</b></span>
                  <span v-if="backendState?.url">{{ t('pcl.auto2.kc5c1437268') }}<b>{{ backendState.url }}</b></span>
                  <span v-if="backendState?.difficulty && backendState.difficulty !== 'unknown'">{{ t('pcl.tools.difficulty', { value: backendState.difficulty }) }}</span>
                </div>

                <div v-show="backendProfiles.length" class="pcl-backend-profiles">
                  <PclListItem
                    v-for="profile in backendProfiles"
                    :key="profile.machineId || profile.name"
                    :title="profile.name"
                    :info="`${profile.kind} · ${profile.vendor || 'Minecraft'}`"
                  >
                    <template #icon><PclIcon name="user" :size="16" /></template>
                  </PclListItem>
                </div>

                <div class="pcl-backend-mode">
                  <button type="button" :class="{ active: backendMode === 'host' }" @click="backendMode = 'host'">{{ t('pcl.auto2.k1688ee1b95') }}</button>
                  <button type="button" :class="{ active: backendMode === 'guest' }" @click="backendMode = 'guest'">{{ t('pcl.auto2.k1884a60e03') }}</button>
                </div>
                <div class="pcl-tools-server-form">
                  <PclInput v-model="playerName" :placeholder="t('pcl.auto2.kbe060c571f')" @enter="startBackend" />
                  <PclInput v-if="backendMode === 'guest'" v-model="roomCode" :placeholder="t('pcl.auto2.ka8e3e28d45')" @enter="startBackend" />
                  <PclButton variant="highlight" icon="play" :disabled="backendBusy || !playerName.trim() || (backendMode === 'guest' && !roomCode.trim())" @click="startBackend">
                    {{ backendMode === 'host' ? t('pcl.auto2.k1688ee1b95') : t('pcl.auto2.k1884a60e03') }}
                  </PclButton>
                </div>
                <div class="pcl-actions">
                  <PclButton variant="red" icon="power" :disabled="backendBusy || !backendState" @click="stopBackend">{{ t('pcl.auto2.k3534d5e04b') }}</PclButton>
                  <PclButton
                    v-show="backendStatus?.processRunning || selectedBackend?.processRunning"
                    variant="ghost"
                    icon="power"
                    :disabled="backendBusy"
                    @click="forceStopBackend"
                  >{{ t('pcl.tools.forceStopBackend') }}</PclButton>
                  <PclButton v-show="backend === 'yukari'" variant="ghost" icon="rotate-ccw" :disabled="backendBusy" @click="retryBackend">{{ t('pcl.auto2.ke3edcec638') }}</PclButton>
                  <PclButton variant="ghost" icon="scroll-text" :disabled="backendBusy" @click="loadBackendLog">{{ t('pcl.auto2.k81b80861ae') }}</PclButton>
                </div>
              </div>

              <PclHint theme="blue">{{ t('pcl.auto2.k67933c3fbd') }}</PclHint>
            </PclCard>

            <PclCard :title="t('pcl.auto2.ka5588822ba')">
              <template #actions>
                <PclButton size="sm" variant="ghost" icon="refresh-cw" :disabled="backendBusy" @click="loadBackendLog">{{ t('pcl.auto2.k38108eaa1d') }}</PclButton>
              </template>
              <pre v-show="backendLog" class="pcl-tools-pre">{{ backendLog }}</pre>
              <p v-show="!backendLog" class="pcl-tools-muted">{{ t('pcl.auto2.k25a176bd86') }}</p>
            </PclCard>
          </div>
</template>
<script setup lang="ts">
import { inject } from 'vue';
import { PclToolsContextKey } from './PclToolsContext';
import { useRuntimeLifecycle } from '../../../../common/runtime/lifecycle-debug';
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
const { t, props, busy, execute, selected, instances, section, tasks, downloads, loading, apiInfo, servers, serverAddress, serverStatus, skinUrl, skinPreview, testServer, achievementTitle, achievementLine, avatarSize, skinVariant, testUrl, urlResult, urlTesting, hashText, hashResult, error, stats, backendScheduler, backends, backend, backendStatus, backendBusy, backendLoading, backendError, backendMode, playerName, roomCode, backendLog, helperStatus, helperBusy, current, isMultiplayerSection, selectedBackend, backendState, backendProfiles, stateLabel, selectBackend, refreshBackend, installBackend, startBackend, stopBackend, forceStopBackend, retryBackend, loadBackendLog, loadTasks, cancelTask, cancelDownload, clearDownloads, loadApi, queryServer, testUrlRequest, hashTextValue, saveSkinPreview, helperKey, loadHelper, installHelper, removeHelper, saveAchievement, loadStats, openInstance } = context;
useRuntimeLifecycle('theme:pcl.tools.multiplayer-panel', isMultiplayerSection);
</script>
