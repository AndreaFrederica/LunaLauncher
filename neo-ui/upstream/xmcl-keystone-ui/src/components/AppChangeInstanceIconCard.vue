<script setup lang="ts">
import { vSharedTooltip } from '@/directives/sharedTooltip'
import { useVModel } from '@vueuse/core'

const { t } = useI18n()
const props = defineProps<{
  icon: string
  color?: string
}>()

const emit = defineEmits<{
  (event: 'update:icon', value: string): void
}>()

const iconModel = useVModel(props, 'icon', emit)

const dragOver = ref(false)

function pickIconFile() {
  windowController
    .showOpenDialog({
      title: t('instanceSetting.icon'),
      filters: [
        {
          name: t('instanceSetting.icon'),
          extensions: ['png', 'jpg', 'jpeg', 'bmp', 'gif', 'svg'],
        },
      ],
      properties: ['openFile'],
    })
    .then((result) => {
      if (result.canceled) return
      const filePath = result.filePaths[0]
      if (filePath) {
        iconModel.value = `http://launcher/media?path=${filePath}`
      }
    })
}

function onDrop(event: DragEvent) {
  dragOver.value = false
  const file = event.dataTransfer?.files?.[0]
  if (!file) return
  const path = (file as any).path as string | undefined
  if (path) {
    iconModel.value = `http://launcher/media?path=${path}`
  }
}
</script>

<template>
  <v-card :color="color" class="change-icon-card transition-colors" width="400">
    <v-card-item>
      <template #prepend>
        <v-avatar
          variant="text"
          size="48"
          rounded="lg"
          class="change-icon-card__preview"
          :class="{ 'change-icon-card__preview--empty': !iconModel }"
        >
          <v-img v-if="iconModel" :src="iconModel" :alt="t('instance.icon')" />
          <v-icon v-else size="24">image</v-icon>
        </v-avatar>
      </template>
      <v-card-title class="text-subtitle-1 pa-0">
        {{ t('instance.icon') }}
      </v-card-title>
      <v-card-subtitle class="pa-0 text-caption">
        {{ t('instance.iconHint') }}
      </v-card-subtitle>
      <template #append>
        <v-btn
          v-shared-tooltip="() => t('modified.reset')"
          :disabled="!iconModel"
          icon
          variant="text"
          density="comfortable"
          @click="iconModel = ''"
        >
          <v-icon>restart_alt</v-icon>
        </v-btn>
      </template>
    </v-card-item>

    <v-card-text class="pt-0">
      <div
        class="change-icon-card__drop mb-3"
        :class="{ 'change-icon-card__drop--active': dragOver }"
        @dragover.prevent="dragOver = true"
        @dragleave.prevent="dragOver = false"
        @drop.prevent="onDrop"
        @click="pickIconFile"
      >
        <v-icon class="mb-1">upload_file</v-icon>
        <span class="text-caption">
          {{ t('instanceSetting.icon') }}
        </span>
      </div>

      <v-text-field
        v-model="iconModel"
        :label="t('instance.iconUrl')"
        hide-details
        variant="outlined"
        density="compact"
        clearable
        prepend-inner-icon="link"
      />
    </v-card-text>
  </v-card>
</template>

<style scoped>
.change-icon-card__preview {
  background-color: rgba(var(--v-theme-on-surface), 0.06);
}
.change-icon-card__preview--empty {
  border: 1px dashed rgba(var(--v-theme-on-surface), 0.24);
}
.change-icon-card__drop {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 4px;
  padding: 16px;
  border: 1px dashed rgba(var(--v-theme-on-surface), 0.24);
  border-radius: 8px;
  cursor: pointer;
  transition:
    background-color 0.15s ease,
    border-color 0.15s ease;
  user-select: none;
}
.change-icon-card__drop:hover {
  background-color: rgba(var(--v-theme-on-surface), 0.04);
}
.change-icon-card__drop--active {
  border-color: rgb(var(--v-theme-primary));
  background-color: rgba(var(--v-theme-primary), 0.08);
}
</style>
