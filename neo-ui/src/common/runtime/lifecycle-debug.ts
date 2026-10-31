import { onBeforeUnmount, onMounted, watch, type Ref } from 'vue';
import { recordRuntimeDebug } from './runtime-debug';

/**
 * Observability for page components that remain mounted behind v-show.
 * `active` describes whether the page currently owns work such as polling or
 * subscriptions; it does not control rendering and therefore cannot rebuild
 * the component tree.
 */
export function useRuntimeLifecycle(source: string, active?: Ref<boolean>) {
  onMounted(() => {
    recordRuntimeDebug('lifecycle', source, 'mounted');
    if (active) recordRuntimeDebug('lifecycle', source, active.value ? 'activated' : 'inactive');
  });
  if (active) watch(active, value => recordRuntimeDebug('lifecycle', source, value ? 'activated' : 'deactivated'), { flush: 'sync' });
  onBeforeUnmount(() => recordRuntimeDebug('lifecycle', source, 'unmounted'));
}
