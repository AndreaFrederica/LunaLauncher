import type { InjectionKey } from 'vue';
export type PclAccountContext = Record<string, any> & { t: (key: string, params?: Record<string, unknown>) => string };
export const PclAccountContextKey: InjectionKey<PclAccountContext> = Symbol('pcl-account-context');
