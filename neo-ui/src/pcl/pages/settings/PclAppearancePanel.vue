<template>
  <div class="pcl-appearance">
    <PclHint v-if="error" theme="red" closable @close="error = ''">{{ error }}</PclHint>

    <PclCard :title="t('pcl.auto2.k3cb108966b')">
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">{{ t('pcl.auto2.k801c1d7a50') }}</span>
        <div class="pcl-form-row__control">
          <PclSelect :model-value="preferences.pclStylePreset" :options="pclStylePresetOptions" @update:model-value="selectPclPreset" />
        </div>
        <span class="pcl-form-row__hint">{{ t('pcl.auto2.ka4483ddba9') }}</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">{{ t('pcl.auto2.k568704e735') }}</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.shell" :options="shellOptions" />
        </div>
        <span class="pcl-form-row__hint">{{ t('pcl.auto2.ka4da30d951') }}</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">{{ t('pcl.auto2.kbd6b0b793d') }}</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.pclNavLayout" :options="navLayoutOptions" />
        </div>
        <span class="pcl-form-row__hint">{{ t('pcl.auto2.k019755f952') }}</span>
      </div>
      <template v-if="preferences.pclNavLayout === 'mmc-classic'">
        <div class="pcl-form-row">
          <span class="pcl-form-row__label">{{ t('pcl.auto2.kc9d3af0140') }}</span>
          <div class="pcl-form-row__control">
            <PclSelect v-model="preferences.pclClassicSide" :options="classicSideOptions" />
          </div>
          <span class="pcl-form-row__hint">{{ t('pcl.auto2.k0d247203bd') }}</span>
        </div>
        <div class="pcl-form-row">
          <span class="pcl-form-row__label">{{ t('pcl.auto2.k4c0c01ba00') }}</span>
          <div class="pcl-form-row__control">
            <PclSelect v-model="preferences.pclClassicIconSize" :options="classicIconOptions" />
          </div>
          <span class="pcl-form-row__hint">{{ t('pcl.auto2.k82607c19ac') }}</span>
        </div>
      </template>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">{{ t('pcl.auto2.kb9f0a68d24') }}</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.theme" :options="themeOptions" />
        </div>
        <span class="pcl-form-row__hint">{{ t('pcl.auto2.k05debddfa6') }}</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">{{ t('pcl.auto2.kc2fd3fcdd3') }}</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.density" :options="densityOptions" />
        </div>
        <span class="pcl-form-row__hint">{{ t('pcl.auto2.k868adb3546') }}</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">{{ t('pcl.auto2.k812c67106b') }}</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.background" :options="backgroundOptions" />
        </div>
        <span class="pcl-form-row__hint">{{ t('pcl.auto2.k2fb2244bcd') }}</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">{{ t('pcl.auto2.k44fe65399d') }}</span>
        <div class="pcl-form-row__control">
          <PclCheckbox v-model="preferences.animations" :label="t('pcl.auto2.k186b0dbbed')" />
        </div>
        <span class="pcl-form-row__hint">{{ t('pcl.auto2.k60f0ddc680') }}</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">{{ t('pcl.auto2.k1ee93134f2') }}</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.pclLoadingAnimation" :options="loadingAnimationOptions" />
        </div>
        <span class="pcl-form-row__hint">{{ t('pcl.auto2.kafa5fbe627') }}</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">{{ t('pcl.auto2.k785c90db9e') }}</span>
        <div class="pcl-form-row__control">
          <PclCheckbox v-model="preferences.pclAutoRefreshOnPageOpen" :label="t('pcl.auto2.k08799dfbdd')" />
        </div>
        <span class="pcl-form-row__hint">{{ t('pcl.auto2.k9bef92f3a5') }}</span>
      </div>

      <div class="pcl-appearance-preview" :class="{ light: preferences.theme === 'light' }">
        <div class="pcl-appearance-preview__bar">
          <span class="pcl-appearance-preview__logo"><img :src="lunaLogo" alt="Luna Launcher" /></span>
          <span class="pcl-appearance-preview__nav"><i />{{ t('pcl.auto2.kebd26da421') }}</span>
          <span class="pcl-appearance-preview__nav active"><i />{{ t('pcl.auto2.k2b9d013177') }}</span>
          <span class="pcl-appearance-preview__nav"><i />{{ t('pcl.auto2.k7debf9cb03') }}</span>
        </div>
        <div class="pcl-appearance-preview__body">
          <div class="pcl-appearance-preview__side"><i /><i /><i /><i /></div>
          <div class="pcl-appearance-preview__card"><b /><span /><span /></div>
        </div>
      </div>

      <div class="pcl-actions">
        <PclButton icon="rotate-ccw" @click="reset">{{ t('pcl.auto2.k73a911c5ba') }}</PclButton>
      </div>
    </PclCard>

    <PclCard :title="t('pcl.auto2.k90b878eaa4')">
      <div class="pcl-accent-row">
        <div>
          <strong>{{ t('pcl.auto2.k6f67371e05') }}</strong>
          <small>{{ t('pcl.auto2.k955ef92a45') }}</small>
        </div>
        <div class="pcl-accent-preview" aria-hidden="true">
          <span class="pcl-accent-preview__bar" />
          <span class="pcl-accent-preview__button">{{ t('pcl.auto2.ke05557937a') }}</span>
          <span class="pcl-accent-preview__dot" />
        </div>
      </div>
      <PclColorPicker v-model="preferences.pclAccentColor" />
      <PclHint theme="blue">{{ t('pcl.auto2.k4b93c0d33c') }}</PclHint>
    </PclCard>

    <PclCard :title="t('pcl.auto2.kb50d4d8352')">
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">{{ t('pcl.auto2.ke809454466') }}</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.pclFont" :options="fontOptions" />
        </div>
        <span class="pcl-form-row__hint">{{ t('pcl.auto2.k84e49b65d1') }}</span>
      </div>
      <PclHint theme="blue">{{ t('pcl.auto2.ked9a1fb76d') }}</PclHint>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">{{ t('pcl.auto2.k5230f6d1f1') }}</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.pclBaseFontSize" :options="baseFontOptions" />
        </div>
        <span class="pcl-form-row__hint">{{ t('pcl.auto2.k26cad9d136') }}</span>
      </div>
      <div class="pcl-font-sample" :class="{ serif: preferences.pclFont === 'simsun' }">
        <strong>Minecraft 1.21.11</strong>
        <span>{{ t('pcl.auto2.k5ebdf7c69e') }}</span>
        <small>ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789</small>
      </div>
    </PclCard>

    <PclCard :title="t('pcl.auto2.k8c84186614')">
      <input ref="backgroundFile" type="file" accept="image/png,image/jpeg,image/webp,image/gif" class="pcl-hidden-file" @change="onBackgroundFile" />
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">{{ t('pcl.auto2.k8c84186614') }}</span>
        <div class="pcl-form-row__control pcl-background-actions">
          <PclButton size="sm" icon="image" @click="chooseBackground">{{ t('pcl.auto2.k18104edf89') }}</PclButton>
          <PclButton v-if="preferences.pclBackgroundImage" size="sm" variant="ghost" icon="trash-2" @click="clearBackground">{{ t('pcl.auto2.k7b15e5e8e7') }}</PclButton>
        </div>
        <span class="pcl-form-row__hint">{{ preferences.pclBackgroundImageName || t('pcl.auto2.kf15863559a') }}</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">{{ t('pcl.auto2.kb4efb6cdcf') }}</span>
        <div class="pcl-form-row__control">
          <input v-model.number="preferences.pclBackgroundOpacity" class="pcl-range" type="range" min="0.05" max="1" step="0.05" :disabled="!preferences.pclBackgroundImage" />
        </div>
        <span class="pcl-form-row__hint">{{ Math.round(preferences.pclBackgroundOpacity * 100) }}%</span>
      </div>
      <div v-if="preferences.pclBackgroundImage" class="pcl-background-preview" :style="{ backgroundImage: `url(${preferences.pclBackgroundImage})`, opacity: preferences.pclBackgroundOpacity }" />
      <PclHint theme="blue">{{ t('pcl.auto2.k6ffb1c3811') }}</PclHint>
    </PclCard>

    <PclCard :title="t('pcl.auto2.kd77bcb52c1')">
      <input ref="musicFile" type="file" accept="audio/*" class="pcl-hidden-file" @change="onMusicFile" />
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">{{ t('pcl.auto2.k46ff734544') }}</span>
        <div class="pcl-form-row__control pcl-background-actions">
          <PclButton size="sm" icon="music" @click="chooseMusic">{{ t('pcl.auto2.k47261a4981') }}</PclButton>
          <PclButton v-if="pclMusic.loaded" size="sm" icon="play" @click="togglePclMusic">{{ pclMusic.playing ? t('pcl.auto2.k130448bce6') : t('pcl.auto2.k21925350de') }}</PclButton>
          <PclButton v-if="pclMusic.loaded" size="sm" variant="ghost" icon="trash-2" @click="removePclMusic">{{ t('pcl.auto2.k2f752c005e') }}</PclButton>
        </div>
        <span class="pcl-form-row__hint">{{ pclMusic.name || t('pcl.auto2.k5ecfcb94f2') }}</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">{{ t('pcl.auto2.k1aa999a596') }}</span>
        <div class="pcl-form-row__control">
          <input class="pcl-range" type="range" min="0" max="1" step="0.05" :value="pclMusic.volume" :disabled="!pclMusic.loaded" @input="onMusicVolume" />
        </div>
        <span class="pcl-form-row__hint">{{ Math.round(pclMusic.volume * 100) }}%</span>
      </div>
      <PclHint v-if="pclMusic.error" theme="red" closable @close="pclMusic.error = ''">{{ pclMusic.error }}</PclHint>
      <PclHint theme="blue">{{ t('pcl.auto2.kb423e54435') }}</PclHint>
    </PclCard>

    <PclCard :title="t('pcl.auto2.kf1da7b1ad5')">
      <div class="pcl-check-grid">
        <PclCheckbox v-model="preferences.pclHomeShowInstances" :label="t('pcl.auto2.k408dd99bac')" />
        <PclCheckbox v-model="preferences.pclHomeShowServers" :label="t('pcl.auto2.ka071286fa0')" />
        <PclCheckbox v-model="preferences.pclHomeSwitchOnQuickLaunch" :label="t('pcl.auto2.k9369dff72a')" />
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">{{ t('pcl.auto2.kd9f204c974') }}</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.pclHomeInstanceLayout" :options="instanceLayoutOptions" />
        </div>
        <span class="pcl-form-row__hint">{{ t('pcl.auto2.kf79883cc98') }}</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">{{ t('pcl.auto2.k7300be4dff') }}</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.pclHomeInstanceRows" :options="instanceRowOptions" />
        </div>
        <span class="pcl-form-row__hint">{{ t('pcl.auto2.k521663e186') }}</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">{{ t('pcl.auto2.k0150261ae5') }}</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.pclHomeInstanceTextPosition" :options="instanceTextPositionOptions" />
        </div>
        <span class="pcl-form-row__hint">{{ t('pcl.auto2.kb4a979a814') }}</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">{{ t('pcl.auto2.k93327129af') }}</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.pclHomeInstanceFontSize" :options="instanceFontOptions" />
        </div>
        <span class="pcl-form-row__hint">{{ t('pcl.auto2.k31121bacba') }}</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">{{ t('pcl.auto2.k0bb26cb213') }}</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.pclHomeInstanceWidth" :options="instanceWidthOptions" />
        </div>
        <span class="pcl-form-row__hint">{{ t('pcl.auto2.kacfb1c89cf') }}</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">{{ t('pcl.auto2.k74a387ad56') }}</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.pclHomeInstanceGrouping" :options="instanceGroupingOptions" />
        </div>
        <span class="pcl-form-row__hint">{{ t('pcl.auto2.kdd35dc2385') }}</span>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">{{ t('pcl.auto2.k66b117ecd1') }}</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.pclHomeInstanceSort" :options="instanceSortOptions" />
        </div>
        <span class="pcl-form-row__hint">{{ t('pcl.auto2.k8fb1fe0396') }}</span>
      </div>
      <div v-if="preferences.pclHomeInstanceLayout === 'grid'" class="pcl-form-row">
        <span class="pcl-form-row__label">{{ t('pcl.auto2.kf55c6c4f72') }}</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="preferences.pclHomeInstanceColumns" :options="instanceColumnOptions" />
        </div>
        <span class="pcl-form-row__hint">{{ t('pcl.auto2.k6b7572954f') }}</span>
      </div>
      <div class="pcl-home-order">
        <div class="pcl-home-order__title">{{ t('pcl.auto2.kda3e2743dd') }}</div>
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
          <strong>{{ t('pcl.appearance.card', { index: index + 1 }) }}</strong>
          <PclButton size="sm" variant="ghost" icon="trash-2" @click="removeHomeCard(card.id)">{{ t('pcl.auto2.k3755f56f2f') }}</PclButton>
        </div>
        <PclInput v-model="card.title" :placeholder="t('pcl.auto2.k14987b310c')" />
        <PclInput v-model="card.content" multiline :rows="2" :placeholder="t('pcl.auto2.kc72b6a2a9e')" />
      </div>
      <div v-if="!preferences.pclHomeCards.length" class="pcl-empty pcl-empty--compact">{{ t('pcl.auto2.k72526c8ca3') }}</div>
      <div class="pcl-actions">
        <PclButton size="sm" icon="plus" @click="addHomeCard">{{ t('pcl.auto2.kab89692a67') }}</PclButton>
      </div>
      <PclHint theme="blue">{{ t('pcl.auto2.k32e45be482') }}</PclHint>
    </PclCard>

    <PclCard :title="t('pcl.auto2.ke8bef90efb')">
      <div class="pcl-check-grid">
        <PclCheckbox v-model="preferences.pclTitleLogo" :label="t('pcl.auto2.k06206d6e5a')" />
        <PclCheckbox v-model="preferences.pclTitleBadge" :label="t('pcl.auto2.k4b5ce9f21e')" />
        <PclCheckbox v-model="preferences.pclLaunchAnimation" :label="t('pcl.auto2.k8eb55365e3')" />
        <PclCheckbox v-model="preferences.pclClearLaunchLogOnStart" :label="t('pcl.auto2.ke51a90b26f')" />
        <PclCheckbox v-model="preferences.pclLaunchLogAutoScroll" :label="t('pcl.auto2.k731cff1a1b')" />
        <PclCheckbox v-model="preferences.pclEditCurrentInWindow" :label="t('pcl.auto2.k61a28ec924')" />
      </div>
      <PclHint theme="blue">{{ t('pcl.auto2.ke51c7fd2cd') }}</PclHint>
    </PclCard>

    <PclCard :title="t('pcl.auto2.kbc4abdc526')">
      <div class="pcl-check-grid">
        <PclCheckbox v-model="preferences.pclFloatTasks" :label="t('pcl.auto2.kf692ff9ec5')" />
        <PclCheckbox v-model="preferences.pclFloatLog" :label="t('pcl.auto2.k8ad868abbc')" />
        <PclCheckbox v-model="preferences.pclFloatRefresh" :label="t('pcl.auto2.kd3978de624')" />
        <PclCheckbox v-model="preferences.pclFloatMusic" :label="t('pcl.auto2.kd77bcb52c1')" />
      </div>
      <PclHint theme="blue">{{ t('pcl.auto2.ke7d1f96aed') }}</PclHint>
    </PclCard>

    <PclCard :title="t('pcl.auto2.k8f18baab83')">
      <p class="pcl-appearance-copy">{{ t('pcl.auto2.ka7be7a3347') }}</p>
      <div class="pcl-actions">
        <PclButton variant="highlight" icon="layout-dashboard" @click="openWebUiManager">{{ t('pcl.auto2.k15e516c7b8') }}</PclButton>
      </div>
    </PclCard>

    <PclCard :title="t('pcl.auto2.k6003ab3029')">
      <p class="pcl-appearance-copy">{{ t('pcl.auto2.k6be310b560') }}</p>
      <div class="pcl-hide-groups">
        <div class="pcl-hide-group">
          <h4>{{ t('pcl.auto2.kf4c7977e73') }}</h4>
          <PclCheckbox
            v-for="feature in setupFeatures"
            :key="feature.id"
            :model-value="Boolean(hiddenFeatures[`setup.${feature.id}`])"
            :label="feature.label"
            @update:model-value="setPclFeatureHidden(`setup.${feature.id}`, $event)"
          />
        </div>
        <div class="pcl-hide-group">
          <h4>{{ t('pcl.auto2.kd1bf0f4018') }}</h4>
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
        <PclButton icon="rotate-ccw" @click="resetPclFeatures">{{ t('pcl.auto2.k94468fe606') }}</PclButton>
      </div>
    </PclCard>

    <PclCard :title="t('pcl.auto2.kf29e0171e6')">
      <p class="pcl-appearance-copy">{{ t('pcl.auto2.k81749e6f2a') }}</p>
      <p class="pcl-appearance-copy pcl-muted">{{ t('pcl.auto2.k1b551f758e') }}</p>
    </PclCard>
  </div>
</template>

<script setup lang="ts">
import { useI18n } from "vue-i18n";
const { t } = useI18n();
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
    error.value = t('pcl.auto2.kd2a56a402e');
    return;
  }
  const reader = new FileReader();
  reader.onload = () => {
    preferences.pclBackgroundImage = String(reader.result || '');
    preferences.pclBackgroundImageName = file.name;
    error.value = '';
    persistNeoUiPreferences();
  };
  reader.onerror = () => { error.value = t('pcl.auto2.k25ff2e16db'); };
  reader.readAsDataURL(file);
}

const homeOrderItems = computed(() => {
  const items = [
    { key: 'hint', label: t('pcl.auto2.k096047b598') },
    ...preferences.pclHomeCards.map(card => ({ key: `custom:${card.id}`, label: card.title || t('pcl.auto2.kced8d0f34e') })),
    { key: 'log', label: t('pcl.auto2.k8ad868abbc') },
    { key: 'instances', label: t('pcl.auto2.k9d2dba1a26') },
    { key: 'servers', label: t('pcl.auto2.kb2cfd67505') },
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
    title: t('pcl.auto2.kef4fd94620'),
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
  { label: t('pcl.auto2.ka6dc467052'), value: 'luna-neo' },
  { label: 'PCL', value: 'pcl' },
];
const navLayoutOptions = [
  { label: t('pcl.auto2.ka4421ee0b9'), value: 'pcl' },
  { label: 'MMC Classic', value: 'mmc-classic' },
];
const classicSideOptions = [
  { label: t('pcl.auto2.k440495fc2d'), value: 'left' },
  { label: t('pcl.auto2.k1e4e7a177d'), value: 'right' },
];
const classicIconOptions = [
  { label: '64 px', value: 64 },
  { label: '80 px', value: 80 },
  { label: '96 px', value: 96 },
  { label: '120 px', value: 120 },
  { label: '160 px', value: 160 },
];
const shellOptions = [
  { label: t('pcl.auto2.k67cf30199a'), value: 'pcl' },
  { label: t('pcl.auto2.k453b088186'), value: 'neo' },
];
const themeOptions = [
  { label: t('pcl.auto2.k30b2c979ac'), value: 'dark' },
  { label: t('pcl.auto2.k80ec9e2b1b'), value: 'light' },
  { label: t('pcl.auto2.kf4bbd91f79'), value: 'system' },
];
const densityOptions = [
  { label: t('pcl.auto2.kb97c3bc09e'), value: 'comfortable' },
  { label: t('pcl.auto2.kc837edc258'), value: 'compact' },
];
const backgroundOptions = [
  { label: t('pcl.auto2.k6fd96f07e8'), value: 'gradient' },
  { label: t('pcl.auto2.ke139ab0c18'), value: 'solid' },
];
const loadingAnimationOptions = [
  { label: t('pcl.auto2.kb71ec0b422'), value: 'luna-spinner' },
  { label: t('pcl.auto2.k490a4502f2'), value: 'pcl-pickaxe' },
  { label: t('pcl.auto2.k722c032691'), value: 'pcl-bar' },
];
const instanceWidthOptions = [
  { label: '60%', value: 60 },
  { label: '70%', value: 70 },
  { label: '80%', value: 80 },
  { label: '90%', value: 90 },
  { label: '100%', value: 100 },
];
const instanceLayoutOptions = [
  { label: t('pcl.auto2.kcd1bbb1239'), value: 'grid' },
  { label: t('pcl.auto2.ke94a88a32b'), value: 'list' },
];
const instanceGroupingOptions = [
  { label: t('pcl.auto2.kbf3327d8de'), value: 'flat' },
  { label: t('pcl.auto2.k44c1c6bab7'), value: 'grouped' },
];
const instanceSortOptions = [
  { label: t('pcl.auto2.kc8b7b63a05'), value: 'manual' },
  { label: t('pcl.auto2.k8065cff0a2'), value: 'name' },
  { label: t('pcl.auto2.k265250b276'), value: 'version' },
];
const instanceColumnOptions = [
  { label: t('pcl.auto2.k2907c13b04'), value: 'auto' },
  { label: t('pcl.auto2.k620e6129cf'), value: 1 },
  { label: t('pcl.auto2.k1e7e0fdbaa'), value: 2 },
  { label: t('pcl.auto2.kf32016d001'), value: 3 },
  { label: t('pcl.auto2.k2a5c49468f'), value: 4 },
  { label: t('pcl.auto2.k57d6fb6ef9'), value: 5 },
  { label: t('pcl.auto2.kf90d03d8d0'), value: 6 },
  { label: t('pcl.auto2.ka03a27317c'), value: 7 },
  { label: t('pcl.auto2.k5a2ec4199e'), value: 8 },
  { label: t('pcl.auto2.k68c3715fb2'), value: 10 },
];
const instanceTextPositionOptions = [
  { label: t('pcl.auto2.k381c24c2e7'), value: 'beside' },
  { label: t('pcl.auto2.kb8b901d2ee'), value: 'below' },
];
const instanceRowOptions = [
  { label: t('pcl.auto2.kbf3fa26ef0'), value: 2 },
  { label: t('pcl.auto2.kd60a2cddbc'), value: 3 },
  { label: t('pcl.auto2.kb9af82b78e'), value: 4 },
  { label: t('pcl.auto2.k5fc5b3f3a8'), value: 5 },
  { label: t('pcl.auto2.k12eee8ee0d'), value: 6 },
  { label: t('pcl.auto2.k0b6a6cd748'), value: 8 },
];
const instanceFontOptions = [
  { label: '10 px', value: 10 },
  { label: '11 px', value: 11 },
  { label: '12 px', value: 12 },
  { label: '13 px', value: 13 },
  { label: t('pcl.auto2.kb52c0b7086'), value: 14 },
  { label: '15 px', value: 15 },
  { label: '16 px', value: 16 },
  { label: '18 px', value: 18 },
];
const baseFontOptions = [
  { label: '11 px', value: 11 },
  { label: '12 px', value: 12 },
  { label: '13 px', value: 13 },
  { label: t('pcl.auto2.kb52c0b7086'), value: 14 },
  { label: '15 px', value: 15 },
  { label: '16 px', value: 16 },
  { label: '18 px', value: 18 },
];
const fontOptions = [
  { label: t('pcl.auto2.kaab0c72d2d'), value: 'system' },
  { label: t('pcl.auto2.k85275a6427'), value: 'yahei' },
  { label: t('pcl.auto2.k7791b0d498'), value: 'dengxian' },
  { label: t('pcl.auto2.k24e636c047'), value: 'simsun' },
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
    pclHomeCards: [{ id: 'welcome', title: t('pcl.auto2.ka5d9fdec5a'), content: t('pcl.auto2.k5d577eb245') }],
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
