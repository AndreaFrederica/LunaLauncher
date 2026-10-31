import type { InjectionKey } from 'vue';

export type XmclAccountContext = Record<string, any> & {
  t: (key: string, params?: Record<string, unknown>) => string;
};

export const XmclAccountContextKey: InjectionKey<XmclAccountContext> = Symbol('xmcl-account-context');

