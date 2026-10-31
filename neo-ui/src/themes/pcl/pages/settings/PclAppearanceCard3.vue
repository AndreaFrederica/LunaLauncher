<template>
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
</template>
<script setup lang="ts">
import { inject } from 'vue'; import { PclAppearanceContextKey } from './PclAppearanceContext';
import PclButton from '../../components/PclButton.vue'; import PclCard from '../../components/PclCard.vue'; import PclHint from '../../components/PclHint.vue'; import PclSelect from '../../components/PclSelect.vue'; import PclCheckbox from '../../components/PclCheckbox.vue'; import PclColorPicker from '../../components/PclColorPicker.vue'; import PclInput from '../../components/PclInput.vue'; import PclNumberInput from '../../components/PclNumberInput.vue';
const context = inject(PclAppearanceContextKey); if (!context) throw new Error('PclAppearanceContext is missing');
const { t, lunaLogo, preferences, error, backgroundFile, musicFile, pclMusic, homeOrderItems, pclStylePresetOptions, navLayoutOptions, classicSideOptions, classicIconOptions, shellOptions, themeOptions, densityOptions, backgroundOptions, loadingAnimationOptions, instanceWidthOptions, instanceLayoutOptions, instanceGroupingOptions, instanceSortOptions, instanceColumnOptions, instanceTextPositionOptions, instanceRowOptions, instanceFontOptions, baseFontOptions, fontOptions, chooseMusic, onMusicFile, onMusicVolume, chooseBackground, onBackgroundFile, moveHomeOrder, selectPclPreset, addHomeCard, removeHomeCard, clearBackground, openWebUiManager, reset, togglePclMusic, removePclMusic, choosePclMusic, setPclMusicVolume, resetPclFeatures, downloadFeatures, setupFeatures, hiddenFeatures, setPclFeatureHidden } = context;
</script>
