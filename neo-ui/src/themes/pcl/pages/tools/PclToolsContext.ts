import type { InjectionKey } from 'vue';
export type PclToolsContext = Record<string, any> & { t: (key: string, params?: Record<string, unknown>) => string };
export const PclToolsContextKey: InjectionKey<PclToolsContext> = Symbol('pcl-tools-context');
