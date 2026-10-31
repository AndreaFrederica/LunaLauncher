import type { InjectionKey } from 'vue';
export type PclDownloadContext = Record<string, any> & { t: (key: string, params?: Record<string, unknown>) => string };
export const PclDownloadContextKey: InjectionKey<PclDownloadContext> = Symbol('pcl-download-context');
