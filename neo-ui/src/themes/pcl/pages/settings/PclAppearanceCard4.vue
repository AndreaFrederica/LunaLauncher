<template>
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
</template>
<script setup lang="ts">
import { inject } from 'vue'; import { PclAppearanceContextKey } from './PclAppearanceContext';
import PclButton from '../../components/PclButton.vue'; import PclCard from '../../components/PclCard.vue'; import PclHint from '../../components/PclHint.vue'; import PclSelect from '../../components/PclSelect.vue'; import PclCheckbox from '../../components/PclCheckbox.vue'; import PclColorPicker from '../../components/PclColorPicker.vue'; import PclInput from '../../components/PclInput.vue'; import PclNumberInput from '../../components/PclNumberInput.vue';
const context = inject(PclAppearanceContextKey); if (!context) throw new Error('PclAppearanceContext is missing');
const { t, lunaLogo, preferences, error, backgroundFile, musicFile, pclMusic, homeOrderItems, pclStylePresetOptions, navLayoutOptions, classicSideOptions, classicIconOptions, shellOptions, themeOptions, densityOptions, backgroundOptions, loadingAnimationOptions, instanceWidthOptions, instanceLayoutOptions, instanceGroupingOptions, instanceSortOptions, instanceColumnOptions, instanceTextPositionOptions, instanceRowOptions, instanceFontOptions, baseFontOptions, fontOptions, chooseMusic, onMusicFile, onMusicVolume, chooseBackground, onBackgroundFile, moveHomeOrder, selectPclPreset, addHomeCard, removeHomeCard, clearBackground, openWebUiManager, reset, togglePclMusic, removePclMusic, choosePclMusic, setPclMusicVolume, resetPclFeatures, downloadFeatures, setupFeatures, hiddenFeatures, setPclFeatureHidden } = context;
</script>
