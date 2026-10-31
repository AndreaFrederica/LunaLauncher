<template>
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
</template>
<script setup lang="ts">
import { inject } from 'vue'; import { PclAppearanceContextKey } from './PclAppearanceContext';
import PclButton from '../../components/PclButton.vue'; import PclCard from '../../components/PclCard.vue'; import PclHint from '../../components/PclHint.vue'; import PclSelect from '../../components/PclSelect.vue'; import PclCheckbox from '../../components/PclCheckbox.vue'; import PclColorPicker from '../../components/PclColorPicker.vue'; import PclInput from '../../components/PclInput.vue'; import PclNumberInput from '../../components/PclNumberInput.vue';
const context = inject(PclAppearanceContextKey); if (!context) throw new Error('PclAppearanceContext is missing');
const { t, lunaLogo, preferences, error, backgroundFile, musicFile, pclMusic, homeOrderItems, pclStylePresetOptions, navLayoutOptions, classicSideOptions, classicIconOptions, shellOptions, themeOptions, densityOptions, backgroundOptions, loadingAnimationOptions, instanceWidthOptions, instanceLayoutOptions, instanceGroupingOptions, instanceSortOptions, instanceColumnOptions, instanceTextPositionOptions, instanceRowOptions, instanceFontOptions, baseFontOptions, fontOptions, chooseMusic, onMusicFile, onMusicVolume, chooseBackground, onBackgroundFile, moveHomeOrder, selectPclPreset, addHomeCard, removeHomeCard, clearBackground, openWebUiManager, reset, togglePclMusic, removePclMusic, choosePclMusic, setPclMusicVolume, resetPclFeatures, downloadFeatures, setupFeatures, hiddenFeatures, setPclFeatureHidden } = context;
</script>
