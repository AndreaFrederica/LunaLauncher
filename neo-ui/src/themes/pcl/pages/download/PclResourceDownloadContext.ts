import type { InjectionKey } from 'vue';
export type PclResourceDownloadContext = Record<string, any> & { t: (key: string, params?: Record<string, unknown>) => string };
export const PclResourceDownloadContextKey: InjectionKey<PclResourceDownloadContext> = Symbol('pcl-resource-download-context');
