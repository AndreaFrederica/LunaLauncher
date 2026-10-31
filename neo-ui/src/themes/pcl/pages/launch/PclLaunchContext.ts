import type { InjectionKey } from 'vue';
export type PclLaunchContext = Record<string, any> & { t: (key: string, params?: Record<string, unknown>) => string };
export const PclLaunchContextKey: InjectionKey<PclLaunchContext> = Symbol('pcl-launch-context');
