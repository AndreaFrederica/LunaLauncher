<template>
  <PclButton v-if="pcl" size="sm" icon="upload" :disabled="disabled || preparing || uploading" @click="prepare">{{ preparing ? '准备中…' : '一键上传' }}</PclButton>
  <q-btn v-else flat icon="cloud_upload" label="一键上传" :disable="disabled || preparing || uploading" @click="prepare" />
  <q-dialog v-model="shown" :persistent="uploading">
    <q-card class="log-upload-dialog" :class="{ 'log-upload-dialog--pcl': pcl }">
      <q-card-section>
        <h2>{{ url ? '日志上传成功' : '上传日志' }}</h2>
        <p>{{ snapshot?.name || name }}</p>
        <p v-if="error" role="alert" class="log-upload-error">{{ error }}</p>
        <template v-if="url">
          <p>{{ copied ? '链接已复制，可以直接粘贴分享。' : '日志已上传，可复制下方链接。' }}</p>
          <input class="log-upload-url" aria-label="日志分享链接" :value="url" readonly @focus="($event.target as HTMLInputElement).select()" />
          <p v-if="wasTruncated">超长日志已保留开头和末尾，中间部分已省略。</p>
        </template>
        <template v-else-if="snapshot">
          <p>将上传至 <strong>{{ snapshot.baseUrl }}</strong>。</p>
          <p>上传前会按 C++ 版规则隐藏部分个人信息，请仍检查日志内容后再分享。</p>
          <p v-if="snapshot.file">上传所选日志文件，包含预览未显示的内容（解压后上限 16 MiB）。</p>
          <label v-if="snapshot.pasteType === 3" class="log-upload-option">
            <input v-model="trimLongLog" type="checkbox" :disabled="uploading" />
            超过 25,000 行时保留前 10,000 行和最后 14,900 行
          </label>
        </template>
      </q-card-section>
      <q-card-actions align="right">
        <q-btn flat :label="url || !snapshot ? '关闭' : '取消'" :disable="uploading" @click="shown = false" />
        <template v-if="url">
          <q-btn flat label="复制链接" @click="copyLink" />
          <q-btn color="primary" label="打开链接" @click="openLink" />
        </template>
        <q-btn v-else-if="snapshot" color="primary" label="确认上传" :loading="uploading" @click="upload" />
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup lang="ts">
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
    if (!['https:', 'http:'].includes(new URL(baseUrl).protocol)) throw new Error('日志上传服务地址无效，请检查设置。');
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
    if (!['https:', 'http:'].includes(new URL(link).protocol)) throw new Error('上传服务未返回有效的分享链接。');
    url.value = link;
    wasTruncated.value = Boolean(result?.truncated);
    await copyLink();
  } catch (exception) { error.value = String(exception instanceof Error ? exception.message : exception); }
  finally { uploading.value = false; }
}
async function copyLink() {
  try { await copyToClipboard(url.value); copied.value = true; error.value = ''; }
  catch { copied.value = false; error.value = '自动复制失败，请选中上方链接手动复制。'; }
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
