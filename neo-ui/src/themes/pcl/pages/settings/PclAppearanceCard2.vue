<template>
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
</template>
<script setup lang="ts">
import { inject } from 'vue'; import { PclAppearanceContextKey } from './PclAppearanceContext';
import PclButton from '../../components/PclButton.vue'; import PclCard from '../../components/PclCard.vue'; import PclHint from '../../components/PclHint.vue'; import PclSelect from '../../components/PclSelect.vue'; import PclCheckbox from '../../components/PclCheckbox.vue'; import PclColorPicker from '../../components/PclColorPicker.vue'; import PclInput from '../../components/PclInput.vue'; import PclNumberInput from '../../components/PclNumberInput.vue';
const context = inject(PclAppearanceContextKey); if (!context) throw new Error('PclAppearanceContext is missing');
const { t, lunaLogo, preferences, error, backgroundFile, musicFile, pclMusic, homeOrderItems, pclStylePresetOptions, navLayoutOptions, classicSideOptions, classicIconOptions, shellOptions, themeOptions, densityOptions, backgroundOptions, loadingAnimationOptions, instanceWidthOptions, instanceLayoutOptions, instanceGroupingOptions, instanceSortOptions, instanceColumnOptions, instanceTextPositionOptions, instanceRowOptions, instanceFontOptions, baseFontOptions, fontOptions, chooseMusic, onMusicFile, onMusicVolume, chooseBackground, onBackgroundFile, moveHomeOrder, selectPclPreset, addHomeCard, removeHomeCard, clearBackground, openWebUiManager, reset, togglePclMusic, removePclMusic, choosePclMusic, setPclMusicVolume, resetPclFeatures, downloadFeatures, setupFeatures, hiddenFeatures, setPclFeatureHidden } = context;
</script>
