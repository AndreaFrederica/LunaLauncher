<template>
  <div class="pcl-loading" :class="{ inline }" role="status" aria-live="polite">
    <svg class="pcl-loading__pickaxe" viewBox="0 0 1000 1000" fill="none" aria-hidden="true">
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
withDefaults(defineProps<{
  text?: string;
  inline?: boolean;
  dots?: boolean;
}>(), {
  text: '',
  inline: false,
  dots: false,
});
</script>

<style scoped>
.pcl-loading__pickaxe {
  width: 42px;
  height: 34px;
  color: var(--pcl-c3);
  overflow: visible;
}
.pcl-loading.inline {
  flex-direction: row;
  gap: 8px;
}
.pcl-loading.inline .pcl-loading__pickaxe {
  width: 24px;
  height: 20px;
}
</style>
