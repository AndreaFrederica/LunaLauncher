<template>
  <MelodyModernWindow kind="preferences" @close="$emit('close')">
    <div class="melody-workspace">
      <nav class="melody-modern-nav" aria-label="设置分类">
        <section v-for="group in groups" :key="group.label">
          <h2>{{ group.label }}</h2>
          <button v-for="item in group.items" :key="item.id" :class="{ active: section === item.id }" :aria-current="section === item.id ? 'page' : undefined" @click="section = item.id">{{ item.label }}</button>
        </section>
      </nav>
      <main class="melody-modern-main">
        <p v-if="error || appearanceError" class="melody-error" role="alert">{{ error || appearanceError }}</p>
        <div v-show="section === 'theme'" class="melody-settings-stack">
          <MelodyGroupBox title="启动器主题">
            <div class="melody-preference-row"><span>主题</span>
              <MelodySelect aria-label="主题" model-value="melody" :options="shellOptions" @update:model-value="value => $emit('theme', value as NeoShell)" />
            </div>
          </MelodyGroupBox>
          <MelodyGroupBox title="主界面背景">
            <div class="melody-preference-row"><span>背景方式</span>
              <MelodySelect aria-label="背景方式" :model-value="appearance.backgroundMode" :options="backgroundOptions" @update:model-value="value => update({ backgroundMode: value as typeof appearance.backgroundMode })" />
            </div>
            <img class="melody-background-preview" :src="backgroundUrl" :style="{ objectFit: appearance.fit }" alt="背景预览" />
            <div class="melody-toolbar">
              <MelodyButton :disabled="uploading" @click="fileInput?.click()">选择图片</MelodyButton>
              <MelodyButton :disabled="uploading" @click="pickDirectory">选择目录</MelodyButton>
              <MelodyButton :disabled="appearance.backgroundMode !== 'random' || backgroundLoading || uploading" @click="shuffle">换一张</MelodyButton>
              <MelodyButton :disabled="appearance.backgroundMode === 'builtin' || uploading" @click="resetBackground">恢复原版背景</MelodyButton>
              <span>{{ imageName || '原版背景' }}</span>
              <input ref="fileInput" type="file" accept="image/png,image/jpeg,image/webp,image/gif,image/bmp" aria-label="自定义背景图片" hidden @change="chooseImage" />
              <input ref="directoryInput" type="file" webkitdirectory multiple aria-label="背景图片目录" hidden @change="importDirectory" />
            </div>
            <p v-if="appearance.directory" class="melody-directory-path">背景目录：{{ appearance.directory }}</p>
            <p v-if="appearance.backgroundMode === 'random'" role="status">{{ imageCount }} 张图片 · 每次启动随机选择一张{{ native ? '' : ' · 已导入所选目录' }}</p>
            <div class="melody-preference-row"><span>图片适配</span>
              <MelodySelect aria-label="图片适配" :model-value="appearance.fit" :options="fitOptions" @update:model-value="value => update({ fit: value as typeof appearance.fit })" />
            </div>
          </MelodyGroupBox>
          <MelodyGroupBox title="主界面底部文字">
            <MelodyRadioGroup :model-value="appearance.footer" :options="footerOptions" label="底部文字" vertical @update:model-value="value => update({ footer: value as typeof appearance.footer })" />
            <label v-if="appearance.footer === 'custom'" class="melody-preference-row">文字内容
              <MelodyInput :model-value="appearance.footerText" maxlength="200" @update:model-value="value => update({ footerText: value })" />
            </label>
            <div class="melody-footer-options">
              <label>字号 <input class="melody-input" type="number" :value="appearance.footerSize" min="10" max="24" @change="event => update({ footerSize: Number((event.target as HTMLInputElement).value) || 12 })" /> px</label>
              <label>颜色 <input type="color" :value="appearance.footerColor" @input="event => update({ footerColor: (event.target as HTMLInputElement).value })" /></label>
              <label>不透明度 <input type="range" min="0" max="1" step="0.05" :value="appearance.footerOpacity" @input="event => update({ footerOpacity: Number((event.target as HTMLInputElement).value) })" /></label>
            </div>
            <div class="melody-footer-preview"><span :style="footerStyle">{{ footer }}</span></div>
          </MelodyGroupBox>
          <div class="melody-toolbar">
            <MelodyButton @click="$emit('legacy')">打开原版设置</MelodyButton>
          </div>
        </div>
        <div v-show="section !== 'theme'">
          <PclSetupPage embedded :initial-section="section" :settings="settings" :accounts="accounts" :busy="loading" :scope="{ scope: 'launcher' }" :execute="melodyExecute" @changed="load" />
        </div>
      </main>
    </div>
  </MelodyModernWindow>
</template>
<script setup lang="ts">
import { onMounted, ref } from 'vue';
import { isTauri } from '@tauri-apps/api/core';
import { open as openDialog } from '@tauri-apps/plugin-dialog';
import type { NeoShell } from '../../../../neo-ui-preferences';
import PclSetupPage from '../../../pcl/pages/settings/PclSetupPage.vue';
import MelodyModernWindow from '../../components/MelodyModernWindow.vue';
import MelodyButton from '../../components/MelodyButton.vue';
import MelodyInput from '../../components/MelodyInput.vue';
import MelodySelect from '../../components/MelodySelect.vue';
import MelodyGroupBox from '../../components/MelodyGroupBox.vue';
import MelodyRadioGroup from '../../components/MelodyRadioGroup.vue';
import { useMelodyAppearance } from '../../composables/preferences';
import { melodyExecute } from '../../services/backend';
defineEmits<{ close: []; legacy: []; theme: [shell: NeoShell] }>();
const native = isTauri();
const { appearance, backgroundUrl, imageName, imageCount, loading: backgroundLoading, footer, footerStyle, error: appearanceError, upload, uploadDirectory, chooseDirectory, shuffle, remove, set } = useMelodyAppearance();
const section = ref('theme'), settings = ref<any[]>([]), accounts = ref<any[]>([]), error = ref(''), loading = ref(false), uploading = ref(false);
const fileInput = ref<HTMLInputElement>();
const directoryInput = ref<HTMLInputElement>();
const shellOptions = [{ value: 'neo', label: 'NeoUI' }, { value: 'pcl', label: 'PCL Style' }, { value: 'melody', label: '忘却的旋律' }];
const backgroundOptions = [{ value: 'builtin', label: '原版背景' }, { value: 'single', label: '指定图片' }, { value: 'random', label: '目录随机背景' }];
const groups = [
  { label: '个性化', items: [{ id: 'theme', label: '主题与背景' }, { id: 'accounts', label: '账户' }, { id: 'language', label: '语言' }] },
  { label: '游戏', items: [{ id: 'launch', label: '游戏启动' }, { id: 'memory', label: '内存' }, { id: 'java', label: 'Java' }, { id: 'game', label: '游戏设置' }, { id: 'advanced', label: '高级启动' }] },
  { label: '启动器', items: [{ id: 'network', label: '联机服务' }, { id: 'download-settings', label: '下载设置' }, { id: 'paths', label: '文件与路径' }, { id: 'cache', label: '缓存' }, { id: 'console', label: '控制台' }, { id: 'misc', label: '启动器与代理' }, { id: 'advanced-settings', label: '高级设置' }] },
  { label: '帮助', items: [{ id: 'about', label: '关于' }, { id: 'update', label: '检查更新' }, { id: 'feedback', label: '反馈' }, { id: 'logs', label: '启动器日志' }] },
];
const fitOptions = [{ value: 'cover', label: '填满（裁剪边缘）' }, { value: 'contain', label: '完整显示' }, { value: 'fill', label: '拉伸' }];
const footerOptions = [{ value: 'legacy', label: 'MineCraft Launcher 5.31 by 忘却的旋律' }, { value: 'modern', label: 'Luna Launcher Neo UI' }, { value: 'custom', label: '自定义文字' }];
function update(patch: Parameters<typeof set>[0]) { try { set(patch); error.value = ''; } catch (e) { error.value = String(e); } }
async function chooseImage(event: Event) {
  const input = event.target as HTMLInputElement, file = input.files?.[0];
  if (!file) return;
  uploading.value = true; error.value = '';
  try { await upload(file); } catch (e) { error.value = String(e); }
  finally { uploading.value = false; input.value = ''; }
}
async function resetBackground() { try { await remove(); error.value = ''; } catch (e) { error.value = String(e); } }
async function pickDirectory() {
  if (!native) { directoryInput.value?.click(); return; }
  uploading.value = true; error.value = '';
  try { const path = await openDialog({ directory: true, multiple: false, title: '选择背景图片目录', defaultPath: appearance.directory || undefined }); if (typeof path === 'string') await chooseDirectory(path); }
  catch (e) { error.value = String(e); } finally { uploading.value = false; }
}
async function importDirectory(event: Event) {
  const input = event.target as HTMLInputElement;
  if (!input.files?.length) return;
  uploading.value = true; error.value = '';
  try { await uploadDirectory([...input.files]); } catch (e) { error.value = String(e); }
  finally { uploading.value = false; input.value = ''; }
}
async function load() {
  if (!isTauri()) return;
  loading.value = true;
  try {
    const [values, users] = await Promise.all([melodyExecute('settings.list', { scope: 'launcher' }), melodyExecute('account.list')]);
    settings.value = Array.isArray(values) ? values : values?.entries || [];
    accounts.value = users; error.value = '';
  } catch (e) { error.value = String(e); } finally { loading.value = false; }
}
onMounted(load);
</script>
