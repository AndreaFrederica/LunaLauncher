import { ref } from 'vue';
import water from '../assets/minecraft-water.jpg';
// Only confirmed main-window skins belong here; sky.jpg is a dialog backdrop.
const backgrounds = ref<string[]>([water]);
export const defaultBackground = water;
export function registerMelodyBackground(url: string) {
  const parsed = new URL(url, location.href);
  if (!['http:', 'https:', 'data:', 'blob:', 'asset:'].includes(parsed.protocol)) throw new Error('不支持的背景图片地址');
  if (parsed.protocol === 'data:' && !url.startsWith('data:image/')) throw new Error('背景必须是图片');
  if (!backgrounds.value.includes(parsed.href)) backgrounds.value.push(parsed.href);
}
export function chooseMelodyBackground(mode: string) {
  return mode === 'random' ? backgrounds.value[Math.floor(Math.random() * backgrounds.value.length)] || water : water;
}
