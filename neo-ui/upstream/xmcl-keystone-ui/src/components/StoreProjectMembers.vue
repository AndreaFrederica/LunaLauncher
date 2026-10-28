<template>
  <div class="store-section">
    <div class="store-section__header">
      <v-icon size="16" class="opacity-70">groups</v-icon>
      <span>{{ t('modrinth.projectMembers') }}</span>
    </div>
    <ErrorView :error="error" />
    <v-skeleton-loader
      v-if="loading"
      type="list-item-avatar-two-line, list-item-avatar-two-line"
    />
    <div
      v-else-if="members && members.length"
      class="grid grid-cols-2 lg:grid-cols-1 gap-1"
    >
      <div
        v-for="m of members"
        :key="m.id"
        class="member-item"
        :class="{ 'cursor-pointer': m.url }"
        @click="onClick(m)"
      >
        <v-avatar size="36">
          <v-img :src="m.avatar" />
        </v-avatar>
        <div class="flex flex-col min-w-0 flex-1">
          <span class="text-sm font-medium truncate">{{ m.name }}</span>
          <span v-if="m.role" class="text-xs opacity-60 truncate">{{ m.role }}</span>
        </div>
        <v-icon v-if="m.url" size="14" class="opacity-40">open_in_new</v-icon>
      </div>
    </div>
  </div>
</template>
<script lang="ts" setup>
import ErrorView from '@/components/ErrorView.vue'

export interface TeamMember {
  id: string
  avatar: string
  name: string
  role?: string
  url?: string
}

defineProps<{ members?: TeamMember[]; loading: boolean; error: any }>()

const { t } = useI18n()
const onClick = (u: TeamMember) => {
  if (u.url) {
    window.open(u.url, 'browser')
  }
}
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
.member-item {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 6px 8px;
  border-radius: 8px;
  transition: background-color 0.15s ease;
}
.member-item.cursor-pointer:hover {
  background: rgba(var(--v-theme-on-surface), 0.06);
}
</style>

