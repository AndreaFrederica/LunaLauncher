import type { InjectionKey } from 'vue';
export type PclCatalogContext = Record<string, any> & { t: (key: string, params?: Record<string, unknown>) => string };
export const PclCatalogContextKey: InjectionKey<PclCatalogContext> = Symbol('pcl-catalog-context');
