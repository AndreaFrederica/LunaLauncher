<template>
  <article class="surface-card home-card home-card--worlds" @click="emit('navigate', 'save')">
    <header><span class="home-card-icon material-icons">public</span><strong>{{ t('xmcl.home.worlds') }}</strong><button :aria-label="t('xmcl.home.openWorlds')" @click.stop="emit('navigate', 'save')"><span class="material-icons">open_in_new</span></button></header>
    <div class="home-card-number">{{ worlds.length }}</div>
    <small>{{ t('xmcl.home.localSaves') }}</small>
    <div class="home-card-list"><div v-for="world in worlds.slice(0, 5)" :key="world.folder"><span>{{ world.name || world.folder }}</span><span class="material-icons">chevron_right</span></div><div v-if="!worlds.length" class="home-empty">{{ t('xmcl.home.noWorlds') }}</div></div>
  </article>
</template>

<script setup lang="ts">
import { useI18n } from 'vue-i18n';
defineProps<{ worlds: any[] }>();
const emit = defineEmits<{ navigate: [page: string] }>();
const { t } = useI18n();
</script>

