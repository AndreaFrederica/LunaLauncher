<template>
  <v-card
    class="store-gallery grid h-[360px] max-h-[360px] select-none grid-cols-5 rounded-lg overflow-hidden"
    elevation="3"
    @click="$emit('enter')"
  >
    <!-- Main hero image (left) -->
    <div class="store-gallery__hero relative col-span-3 max-h-[360px] cursor-pointer overflow-hidden">
      <transition name="fade-transition" mode="out-in">
        <v-img
          v-if="gallery.images[currentImageIndex]"
          :key="currentImageIndex"
          cover
          class="store-gallery__hero-img h-full w-full"
          :src="gallery.images[currentImageIndex][1]"
          :lazy-src="gallery.images[currentImageIndex][0]"
        />
      </transition>

      <!-- Bottom gradient + title overlay -->
      <div class="store-gallery__hero-overlay pointer-events-none absolute inset-x-0 bottom-0 p-5">
        <div class="flex items-center gap-2 mb-2">
          <v-icon size="18" :color="brandColor">
            {{ gallery.type === 'modrinth' ? 'xmcl:modrinth' : 'xmcl:curseforge' }}
          </v-icon>
          <span class="text-[11px] font-bold uppercase tracking-wider text-white/90">
            {{ gallery.type === 'modrinth' ? 'Modrinth' : 'CurseForge' }}
          </span>
        </div>
        <h2 class="store-gallery__title text-2xl font-extrabold leading-tight text-white truncate">
          {{ gallery.localizedTitle || gallery.title }}
        </h2>
        <p
          v-if="gallery.developer"
          class="mt-1 text-xs text-white/75 truncate flex items-center gap-1"
        >
          <v-icon size="12" color="white" class="material-icons-outlined">person</v-icon>
          {{ gallery.developer }}
        </p>
      </div>

      <!-- Thumbnail counter (top-right) -->
      <div
        v-if="gallery.images.length > 1"
        class="absolute top-3 right-3 px-2 py-1 rounded-full bg-black/55 backdrop-blur-md text-[10px] font-semibold text-white flex items-center gap-1"
      >
        <v-icon size="12" color="white" class="material-icons-outlined">photo_library</v-icon>
        {{ currentImageIndex + 1 }} / {{ gallery.images.length }}
      </div>
    </div>

    <!-- Right column: thumbnails + meta -->
    <div class="store-gallery__side col-span-2 flex flex-col gap-3 p-4">
      <!-- Thumbnail grid -->
      <div class="grid grid-cols-1 gap-2 lg:grid-cols-2">
        <div
          v-for="i in 4"
          :key="i"
          v-show="gallery.images[i - 1]"
          class="store-gallery__thumb relative cursor-pointer overflow-hidden rounded-md"
          :class="{ 'store-gallery__thumb--active': currentImageIndex === i - 1 }"
          @mouseenter="currentImageIndex = i - 1"
          @click.stop="$emit('enter')"
        >
          <v-img
            v-if="gallery.images[i - 1]"
            class="h-full w-full"
            cover
            :src="gallery.images[i - 1][1]"
            :lazy-src="gallery.images[i - 1][0]"
          />
        </div>
      </div>

      <div class="flex-grow" />

      <!-- Category chips -->
      <div
        v-if="gallery.categories.length > 0"
        class="flex flex-wrap items-center gap-1.5"
      >
        <v-chip
          v-for="cat of gallery.categories.slice(0, 3)"
          :key="'store-gallery-' + cat"
          size="x-small"
          label
          variant="tonal"
          class="font-medium"
        >
          {{ cat }}
        </v-chip>
        <v-chip
          v-if="gallery.categories.length > 3"
          v-shared-tooltip="gallery.categories.slice(3).join(', ')"
          size="x-small"
          label
          variant="text"
          class="font-medium opacity-70"
        >
          +{{ gallery.categories.length - 3 }}
        </v-chip>
      </div>

      <!-- Footer: version + CTA -->
      <div class="flex items-center justify-between gap-2">
        <div class="flex items-center gap-1.5">
          <v-chip
            size="small"
            label
            color="primary"
            variant="flat"
            class="font-bold"
          >
            <v-icon size="13" start class="material-icons-outlined">sports_esports</v-icon>
            {{ gallery.minecraft[0] }}
          </v-chip>
          <span
            v-if="gallery.minecraft.length > 1"
            v-shared-tooltip="gallery.minecraft.slice(1).join(', ')"
            class="text-xs cursor-default opacity-70"
          >
            +{{ gallery.minecraft.length - 1 }}
          </span>
        </div>
        <v-btn
          variant="tonal"
          size="small"
          color="primary"
          append-icon="arrow_forward"
          @click.stop="$emit('enter')"
        >
          {{ t('shared.browse') }}
        </v-btn>
      </div>
    </div>
  </v-card>
</template>
<script setup lang="ts">
import { vSharedTooltip } from '@/directives/sharedTooltip'

const props = defineProps<{
  gallery: GameGallery
}>()

defineEmits(['enter'])

const { t } = useI18n()

export interface GameGallery {
  id: string
  title: string
  images: [string, string][]
  developer: string
  categories: string[]
  minecraft: string[]
  type: 'curseforge' | 'modrinth'
  localizedTitle?: string
}
const currentImageIndex = ref(0)

const brandColor = computed(() =>
  props.gallery.type === 'modrinth' ? '#00AF5C' : '#F16436',
)
</script>
<style scoped>
.store-gallery {
  transition:
    transform 0.3s cubic-bezier(0.4, 0, 0.2, 1),
    box-shadow 0.3s cubic-bezier(0.4, 0, 0.2, 1);
}

.store-gallery:hover {
  transform: translateY(-2px);
}

.store-gallery__hero {
  background-color: rgba(0, 0, 0, 0.1);
}

.store-gallery__hero-img {
  transition: transform 0.6s cubic-bezier(0.4, 0, 0.2, 1);
}

.store-gallery:hover .store-gallery__hero-img {
  transform: scale(1.04);
}

.store-gallery__hero-overlay {
  background: linear-gradient(
    to top,
    rgba(0, 0, 0, 0.85) 0%,
    rgba(0, 0, 0, 0.55) 40%,
    rgba(0, 0, 0, 0) 100%
  );
}

.store-gallery__title {
  text-shadow: 0 2px 12px rgba(0, 0, 0, 0.5);
}

.store-gallery__thumb {
  height: 100px;
  max-height: 100px;
  position: relative;
  border: 2px solid transparent;
  transition:
    transform 0.2s ease,
    border-color 0.2s ease,
    box-shadow 0.2s ease;
}

.store-gallery__thumb::after {
  content: '';
  position: absolute;
  inset: 0;
  background-color: rgba(0, 0, 0, 0.25);
  opacity: 1;
  transition: opacity 0.2s ease;
  pointer-events: none;
}

.store-gallery__thumb:hover {
  transform: scale(1.03);
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.2);
}

.store-gallery__thumb:hover::after {
  opacity: 0;
}

.store-gallery__thumb--active {
  border-color: rgb(var(--v-theme-primary));
  box-shadow: 0 0 0 2px rgba(var(--v-theme-primary), 0.25);
}

.store-gallery__thumb--active::after {
  opacity: 0;
}
</style>

