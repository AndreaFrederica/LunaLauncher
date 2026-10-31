import type { InjectionKey } from 'vue';
export type PclInstanceSectionsContext = Record<string, any> & { t: (key: string, params?: Record<string, unknown>) => string };
export const PclInstanceSectionsContextKey: InjectionKey<PclInstanceSectionsContext> = Symbol('pcl-instance-sections-context');
