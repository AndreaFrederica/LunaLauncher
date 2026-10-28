<template>
  <div class="store-section">
    <div class="store-section__header">
      <v-icon size="16" class="opacity-70">link</v-icon>
      <span>{{ t('modrinth.externalResources') }}</span>
    </div>
    <div class="flex flex-wrap gap-2">
      <a
        v-for="link of project.links"
        :key="link.url"
        :href="link.url"
        target="browser"
        class="external-link"
      >
        <v-icon size="14">open_in_new</v-icon>
        <span class="truncate">{{ link.name }}</span>
      </a>
    </div>
  </div>
</template>
<script lang="ts" setup>
import { StoreProject } from './StoreProject.vue'

defineProps<{
  project: StoreProject
}>()
const { t } = useI18n()
</script>

<style scoped>
.store-section__header {
  display: flex;
  align-items: center;
  gap: 6px;
  font-size: 0.7rem;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: 0.06em;
  opacity: 0.7;
  margin-bottom: 8px;
}
.external-link {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  padding: 4px 10px;
  border-radius: 999px;
  background: rgba(var(--v-theme-on-surface), 0.06);
  color: inherit;
  text-decoration: none;
  font-size: 0.8rem;
  max-width: 100%;
  transition: background 0.15s ease, color 0.15s ease;
}
.external-link:hover {
  background: rgba(var(--v-theme-primary), 0.14);
  color: rgb(var(--v-theme-primary));
}
</style>

