<template>
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
          <strong>{{ t('pcl.appearance.card', { index: Number(index) + 1 }) }}</strong>
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
</template>
<script setup lang="ts">
import { inject } from 'vue'; import { PclAppearanceContextKey } from './PclAppearanceContext';
import PclButton from '../../components/PclButton.vue'; import PclCard from '../../components/PclCard.vue'; import PclHint from '../../components/PclHint.vue'; import PclSelect from '../../components/PclSelect.vue'; import PclCheckbox from '../../components/PclCheckbox.vue'; import PclColorPicker from '../../components/PclColorPicker.vue'; import PclInput from '../../components/PclInput.vue'; import PclNumberInput from '../../components/PclNumberInput.vue';
const context = inject(PclAppearanceContextKey); if (!context) throw new Error('PclAppearanceContext is missing');
const { t, lunaLogo, preferences, error, backgroundFile, musicFile, pclMusic, homeOrderItems, pclStylePresetOptions, navLayoutOptions, classicSideOptions, classicIconOptions, shellOptions, themeOptions, densityOptions, backgroundOptions, loadingAnimationOptions, instanceWidthOptions, instanceLayoutOptions, instanceGroupingOptions, instanceSortOptions, instanceColumnOptions, instanceTextPositionOptions, instanceRowOptions, instanceFontOptions, baseFontOptions, fontOptions, chooseMusic, onMusicFile, onMusicVolume, chooseBackground, onBackgroundFile, moveHomeOrder, selectPclPreset, addHomeCard, removeHomeCard, clearBackground, openWebUiManager, reset, togglePclMusic, removePclMusic, choosePclMusic, setPclMusicVolume, resetPclFeatures, downloadFeatures, setupFeatures, hiddenFeatures, setPclFeatureHidden } = context;
</script>

