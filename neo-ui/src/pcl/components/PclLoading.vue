<template>
  <div class="pcl-loading" :class="{ inline, 'pcl-loading--spinner': isSpinner }" role="status" aria-live="polite">
    <span v-if="isSpinner" class="pcl-loading__spinner" aria-hidden="true" />
    <span v-else-if="isBar" class="pcl-loading__bar" aria-hidden="true" />
    <svg v-else class="pcl-loading__pickaxe" viewBox="0 0 1000 1000" fill="none" aria-hidden="true">
      <path
        d="M963.6 858.2 410.8 305.5C508.1 213.3 609.2 196.8 711.1 128.6 837.1 49.6 879.3 50.3 751.5 49.5 611.3 52 471.8 96.2 353.3 182.4 309.8 155.7 252.1 161.2 214.5 198.9 176.9 236.6 171.3 294.2 198 337.7 111.8 456.3 67.6 595.8 65.1 735.9 63.3 883.8 65.1 837.3 144.2 695.5 212.4 593.6 228.9 492.4 321.1 395.2L873.9 948c.6.6 1.6.6 2.2 0l87.5-87.5c.6-.7.6-1.6 0-2.3z"
        stroke="currentColor"
        stroke-width="42"
        stroke-linecap="round"
        stroke-linejoin="round"
      />
    </svg>
    <span v-if="text" class="pcl-loading__text">{{ text }}</span>
    <span v-if="dots" class="pcl-loading__dots" aria-hidden="true"><i /><i /><i /></span>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue';
import { neoUiPreferences } from '../../neo-ui-preferences';

withDefaults(defineProps<{
  text?: string;
  inline?: boolean;
  dots?: boolean;
}>(), {
  text: '',
  inline: false,
  dots: false,
});

const isSpinner = computed(() => neoUiPreferences.pclLoadingAnimation === 'luna-spinner');
const isBar = computed(() => neoUiPreferences.pclLoadingAnimation === 'pcl-bar');
</script>

<style scoped>
.pcl-loading__pickaxe {
  width: 42px;
  height: 34px;
  color: var(--pcl-c3);
  overflow: visible;
}
.pcl-loading__spinner {
  display: block;
  width: 30px;
  height: 30px;
  box-sizing: border-box;
  border: 3px solid color-mix(in srgb, var(--pcl-c3) 22%, transparent);
  border-top-color: var(--pcl-c3);
  border-right-color: var(--pcl-c3);
  border-radius: 50%;
  animation: pcl-loading-spinner .8s linear infinite;
}
.pcl-loading__bar {
  display: block;
  position: relative;
  width: 150px;
  height: 4px;
  overflow: hidden;
  border-radius: 999px;
  background: color-mix(in srgb, var(--pcl-c3) 20%, transparent);
}
.pcl-loading__bar::after {
  content: '';
  display: block;
  width: 42%;
  height: 100%;
  border-radius: inherit;
  background: var(--pcl-c3);
  animation: pcl-loading-bar 1.15s ease-in-out infinite;
}
.pcl-loading.inline .pcl-loading__spinner {
  width: 18px;
  height: 18px;
  border-width: 2px;
}
.pcl-loading.inline {
  flex-direction: row;
  gap: 8px;
}
.pcl-loading.inline .pcl-loading__pickaxe {
  width: 24px;
  height: 20px;
}
.pcl-loading.inline .pcl-loading__bar {
  width: 28px;
  height: 3px;
}
@keyframes pcl-loading-spinner {
  to { transform: rotate(360deg); }
}
@keyframes pcl-loading-bar {
  0% { transform: translateX(-120%); }
  100% { transform: translateX(260%); }
}
</style>
