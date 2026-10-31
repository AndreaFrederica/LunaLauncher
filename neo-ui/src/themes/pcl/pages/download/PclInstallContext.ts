import type { InjectionKey } from 'vue';
export type PclInstallContext = Record<string, any> & { t: (key: string, params?: Record<string, unknown>) => string };
export const PclInstallContextKey: InjectionKey<PclInstallContext> = Symbol('pcl-install-context');
