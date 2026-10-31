import type { InjectionKey } from 'vue';

export type XmclSettingsContext = Record<string, any> & {
  t: (key: string, params?: Record<string, unknown>) => string;
};

export const XmclSettingsContextKey: InjectionKey<XmclSettingsContext> = Symbol('xmcl-settings-context');

