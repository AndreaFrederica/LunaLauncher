import { computed, reactive, ref, watch, type WatchStopHandle } from 'vue';
import { defineStore } from 'pinia';
import { invoke, isTauri } from '@tauri-apps/api/core';
import { emit, listen, type UnlistenFn } from '@tauri-apps/api/event';
import { defaultBackground } from '../services/backgrounds';
import { useThemeResource } from '../../../common/runtime/theme-runtime';
const key = 'luna-melody.appearance', selectionKey = 'luna-melody.background-selection';
export type Appearance = {
  footer: 'legacy' | 'modern' | 'custom'; footerText: string; footerSize: number; footerColor: string; footerOpacity: number;
  fit: 'cover' | 'contain' | 'fill'; customBackground: boolean;
  backgroundMode: 'builtin' | 'single' | 'random'; directory: string;
};
// The original launcher artwork is authored at the exact 720×450 window size.
// Stretch that legacy image to the client area so a native frame/DPI rounding
// difference cannot leave a light strip along an edge. Custom images can still
// opt into cover/contain in the appearance window.
const defaults: Appearance = { footer: 'legacy', footerText: 'Luna Launcher Neo UI', footerSize: 12, footerColor: '#ffffff', footerOpacity: .75, fit: 'fill', customBackground: false, backgroundMode: 'builtin', directory: '' };
function normalize(data: Partial<Appearance>): Appearance {
  const number = (v: unknown, fallback: number, min: number, max: number) => typeof v === 'number' && Number.isFinite(v) ? Math.max(min, Math.min(max, v)) : fallback;
  const mode = ['builtin', 'single', 'random'].includes(data.backgroundMode || '') ? data.backgroundMode! : data.customBackground ? 'single' : 'builtin';
  return { footer: ['legacy', 'modern', 'custom'].includes(data.footer || '') ? data.footer! : 'legacy', footerText: typeof data.footerText === 'string' ? data.footerText.slice(0, 200) : defaults.footerText,
    footerSize: number(data.footerSize, 12, 10, 24), footerColor: /^#[\da-f]{6}$/i.test(data.footerColor || '') ? data.footerColor! : '#ffffff', footerOpacity: number(data.footerOpacity, .75, 0, 1),
    fit: ['cover', 'contain', 'fill'].includes(data.fit || '') ? data.fit! : 'cover', customBackground: mode !== 'builtin', backgroundMode: mode, directory: typeof data.directory === 'string' ? data.directory : '' };
}
function read(): Appearance {
  if (isTauri()) return { ...defaults };
  try { return normalize(JSON.parse(localStorage.getItem(key) || '{}')); } catch { return { ...defaults }; }
}
export const melodyAppearance = reactive(read());
export function setMelodyAppearance(patch: Partial<Appearance>) {
  const next = normalize({ ...melodyAppearance, ...patch });
  Object.assign(melodyAppearance, next);
  if (isTauri()) {
    void invoke('set_melody_appearance', { appearance: next }).catch(error => console.error('Unable to persist Melody appearance', error));
    void emit('melody-appearance-changed', next).catch(error => console.error('Unable to broadcast Melody appearance', error));
  } else localStorage.setItem(key, JSON.stringify(next));
}
export const melodyFooter = computed(() => melodyAppearance.footer === 'custom' ? melodyAppearance.footerText : melodyAppearance.footer === 'modern' ? 'Luna Launcher Neo UI' : 'MineCraft Launcher 5.31 by 忘却的旋律');
const footerStyle = computed(() => ({ fontSize: `${melodyAppearance.footerSize}px`, color: melodyAppearance.footerColor, opacity: melodyAppearance.footerOpacity }));
let appearanceBridge: Promise<void> | undefined;
let appearanceUnlisten: UnlistenFn | undefined;
let appearanceBridgeGeneration = 0;
function startAppearanceBridge() {
  if (!isTauri() || appearanceBridge) return;
  const generation = ++appearanceBridgeGeneration;
  appearanceBridge = listen<Partial<Appearance>>('melody-appearance-changed', event => {
    if (!event.payload || typeof event.payload !== 'object') return;
    Object.assign(melodyAppearance, normalize(event.payload));
  }).then(unlisten => {
    if (generation !== appearanceBridgeGeneration) { unlisten(); return; }
    appearanceUnlisten = unlisten;
  }).catch(error => { if (generation === appearanceBridgeGeneration) console.error('Unable to bridge Melody appearance', error); });
}
function stopAppearanceBridge() {
  appearanceBridgeGeneration++;
  appearanceUnlisten?.();
  appearanceUnlisten = undefined;
  appearanceBridge = undefined;
}
export const useMelodyStore = defineStore('melody', {
  state: () => ({ appearance: melodyAppearance, loaded: false }),
  actions: {
    async hydrate() {
      if (this.loaded) return;
      if (isTauri()) {
        const stored = await invoke<Partial<Appearance> | null>('get_melody_appearance');
        if (stored) Object.assign(melodyAppearance, normalize(stored));
      } else Object.assign(melodyAppearance, read());
      this.loaded = true;
    },
    bridge() { startAppearanceBridge(); },
    stopBridge() { stopAppearanceBridge(); },
    setAppearance(patch: Partial<Appearance>) { setMelodyAppearance(patch); },
  },
});
type Background = { file: Blob; name: string };
type Album = { name: string; files: Background[] };
let database: Promise<IDBDatabase> | undefined;
function db() {
  return database ||= new Promise<IDBDatabase>((resolve, reject) => {
    const request = indexedDB.open('luna-melody', 1);
    request.onupgradeneeded = () => request.result.createObjectStore('images');
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => { database = undefined; reject(request.error); };
  });
}
async function record<T>(id: string, value?: T | null): Promise<T | undefined> {
  const database = await db();
  return new Promise((resolve, reject) => {
    const tx = database.transaction('images', value === undefined ? 'readonly' : 'readwrite'), store = tx.objectStore('images');
    const request = value === undefined ? store.get(id) : value === null ? store.delete(id) : store.put(value, id);
    tx.oncomplete = () => resolve(value === undefined ? request.result : value || undefined);
    tx.onerror = () => reject(tx.error); tx.onabort = () => reject(tx.error || new Error('图片保存失败'));
  });
}
// A single state per webview keeps the settings preview and launcher on the same image.
const imageUrl = ref(''), imageName = ref(''), error = ref(''), imageCount = ref(0), loading = ref(false);
const backgroundUrl = computed(() => melodyAppearance.backgroundMode !== 'builtin' && imageUrl.value ? imageUrl.value : defaultBackground);
let users = 0, generation = 0, stopWatch: WatchStopHandle | undefined;
const imageExtension = /\.(png|jpe?g|webp|gif|bmp)$/i;
async function decodedUrl(file: Blob): Promise<string> {
  if (file.size > 32 * 1024 * 1024) throw new Error('请选择不超过 32 MB 的图片。');
  const url = URL.createObjectURL(file);
  try { const image = new Image(); image.src = url; await image.decode(); return url; }
  catch { URL.revokeObjectURL(url); throw new Error('图片无法读取。'); }
}
function selected(): { source?: string; name?: string } {
  if (isTauri()) return {};
  try { return JSON.parse(localStorage.getItem(selectionKey) || '{}'); } catch { return {}; }
}
function replaceImage(url: string, name: string) {
  if (imageUrl.value) URL.revokeObjectURL(imageUrl.value);
  imageUrl.value = url; imageName.value = name;
}
async function reload(randomize = false) {
  const current = ++generation, { backgroundMode: mode, directory } = melodyAppearance, source = `${mode}:${directory}`;
  loading.value = true; error.value = '';
  try {
    if (mode === 'builtin') { replaceImage('', ''); imageCount.value = 0; return; }
    let names: string[], readImage: (name: string) => Promise<Blob>;
    if (mode === 'single') {
      const saved = await record<Background>('background');
      if (!saved) throw new Error('请先选择背景图片。');
      names = [saved.name]; readImage = async () => saved.file;
    } else if (isTauri()) {
      if (!directory) throw new Error('请先选择背景目录。');
      names = await invoke<string[]>('list_melody_backgrounds', { directory });
      readImage = async name => new Blob([await invoke<ArrayBuffer>('read_melody_background', { directory, name })]);
    } else {
      const album = await record<Album>('album');
      if (!album) throw new Error('请先选择背景目录。');
      names = album.files.map(file => file.name); readImage = async name => album.files.find(file => file.name === name)!.file;
    }
    if (current !== generation || !users) return;
    imageCount.value = names.length;
    if (!names.length) throw new Error('目录中没有可用的背景图片。');
    const previous = selected();
    let first = previous.source === source && names.includes(previous.name || '') ? previous.name! : '';
    if (mode === 'random' && (randomize || !first)) {
      const pool = names.filter(name => name !== first);
      first = pool.length ? pool[Math.floor(Math.random() * pool.length)]! : names[0]!;
    }
    // Decode before displaying; skip removed, oversized or corrupt files.
    for (const name of [first, ...names.filter(name => name !== first)].filter(Boolean)) {
      let url: string;
      try { url = await decodedUrl(await readImage(name)); } catch { continue; }
      if (current !== generation || !users) { URL.revokeObjectURL(url); return; }
      replaceImage(url, name);
      const selection = JSON.stringify({ source, name });
      if (!isTauri() && localStorage.getItem(selectionKey) !== selection) localStorage.setItem(selectionKey, selection);
      return;
    }
    throw new Error('背景图片均无法读取，请检查目录中的文件。');
  } catch (e) {
    if (current === generation && users) { replaceImage('', ''); imageCount.value = 0; error.value = String(e); }
  } finally { if (current === generation) loading.value = false; }
}
function sync(event: StorageEvent) {
  if (event.key === key || event.key === null) Object.assign(melodyAppearance, read());
  if ([selectionKey, 'luna-melody.image-revision', null].includes(event.key)) void reload();
}
async function changed(randomize = false) {
  await reload(randomize);
  if (!isTauri()) localStorage.setItem('luna-melody.image-revision', crypto.randomUUID());
}
export function useMelodyAppearance(options: { randomizeOnMount?: boolean } = {}) {
  const store = useMelodyStore();
  async function upload(file: File) {
    if (!imageExtension.test(file.name)) throw new Error('请选择 PNG、JPEG、WebP、GIF 或 BMP 图片。');
    URL.revokeObjectURL(await decodedUrl(file));
    await record('background', { file, name: file.name });
    store.setAppearance({ backgroundMode: 'single' }); await changed();
  }
  async function uploadDirectory(files: File[]) {
    const images = files.filter(file => imageExtension.test(file.name));
    if (!images.length) throw new Error('目录中没有可用的背景图片。');
    if (images.reduce((sum, file) => sum + file.size, 0) > 128 * 1024 * 1024) throw new Error('导入的背景图片总大小不能超过 128 MB。');
    const valid: Background[] = [];
    for (const file of images) {
      try { URL.revokeObjectURL(await decodedUrl(file)); valid.push({ file, name: file.webkitRelativePath || file.name }); } catch { /* Skip invalid files. */ }
    }
    if (!valid.length) throw new Error('目录中没有可读取的背景图片。');
    const name = images[0]!.webkitRelativePath.split('/')[0] || '背景图片';
    await record<Album>('album', { name, files: valid });
    store.setAppearance({ directory: name, backgroundMode: 'random' }); await changed(true);
  }
  async function chooseDirectory(directory: string) {
    const names = await invoke<string[]>('list_melody_backgrounds', { directory });
    if (!names.length) throw new Error('目录中没有可用的背景图片。');
    store.setAppearance({ directory, backgroundMode: 'random' }); await changed(true);
  }
  async function remove() { await record('background', null); store.setAppearance({ backgroundMode: 'builtin' }); await changed(); }
  useThemeResource('melody', () => {
    if (users++ !== 0) return () => { users--; };
    if (!isTauri()) window.addEventListener('storage', sync);
    startAppearanceBridge();
    stopWatch = watch(() => [melodyAppearance.backgroundMode, melodyAppearance.directory], () => { void reload(); }, { flush: 'sync' });
    void store.hydrate().finally(() => { void reload(options.randomizeOnMount && melodyAppearance.backgroundMode === 'random'); });
    return () => {
      if (--users !== 0) return;
      ++generation;
      stopWatch?.();
      stopWatch = undefined;
      window.removeEventListener('storage', sync);
      stopAppearanceBridge();
      replaceImage('', '');
    };
  });
  return { appearance: melodyAppearance, footer: melodyFooter, footerStyle, imageUrl, imageName, backgroundUrl, imageCount, loading, error, upload, uploadDirectory, chooseDirectory, remove, shuffle: () => changed(true), set: store.setAppearance };
}
