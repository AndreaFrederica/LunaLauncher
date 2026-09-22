<template>
  <v-carousel
    :interval="7000"
    :cycle="true"
    :show-arrows="galleries.length > 1"
    hide-delimiter-background
    :hide-delimiters="galleries.length > 9"
  >
    <template #prev="{ props: btnProps }">
      <v-btn variant="plain" icon="chevron_left" :color="arrowColor" @click="btnProps.onClick" />
    </template>
    <template #next="{ props: btnProps }">
      <v-btn variant="plain" icon="chevron_right" :color="arrowColor" @click="btnProps.onClick" />
    </template>
    <v-carousel-item
      v-for="(g, i) in galleries"
      :key="i"
      class="cursor-pointer"
      :src="g.rawUrl || g.url"
      @click="
        imageDialog.showAll(
          galleries.map((g) => ({ src: g.rawUrl || g.url, description: g.description })),
          i,
        )
      "
    />
  </v-carousel>
</template>
<script setup lang="ts">
import { injection } from '@/util/inject'
import { StoreProject } from './StoreProject.vue'
import { kImageDialog } from '@/composables/imageDialog'
import { kTheme } from '@/composables/theme'

const props = defineProps<{
  project: StoreProject
}>()

const galleries = computed(() => [
  ...props.project.gallery,
  { url: props.project.iconUrl ?? '', description: '' },
])
const imageDialog = injection(kImageDialog)
const { isDark } = injection(kTheme)
const arrowColor = computed(() => isDark.value ? 'white' : 'black')
</script>
