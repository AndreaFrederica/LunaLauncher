<template>
  <PclCard :title="t('pcl.dataLocation.title')">
    <PclHint v-if="error" theme="red" closable @close="error = ''">{{ error }}</PclHint>
    <PclHint v-if="notice" theme="blue" closable @close="notice = ''">{{ notice }}</PclHint>
    <div class="pcl-form-row">
      <span class="pcl-form-row__label">{{ t('pcl.dataLocation.current') }}</span>
      <div class="pcl-form-row__control pcl-data-location-path">
        <code>{{ status?.current || runtimeInfo?.dataRoot || '—' }}</code>
        <PclButton size="sm" icon="folder-open" :disabled="!status?.current" @click="openPath(status?.current)">{{ t('account.open') }}</PclButton>
      </div>
    </div>
    <div class="pcl-form-row">
      <span class="pcl-form-row__label">{{ t('pcl.dataLocation.mode') }}</span>
      <div class="pcl-form-row__control">
        <PclSelect v-model="mode" :options="modeOptions" :disabled="busy || loading" />
      </div>
      <span class="pcl-form-row__hint">{{ modeHint }} · {{ currentMode === 'portable' ? t('pcl.dataLocation.portable') : t('pcl.dataLocation.userData') }}</span>
    </div>
    <div class="pcl-form-row">
      <span class="pcl-form-row__label">{{ t('pcl.dataLocation.target') }}</span>
      <div class="pcl-form-row__control pcl-data-location-path"><code>{{ targetPath || '—' }}</code></div>
    </div>
    <div class="pcl-actions">
      <PclButton variant="highlight" icon="save" :disabled="busy || loading || !status || mode === currentMode" @click="apply">
        {{ t('pcl.dataLocation.apply') }}
      </PclButton>
      <PclButton icon="folder-open" :disabled="!status?.portablePath" @click="openPath(status?.portablePath)">{{ t('pcl.dataLocation.openPortable') }}</PclButton>
      <PclButton icon="folder-open" :disabled="!status?.systemPath" @click="openPath(status?.systemPath)">{{ t('pcl.dataLocation.openUser') }}</PclButton>
    </div>
    <PclHint theme="yellow">{{ t('pcl.dataLocation.restartHint') }}</PclHint>
  </PclCard>
</template>

<script setup lang="ts">
import { computed, inject, onMounted, ref } from 'vue';
import { PclSetupContextKey } from './PclSetupContext';
import PclButton from '../../components/PclButton.vue';
import PclCard from '../../components/PclCard.vue';
import PclHint from '../../components/PclHint.vue';
import PclSelect from '../../components/PclSelect.vue';

const context = inject(PclSetupContextKey);
if (!context) throw new Error('PclSetupContext is missing');
const { t, runtimeInfo, execute } = context;

const status = ref<any>(null);
const mode = ref<'portable' | 'user'>('user');
const loading = ref(false);
const busy = ref(false);
const error = ref('');
const notice = ref('');
const modeOptions = computed(() => [
  { label: t('pcl.dataLocation.portable'), value: 'portable' },
  { label: t('pcl.dataLocation.userData'), value: 'user' },
]);
function unwrapStatus(value: any): any {
  if (value?.ok === true && value.data && typeof value.data === 'object') return value.data;
  if (value?.status && typeof value.status === 'object') return value.status;
  return value;
}

function pathKey(value: unknown): string {
  return String(value || '').replaceAll('\\', '/').replace(/\/+$/, '').toLowerCase();
}

const currentMode = computed<'portable' | 'user'>(() => {
  if (!status.value) return 'user';
  if (typeof status.value.portable === 'boolean') return status.value.portable ? 'portable' : 'user';
  return pathKey(status.value.current) === pathKey(status.value.portablePath) ? 'portable' : 'user';
});
const targetPath = computed(() => mode.value === 'portable' ? status.value?.portablePath : status.value?.systemPath);
const modeHint = computed(() => mode.value === 'portable' ? t('pcl.dataLocation.portableHint') : t('pcl.dataLocation.userDataHint'));

async function load() {
  loading.value = true;
  error.value = '';
  try {
    status.value = unwrapStatus(await execute('startup.data-location'));
    mode.value = currentMode.value;
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    loading.value = false;
  }
}

async function openPath(path: string | undefined) {
  if (!path) return;
  try {
    await execute('desktop.open-path', { path });
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  }
}

async function apply() {
  if (!status.value || busy.value) return;
  busy.value = true;
  error.value = '';
  notice.value = '';
  try {
    const result = unwrapStatus(await execute('startup.set-data-location', { portable: mode.value === 'portable' }));
    status.value = result || status.value;
    notice.value = t('pcl.dataLocation.applied');
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    busy.value = false;
  }
}

onMounted(load);
</script>

<style scoped>
.pcl-data-location-path { display: flex; align-items: center; gap: 10px; flex-wrap: wrap; }
.pcl-data-location-path code { word-break: break-all; }
</style>
