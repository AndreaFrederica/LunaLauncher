import type { InjectionKey } from 'vue';

export type XmclAddInstanceContext = Record<string, any> & {
  t: (key: string, params?: Record<string, unknown>) => string;
};

export const XmclAddInstanceContextKey: InjectionKey<XmclAddInstanceContext> = Symbol('xmcl-add-instance-context');

