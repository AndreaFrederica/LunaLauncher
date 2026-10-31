<template>
  <article class="surface-card home-card home-card--screenshots" @click="emit('extras')">
    <header><span class="home-card-icon material-icons">photo_library</span><strong>{{ t('xmcl.home.screenshots') }}</strong><button :aria-label="t('xmcl.home.openScreenshots')" @click.stop="emit('extras')"><span class="material-icons">open_in_new</span></button></header>
    <div class="screenshot-grid"><img v-for="shot in screenshots.slice(0, 6)" :key="shot.name" :src="shot.dataUrl || shot.url" :alt="shot.name" /><div v-if="loading && !screenshots.length" class="home-empty">{{ t('xmcl.home.loadingScreenshots') }}</div><div v-else-if="!screenshots.length" class="home-empty">{{ t('xmcl.home.noScreenshots') }}</div></div>
  </article>
</template>

<script setup lang="ts">
import { useI18n } from 'vue-i18n';
defineProps<{ screenshots: any[]; loading: boolean }>();
const emit = defineEmits<{ extras: [] }>();
const { t } = useI18n();
</script>

