<template>
  <div class="store-section">
    <div class="store-section__header">
      <v-icon size="16" class="opacity-70">info</v-icon>
      <span>{{ t('modrinth.technicalInformation') }}</span>
    </div>
    <div class="grid grid-cols-1 gap-1">
      <div
        v-for="item of project.info"
        :key="item.name"
        class="info-row"
      >
        <v-icon size="16" class="material-icons-outlined opacity-70">
          {{ item.icon }}
        </v-icon>
        <span class="info-row__name">{{ item.name }}</span>
        <span class="flex-grow" />
        <AppCopyChip
          outlined
          :value="item.value"
        />
      </div>
    </div>
  </div>
</template>
<script lang="ts" setup>
import AppCopyChip from './AppCopyChip.vue'
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
.info-row {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 4px 6px;
  border-radius: 6px;
  min-width: 0;
}
.info-row:hover {
  background: rgba(var(--v-theme-on-surface), 0.04);
}
.info-row__name {
  font-size: 0.8rem;
  white-space: nowrap;
  user-select: none;
  opacity: 0.8;
}
</style>

