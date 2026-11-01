<template>
<PclCard :title="t('pcl.auto2.ke8bef90efb')">
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">{{ t('pcl.appearance.launcherName') }}</span>
        <div class="pcl-form-row__control">
          <PclInput v-model="preferences.pclTitleText" placeholder="LunaLauncher" />
        </div>
        <span class="pcl-form-row__hint">{{ t('pcl.appearance.launcherNameHint') }}</span>
      </div>
      <div class="pcl-check-grid">
        <PclCheckbox v-model="preferences.pclTitleIcon" :label="t('pcl.appearance.titleIcon')" />
        <PclCheckbox v-model="preferences.pclTitleLogo" :label="t('pcl.appearance.titleText')" />
        <PclCheckbox v-model="preferences.pclTitleBadge" :label="t('pcl.appearance.titleBadge')" />
        <PclCheckbox v-model="preferences.pclLaunchAnimation" :label="t('pcl.auto2.k8eb55365e3')" />
        <PclCheckbox v-model="preferences.pclClearLaunchLogOnStart" :label="t('pcl.auto2.ke51a90b26f')" />
        <PclCheckbox v-model="preferences.pclLaunchLogAutoScroll" :label="t('pcl.auto2.k731cff1a1b')" />
        <PclCheckbox v-model="preferences.pclEditCurrentInWindow" :label="t('pcl.auto2.k61a28ec924')" />
      </div>
      <PclHint theme="blue">{{ t('pcl.appearance.titleTextWhiteHint') }}</PclHint>
    </PclCard>
</template>
<script setup lang="ts">
import { inject } from 'vue'; import { PclAppearanceContextKey } from './PclAppearanceContext';
import PclButton from '../../components/PclButton.vue'; import PclCard from '../../components/PclCard.vue'; import PclHint from '../../components/PclHint.vue'; import PclSelect from '../../components/PclSelect.vue'; import PclCheckbox from '../../components/PclCheckbox.vue'; import PclColorPicker from '../../components/PclColorPicker.vue'; import PclInput from '../../components/PclInput.vue'; import PclNumberInput from '../../components/PclNumberInput.vue';
const context = inject(PclAppearanceContextKey); if (!context) throw new Error('PclAppearanceContext is missing');
const { t, lunaLogo, preferences, error, backgroundFile, musicFile, pclMusic, homeOrderItems, pclStylePresetOptions, navLayoutOptions, classicSideOptions, classicIconOptions, shellOptions, themeOptions, densityOptions, backgroundOptions, loadingAnimationOptions, instanceWidthOptions, instanceLayoutOptions, instanceGroupingOptions, instanceSortOptions, instanceColumnOptions, instanceTextPositionOptions, instanceRowOptions, instanceFontOptions, baseFontOptions, fontOptions, chooseMusic, onMusicFile, onMusicVolume, chooseBackground, onBackgroundFile, moveHomeOrder, selectPclPreset, addHomeCard, removeHomeCard, clearBackground, openWebUiManager, reset, togglePclMusic, removePclMusic, choosePclMusic, setPclMusicVolume, resetPclFeatures, downloadFeatures, setupFeatures, hiddenFeatures, setPclFeatureHidden } = context;
</script>
