<template>
  <v-chip
    v-shared-tooltip="() => clicked ? t('copyClipboard.success') : value"
    v-ripple
    class="app-copy-chip"
    :class="{ 'app-copy-chip--clicked': clicked }"
    :label="label"
    :size="large ? 'default' : 'small'"
    :variant="outlined ? 'outlined' : 'tonal'"
    :color="clicked ? 'success' : undefined"
    @click="onInfoClicked(value)"
  >
    <span class="app-copy-chip__value select-text">
      {{ value }}
    </span>
    <template #append>
      <Transition name="fade-transition" mode="out-in">
        <v-icon
          :key="clicked ? 'check' : 'copy'"
          size="14"
          class="app-copy-chip__icon"
        >
          {{ clicked ? 'check' : 'content_copy' }}
        </v-icon>
      </Transition>
    </template>
  </v-chip>
</template>
<script lang="ts" setup>
import { vSharedTooltip } from '@/directives/sharedTooltip'

defineProps<{
  value: string
  large?: boolean
  label?: boolean
  outlined?: boolean
}>()

const { t } = useI18n()
const clicked = ref(false)
let timer: ReturnType<typeof setTimeout> | undefined

const onInfoClicked = (value: string) => {
  windowController.writeClipboard(value)
  clicked.value = true
  if (timer) clearTimeout(timer)
  timer = setTimeout(() => {
    clicked.value = false
  }, 1500)
}
</script>

<style scoped>
.app-copy-chip {
  cursor: pointer;
  max-width: 100%;
  transition: color 0.15s ease, background-color 0.15s ease, border-color 0.15s ease;
}
.app-copy-chip:hover:not(.app-copy-chip--clicked) {
  color: rgb(var(--v-theme-primary));
}
.app-copy-chip__value {
  display: inline-block;
  max-width: 16rem;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  font-variant-numeric: tabular-nums;
}
.app-copy-chip__icon {
  margin-left: 6px;
  opacity: 0.65;
  transition: opacity 0.15s ease;
}
.app-copy-chip:hover .app-copy-chip__icon {
  opacity: 1;
}
</style>

