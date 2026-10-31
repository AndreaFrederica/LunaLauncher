import type { InjectionKey } from 'vue';
/** A shell may host resource downloads in its own window system. */
export const resourceWindowHostKey: InjectionKey<(instance: string, kind: string) => Promise<void> | void> = Symbol('resource-window-host');
