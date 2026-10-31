<template>
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
        <span class="pcl-form-row__label">{{ t('pcl.appearance.pageNavWidth') }}</span>
        <div class="pcl-form-row__control">
          <PclNumberInput v-model="preferences.pclPageNavWidth" :min="140" :max="360" :step="1" :unit="t('pcl.appearance.pixels')" :label="t('pcl.appearance.pageNavWidth')" />
        </div>
        <span class="pcl-form-row__hint">{{ t('pcl.appearance.pageNavWidthHint') }}</span>
      </div>
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
          <div class="pcl-appearance-preview__brand">
            <img v-if="preferences.pclTitleIcon" class="pcl-appearance-preview__logo" :src="lunaLogo" alt="Luna Launcher" />
            <strong v-if="preferences.pclTitleLogo">LunaLauncher</strong>
            <small v-if="preferences.pclTitleBadge">NEOUI</small>
          </div>
          <nav class="pcl-appearance-preview__navs">
            <span class="pcl-appearance-preview__nav"><PclIcon name="play" :size="13" />{{ t('pcl.auto2.kebd26da421') }}</span>
            <span class="pcl-appearance-preview__nav active"><PclIcon name="download" :size="13" />{{ t('pcl.auto2.k2b9d013177') }}</span>
            <span class="pcl-appearance-preview__nav"><PclIcon name="settings" :size="13" />{{ t('pcl.auto2.k7debf9cb03') }}</span>
            <span class="pcl-appearance-preview__nav"><PclIcon name="wrench" :size="13" />{{ t('xmcl.system.tools') }}</span>
          </nav>
          <div class="pcl-appearance-preview__window-actions" aria-hidden="true"><i>−</i><i>□</i><i class="close">×</i></div>
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
</template>
<script setup lang="ts">
import { inject } from 'vue'; import { PclAppearanceContextKey } from './PclAppearanceContext';
import PclButton from '../../components/PclButton.vue'; import PclCard from '../../components/PclCard.vue'; import PclHint from '../../components/PclHint.vue'; import PclSelect from '../../components/PclSelect.vue'; import PclCheckbox from '../../components/PclCheckbox.vue'; import PclColorPicker from '../../components/PclColorPicker.vue'; import PclInput from '../../components/PclInput.vue'; import PclNumberInput from '../../components/PclNumberInput.vue'; import PclIcon from '../../components/PclIcon.vue';
const context = inject(PclAppearanceContextKey); if (!context) throw new Error('PclAppearanceContext is missing');
const { t, lunaLogo, preferences, error, backgroundFile, musicFile, pclMusic, homeOrderItems, pclStylePresetOptions, navLayoutOptions, classicSideOptions, classicIconOptions, shellOptions, themeOptions, densityOptions, backgroundOptions, loadingAnimationOptions, instanceWidthOptions, instanceLayoutOptions, instanceGroupingOptions, instanceSortOptions, instanceColumnOptions, instanceTextPositionOptions, instanceRowOptions, instanceFontOptions, baseFontOptions, fontOptions, chooseMusic, onMusicFile, onMusicVolume, chooseBackground, onBackgroundFile, moveHomeOrder, selectPclPreset, addHomeCard, removeHomeCard, clearBackground, openWebUiManager, reset, togglePclMusic, removePclMusic, choosePclMusic, setPclMusicVolume, resetPclFeatures, downloadFeatures, setupFeatures, hiddenFeatures, setPclFeatureHidden } = context;
</script>
