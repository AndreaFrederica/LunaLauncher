import { reactive } from 'vue';
import { neoUiPreferences, persistNeoUiPreferences } from '../../neo-ui-preferences';

const databaseName = 'luna-pcl-media';
const storeName = 'audio';
const trackKey = 'background';

export const pclMusic = reactive({
  loaded: false,
  playing: false,
  name: neoUiPreferences.pclBackgroundMusicName || '',
  volume: neoUiPreferences.pclBackgroundMusicVolume,
  enabled: neoUiPreferences.pclBackgroundMusicEnabled,
  error: '',
});

let audio: HTMLAudioElement | null = null;
let objectUrl = '';
let initPromise: Promise<void> | null = null;

function openDatabase(): Promise<IDBDatabase> {
  return new Promise((resolve, reject) => {
    const request = indexedDB.open(databaseName, 1);
    request.onupgradeneeded = () => {
      if (!request.result.objectStoreNames.contains(storeName)) request.result.createObjectStore(storeName);
    };
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error || new Error('Could not open the local media store.'));
  });
}

async function readTrack(): Promise<Blob | null> {
  const database = await openDatabase();
  return new Promise((resolve, reject) => {
    const request = database.transaction(storeName, 'readonly').objectStore(storeName).get(trackKey);
    request.onsuccess = () => {
      database.close();
      resolve(request.result instanceof Blob ? request.result : null);
    };
    request.onerror = () => {
      database.close();
      reject(request.error || new Error('Could not read the background music.'));
    };
  });
}

async function writeTrack(file: Blob): Promise<void> {
  const database = await openDatabase();
  return new Promise((resolve, reject) => {
    const request = database.transaction(storeName, 'readwrite').objectStore(storeName).put(file, trackKey);
    request.onsuccess = () => {
      database.close();
      resolve();
    };
    request.onerror = () => {
      database.close();
      reject(request.error || new Error('Could not save the background music.'));
    };
  });
}

async function deleteTrack(): Promise<void> {
  const database = await openDatabase();
  return new Promise((resolve, reject) => {
    const request = database.transaction(storeName, 'readwrite').objectStore(storeName).delete(trackKey);
    request.onsuccess = () => {
      database.close();
      resolve();
    };
    request.onerror = () => {
      database.close();
      reject(request.error || new Error('Could not remove the background music.'));
    };
  });
}

function ensureAudio(): HTMLAudioElement {
  if (!audio) {
    audio = new Audio();
    audio.loop = true;
    audio.volume = pclMusic.volume;
    audio.addEventListener('play', () => { pclMusic.playing = true; });
    audio.addEventListener('pause', () => { pclMusic.playing = false; });
    audio.addEventListener('error', () => { pclMusic.error = '背景音乐无法播放，请重新选择文件。'; });
  }
  return audio;
}

async function loadCurrent(): Promise<void> {
  const element = ensureAudio();
  if (!neoUiPreferences.pclBackgroundMusicName) {
    pclMusic.loaded = false;
    pclMusic.name = '';
    element.removeAttribute('src');
    return;
  }
  const blob = await readTrack();
  if (!blob) {
    pclMusic.loaded = false;
    pclMusic.name = '';
    return;
  }
  if (objectUrl) URL.revokeObjectURL(objectUrl);
  objectUrl = URL.createObjectURL(blob);
  element.src = objectUrl;
  pclMusic.loaded = true;
  pclMusic.name = neoUiPreferences.pclBackgroundMusicName || 'background-music';
  pclMusic.volume = neoUiPreferences.pclBackgroundMusicVolume;
  element.volume = pclMusic.volume;
}

export async function initPclMusic(): Promise<void> {
  if (initPromise) return initPromise;
  initPromise = (async () => {
    try {
      await loadCurrent();
      if (pclMusic.loaded && neoUiPreferences.pclBackgroundMusicEnabled) {
        await ensureAudio().play().catch(() => { /* Autoplay requires a user gesture. */ });
      }
    } catch (exception) {
      pclMusic.error = exception instanceof Error ? exception.message : String(exception);
    }
  })();
  return initPromise;
}

export async function choosePclMusic(file: File): Promise<void> {
  if (!file.type.startsWith('audio/')) {
    pclMusic.error = '请选择音频文件。';
    return;
  }
  if (file.size > 30 * 1024 * 1024) {
    pclMusic.error = '背景音乐不能超过 30 MB。';
    return;
  }
  try {
    await writeTrack(file);
    neoUiPreferences.pclBackgroundMusicName = file.name;
    neoUiPreferences.pclBackgroundMusicEnabled = true;
    persistNeoUiPreferences();
    await loadCurrent();
    pclMusic.error = '';
    await ensureAudio().play().catch(() => { /* The user can press play again. */ });
  } catch (exception) {
    pclMusic.error = exception instanceof Error ? exception.message : String(exception);
  }
}

export function togglePclMusic(): void {
  if (!pclMusic.loaded) return;
  const element = ensureAudio();
  if (element.paused) {
    void element.play().then(() => {
      neoUiPreferences.pclBackgroundMusicEnabled = true;
      persistNeoUiPreferences();
    }).catch((exception) => {
      pclMusic.error = exception instanceof Error ? exception.message : String(exception);
    });
  } else {
    element.pause();
    neoUiPreferences.pclBackgroundMusicEnabled = false;
    persistNeoUiPreferences();
  }
}

export function setPclMusicVolume(value: number): void {
  pclMusic.volume = Math.max(0, Math.min(1, value));
  ensureAudio().volume = pclMusic.volume;
  neoUiPreferences.pclBackgroundMusicVolume = pclMusic.volume;
  persistNeoUiPreferences();
}

export async function removePclMusic(): Promise<void> {
  try {
    ensureAudio().pause();
    ensureAudio().removeAttribute('src');
    if (objectUrl) URL.revokeObjectURL(objectUrl);
    objectUrl = '';
    await deleteTrack();
    pclMusic.loaded = false;
    pclMusic.playing = false;
    pclMusic.name = '';
    neoUiPreferences.pclBackgroundMusicName = '';
    neoUiPreferences.pclBackgroundMusicEnabled = false;
    persistNeoUiPreferences();
  } catch (exception) {
    pclMusic.error = exception instanceof Error ? exception.message : String(exception);
  }
}
