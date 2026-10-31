import type { InjectionKey } from 'vue';
export type PclResourceContext = Record<string, any> & { t: (key: string, params?: Record<string, unknown>) => string };
export const PclResourceContextKey: InjectionKey<PclResourceContext> = Symbol('pcl-resource-context');
