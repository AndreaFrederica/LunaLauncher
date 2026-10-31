import type { InjectionKey, Ref } from 'vue';
import type { MelodyWindowEntry } from '../types';
export interface MelodyWindowContext {
  entry: Ref<MelodyWindowEntry>; native: boolean;
  activate: () => void; close: () => void; move: (x: number, y: number) => void;
}
export const melodyWindowKey: InjectionKey<MelodyWindowContext> = Symbol('melody-window');
