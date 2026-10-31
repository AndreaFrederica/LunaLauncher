import type { InjectionKey } from 'vue';
export type XmclInstanceContext = Record<string, any> & { t: (key: string, params?: Record<string, unknown>) => string };
export const XmclInstanceContextKey: InjectionKey<XmclInstanceContext> = Symbol('xmcl-instance-context');
