<template>
  <div class="pcl-appearance">
    <PclHint v-if="error" theme="red" closable @close="error = ''">{{ error }}</PclHint>

    <PclCard title="界面外观">
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">PCL Style 预设</span>
        <div class="pcl-form-row__control">
          <PclSelect :model-value="preferences.pclStylePreset" :options="pclStylePresetOptions" @update:model-value="selectPclPreset" />
        </div>
        <span class="pcl-form-row__hint">仅影响 PCL Style 的子设置</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">界面风格</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.shell" :options="shellOptions" />
        </div>
        <span class="pcl-form-row__hint">PCL Style外观 / Neo 原生外观</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">导航栏布局</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.pclNavLayout" :options="navLayoutOptions" />
        </div>
        <span class="pcl-form-row__hint">PCL 侧栏或 MMC Classic 实例面板</span>
      </div>
      <template v-if="preferences.pclNavLayout === 'mmc-classic'">
        <div class="pcl-form-row">
          <span class="pcl-form-row__label">Classic 面板位置</span>
          <div class="pcl-form-row__control">
            <PclSelect v-model="preferences.pclClassicSide" :options="classicSideOptions" />
          </div>
          <span class="pcl-form-row__hint">左侧或右侧</span>
        </div>
        <div class="pcl-form-row">
          <span class="pcl-form-row__label">Classic 大图标</span>
          <div class="pcl-form-row__control">
            <PclSelect v-model="preferences.pclClassicIconSize" :options="classicIconOptions" />
          </div>
          <span class="pcl-form-row__hint">显示当前实例封面</span>
        </div>
      </template>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">颜色模式</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.theme" :options="themeOptions" />
        </div>
        <span class="pcl-form-row__hint">深色 / 浅色 / 跟随系统</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">界面密度</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.density" :options="densityOptions" />
        </div>
        <span class="pcl-form-row__hint">影响列表和卡片间距</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">背景样式</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.background" :options="backgroundOptions" />
        </div>
        <span class="pcl-form-row__hint">渐变或纯色</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">界面动效</span>
        <div class="pcl-form-row__control">
          <PclCheckbox v-model="preferences.animations" label="启用页面与控件动画" />
        </div>
        <span class="pcl-form-row__hint">关闭后立即切换</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">加载动画</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.pclLoadingAnimation" :options="loadingAnimationOptions" />
        </div>
        <span class="pcl-form-row__hint">版本列表、资源列表、Java 扫描和任务加载都会使用</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">页面进入时刷新</span>
        <div class="pcl-form-row__control">
          <PclCheckbox v-model="preferences.pclAutoRefreshOnPageOpen" label="自动加载资源目录最新数据" />
        </div>
        <span class="pcl-form-row__hint">打开模组、整合包、数据包、资源包和光影包页面时生效</span>
      </div>

      <div class="pcl-appearance-preview" :class="{ light: preferences.theme === 'light' }">
        <div class="pcl-appearance-preview__bar">
          <span class="pcl-appearance-preview__logo"><img :src="lunaLogo" alt="Luna Launcher" /></span>
          <span class="pcl-appearance-preview__nav"><i />启动</span>
          <span class="pcl-appearance-preview__nav active"><i />下载</span>
          <span class="pcl-appearance-preview__nav"><i />设置</span>
        </div>
        <div class="pcl-appearance-preview__body">
          <div class="pcl-appearance-preview__side"><i /><i /><i /><i /></div>
          <div class="pcl-appearance-preview__card"><b /><span /><span /></div>
        </div>
      </div>

      <div class="pcl-actions">
        <PclButton icon="rotate-ccw" @click="reset">恢复默认外观</PclButton>
      </div>
    </PclCard>

    <PclCard title="主题色">
      <div class="pcl-accent-row">
        <div>
          <strong>强调色</strong>
          <small>按钮、链接、选中项、进度条和标题栏高亮都会跟随这个颜色。</small>
        </div>
        <div class="pcl-accent-preview" aria-hidden="true">
          <span class="pcl-accent-preview__bar" />
          <span class="pcl-accent-preview__button">启动游戏</span>
          <span class="pcl-accent-preview__dot" />
        </div>
      </div>
      <PclColorPicker v-model="preferences.pclAccentColor" />
      <PclHint theme="blue">推荐色包含 PCL 猫蓝、薄荷、翠绿、琥珀、绯红、紫罗兰、樱花和石墨；也可以直接输入 6 位十六进制颜色。</PclHint>
    </PclCard>

    <PclCard title="字体">
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">界面字体</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.pclFont" :options="fontOptions" />
        </div>
        <span class="pcl-form-row__hint">只作用于 PCL 外观</span>
      </div>
      <PclHint theme="blue">字体会应用到 PCL 外观的导航、卡片、按钮、输入框和对话框；切回 Neo 原生外观不受影响。</PclHint>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">基准字体大小</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.pclBaseFontSize" :options="baseFontOptions" />
        </div>
        <span class="pcl-form-row__hint">影响 PCL 界面的整体文字大小</span>
      </div>
      <div class="pcl-font-sample" :class="{ serif: preferences.pclFont === 'simsun' }">
        <strong>Minecraft 1.21.11</strong>
        <span>启动游戏 · 下载 · 设置 · 工具</span>
        <small>ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789</small>
      </div>
    </PclCard>

    <PclCard title="背景图片">
      <input ref="backgroundFile" type="file" accept="image/png,image/jpeg,image/webp,image/gif" class="pcl-hidden-file" @change="onBackgroundFile" />
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">背景图片</span>
        <div class="pcl-form-row__control pcl-background-actions">
          <PclButton size="sm" icon="image" @click="chooseBackground">选择图片</PclButton>
          <PclButton v-if="preferences.pclBackgroundImage" size="sm" variant="ghost" icon="trash-2" @click="clearBackground">清除</PclButton>
        </div>
        <span class="pcl-form-row__hint">{{ preferences.pclBackgroundImageName || 'PNG / JPEG / WebP，最大 1.4 MB' }}</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">不透明度</span>
        <div class="pcl-form-row__control">
          <input v-model.number="preferences.pclBackgroundOpacity" class="pcl-range" type="range" min="0.05" max="1" step="0.05" :disabled="!preferences.pclBackgroundImage" />
        </div>
        <span class="pcl-form-row__hint">{{ Math.round(preferences.pclBackgroundOpacity * 100) }}%</span>
      </div>
      <div v-if="preferences.pclBackgroundImage" class="pcl-background-preview" :style="{ backgroundImage: `url(${preferences.pclBackgroundImage})`, opacity: preferences.pclBackgroundOpacity }" />
      <PclHint theme="blue">图片会作为启动页、实例页和设置页的页面背景，卡片与导航保持不透明；图片保存在本机 localStorage，不上传。</PclHint>
    </PclCard>

    <PclCard title="背景音乐">
      <input ref="musicFile" type="file" accept="audio/*" class="pcl-hidden-file" @change="onMusicFile" />
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">音乐文件</span>
        <div class="pcl-form-row__control pcl-background-actions">
          <PclButton size="sm" icon="music" @click="chooseMusic">选择音乐</PclButton>
          <PclButton v-if="pclMusic.loaded" size="sm" icon="play" @click="togglePclMusic">{{ pclMusic.playing ? '暂停' : '播放' }}</PclButton>
          <PclButton v-if="pclMusic.loaded" size="sm" variant="ghost" icon="trash-2" @click="removePclMusic">移除</PclButton>
        </div>
        <span class="pcl-form-row__hint">{{ pclMusic.name || 'MP3 / OGG / WAV，最大 30 MB' }}</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">音量</span>
        <div class="pcl-form-row__control">
          <input class="pcl-range" type="range" min="0" max="1" step="0.05" :value="pclMusic.volume" :disabled="!pclMusic.loaded" @input="onMusicVolume" />
        </div>
        <span class="pcl-form-row__hint">{{ Math.round(pclMusic.volume * 100) }}%</span>
      </div>
      <PclHint v-if="pclMusic.error" theme="red" closable @close="pclMusic.error = ''">{{ pclMusic.error }}</PclHint>
      <PclHint theme="blue">音乐保存在本机 IndexedDB，不写入 localStorage；切换页面时保持播放。</PclHint>
    </PclCard>

    <PclCard title="主页卡片">
      <div class="pcl-check-grid">
        <PclCheckbox v-model="preferences.pclHomeShowInstances" label="显示全部实例面板" />
        <PclCheckbox v-model="preferences.pclHomeShowServers" label="显示当前实例服务器" />
        <PclCheckbox v-model="preferences.pclHomeSwitchOnQuickLaunch" label="双击启动时切换当前实例" />
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">实例面板排列</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.pclHomeInstanceLayout" :options="instanceLayoutOptions" />
        </div>
        <span class="pcl-form-row__hint">网格或单列列表</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">实例面板行数</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.pclHomeInstanceRows" :options="instanceRowOptions" />
        </div>
        <span class="pcl-form-row__hint">超出后滚动</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">实例文字位置</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.pclHomeInstanceTextPosition" :options="instanceTextPositionOptions" />
        </div>
        <span class="pcl-form-row__hint">图标右侧或图标下方</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">实例面板字号</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.pclHomeInstanceFontSize" :options="instanceFontOptions" />
        </div>
        <span class="pcl-form-row__hint">实例名称和副标题字号</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">实例面板宽度</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.pclHomeInstanceWidth" :options="instanceWidthOptions" />
        </div>
        <span class="pcl-form-row__hint">占右侧内容区宽度</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">实例分类方式</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.pclHomeInstanceGrouping" :options="instanceGroupingOptions" />
        </div>
        <span class="pcl-form-row__hint">平铺或按 MMC 分组显示</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">实例排序方式</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.pclHomeInstanceSort" :options="instanceSortOptions" />
        </div>
        <span class="pcl-form-row__hint">手动排序可拖拽调整</span>
      </div>
      <div v-if="preferences.pclHomeInstanceLayout === 'grid'" class="pcl-form-row">
        <span class="pcl-form-row__label">实例面板列数</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.pclHomeInstanceColumns" :options="instanceColumnOptions" />
        </div>
        <span class="pcl-form-row__hint">每行显示的实例数量</span>
      </div>
      <div class="pcl-home-order">
        <div class="pcl-home-order__title">首页卡片顺序</div>
        <div v-for="(item, index) in homeOrderItems" :key="item.key" class="pcl-home-order__row">
          <span>{{ item.label }}</span>
          <div>
            <PclButton size="sm" variant="ghost" icon="chevron-up" :disabled="index === 0" @click="moveHomeOrder(index, -1)" />
            <PclButton size="sm" variant="ghost" icon="chevron-down" :disabled="index === homeOrderItems.length - 1" @click="moveHomeOrder(index, 1)" />
          </div>
        </div>
      </div>
      <div v-for="(card, index) in preferences.pclHomeCards" :key="card.id" class="pcl-home-editor">
        <div class="pcl-home-editor__head">
          <strong>卡片 {{ index + 1 }}</strong>
          <PclButton size="sm" variant="ghost" icon="trash-2" @click="removeHomeCard(card.id)">删除</PclButton>
        </div>
        <PclInput v-model="card.title" placeholder="卡片标题" />
        <PclInput v-model="card.content" multiline :rows="2" placeholder="卡片内容" />
      </div>
      <div v-if="!preferences.pclHomeCards.length" class="pcl-empty pcl-empty--compact">还没有主页卡片。</div>
      <div class="pcl-actions">
        <PclButton size="sm" icon="plus" @click="addHomeCard">添加卡片</PclButton>
      </div>
      <PclHint theme="blue">卡片显示在 PCL 启动页右上角，可以用来放公告、服务器地址或使用提示。</PclHint>
    </PclCard>

    <PclCard title="标题栏">
      <div class="pcl-check-grid">
        <PclCheckbox v-model="preferences.pclTitleLogo" label="显示 LunaLauncher 文字" />
        <PclCheckbox v-model="preferences.pclTitleBadge" label="显示 NEOUI 副标题" />
        <PclCheckbox v-model="preferences.pclLaunchAnimation" label="启用 PCL 启动过渡动画" />
        <PclCheckbox v-model="preferences.pclClearLaunchLogOnStart" label="每次启动时清除历史启动日志" />
        <PclCheckbox v-model="preferences.pclLaunchLogAutoScroll" label="启动日志自动滚动到底部" />
        <PclCheckbox v-model="preferences.pclEditCurrentInWindow" label="当前实例也用独立编辑窗口" />
      </div>
      <PclHint theme="blue">只修改 PCL 外观顶部标题栏，不影响窗口按钮、导航和内容区。</PclHint>
    </PclCard>

    <PclCard title="启动页浮动按钮">
      <div class="pcl-check-grid">
        <PclCheckbox v-model="preferences.pclFloatTasks" label="任务中心" />
        <PclCheckbox v-model="preferences.pclFloatLog" label="启动日志" />
        <PclCheckbox v-model="preferences.pclFloatRefresh" label="刷新按钮" />
        <PclCheckbox v-model="preferences.pclFloatMusic" label="背景音乐" />
      </div>
      <PclHint theme="blue">关闭后不会在 PCL 启动页右侧显示对应浮动按钮，功能仍可从页面内进入。</PclHint>
    </PclCard>

    <PclCard title="Web UI 加载">
      <p class="pcl-appearance-copy">设置启动界面、扫描目录和是否记住上次选择，也可以切换到独立 Web UI。</p>
      <div class="pcl-actions">
        <PclButton variant="highlight" icon="layout-dashboard" @click="openWebUiManager">加载设置与界面管理</PclButton>
      </div>
    </PclCard>

    <PclCard title="功能隐藏">
      <p class="pcl-appearance-copy">隐藏不常用的页面入口。隐藏只影响导航，不会删除实例、设置或下载内容；随时可以在这里恢复。</p>
      <div class="pcl-hide-groups">
        <div class="pcl-hide-group">
          <h4>设置页</h4>
          <PclCheckbox
            v-for="feature in setupFeatures"
            :key="feature.id"
            :model-value="Boolean(hiddenFeatures[`setup.${feature.id}`])"
            :label="feature.label"
            @update:model-value="setPclFeatureHidden(`setup.${feature.id}`, $event)"
          />
        </div>
        <div class="pcl-hide-group">
          <h4>下载页</h4>
          <PclCheckbox
            v-for="feature in downloadFeatures"
            :key="feature.id"
            :model-value="Boolean(hiddenFeatures[`download.${feature.id}`])"
            :label="feature.label"
            @update:model-value="setPclFeatureHidden(`download.${feature.id}`, $event)"
          />
        </div>
      </div>
      <div class="pcl-actions">
        <PclButton icon="rotate-ccw" @click="resetPclFeatures">恢复全部入口</PclButton>
      </div>
    </PclCard>

    <PclCard title="PCL 主题说明">
      <p class="pcl-appearance-copy">PCL Style外观使用 CatBlue 配色：标题栏、选中条和强调色固定为 #2a80e3，深浅模式共用同一套色阶。</p>
      <p class="pcl-appearance-copy pcl-muted">如果你希望调整强调色，请切换到 Neo 原生外观后使用原有外观设置。</p>
    </PclCard>
  </div>
</template>

<script setup lang="ts">
import { computed, ref } from 'vue';
import lunaLogo from '../../../assets/luna-launcher.png';
import PclCard from '../../components/PclCard.vue';
import PclButton from '../../components/PclButton.vue';
import PclHint from '../../components/PclHint.vue';
import PclSelect from '../../components/PclSelect.vue';
import PclCheckbox from '../../components/PclCheckbox.vue';
import PclColorPicker from '../../components/PclColorPicker.vue';
import PclInput from '../../components/PclInput.vue';
import { choosePclMusic, pclMusic, removePclMusic, setPclMusicVolume, togglePclMusic } from '../../composables/usePclMusic';
import { applyPclStylePreset, neoUiPreferences, persistNeoUiPreferences } from '../../../neo-ui-preferences';
import { launcher } from '../../../boot/launcher-api';
import {
  hiddenPclFeatures as hiddenFeatures,
  pclDownloadFeatures as downloadFeatures,
  pclSetupFeatures as setupFeatures,
  resetPclFeatures,
  setPclFeatureHidden,
} from '../../composables/usePclFeatures';

const preferences = neoUiPreferences;
const error = ref('');
const backgroundFile = ref<HTMLInputElement | null>(null);
const musicFile = ref<HTMLInputElement | null>(null);

function chooseMusic() {
  musicFile.value?.click();
}

function onMusicFile(event: Event) {
  const input = event.target as HTMLInputElement;
  const file = input.files?.[0];
  input.value = '';
  if (file) void choosePclMusic(file);
}

function onMusicVolume(event: Event) {
  setPclMusicVolume(Number((event.target as HTMLInputElement).value));
}

function chooseBackground() {
  backgroundFile.value?.click();
}

function onBackgroundFile(event: Event) {
  const input = event.target as HTMLInputElement;
  const file = input.files?.[0];
  input.value = '';
  if (!file) return;
  if (file.size > 1_400_000) {
    error.value = '背景图片过大（超过 1.4 MB），请压缩后再选择。';
    return;
  }
  const reader = new FileReader();
  reader.onload = () => {
    preferences.pclBackgroundImage = String(reader.result || '');
    preferences.pclBackgroundImageName = file.name;
    error.value = '';
    persistNeoUiPreferences();
  };
  reader.onerror = () => { error.value = '读取背景图片失败。'; };
  reader.readAsDataURL(file);
}

const homeOrderItems = computed(() => {
  const items = [
    { key: 'hint', label: 'Neo UI 提示' },
    ...preferences.pclHomeCards.map(card => ({ key: `custom:${card.id}`, label: card.title || '自定义卡片' })),
    { key: 'log', label: '启动日志' },
    { key: 'instances', label: '全部实例' },
    { key: 'servers', label: '当前实例服务器' },
  ];
  const valid = new Set(items.map(item => item.key));
  const ordered = preferences.pclHomeCardOrder.filter(key => valid.has(key));
  for (const item of items) if (!ordered.includes(item.key)) ordered.push(item.key);
  return ordered.map(key => items.find(item => item.key === key)!).filter(Boolean);
});
function moveHomeOrder(index: number, delta: number) {
  const keys = homeOrderItems.value.map(item => item.key);
  const target = index + delta;
  if (target < 0 || target >= keys.length) return;
  [keys[index], keys[target]] = [keys[target], keys[index]];
  preferences.pclHomeCardOrder = keys;
  persistNeoUiPreferences();
}
function selectPclPreset(value: string) {
  applyPclStylePreset(value === 'pcl' ? 'pcl' : 'luna-neo');
}
function addHomeCard() {
  preferences.pclHomeCards.push({
    id: `${Date.now()}-${Math.random().toString(36).slice(2, 7)}`,
    title: '新卡片',
    content: '',
  });
  persistNeoUiPreferences();
}

function removeHomeCard(id: string) {
  preferences.pclHomeCards = preferences.pclHomeCards.filter(card => card.id !== id);
  persistNeoUiPreferences();
}

function clearBackground() {
  preferences.pclBackgroundImage = '';
  preferences.pclBackgroundImageName = '';
  persistNeoUiPreferences();
}
const pclStylePresetOptions = [
  { label: 'Luna Neo（默认）', value: 'luna-neo' },
  { label: 'PCL', value: 'pcl' },
];
const navLayoutOptions = [
  { label: 'PCL 侧栏', value: 'pcl' },
  { label: 'MMC Classic', value: 'mmc-classic' },
];
const classicSideOptions = [
  { label: '左侧', value: 'left' },
  { label: '右侧', value: 'right' },
];
const classicIconOptions = [
  { label: '64 px', value: 64 },
  { label: '80 px', value: 80 },
  { label: '96 px', value: 96 },
  { label: '120 px', value: 120 },
  { label: '160 px', value: 160 },
];
const shellOptions = [
  { label: 'PCL Style外观', value: 'pcl' },
  { label: 'Neo UI 原生外观', value: 'neo' },
];
const themeOptions = [
  { label: '深色', value: 'dark' },
  { label: '浅色', value: 'light' },
  { label: '跟随系统', value: 'system' },
];
const densityOptions = [
  { label: '舒适', value: 'comfortable' },
  { label: '紧凑', value: 'compact' },
];
const backgroundOptions = [
  { label: '渐变背景', value: 'gradient' },
  { label: '纯色背景', value: 'solid' },
];
const loadingAnimationOptions = [
  { label: 'Luna 转圈', value: 'luna-spinner' },
  { label: 'PCL 镐子', value: 'pcl-pickaxe' },
  { label: '横向加载条', value: 'pcl-bar' },
];
const instanceWidthOptions = [
  { label: '60%', value: 60 },
  { label: '70%', value: 70 },
  { label: '80%', value: 80 },
  { label: '90%', value: 90 },
  { label: '100%', value: 100 },
];
const instanceLayoutOptions = [
  { label: '网格', value: 'grid' },
  { label: '单列列表', value: 'list' },
];
const instanceGroupingOptions = [
  { label: '平铺显示', value: 'flat' },
  { label: 'MMC 分组', value: 'grouped' },
];
const instanceSortOptions = [
  { label: '手动排序（可拖拽）', value: 'manual' },
  { label: '按名称自动排序', value: 'name' },
  { label: '按版本自动排序', value: 'version' },
];
const instanceColumnOptions = [
  { label: '自动（按宽度）', value: 'auto' },
  { label: '1 列', value: 1 },
  { label: '2 列', value: 2 },
  { label: '3 列', value: 3 },
  { label: '4 列', value: 4 },
  { label: '5 列', value: 5 },
  { label: '6 列', value: 6 },
  { label: '7 列', value: 7 },
  { label: '8 列', value: 8 },
  { label: '10 列', value: 10 },
];
const instanceTextPositionOptions = [
  { label: '图标右侧', value: 'beside' },
  { label: '图标下方（MMC）', value: 'below' },
];
const instanceRowOptions = [
  { label: '2 行', value: 2 },
  { label: '3 行', value: 3 },
  { label: '4 行', value: 4 },
  { label: '5 行', value: 5 },
  { label: '6 行', value: 6 },
  { label: '8 行', value: 8 },
];
const instanceFontOptions = [
  { label: '10 px', value: 10 },
  { label: '11 px', value: 11 },
  { label: '12 px', value: 12 },
  { label: '13 px', value: 13 },
  { label: '14 px（默认）', value: 14 },
  { label: '15 px', value: 15 },
  { label: '16 px', value: 16 },
  { label: '18 px', value: 18 },
];
const baseFontOptions = [
  { label: '11 px', value: 11 },
  { label: '12 px', value: 12 },
  { label: '13 px', value: 13 },
  { label: '14 px（默认）', value: 14 },
  { label: '15 px', value: 15 },
  { label: '16 px', value: 16 },
  { label: '18 px', value: 18 },
];
const fontOptions = [
  { label: '系统默认（Segoe UI / 微软雅黑）', value: 'system' },
  { label: '微软雅黑 UI', value: 'yahei' },
  { label: '等线 DengXian', value: 'dengxian' },
  { label: '宋体 SimSun', value: 'simsun' },
];
async function openWebUiManager() {
  error.value = '';
  try {
    await launcher.openWebUiManager();
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  }
}
function reset() {
  Object.assign(neoUiPreferences, {
    shell: 'pcl',
    theme: 'dark',
    accent: '#2a80e3',
    density: 'comfortable',
    background: 'gradient',
    animations: true,
    pclFont: 'system',
    pclStylePreset: 'luna-neo',
    pclLoadingAnimation: 'luna-spinner',
    pclAutoRefreshOnPageOpen: true,
    pclBaseFontSize: 14,
    pclNavLayout: 'mmc-classic',
    pclClassicSide: 'right',
    pclClassicIconSize: 96,
    pclLaunchAnimation: false,
    pclClearLaunchLogOnStart: true,
    pclLaunchLogAutoScroll: true,
    pclEditCurrentInWindow: true,
    pclFloatTasks: false,
    pclFloatLog: false,
    pclFloatRefresh: false,
    pclFloatMusic: false,
    pclTitleLogo: true,
    pclTitleBadge: true,
    pclAccentColor: '#d65c9a',
    pclBackgroundImage: '',
    pclBackgroundImageName: '',
    pclBackgroundOpacity: 0.45,
    pclBackgroundMusicName: '',
    pclBackgroundMusicVolume: 0.35,
    pclBackgroundMusicEnabled: false,
    pclHomeCards: [{ id: 'welcome', title: '欢迎回来', content: '在 设置 → 个性化 → 主页卡片 中编辑这张卡片。' }],
    pclHomeShowInstances: true,
    pclHomeShowServers: true,
    pclHomeSwitchOnQuickLaunch: false,
    pclHomeInstanceLayout: 'grid',
    pclHomeInstanceColumns: 'auto',
    pclHomeInstanceRows: 4,
    pclHomeInstanceWidth: 100,
    pclHomeInstanceFontSize: 14,
    pclHomeInstanceTextPosition: 'below',
    pclHomeInstanceGrouping: 'grouped',
    pclHomeInstanceSort: 'name',
    pclHomeInstanceOrder: [],
    pclHomeCardOrder: ['instances', 'log', 'hint', 'custom:welcome', 'servers'],
  });
  persistNeoUiPreferences();
}
</script>

<style scoped>
.pcl-appearance {
  max-width: 100%;
}

.pcl-hidden-file {
  display: none;
}

.pcl-home-editor {
  display: flex;
  flex-direction: column;
  gap: 7px;
  margin-bottom: 10px;
  padding: 9px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-half-white);
}

.pcl-home-editor__head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-background-actions {
  display: flex;
  flex-wrap: wrap;
  gap: 7px;
}

.pcl-background-preview {
  height: 120px;
  margin: 4px 0 10px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background-position: center;
  background-size: cover;
  background-repeat: no-repeat;
}

.pcl-range {
  width: 100%;
  accent-color: var(--pcl-c3);
}

.pcl-appearance-copy {
  margin: 0 0 8px;
  color: var(--pcl-text);
  font-size: calc(13px * var(--pcl-font-scale, 1));
  line-height: 1.8;
}

.pcl-appearance-preview {
  margin: 16px 0 4px;
  overflow: hidden;
  border: 1px solid var(--pcl-c5);
  border-radius: 5px;
  background: var(--pcl-c8);
}

.pcl-appearance-preview__bar {
  display: flex;
  align-items: center;
  gap: 12px;
  height: 34px;
  padding: 0 12px;
  background: var(--pcl-c3);
  color: #fff;
}

.pcl-appearance-preview__logo {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 22px;
  height: 22px;
  border: 2px solid #fff;
  border-radius: 4px;
  font-size: calc(12px * var(--pcl-font-scale, 1));
  font-weight: 800;
}

.pcl-appearance-preview__logo img {
  width: 100%;
  height: 100%;
  object-fit: contain;
}

.pcl-appearance-preview__nav {
  display: inline-flex;
  align-items: center;
  gap: 5px;
  height: 22px;
  padding: 0 10px;
  border-radius: 11px;
  font-size: calc(11px * var(--pcl-font-scale, 1));
  opacity: .9;
}

.pcl-appearance-preview__nav i {
  width: 9px;
  height: 9px;
  border: 1px solid currentColor;
  border-radius: 2px;
}

.pcl-appearance-preview__nav.active {
  background: #fff;
  color: var(--pcl-c3);
  opacity: 1;
}

.pcl-appearance-preview__body {
  display: flex;
  min-height: 92px;
}

.pcl-appearance-preview__side {
  display: flex;
  flex-direction: column;
  gap: 9px;
  width: 92px;
  padding: 12px;
  background: var(--pcl-nav-bg);
}

.pcl-appearance-preview__side i {
  height: 6px;
  border-radius: 3px;
  background: var(--pcl-c5);
  opacity: .55;
}

.pcl-appearance-preview__card {
  display: flex;
  flex: 1;
  flex-direction: column;
  gap: 8px;
  margin: 12px;
  padding: 12px;
  border-radius: 4px;
  background: var(--pcl-card);
}

.pcl-appearance-preview__card b {
  width: 90px;
  height: 7px;
  border-radius: 3px;
  background: var(--pcl-c3);
}

.pcl-appearance-preview__card span {
  height: 5px;
  border-radius: 3px;
  background: var(--pcl-c5);
  opacity: .55;
}

.pcl-appearance-preview.light {
  background: #eef4fb;
}

.pcl-appearance-preview.light .pcl-appearance-preview__side {
  background: #f7fafd;
}

.pcl-hide-groups {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 18px 28px;
  margin-top: 4px;
}

.pcl-hide-group h4 {
  margin: 0 0 7px;
  color: var(--pcl-text);
  font-size: calc(12px * var(--pcl-font-scale, 1));
  font-weight: 700;
  opacity: .72;
}

.pcl-hide-group .pcl-check {
  display: flex;
  margin-bottom: 4px;
}

.pcl-font-sample {
  display: flex;
  flex-direction: column;
  gap: 4px;
  margin-top: 12px;
  padding: 12px 14px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-c7);
  color: var(--pcl-text);
  font-family: var(--pcl-font);
}

.pcl-font-sample strong {
  font-size: calc(15px * var(--pcl-font-scale, 1));
  font-weight: 700;
}

.pcl-font-sample span {
  font-size: calc(13px * var(--pcl-font-scale, 1));
}

.pcl-font-sample small {
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-font-sample.serif {
  font-family: SimSun, "Songti SC", serif;
}

@media (max-width: 720px) {
  .pcl-hide-groups {
    grid-template-columns: 1fr;
  }
}

.pcl-accent-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 18px;
  margin-bottom: 12px;
}

.pcl-accent-row > div:first-child {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.pcl-accent-row strong {
  color: var(--pcl-text);
  font-size: calc(12px * var(--pcl-font-scale, 1));
  font-weight: 500;
}

.pcl-accent-row small {
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}

.pcl-accent-preview {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 8px 10px;
  border: 1px solid var(--pcl-border);
  border-radius: 5px;
  background: var(--pcl-c8);
}

.pcl-accent-preview__bar {
  display: block;
  width: 42px;
  height: 5px;
  border-radius: 3px;
  background: var(--pcl-c3);
}

.pcl-accent-preview__button {
  padding: 4px 9px;
  border-radius: 4px;
  background: var(--pcl-c3);
  color: #fff;
  font-size: calc(9px * var(--pcl-font-scale, 1));
}

.pcl-accent-preview__dot {
  display: block;
  width: 20px;
  height: 20px;
  border: 2px solid var(--pcl-c3);
  border-radius: 50%;
  box-shadow: inset 0 0 0 4px var(--pcl-c8);
  background: var(--pcl-c3);
}

@media (max-width: 620px) {
  .pcl-accent-row {
    align-items: flex-start;
    flex-direction: column;
  }
}

.pcl-home-order {
  display: flex;
  flex-direction: column;
  gap: 3px;
  margin: 10px 0 14px;
  padding: 9px 10px;
  border: 1px solid var(--pcl-border);
  border-radius: 4px;
  background: var(--pcl-c8);
}

.pcl-home-order__title {
  margin-bottom: 4px;
  color: var(--pcl-text);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-home-order__row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 8px;
  min-height: 24px;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}
</style>
