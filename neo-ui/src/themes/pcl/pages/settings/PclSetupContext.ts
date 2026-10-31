import type { InjectionKey, Ref, ComputedRef } from 'vue';

export type PclSetupContext = Record<string, any> & {
  t: (key: string, params?: Record<string, unknown>) => string;
  drafts: Record<string, any>;
  settings: any[];
  scope: Record<string, unknown>;
  isInstance: ComputedRef<boolean>;
  section: Ref<string>;
  error: Ref<string>;
  notice: Ref<string>;
};

export const PclSetupContextKey: InjectionKey<PclSetupContext> = Symbol('pcl-setup-context');
