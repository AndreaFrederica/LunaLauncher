<template>
  <section class="pcl-page pcl-tools-page">
    <aside class="pcl-page-nav pcl-tools-nav">
      <div class="pcl-tools-title">
        <PclIcon name="tool-case" :size="19" />
        <span>{{ t('pcl.auto2.ka72ef18d9a') }}</span>
      </div>
      <template v-for="group in itemGroups" :key="group.label">
        <div class="pcl-tools-group">{{ group.label }}</div>
        <button
          v-for="item in group.items"
          :key="item.id"
          type="button"
          class="pcl-page-nav__item"
          :class="{ active: section === item.id }"
          @click="section = item.id"
        >
          <PclIcon :name="item.icon" :size="18" />
          <span>{{ item.label }}</span>
        </button>
      </template>
      <div class="pcl-tools-hint">
        <PclIcon name="info" :size="15" />
        <span>{{ t('pcl.auto2.kaf33a4dbeb') }}</span>
      </div>
    </aside>

    <main class="pcl-page-main">
      <div class="pcl-page-view">
          <div class="pcl-tools-toolbar">
            <div>
              <h1>{{ current.label }}</h1>
              <p>{{ current.description }}</p>
            </div>
            <PclButton size="sm" icon="refresh-cw" :disabled="busy" @click="reload">{{ t('pcl.auto2.k38108eaa1d') }}</PclButton>
          </div>

          <PclHint v-if="error" theme="red" closable @close="error = ''">{{ error }}</PclHint>

          <div v-show="isMultiplayerSection" class="pcl-tools-stack">
            <PclCard :title="t('pcl.auto2.k85bca71c6c')">
              <template #actions>
                <PclButton size="sm" icon="refresh-cw" :disabled="backendBusy" @click="refreshBackend">{{ t('pcl.auto2.k38108eaa1d') }}</PclButton>
              </template>

              <div v-if="section === 'multiplayer'" class="pcl-backend-tabs">
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
              <div v-if="!backends.length" class="pcl-tools-muted">{{ t('pcl.auto2.ka5bc709136') }}</div>

              <PclLoading v-if="backendBusy" :text="t('pcl.auto2.k8fde84e9ec')" inline />
              <PclHint v-if="backendError" theme="red" closable @close="backendError = ''">{{ backendError }}</PclHint>

              <template v-if="backendStatus">
                <div class="pcl-backend-head">
                  <div>
                    <strong>{{ selectedBackend?.name || backendStatus.name || backend }}</strong>
                    <span>{{ backendStatus.version || t('pcl.auto2.k59b35caf64') }} · {{ backendStatus.available ? t('pcl.auto2.k4dd55b4dad') : t('pcl.auto2.k8a036450ac') }}</span>
                  </div>
                  <div class="pcl-actions">
                    <PclButton size="sm" icon="download" :disabled="backendBusy" @click="installBackend">
                      {{ backendStatus.installed ? t('pcl.auto2.ka6df38586d') : t('pcl.auto2.k4246ff8817') }}
                    </PclButton>
                    <PclButton size="sm" variant="ghost" icon="refresh-cw" :disabled="backendBusy" @click="refreshBackend">{{ t('pcl.auto2.k7cc7f07a2c') }}</PclButton>
                  </div>
                </div>

                <div v-if="backendState" class="pcl-backend-state">
                  <span class="pcl-backend-pill" :class="`pcl-backend-pill--${backendState.state}`">{{ stateLabel(backendState.state) }}</span>
                  <span v-if="backendState.room">{{ t('pcl.auto2.ke007bf989b') }}<b>{{ backendState.room }}</b></span>
                  <span v-if="backendState.url">{{ t('pcl.auto2.kc5c1437268') }}<b>{{ backendState.url }}</b></span>
                  <span v-if="backendState.difficulty && backendState.difficulty !== 'unknown'">{{ t('pcl.tools.difficulty', { value: backendState.difficulty }) }}</span>
                </div>

                <div v-if="backendProfiles.length" class="pcl-backend-profiles">
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
                  <PclButton v-if="backend === 'yukari'" variant="ghost" icon="rotate-ccw" :disabled="backendBusy" @click="retryBackend">{{ t('pcl.auto2.ke3edcec638') }}</PclButton>
                  <PclButton variant="ghost" icon="scroll-text" :disabled="backendBusy" @click="loadBackendLog">{{ t('pcl.auto2.k81b80861ae') }}</PclButton>
                </div>
              </template>

              <PclHint theme="blue">{{ t('pcl.auto2.k67933c3fbd') }}</PclHint>
            </PclCard>

            <PclCard :title="t('pcl.auto2.ka5588822ba')">
              <template #actions>
                <PclButton size="sm" variant="ghost" icon="refresh-cw" :disabled="backendBusy" @click="loadBackendLog">{{ t('pcl.auto2.k38108eaa1d') }}</PclButton>
              </template>
              <pre v-if="backendLog" class="pcl-tools-pre">{{ backendLog }}</pre>
              <p v-else class="pcl-tools-muted">{{ t('pcl.auto2.k25a176bd86') }}</p>
            </PclCard>
          </div>

          <div v-show="section === 'tasks'" class="pcl-tools-grid">
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

          <PclCard v-show="section === 'logs'" :title="t('pcl.auto2.ka8ce402665')">
            <template #actions>
              <span class="pcl-tools-muted">{{ t('pcl.auto2.ka0360c967e') }}</span>
            </template>
            <div class="pcl-tools-list">
              <PclListItem
                v-for="instance in instances"
                :key="instance.id"
                :title="instance.name"
                :info="instance.running ? t('pcl.auto2.k1ca6d09614') : t('pcl.auto2.kb2ba9634d9')"
                @click="emit('open-instance', instance)"
              >
                <template #icon><PclIcon name="scroll-text" :size="16" /></template>
                <template #actions><PclIcon name="chevron-right" :size="14" /></template>
              </PclListItem>
              <div v-if="!instances.length" class="pcl-empty pcl-empty--compact">{{ t('pcl.auto2.k641554a729') }}</div>
            </div>
          </PclCard>

          <PclCard v-show="section === 'scheduler'" :title="t('pcl.auto2.ka424d76c91')">
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

          <div v-show="section === 'authlib' || section === 'nide8auth'" class="pcl-tools-stack">
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

          <div v-show="section === 'test'" class="pcl-tools-grid">
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
      </div>
    </main>
  </section>
</template>

<script setup lang="ts">
import { useI18n } from "vue-i18n";
const { t } = useI18n();
import { computed, onMounted, onUnmounted, ref, watch } from 'vue';
import PclButton from '../../components/PclButton.vue';
import PclCard from '../../components/PclCard.vue';
import PclHint from '../../components/PclHint.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclIconButton from '../../components/PclIconButton.vue';
import PclInput from '../../components/PclInput.vue';
import PclListItem from '../../components/PclListItem.vue';
import PclLoading from '../../components/PclLoading.vue';
import PclSelect from '../../components/PclSelect.vue';
import { launcher } from '../../../boot/launcher-api';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
const props = defineProps<{ busy: boolean; execute: Execute; selected?: any; instances: any[] }>();
const emit = defineEmits<{ refresh: []; 'open-instance': [instance: any] }>();

const itemGroups = [
  { label: t('pcl.auto2.k894f1c3e8d'), items: [
    { id: 'multiplayer', label: t('pcl.auto2.k1d5e43605d'), icon: 'earth', description: t('pcl.auto2.kccf2623767') },
    { id: 'tasks', label: t('pcl.auto2.kff5e8b99ae'), icon: 'list', description: t('pcl.auto2.ke31d997706') },
    { id: 'logs', label: t('pcl.auto2.ka9706f8c7f'), icon: 'scroll-text', description: t('pcl.auto2.k6ca5b1e7ba') },
    { id: 'scheduler', label: t('pcl.auto2.kc13a522bb1'), icon: 'gauge', description: t('pcl.auto2.k3c52f688ed') },
    { id: 'test', label: t('pcl.auto2.k6c5c08c370'), icon: 'flask-conical', description: t('pcl.auto2.kc572d9f9cc') },
  ] },
  { label: t('pcl.auto2.k726090c5c4'), items: [
    { id: 'yukari', label: 'YukariConnect', icon: 'globe', description: t('pcl.auto2.k24d2a8fe1f') },
    { id: 'terracotta', label: 'Terracotta', icon: 'network', description: t('pcl.auto2.k215c9d6ce4') },
    { id: 'authlib', label: 'authlib-injector', icon: 'shield-check', description: t('pcl.auto2.keac13be806') },
    { id: 'nide8auth', label: 'Nide8Auth', icon: 'key-round', description: t('pcl.auto2.k2e152edae1') },
  ] },
];
const items = itemGroups.flatMap(group => group.items);
const section = ref('multiplayer'), tasks = ref<any[]>([]), downloads = ref<any[]>([]), loading = ref(false);
const apiInfo = ref<any>(null), servers = ref<any[]>([]), serverAddress = ref(''), serverStatus = ref<any>(null);
const skinUrl = ref(''), skinPreview = ref(''), testServer = ref(''), achievementTitle = ref(''), achievementLine = ref('');
const avatarSize = ref(128), skinVariant = ref('classic'), testUrl = ref(''), urlResult = ref(''), urlTesting = ref(false);
const hashText = ref(''), hashResult = ref(''), error = ref('');
const stats = ref<any[]>([]);
const backendScheduler = ref<any>(null);
async function loadStats() {
  stats.value = launcher.getStats();
  try { backendScheduler.value = await props.execute('scheduler.status'); }
  catch { backendScheduler.value = null; }
}
const backends = ref<any[]>([]);
const backend = ref('terracotta');
const backendStatus = ref<any>(null);
const backendBusy = ref(false);
const backendError = ref('');
const backendMode = ref<'host' | 'guest'>('host');
const playerName = ref('Player');
const roomCode = ref('');
const backendLog = ref('');
const helperStatus = ref<Record<string, any>>({});
const helperBusy = ref(false);
let refreshTimer: ReturnType<typeof setInterval> | undefined;
let refreshInFlight = false;
const current = computed(() => items.find(item => item.id === section.value) || items[0]);
const isMultiplayerSection = computed(() => ['multiplayer', 'yukari', 'terracotta'].includes(section.value));
const selectedBackend = computed(() => backends.value.find(item => item.id === backend.value) || null);
const backendState = computed(() => backendStatus.value?.state || null);
const backendProfiles = computed(() => Array.isArray(backendState.value?.profiles) ? backendState.value.profiles : []);

function message(value: unknown) {
  return value instanceof Error ? value.message : String(value);
}

function stateLabel(state: string) {
  const labels: Record<string, string> = {
    waiting: t('pcl.auto2.kbd3488d0a9'), 'host-scanning': t('pcl.auto2.k5f1f1ae10b'), 'host-starting': t('pcl.auto2.k03c625d2d1'),
    'host-ok': t('pcl.auto2.kf79b86a571'), 'guest-connecting': t('pcl.auto2.ka5c84f5771'), 'guest-starting': t('pcl.auto2.k8c20e1cda9'),
    'guest-ok': t('pcl.auto2.kf71c183251'), exception: t('pcl.auto2.k20a0c1a2c0'),
  };
  return labels[state] || state || t('pcl.auto2.k4e72a1ffe5');
}

function readPreference(key: string, fallback: string) {
  try { return localStorage.getItem(key) || fallback; } catch { return fallback; }
}

function savePreference(key: string, value: string) {
  try { localStorage.setItem(key, value); } catch { /* storage unavailable */ }
}

function selectBackend(id: string) {
  backend.value = id;
  savePreference('luna-multiplayer-backend', id);
  backendStatus.value = null;
  backendError.value = '';
  void refreshBackend();
}

async function loadBackends() {
  try {
    const result = await props.execute('multiplayer.backends');
    backends.value = Array.isArray(result?.backends) ? result.backends : [];
    if (backends.value.length && !backends.value.some(item => item.id === backend.value)) backend.value = backends.value[0].id;
    await refreshBackend();
  } catch (exception) {
    backendError.value = message(exception);
  }
}

async function refreshBackend() {
  if (!backend.value) return;
  backendBusy.value = true;
  backendError.value = '';
  try {
    backendStatus.value = await props.execute('multiplayer.status', { backend: backend.value, fetchState: true });
    const currentStatus = backendStatus.value;
    backends.value = backends.value.map(item => item.id === backend.value
      ? { ...item, installed: Boolean(currentStatus?.installed), version: currentStatus?.version || item.version, processRunning: Boolean(currentStatus?.processRunning) }
      : item);
    if (backendStatus.value?.state?.room) roomCode.value = backendStatus.value.state.room;
  } catch (exception) {
    backendError.value = message(exception);
  } finally {
    backendBusy.value = false;
  }
}

async function installBackend() {
  backendBusy.value = true;
  backendError.value = '';
  try {
    await props.execute('multiplayer.install', { backend: backend.value });
    await loadBackends();
  } catch (exception) {
    backendError.value = message(exception);
  } finally {
    backendBusy.value = false;
  }
}

async function startBackend() {
  if (!playerName.value.trim()) return;
  backendBusy.value = true;
  backendError.value = '';
  try {
    const parameters: Record<string, unknown> = { backend: backend.value, mode: backendMode.value, player: playerName.value.trim() };
    if (backendMode.value === 'guest') parameters.room = roomCode.value.trim();
    await props.execute('multiplayer.start', parameters);
    savePreference('luna-multiplayer-player', playerName.value.trim());
    await refreshBackend();
  } catch (exception) {
    backendError.value = message(exception);
  } finally {
    backendBusy.value = false;
  }
}

async function stopBackend() {
  backendBusy.value = true;
  backendError.value = '';
  try {
    await props.execute('multiplayer.stop', { backend: backend.value });
    await refreshBackend();
  } catch (exception) {
    backendError.value = message(exception);
  } finally {
    backendBusy.value = false;
  }
}

async function retryBackend() {
  backendBusy.value = true;
  backendError.value = '';
  try {
    await props.execute('multiplayer.retry', { backend: backend.value });
    await refreshBackend();
  } catch (exception) {
    backendError.value = message(exception);
  } finally {
    backendBusy.value = false;
  }
}

async function loadBackendLog() {
  backendBusy.value = true;
  backendError.value = '';
  try {
    const result = await props.execute('multiplayer.log', { backend: backend.value });
    backendLog.value = String(result?.log || '');
  } catch (exception) {
    backendError.value = message(exception);
  } finally {
    backendBusy.value = false;
  }
}
async function refreshVisibleData() {
  if (refreshInFlight || props.busy || backendBusy.value || helperBusy.value) return;
  refreshInFlight = true;
  try {
    if (isMultiplayerSection.value) {
      await refreshBackend();
      if (backendLog.value || backendState.value) await loadBackendLog();
    } else if (section.value === 'tasks') {
      await loadTasks();
    } else if (section.value === 'scheduler') {
      await loadStats();
    }
  } finally {
    refreshInFlight = false;
  }
}
async function loadTasks() {
  loading.value = true;
  error.value = '';
  try {
    const result = await props.execute('task.list');
    tasks.value = Array.isArray(result) ? result : (result?.tasks || []);
  } catch (exception) {
    error.value = message(exception);
  }
  try {
    downloads.value = await props.execute('aria2.downloads');
  } catch {
    downloads.value = [];
  } finally {
    loading.value = false;
  }
}
async function reload() {
  if (isMultiplayerSection.value) await refreshVisibleData();
  else if (section.value === 'tasks') await loadTasks();
  else if (section.value === 'scheduler') await loadStats();
  else emit('refresh');
}
async function cancelTask(task: any) {
  try { await props.execute('task.cancel', task.id ? { taskId: task.id } : {}); await loadTasks(); }
  catch (exception) { error.value = message(exception); }
}
async function cancelDownload(download: any) {
  try { await props.execute('aria2.cancel', { gid: download.gid }); await loadTasks(); }
  catch (exception) { error.value = message(exception); }
}
async function clearDownloads() {
  try { await props.execute('aria2.clear-finished'); await loadTasks(); }
  catch (exception) { error.value = message(exception); }
}
async function loadApi() {
  try { apiInfo.value = await props.execute('api.describe'); }
  catch (exception) { apiInfo.value = { error: message(exception) }; }
}
async function loadServers() {
  try {
    const result = JSON.parse(localStorage.getItem('luna-server-list') || '[]');
    servers.value = Array.isArray(result) ? result : [];
  } catch { servers.value = []; }
}
async function queryServer(address = serverAddress.value) {
  if (!address) return;
  serverAddress.value = address;
  error.value = '';
  try {
    const response = await fetch(`https://api.mcsrvstat.us/2/${encodeURIComponent(address)}`);
    const data = await response.json();
    serverStatus.value = {
      name: data.hostname || address, address,
      players: data.players?.online || 0, maxPlayers: data.players?.max || 0,
      latency: data.debug?.ping || undefined, online: Boolean(data.online),
    };
    const next = [...servers.value.filter(item => item.address !== address), { address, name: data.hostname || address, players: data.players?.online || 0 }];
    servers.value = next;
    localStorage.setItem('luna-server-list', JSON.stringify(next.slice(-50)));
  } catch (exception) {
    serverStatus.value = { name: address, error: message(exception) };
  }
}
async function testUrlRequest() {
  urlTesting.value = true;
  urlResult.value = '';
  const start = performance.now();
  try {
    const response = await fetch(testUrl.value, { method: 'HEAD' });
    urlResult.value = `HTTP ${response.status} ${response.statusText} · ${Math.round(performance.now() - start)} ms`;
  } catch (exception) {
    urlResult.value = t('pcl.tools.requestFailed', { error: message(exception) });
  } finally {
    urlTesting.value = false;
  }
}
async function hashTextValue() {
  if (!hashText.value) return;
  const bytes = new TextEncoder().encode(hashText.value);
  const digest = await crypto.subtle.digest('SHA-256', bytes);
  hashResult.value = Array.from(new Uint8Array(digest)).map(value => value.toString(16).padStart(2, '0')).join('');
}
function saveSkinPreview() {
  try { localStorage.setItem('luna-tool-skin-preview', JSON.stringify({ url: skinPreview.value, size: avatarSize.value, variant: skinVariant.value })); }
  catch { /* storage unavailable */ }
}
function helperKey(kind: string) {
  return kind === 'authlib' || kind === 'nide8auth' ? (kind === 'authlib' ? 'authlib-injector' : 'nide8auth') : '';
}
async function loadHelper(helper: string) {
  if (!helper) return;
  helperBusy.value = true;
  try { helperStatus.value[helper] = await props.execute('authentication.helper.status', { helper }); }
  catch (exception) { helperStatus.value[helper] = { error: message(exception) }; }
  finally { helperBusy.value = false; }
}
async function installHelper(helper: string) {
  helperBusy.value = true;
  try { helperStatus.value[helper] = await props.execute('authentication.helper.install', { helper }); }
  catch (exception) { helperStatus.value[helper] = { error: message(exception) }; }
  finally { helperBusy.value = false; }
}
async function removeHelper(helper: string) {
  if (!window.confirm(t('pcl.tools.confirmDelete', { name: helper }))) return;
  helperBusy.value = true;
  try { helperStatus.value[helper] = await props.execute('authentication.helper.remove', { helper, confirm: true }); }
  catch (exception) { helperStatus.value[helper] = { error: message(exception) }; }
  finally { helperBusy.value = false; }
}

function saveAchievement() {
  try { localStorage.setItem('luna-tool-achievement-preview', JSON.stringify({ title: achievementTitle.value, line: achievementLine.value })); }
  catch { /* storage unavailable */ }
}
watch(section, value => {
  if (value === 'scheduler') loadStats();
  if (value === 'yukari' || value === 'terracotta') {
    backend.value = value;
    backendStatus.value = null;
    void refreshBackend();
  }
  if (value === 'authlib' || value === 'nide8auth') void loadHelper(helperKey(value));
});

onMounted(() => {
  backend.value = readPreference('luna-multiplayer-backend', 'terracotta');
  playerName.value = readPreference('luna-multiplayer-player', 'Player') || 'Player';
  void loadTasks();
  void loadBackends();
  void refreshBackend();
  refreshTimer = setInterval(() => { void refreshVisibleData(); }, 3000);
});
onUnmounted(() => { if (refreshTimer) clearInterval(refreshTimer); });
</script>

<style scoped>
.pcl-tools-page {
  width: 100%;
  max-width: none;
  margin: 0;
}

.pcl-tools-nav {
  width: 170px;
  min-width: 170px;
  padding: 14px 0 16px;
}

.pcl-tools-title {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 0 13px 12px;
  border-bottom: 1px solid var(--pcl-border);
  color: var(--pcl-text);
  font-size: calc(13px * var(--pcl-font-scale, 1));
  font-weight: 700;
}

.pcl-tools-group {
  padding: 9px 13px 5px;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
  font-weight: 700;
  letter-spacing: .08em;
}

.pcl-tools-hint {
  display: flex;
  gap: 6px;
  margin: 16px 10px 0;
  padding: 8px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
  line-height: 1.6;
}

.pcl-tools-toolbar {
  display: flex;
  align-items: flex-end;
  justify-content: space-between;
  gap: 12px;
  padding-bottom: 14px;
  border-bottom: 1px solid var(--pcl-border);
}

.pcl-tools-toolbar h1 {
  margin: 0;
  color: var(--pcl-text);
  font-size: calc(19px * var(--pcl-font-scale, 1));
  font-weight: 700;
}

.pcl-tools-toolbar p {
  margin: 3px 0 0;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-tools-stack {
  display: grid;
  grid-template-columns: minmax(0, 1.25fr) minmax(0, .75fr);
  gap: 12px;
  align-items: start;
}

.pcl-tools-grid {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 12px;
}

.pcl-tools-grid :deep(.pcl-card) {
  margin-bottom: 0;
}

.pcl-tools-server-form {
  display: flex;
  gap: 7px;
  margin-bottom: 10px;
}

.pcl-tools-server-form :deep(.pcl-input) {
  flex: 1;
}

.pcl-tools-server-list,
.pcl-tools-list {
  max-height: 360px;
  overflow-y: auto;
}

.pcl-tools-muted {
  margin: 0;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
  line-height: 1.7;
}

.pcl-tools-pre {
  max-height: 240px;
  margin: 0;
  overflow: auto;
  padding: 9px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-c8);
  color: var(--pcl-text);
  white-space: pre-wrap;
  font: calc(11px * var(--pcl-font-scale, 1))/1.55 ui-monospace, Consolas, monospace;
}

.pcl-tools-stack > :deep(.pcl-card) { min-width: 0; }
.pcl-tools-stack > :deep(.pcl-card) .pcl-tools-pre { max-height: calc(100vh - 300px); min-height: 220px; }

.pcl-tools-hash {
  margin-top: 8px;
  overflow-wrap: anywhere;
}

.pcl-tools-skin-form {
  display: grid;
  grid-template-columns: minmax(0, 1fr) 110px;
  gap: 7px;
  margin-bottom: 8px;
}

.pcl-tools-skin-form :deep(.pcl-input) {
  grid-column: 1 / -1;
}

.pcl-tools-skin-preview {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 72px;
  height: 72px;
  margin: 8px 0;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-c6) center/cover no-repeat;
  color: var(--pcl-muted);
  image-rendering: pixelated;
}

.pcl-tools-skin-image {
  display: block;
  max-width: 180px;
  max-height: 120px;
  margin-top: 8px;
  object-fit: contain;
  image-rendering: pixelated;
}

.pcl-tools-achievement {
  display: flex;
  flex-direction: column;
  gap: 4px;
  margin-top: 9px;
  padding: 12px;
  border-radius: 4px;
  background: linear-gradient(135deg, var(--pcl-c6), var(--pcl-c8));
  color: var(--pcl-text);
}

.pcl-tools-achievement span {
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-backend-tabs {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 7px;
  margin-bottom: 10px;
}

.pcl-backend-tabs button {
  display: flex;
  flex-direction: column;
  align-items: flex-start;
  gap: 2px;
  padding: 8px 10px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-half-white);
  color: var(--pcl-text);
  text-align: left;
  cursor: pointer;
}

.pcl-backend-tabs button.active {
  border-color: var(--pcl-c2);
  color: var(--pcl-c2);
}

.pcl-backend-tabs strong {
  font-size: calc(12px * var(--pcl-font-scale, 1));
}

.pcl-backend-tabs span {
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}

.pcl-backend-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
  margin-bottom: 9px;
}

.pcl-backend-head strong {
  display: block;
  color: var(--pcl-text);
  font-size: calc(13px * var(--pcl-font-scale, 1));
}

.pcl-backend-head span {
  display: block;
  margin-top: 2px;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}

.pcl-backend-state {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 8px;
  margin-bottom: 9px;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-backend-state b {
  color: var(--pcl-text);
  font-weight: 600;
}

.pcl-backend-pill {
  padding: 2px 8px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 10px;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}

.pcl-backend-pill--host-ok,
.pcl-backend-pill--guest-ok {
  border-color: rgba(52, 211, 153, .5);
  color: #34d399;
}

.pcl-backend-pill--exception {
  border-color: rgba(255, 107, 107, .5);
  color: #ff6b6b;
}

.pcl-backend-profiles {
  max-height: 150px;
  margin-bottom: 8px;
  overflow-y: auto;
}

.pcl-backend-mode {
  display: inline-flex;
  gap: 2px;
  margin-bottom: 8px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-c8);
  padding: 2px;
}

.pcl-backend-mode button {
  height: 26px;
  padding: 0 11px;
  border: 0;
  border-radius: 3px;
  background: transparent;
  color: var(--pcl-muted);
  font: 400 calc(11px * var(--pcl-font-scale, 1))/1 var(--pcl-font);
  cursor: pointer;
}

.pcl-backend-mode button.active {
  background: var(--pcl-c3);
  color: #fff;
}

.pcl-scheduler-summary {
  display: flex;
  flex-wrap: wrap;
  gap: 8px 14px;
  margin-bottom: 10px;
  padding: 8px 10px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-half-white);
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-scheduler-summary b {
  color: var(--pcl-c2);
  font-weight: 600;
}

.pcl-stats-table {
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  overflow: hidden;
}

.pcl-stats-row {
  display: grid;
  grid-template-columns: minmax(0, 1.7fr) 52px 52px 72px 72px 58px;
  gap: 6px;
  padding: 7px 9px;
  border-bottom: 1px solid var(--pcl-border);
  color: var(--pcl-text);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-stats-row:last-child {
  border-bottom: 0;
}

.pcl-stats-row--head {
  background: var(--pcl-half-white);
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
  font-weight: 700;
}

.pcl-stats-op {
  overflow: hidden;
  color: var(--pcl-c2);
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-stats-slow {
  color: #ff6b6b;
  font-weight: 700;
}

@media (max-width: 900px) {
  .pcl-tools-nav {
    width: 152px;
    min-width: 152px;
  }

  .pcl-tools-stack,
  .pcl-tools-grid {
    grid-template-columns: 1fr;
  }
}

.pcl-helper-status { display: flex; align-items: flex-start; gap: 8px; padding: 10px; margin: 8px 0; border: 1px solid var(--pcl-border); border-radius: 4px; background: var(--pcl-c8); }
.pcl-helper-status > div { display: flex; flex-direction: column; gap: 2px; min-width: 0; }
.pcl-helper-status strong { color: var(--pcl-text); font-size: calc(11px * var(--pcl-font-scale, 1)); }
.pcl-helper-status small { color: var(--pcl-muted); font-size: calc(9px * var(--pcl-font-scale, 1)); overflow-wrap: anywhere; }
.pcl-helper-error { color: #fb7185 !important; }
</style>
