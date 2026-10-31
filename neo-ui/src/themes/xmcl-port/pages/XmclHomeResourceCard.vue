<template>
  <article class="surface-card home-card" :class="cardClass" @click="emit('navigate', kind)">
    <header>
      <span class="home-card-icon material-icons">{{ icon }}</span>
      <strong>{{ title }}</strong>
      <button :aria-label="openLabel" @click.stop="emit('navigate', kind)"><span class="material-icons">open_in_new</span></button>
    </header>
    <div class="home-card-number">{{ count }}</div>
    <small>{{ installedLabel }}</small>
    <div class="home-card-list">
      <div v-for="item in items.slice(0, 5)" :key="item.fileName"><span>{{ item.name || item.fileName }}</span><span class="material-icons">{{ item.enabled === false ? 'visibility_off' : 'check' }}</span></div>
      <div v-if="loading && !items.length" class="home-empty">{{ loadingLabel }}</div>
      <div v-else-if="!items.length" class="home-empty">{{ emptyLabel }}</div>
    </div>
    <small v-if="path" class="home-path" :title="path">{{ pathLabel }}：{{ path }}</small>
  </article>
</template>

<script setup lang="ts">
defineProps<{ kind: string; cardClass: string; icon: string; title: string; openLabel: string; count: number; installedLabel: string; items: any[]; loading: boolean; loadingLabel: string; emptyLabel: string; path?: string; pathLabel: string }>();
const emit = defineEmits<{ navigate: [page: string] }>();
</script>

