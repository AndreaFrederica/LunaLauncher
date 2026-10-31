import windows from './config/windows.json';
export { windows };
export type MelodyWindowKind = keyof typeof windows;
export interface MelodyOption { value: string; label: string; disabled?: boolean }
export interface MelodySettings {
  name: string; password: string; official: boolean;
  memoryMode: string; memory: string; skinMode: string; javaPath: string;
  game: string; directory: string; debug: boolean;
}
export const defaultSettings: MelodySettings = {
  name: 'Andrea', password: '', official: false, memoryMode: 'auto', memory: '1024',
  skinMode: 'random', javaPath: '', game: '', directory: 'shared', debug: false,
};
export interface MelodyWindowEntry {
  id: string; kind: MelodyWindowKind; parent?: string;
  x: number; y: number; z: number; message?: string; title?: string;
  width?: number; instanceId?: string; componentUid?: string;
}
