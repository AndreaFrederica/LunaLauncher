<template>
  <PclButton v-if="pcl" size="sm" icon="upload" :disabled="disabled || preparing || uploading" @click="prepare">{{ preparing ? t('neo.auto2.ke25f6e877f') : t('neo.auto2.kd8c6114d5c') }}</PclButton>
  <q-btn v-else flat icon="cloud_upload" :label="t('neo.auto2.kd8c6114d5c')" :disable="disabled || preparing || uploading" @click="prepare" />
  <q-dialog v-model="shown" :persistent="uploading">
    <q-card class="log-upload-dialog" :class="{ 'log-upload-dialog--pcl': pcl }">
      <q-card-section>
        <h2>{{ url ? t('neo.auto2.keb54b37320') : t('neo.auto2.k786977870f') }}</h2>
        <p>{{ snapshot?.name || name }}</p>
        <p v-if="error" role="alert" class="log-upload-error">{{ error }}</p>
        <template v-if="url">
          <p>{{ copied ? t('neo.auto2.kd2de83a42b') : t('neo.auto2.kf7035228b9') }}</p>
          <input class="log-upload-url" :aria-label="t('neo.auto2.k82df2b61e8')" :value="url" readonly @focus="($event.target as HTMLInputElement).select()" />
          <p v-if="wasTruncated">{{ t('neo.auto2.k7614d11602') }}</p>
        </template>
        <template v-else-if="snapshot">
          <p>{{ t('neo.auto2.k9bf1269e4b') }}<strong>{{ snapshot.baseUrl }}</strong>。</p>
          <p>{{ t('neo.auto2.ked0b536e8f') }}</p>
          <p v-if="snapshot.file">{{ t('neo.auto2.k15162760a3') }}</p>
          <label v-if="snapshot.pasteType === 3" class="log-upload-option">
            <input v-model="trimLongLog" type="checkbox" :disabled="uploading" />{{ t('neo.auto2.k4f441902b5') }}</label>
        </template>
      </q-card-section>
      <q-card-actions align="right">
        <q-btn flat :label="url || !snapshot ? t('neo.auto2.k6c14bd7f6f') : t('neo.auto2.k4d0b4688c7')" :disable="uploading" @click="shown = false" />
        <template v-if="url">
          <q-btn flat :label="t('neo.auto2.kabb22bd95c')" @click="copyLink" />
          <q-btn color="primary" :label="t('neo.auto2.k6d02ca8ccb')" @click="openLink" />
        </template>
        <q-btn v-else-if="snapshot" color="primary" :label="t('neo.auto2.k97d588373b')" :loading="uploading" @click="upload" />
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup lang="ts">
import { useI18n } from "vue-i18n";
const { t } = useI18n();
import { ref } from 'vue';
import { copyToClipboard } from 'quasar';
import { openUrl } from '@tauri-apps/plugin-opener';
import PclButton from '../pcl/components/PclButton.vue';

const props = defineProps<{
  instance: string; name: string; file?: string; content?: string; disabled?: boolean; pcl?: boolean;
  execute: (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
}>();
type Upload = { instance: string; name: string; file?: string; content?: string; pasteType: number; baseUrl: string };
const snapshot = ref<Upload | null>(null);
const shown = ref(false), preparing = ref(false), uploading = ref(false), copied = ref(false);
const trimLongLog = ref(true), wasTruncated = ref(false), url = ref(''), error = ref('');
const bases = ['https://0x0.st', 'https://hst.sh', 'https://paste.gg', 'https://api.mclo.gs'];

async function prepare() {
  if (props.disabled || preparing.value || uploading.value) return;
  preparing.value = true;
  snapshot.value = null;
  url.value = ''; error.value = ''; copied.value = false; trimLongLog.value = true;
  const source = { instance: props.instance, name: props.name, ...(props.file ? { file: props.file } : { content: props.content }) };
  try {
    const settings = await props.execute('settings.list', { scope: 'launcher' });
    const value = (key: string) => settings.find((entry: any) => entry.key === key)?.value;
    const pasteType = Math.max(0, Math.min(3, Math.trunc(Number(value('PastebinType') ?? 3)) || 0));
    const baseUrl = String(value('PastebinCustomAPIBase') || bases[pasteType]).trim().replace(/\/+$/, '');
    if (!['https:', 'http:'].includes(new URL(baseUrl).protocol)) throw new Error(t('neo.auto2.k2007f2b8e0'));
    snapshot.value = { ...source, pasteType, baseUrl };
  } catch (exception) { error.value = String(exception instanceof Error ? exception.message : exception); }
  finally { preparing.value = false; shown.value = true; }
}
async function upload() {
  if (!snapshot.value || uploading.value) return;
  uploading.value = true; error.value = '';
  try {
    const { name: _name, ...parameters } = snapshot.value;
    const result = await props.execute('instance.log.upload', { ...parameters, truncate: trimLongLog.value });
    const link = String(result?.url || '');
    if (!['https:', 'http:'].includes(new URL(link).protocol)) throw new Error(t('neo.auto2.kde098201fe'));
    url.value = link;
    wasTruncated.value = Boolean(result?.truncated);
    await copyLink();
  } catch (exception) { error.value = String(exception instanceof Error ? exception.message : exception); }
  finally { uploading.value = false; }
}
async function copyLink() {
  try { await copyToClipboard(url.value); copied.value = true; error.value = ''; }
  catch { copied.value = false; error.value = t('neo.auto2.ka9bfbb49f9'); }
}
async function openLink() {
  try { await openUrl(url.value); }
  catch (exception) { error.value = String(exception instanceof Error ? exception.message : exception); }
}
</script>

<style scoped>
.log-upload-dialog { width: 560px; max-width: 92vw; background: #202733; color: #eef4ff; overflow-wrap: anywhere; }
.log-upload-dialog--pcl { background: var(--pcl-c7); color: var(--pcl-text); border-radius: var(--pcl-radius); }
h2 { margin: 0 0 12px; font-size: 19px; }
p { margin: 8px 0; line-height: 1.6; }
.log-upload-error { color: #f08080; }
.log-upload-url { width: 100%; padding: 10px; border: 1px solid #64748b; border-radius: 4px; background: transparent; color: inherit; user-select: text; }
.log-upload-option { display: flex; align-items: flex-start; gap: 8px; margin-top: 16px; line-height: 1.6; }
</style>
