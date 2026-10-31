<template>
<PclCard :title="t('pcl.auto2.kbc4abdc526')">
      <div class="pcl-check-grid">
        <PclCheckbox v-model="preferences.pclFloatTasks" :label="t('pcl.auto2.kf692ff9ec5')" />
        <PclCheckbox v-model="preferences.pclFloatLog" :label="t('pcl.auto2.k8ad868abbc')" />
        <PclCheckbox v-model="preferences.pclFloatRefresh" :label="t('pcl.auto2.kd3978de624')" />
        <PclCheckbox v-model="preferences.pclFloatMusic" :label="t('pcl.auto2.kd77bcb52c1')" />
      </div>
      <PclHint theme="blue">{{ t('pcl.auto2.ke7d1f96aed') }}</PclHint>
    </PclCard>
</template>
<script setup lang="ts">
import { inject } from 'vue'; import { PclAppearanceContextKey } from './PclAppearanceContext';
import PclButton from '../../components/PclButton.vue'; import PclCard from '../../components/PclCard.vue'; import PclHint from '../../components/PclHint.vue'; import PclSelect from '../../components/PclSelect.vue'; import PclCheckbox from '../../components/PclCheckbox.vue'; import PclColorPicker from '../../components/PclColorPicker.vue'; import PclInput from '../../components/PclInput.vue'; import PclNumberInput from '../../components/PclNumberInput.vue';
const context = inject(PclAppearanceContextKey); if (!context) throw new Error('PclAppearanceContext is missing');
const { t, lunaLogo, preferences, error, backgroundFile, musicFile, pclMusic, homeOrderItems, pclStylePresetOptions, navLayoutOptions, classicSideOptions, classicIconOptions, shellOptions, themeOptions, densityOptions, backgroundOptions, loadingAnimationOptions, instanceWidthOptions, instanceLayoutOptions, instanceGroupingOptions, instanceSortOptions, instanceColumnOptions, instanceTextPositionOptions, instanceRowOptions, instanceFontOptions, baseFontOptions, fontOptions, chooseMusic, onMusicFile, onMusicVolume, chooseBackground, onBackgroundFile, moveHomeOrder, selectPclPreset, addHomeCard, removeHomeCard, clearBackground, openWebUiManager, reset, togglePclMusic, removePclMusic, choosePclMusic, setPclMusicVolume, resetPclFeatures, downloadFeatures, setupFeatures, hiddenFeatures, setPclFeatureHidden } = context;
</script>
