<template>
  <div class="pcl-skin-preview">
    <div class="pcl-skin-preview__stage">
      <img v-if="mode === '2d'" :src="src" class="pcl-skin-2d" :alt="t('pcl.skinPreview.alt')" />
      <div v-else class="pcl-skin-3d">
        <div class="pcl-skin-model">
          <div
            v-for="part in parts"
            :key="part.name"
            class="pcl-skin-part"
            :class="`part-${part.name}`"
            :style="partStyle(part)"
          >
            <div
              v-for="face in faces"
              :key="face"
              class="pcl-skin-face"
              :class="`face-${face}`"
              :style="faceStyle(part, face)"
            />
          </div>
        </div>
      </div>
    </div>
    <div class="pcl-skin-preview__modes">
      <button type="button" :class="{ active: mode === '2d' }" @click="mode = '2d'">{{ t('ui.view2d') }}</button>
      <button type="button" :class="{ active: mode === '3d' }" @click="mode = '3d'">{{ t('ui.view3d') }}</button>
    </div>
  </div>
</template>

<script setup lang="ts">
import { useI18n } from 'vue-i18n';
const { t } = useI18n();
import { computed, ref } from 'vue';

const props = withDefaults(defineProps<{ src: string; initialMode?: '2d' | '3d' }>(), {
  initialMode: '2d',
});
const mode = ref<'2d' | '3d'>(props.initialMode);
const SCALE = 8;
const faces = ['front', 'back', 'right', 'left', 'top', 'bottom'] as const;
type Face = typeof faces[number];
type Uv = [number, number];

const parts = [
  {
    name: 'head', w: 8, h: 8, d: 8, x: 0, y: -28,
    uv: { front: [8, 8], back: [24, 8], right: [0, 8], left: [16, 8], top: [8, 0], bottom: [16, 0] } as Record<Face, Uv>,
  },
  {
    name: 'body', w: 8, h: 12, d: 4, x: 0, y: -18,
    uv: { front: [20, 20], back: [32, 20], right: [16, 20], left: [28, 20], top: [20, 16], bottom: [28, 16] } as Record<Face, Uv>,
  },
  {
    name: 'right-arm', w: 4, h: 12, d: 4, x: -6, y: -18,
    uv: { front: [44, 20], back: [52, 20], right: [40, 20], left: [48, 20], top: [44, 16], bottom: [48, 16] } as Record<Face, Uv>,
  },
  {
    name: 'left-arm', w: 4, h: 12, d: 4, x: 6, y: -18,
    uv: { front: [36, 52], back: [44, 52], right: [32, 52], left: [40, 52], top: [36, 48], bottom: [40, 48] } as Record<Face, Uv>,
  },
  {
    name: 'right-leg', w: 4, h: 12, d: 4, x: -2, y: -6,
    uv: { front: [4, 20], back: [12, 20], right: [0, 20], left: [8, 20], top: [4, 16], bottom: [8, 16] } as Record<Face, Uv>,
  },
  {
    name: 'left-leg', w: 4, h: 12, d: 4, x: 2, y: -6,
    uv: { front: [20, 52], back: [28, 52], right: [16, 52], left: [24, 52], top: [20, 48], bottom: [24, 48] } as Record<Face, Uv>,
  },
];

const skinImage = computed(() => `url("${String(props.src).replace(/"/g, '%22')}")`);

function partStyle(part: typeof parts[number]) {
  return {
    width: `${part.w * SCALE}px`,
    height: `${part.h * SCALE}px`,
    marginLeft: `${-part.w * SCALE / 2}px`,
    marginTop: `${-part.h * SCALE / 2}px`,
    transform: `translate3d(${part.x * SCALE}px, ${part.y * SCALE}px, 0)`,
    '--w': `${part.w * SCALE}px`,
    '--h': `${part.h * SCALE}px`,
    '--d': `${part.d * SCALE}px`,
  };
}

function faceStyle(part: typeof parts[number], face: Face) {
  const [u, v] = part.uv[face];
  const horizontal = face === 'top' || face === 'bottom' ? part.w : (face === 'right' || face === 'left' ? part.d : part.w);
  const vertical = face === 'top' || face === 'bottom' ? part.d : part.h;
  return {
    width: `${horizontal * SCALE}px`,
    height: `${vertical * SCALE}px`,
    backgroundImage: skinImage.value,
    backgroundSize: `${64 * SCALE}px ${64 * SCALE}px`,
    backgroundPosition: `${-u * SCALE}px ${-v * SCALE}px`,
  };
}
</script>

<style scoped>
.pcl-skin-preview {
  position: relative;
  width: 100%;
  max-width: 320px;
  aspect-ratio: 1;
  margin: 2px 0 12px;
  overflow: hidden;
  border: 1px solid var(--pcl-card-border);
  border-radius: 5px;
  background:
    repeating-conic-gradient(var(--pcl-c6) 0% 25%, var(--pcl-c7) 0% 50%) 50% / 16px 16px;
}

.pcl-skin-preview__stage {
  position: absolute;
  inset: 0;
  display: flex;
  align-items: center;
  justify-content: center;
}

.pcl-skin-2d {
  width: 100%;
  height: 100%;
  object-fit: contain;
  image-rendering: pixelated;
  image-rendering: crisp-edges;
}

.pcl-skin-3d {
  width: 100%;
  height: 100%;
  perspective: 900px;
}

.pcl-skin-model {
  position: absolute;
  left: 50%;
  top: 50%;
  width: 0;
  height: 0;
  transform-style: preserve-3d;
  transform: translateY(104px) rotateX(-6deg) rotateY(-24deg);
  animation: pcl-skin-turn 9s ease-in-out infinite;
}

.pcl-skin-part {
  position: absolute;
  left: 0;
  top: 0;
  transform-style: preserve-3d;
}

.pcl-skin-face {
  position: absolute;
  left: 50%;
  top: 50%;
  background-repeat: no-repeat;
  image-rendering: pixelated;
  image-rendering: crisp-edges;
  backface-visibility: hidden;
}

.face-front {
  transform: translate(-50%, -50%) translateZ(calc(var(--d) / 2));
}

.face-back {
  transform: translate(-50%, -50%) rotateY(180deg) translateZ(calc(var(--d) / 2));
  filter: brightness(.86);
}

.face-right {
  transform: translate(-50%, -50%) rotateY(90deg) translateZ(calc(var(--w) / 2));
  filter: brightness(.78);
}

.face-left {
  transform: translate(-50%, -50%) rotateY(-90deg) translateZ(calc(var(--w) / 2));
  filter: brightness(.82);
}

.face-top {
  transform: translate(-50%, -50%) rotateX(90deg) translateZ(calc(var(--h) / 2));
  filter: brightness(1.08);
}

.face-bottom {
  transform: translate(-50%, -50%) rotateX(-90deg) translateZ(calc(var(--h) / 2));
  filter: brightness(.7);
}

.pcl-skin-preview__modes {
  position: absolute;
  right: 8px;
  bottom: 8px;
  display: flex;
  overflow: hidden;
  border: 1px solid var(--pcl-c5);
  border-radius: 4px;
  background: var(--pcl-c7);
}

.pcl-skin-preview__modes button {
  width: 34px;
  height: 24px;
  padding: 0;
  border: 0;
  background: transparent;
  color: var(--pcl-muted);
  font: 400 calc(11px * var(--pcl-font-scale, 1))/1 var(--pcl-font);
  cursor: pointer;
}

.pcl-skin-preview__modes button.active {
  background: var(--pcl-c3);
  color: #fff;
}

@keyframes pcl-skin-turn {
  0%, 100% { transform: translateY(104px) rotateX(-6deg) rotateY(-28deg); }
  50% { transform: translateY(104px) rotateX(-6deg) rotateY(28deg); }
}
</style>
