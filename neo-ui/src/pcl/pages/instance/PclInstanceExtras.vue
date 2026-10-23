<template>
  <PclCard :title="section === 'worlds' ? '存档' : '截图'" class="pcl-extras">

    <template v-if="section === 'worlds'">
      <div class="pcl-extras-head">
        <div>
          <strong>世界存档</strong>
          <small>管理存档、导入世界并导出备份。</small>
        </div>
        <PclButton size="sm" icon="folder-open" :disabled="busy" @click="openFolder('saves')">打开目录</PclButton>
        <PclButton size="sm" icon="refresh-cw" :disabled="busy" @click="loadWorlds">刷新</PclButton>
      </div>
      <div class="pcl-extras-form">
        <PclInput v-model="worldSource" placeholder="世界目录或 ZIP 路径" />
        <PclInput v-model="worldName" placeholder="导入后的名称（可选）" />
        <PclButton variant="highlight" icon="arrow-up-from-line" :disabled="busy || !worldSource.trim()" @click="importWorld">导入</PclButton>
      </div>
      <PclHint v-if="worldError" theme="red" closable @close="worldError = ''">{{ worldError }}</PclHint>
      <div v-if="worlds.length" class="pcl-world-list">
        <div v-for="world in worlds" :key="world.folder || world.name" class="pcl-world-row">
          <span class="pcl-world-row__icon"><PclIcon name="earth" :size="20" /></span>
          <div class="pcl-world-row__copy">
            <strong>{{ world.name || world.folder }}</strong>
            <small>{{ world.folder }} · {{ formatWorldDate(world.lastPlayed) }} · {{ formatSize(world.size) }}</small>
          </div>
          <div class="pcl-world-row__actions">
            <PclIconButton icon="pencil" label="重命名" :size="24" :disabled="busy" @click="renameWorld(world)" />
            <PclIconButton icon="copy" label="复制" :size="24" :disabled="busy" @click="copyWorld(world)" />
            <PclIconButton icon="download" label="导出" :size="24" :disabled="busy" @click="exportWorld(world)" />
            <PclIconButton icon="image" label="重置图标" :size="24" :disabled="busy" @click="resetWorldIcon(world)" />
            <PclIconButton icon="trash-2" label="删除" :size="24" danger :disabled="busy" @click="deleteWorld(world)" />
          </div>
        </div>
      </div>
      <div v-else-if="!loading" class="pcl-empty pcl-extras-empty">
        <PclIcon name="earth" :size="34" />
        <strong>这个实例还没有世界存档。</strong>
        <span>可以导入世界目录或 ZIP 备份。</span>
      </div>
      <PclLoading v-if="loading" text="正在读取世界" inline />
    </template>

    <template v-else>
      <div class="pcl-extras-head">
        <div>
          <strong>截图</strong>
          <small>浏览、导入、上传或删除游戏截图。</small>
        </div>
        <PclButton size="sm" icon="folder-open" :disabled="busy" @click="openFolder('screenshots')">打开目录</PclButton>
        <PclButton size="sm" icon="refresh-cw" :disabled="busy" @click="loadScreenshots">刷新</PclButton>
      </div>
      <div class="pcl-extras-form">
        <PclInput v-model="screenshotSource" placeholder="本地图片路径" />
        <PclInput v-model="screenshotName" placeholder="导入文件名（可选）" />
        <PclButton variant="highlight" icon="arrow-up-from-line" :disabled="busy || !screenshotSource.trim()" @click="importScreenshot">导入</PclButton>
      </div>
      <PclHint v-if="screenshotError" theme="red" closable @close="screenshotError = ''">{{ screenshotError }}</PclHint>
      <div v-if="preview" class="pcl-shot-preview">
        <img :src="preview" alt="截图预览" />
        <PclIconButton icon="x" label="关闭预览" :size="26" @click="preview = ''" />
      </div>
      <div v-if="screenshots.length" class="pcl-shot-grid">
        <article v-for="shot in screenshots" :key="shot.name" class="pcl-shot-card">
          <button class="pcl-shot-card__thumb" type="button" :disabled="busy" @click="readScreenshot(shot)">
            <img v-if="shot.thumbnail" :src="shot.thumbnail" alt="" />
            <PclIcon v-else name="image" :size="30" />
          </button>
          <div class="pcl-shot-card__copy">
            <strong>{{ shot.name }}</strong>
            <small>{{ formatSize(shot.size) }} · {{ formatWorldDate(shot.modified) }}</small>
          </div>
          <div class="pcl-shot-card__actions">
            <PclIconButton icon="search" label="查看" :size="22" :disabled="busy" @click="readScreenshot(shot)" />
            <PclIconButton icon="pencil" label="重命名" :size="22" :disabled="busy" @click="renameScreenshot(shot)" />
            <PclIconButton icon="arrow-up-from-line" label="上传" :size="22" :disabled="busy" @click="uploadScreenshot(shot)" />
            <PclIconButton icon="trash-2" label="删除" :size="22" danger :disabled="busy" @click="deleteScreenshot(shot)" />
          </div>
        </article>
      </div>
      <div v-else-if="!loading" class="pcl-empty pcl-extras-empty">
        <PclIcon name="image" :size="34" />
        <strong>这个实例还没有截图。</strong>
        <span>可以导入本地图片。</span>
      </div>
      <PclLoading v-if="loading" text="正在读取截图" inline />
    </template>
  </PclCard>
</template>

<script setup lang="ts">
import { onMounted, ref, watch } from 'vue';
import PclButton from '../../components/PclButton.vue';
import PclCard from '../../components/PclCard.vue';
import PclHint from '../../components/PclHint.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclIconButton from '../../components/PclIconButton.vue';
import PclInput from '../../components/PclInput.vue';
import PclLoading from '../../components/PclLoading.vue';
import { launcher } from '../../../boot/launcher-api';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
const props = defineProps<{ instance: string; section: 'worlds' | 'screenshots'; busy: boolean; execute: Execute }>();
const emit = defineEmits<{ changed: [] }>();

const loading = ref(false);
const worldError = ref('');
const screenshotError = ref('');
const worlds = ref<any[]>([]);
const screenshots = ref<any[]>([]);
const worldSource = ref('');
const worldName = ref('');
const screenshotSource = ref('');
const screenshotName = ref('');
const preview = ref('');

function formatSize(value: unknown) {
  const size = Number(value || 0);
  if (!size) return '大小未知';
  if (size < 1024) return `${size} B`;
  if (size < 1024 * 1024) return `${(size / 1024).toFixed(1)} KiB`;
  return `${(size / 1024 / 1024).toFixed(1)} MiB`;
}

function formatWorldDate(value: unknown) {
  if (!value) return '从未游玩';
  const date = new Date(String(value));
  return Number.isNaN(date.getTime()) ? String(value) : date.toLocaleString();
}

async function openFolder(kind: string) {
  try {
    const result = await props.execute('instance.folder-path', { instance: props.instance, kind });
    if (result?.path) await launcher.openPath(result.path);
  } catch (exception) {
    const message = exception instanceof Error ? exception.message : String(exception);
    if (props.section === 'worlds') worldError.value = message;
    else screenshotError.value = message;
  }
}

async function loadWorlds() {
  loading.value = true; worldError.value = '';
  try { const result = await props.execute('instance.world.list', { instance: props.instance }); worlds.value = Array.isArray(result) ? result : []; }
  catch (error) { worldError.value = error instanceof Error ? error.message : String(error); }
  finally { loading.value = false; }
}

async function loadScreenshots() {
  loading.value = true; screenshotError.value = '';
  try {
    const result = await props.execute('instance.screenshot.list', { instance: props.instance });
    const list = Array.isArray(result) ? result : [];
    screenshots.value = list.map((shot: any) => ({ ...shot, thumbnail: '' }));
    // PCL shows a real thumbnail in every card. Read the bounded image payload
    // through the screenshot API so the native frontend and web preview share
    // exactly the same path and size checks.
    await Promise.all(screenshots.value.map(async (shot: any) => {
      try { const image = await props.execute('instance.screenshot.read', { instance: props.instance, file: shot.name }); shot.thumbnail = `data:image/${image.format || 'png'};base64,${image.content}`; } catch { /* an unreadable image remains a placeholder */ }
    }));
  } catch (error) { screenshotError.value = error instanceof Error ? error.message : String(error); }
  finally { loading.value = false; }
}

async function renameWorld(world: any) {
  const name = window.prompt('新的世界名称', world.name || world.folder);
  if (!name || name === (world.name || world.folder)) return;
  await props.execute('instance.world.rename', { instance: props.instance, world: world.folder, name }); emit('changed'); await loadWorlds();
}

async function copyWorld(world: any) {
  const name = window.prompt('复制到新的世界名称', `${world.name || world.folder} 副本`);
  if (!name) return;
  await props.execute('instance.world.copy', { instance: props.instance, world: world.folder, name }); emit('changed'); await loadWorlds();
}

async function deleteWorld(world: any) {
  if (!window.confirm(`确定删除世界“${world.name || world.folder}”吗？`)) return;
  await props.execute('instance.world.delete', { instance: props.instance, world: world.folder, confirm: true }); emit('changed'); await loadWorlds();
}

async function importWorld() {
  await props.execute('instance.world.import', { instance: props.instance, source: worldSource.value.trim(), ...(worldName.value.trim() ? { name: worldName.value.trim() } : {}) });
  worldSource.value = ''; worldName.value = ''; emit('changed'); await loadWorlds();
}

async function exportWorld(world: any) {
  const output = window.prompt('导出 ZIP 的完整路径', `${world.name || world.folder}.zip`);
  if (!output) return;
  await props.execute('instance.world.export', { instance: props.instance, world: world.folder, output, overwrite: true });
}

async function resetWorldIcon(world: any) {
  if (!window.confirm(`移除世界“${world.name || world.folder}”的图标吗？`)) return;
  await props.execute('instance.world.reset-icon', { instance: props.instance, world: world.folder });
  emit('changed'); await loadWorlds();
}

async function importScreenshot() {
  const source = screenshotSource.value.trim(); if (!source) return;
  const name = screenshotName.value.trim() || source.split(/[\\/]/).pop() || 'screenshot.png';
  await props.execute('instance.screenshot.import', { instance: props.instance, source, name });
  screenshotSource.value = ''; screenshotName.value = ''; emit('changed'); await loadScreenshots();
}

async function readScreenshot(shot: any) {
  try {
    const result = await props.execute('instance.screenshot.read', { instance: props.instance, file: shot.name });
    preview.value = `data:image/${result.format || 'png'};base64,${result.content}`;
  } catch (error) { screenshotError.value = error instanceof Error ? error.message : String(error); }
}

async function uploadScreenshot(shot: any) { await props.execute('instance.screenshot.upload', { instance: props.instance, file: shot.name }); }
async function renameScreenshot(shot: any) {
  const name = window.prompt('新的截图文件名（png/jpg/jpeg）', shot.name);
  if (!name || name === shot.name) return;
  await props.execute('instance.screenshot.rename', { instance: props.instance, file: shot.name, name });
  emit('changed'); await loadScreenshots();
}
async function deleteScreenshot(shot: any) {
  if (!window.confirm(`确定删除截图“${shot.name}”吗？`)) return;
  await props.execute('instance.screenshot.delete', { instance: props.instance, file: shot.name, confirm: true });
  emit('changed'); await loadScreenshots();
}

watch(() => [props.instance, props.section], () => { preview.value = ''; if (props.section === 'worlds') void loadWorlds(); else void loadScreenshots(); });
onMounted(() => { if (props.section === 'worlds') void loadWorlds(); else void loadScreenshots(); });
</script>

<style scoped>
.pcl-extras-head {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 12px;
  margin-bottom: 9px;
}

.pcl-extras-head > div {
  display: flex;
  flex-direction: column;
  gap: 3px;
}

.pcl-extras-head strong {
  color: var(--pcl-text);
  font-size: calc(12px * var(--pcl-font-scale, 1));
  font-weight: 500;
}

.pcl-extras-head small {
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}

.pcl-extras-form {
  display: grid;
  grid-template-columns: minmax(0, 1fr) minmax(130px, .45fr) auto;
  align-items: center;
  gap: 6px;
  margin-bottom: 9px;
}

.pcl-world-list {
  display: flex;
  flex-direction: column;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
}

.pcl-world-row {
  display: flex;
  align-items: center;
  gap: 9px;
  min-height: 44px;
  padding: 5px 8px;
  border-bottom: 1px solid var(--pcl-border);
  background: var(--pcl-c7);
}

.pcl-world-row:last-child {
  border-bottom: 0;
}

.pcl-world-row:hover {
  background: var(--pcl-c6);
}

.pcl-world-row__icon {
  display: flex;
  align-items: center;
  justify-content: center;
  flex: 0 0 28px;
  width: 28px;
  height: 28px;
  border-radius: 4px;
  background: var(--pcl-c6);
  color: var(--pcl-c2);
}

.pcl-world-row__copy {
  display: flex;
  flex-direction: column;
  min-width: 0;
  flex: 1;
}

.pcl-world-row__copy strong {
  overflow: hidden;
  color: var(--pcl-text);
  font-size: calc(11px * var(--pcl-font-scale, 1));
  font-weight: 500;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-world-row__copy small {
  margin-top: 2px;
  overflow: hidden;
  color: var(--pcl-muted);
  font-size: calc(9px * var(--pcl-font-scale, 1));
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-world-row__actions {
  display: flex;
  align-items: center;
  gap: 1px;
}

.pcl-shot-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(190px, 1fr));
  gap: 8px;
}

.pcl-shot-card {
  overflow: hidden;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-c7);
}

.pcl-shot-card__thumb {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 100%;
  aspect-ratio: 16 / 9;
  overflow: hidden;
  border: 0;
  background: var(--pcl-c8);
  color: var(--pcl-c5);
  cursor: pointer;
}

.pcl-shot-card__thumb img {
  width: 100%;
  height: 100%;
  object-fit: cover;
}

.pcl-shot-card__copy {
  display: flex;
  flex-direction: column;
  min-width: 0;
  padding: 6px 8px 0;
}

.pcl-shot-card__copy strong {
  overflow: hidden;
  color: var(--pcl-text);
  font-size: calc(10px * var(--pcl-font-scale, 1));
  font-weight: 500;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-shot-card__copy small {
  margin-top: 2px;
  color: var(--pcl-muted);
  font-size: calc(9px * var(--pcl-font-scale, 1));
}

.pcl-shot-card__actions {
  display: flex;
  justify-content: flex-end;
  gap: 1px;
  padding: 2px 4px 5px;
}

.pcl-shot-preview {
  position: relative;
  max-height: 360px;
  margin-bottom: 10px;
  overflow: hidden;
  border: 1px solid var(--pcl-border);
  border-radius: 4px;
  background: var(--pcl-c8);
}

.pcl-shot-preview img {
  display: block;
  width: 100%;
  max-height: 360px;
  object-fit: contain;
}

.pcl-shot-preview :deep(.pcl-icon-btn) {
  position: absolute;
  top: 6px;
  right: 6px;
  background: var(--pcl-c7);
}

.pcl-extras-empty {
  min-height: 160px;
}

@media (max-width: 660px) {
  .pcl-extras-form {
    grid-template-columns: 1fr;
  }
}
</style>
