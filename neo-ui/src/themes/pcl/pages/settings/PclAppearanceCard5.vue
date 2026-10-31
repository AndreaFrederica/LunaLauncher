<template>
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
</template>
<script setup lang="ts">
import { inject } from 'vue'; import { PclAppearanceContextKey } from './PclAppearanceContext';
import PclButton from '../../components/PclButton.vue'; import PclCard from '../../components/PclCard.vue'; import PclHint from '../../components/PclHint.vue'; import PclSelect from '../../components/PclSelect.vue'; import PclCheckbox from '../../components/PclCheckbox.vue'; import PclColorPicker from '../../components/PclColorPicker.vue'; import PclInput from '../../components/PclInput.vue'; import PclNumberInput from '../../components/PclNumberInput.vue';
const context = inject(PclAppearanceContextKey); if (!context) throw new Error('PclAppearanceContext is missing');
const { t, lunaLogo, preferences, error, backgroundFile, musicFile, pclMusic, homeOrderItems, pclStylePresetOptions, navLayoutOptions, classicSideOptions, classicIconOptions, shellOptions, themeOptions, densityOptions, backgroundOptions, loadingAnimationOptions, instanceWidthOptions, instanceLayoutOptions, instanceGroupingOptions, instanceSortOptions, instanceColumnOptions, instanceTextPositionOptions, instanceRowOptions, instanceFontOptions, baseFontOptions, fontOptions, chooseMusic, onMusicFile, onMusicVolume, chooseBackground, onBackgroundFile, moveHomeOrder, selectPclPreset, addHomeCard, removeHomeCard, clearBackground, openWebUiManager, reset, togglePclMusic, removePclMusic, choosePclMusic, setPclMusicVolume, resetPclFeatures, downloadFeatures, setupFeatures, hiddenFeatures, setPclFeatureHidden } = context;
</script>
