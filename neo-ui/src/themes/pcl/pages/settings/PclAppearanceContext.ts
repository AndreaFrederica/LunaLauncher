import type { InjectionKey } from 'vue';
export type PclAppearanceContext = Record<string, any> & { t: (key: string, params?: Record<string, unknown>) => string };
export const PclAppearanceContextKey: InjectionKey<PclAppearanceContext> = Symbol('pcl-appearance-context');
