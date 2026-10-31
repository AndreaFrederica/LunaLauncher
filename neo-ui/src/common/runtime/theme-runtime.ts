import { computed, onBeforeUnmount, onMounted, ref, watch, type Ref } from 'vue';
import { recordRuntimeDebug } from './runtime-debug';

type RuntimeResourceSetup = () => void | (() => void);
type RuntimeResource = { setup: RuntimeResourceSetup; cleanup?: () => void };

export interface ThemeRuntime {
  name: string;
  active: Ref<boolean>;
  start(): void;
  stop(): void;
  register(cleanup: () => void): () => void;
  registerResource(setup: RuntimeResourceSetup): () => void;
}

const runtimeRegistry = new Map<string, ThemeRuntime>();

/** Return the active theme boundary for composables that own theme resources. */
export function getThemeRuntime(name: string): ThemeRuntime | undefined {
  return runtimeRegistry.get(name);
}

/**
 * Register a resource from a child component. The setup runs whenever the
 * theme is active and runs again after a later theme reactivation. Its
 * returned cleanup runs before each stop and when the component unmounts.
 */
export function useThemeResource(name: string, setup: RuntimeResourceSetup) {
  let unregister: (() => void) | undefined;
  onMounted(() => {
    const runtime = getThemeRuntime(name);
    if (runtime) {
      unregister = runtime.registerResource(setup);
      return;
    }
    // A resource can be used by a standalone theme preview without a root
    // runtime. Keep the composable safe in that environment as well.
    const cleanup = setup();
    unregister = () => { if (typeof cleanup === 'function') cleanup(); unregister = undefined; };
  });
  onBeforeUnmount(() => { unregister?.(); unregister = undefined; });
}

/**
 * Lifecycle boundary for a theme sub-app. Core data stores remain alive, while
 * theme-owned subscriptions and timers are registered here and are disposed
 * whenever that theme is no longer active.
 */
export function useThemeRuntime(name: string, enabled: Ref<boolean> | (() => boolean)): ThemeRuntime {
  const active = ref(false);
  const cleanups = new Set<() => void>();
  const resources = new Set<RuntimeResource>();
  const isEnabled = computed(() => typeof enabled === 'function' ? enabled() : enabled.value);
  function activateResource(resource: RuntimeResource) {
    if (!active.value || resource.cleanup) return;
    const cleanup = resource.setup();
    resource.cleanup = typeof cleanup === 'function' ? cleanup : undefined;
    recordRuntimeDebug('lifecycle', `theme:${name}`, 'resource activated', { resourceCount: resources.size });
  }
  function deactivateResource(resource: RuntimeResource) {
    const cleanup = resource.cleanup;
    resource.cleanup = undefined;
    try { cleanup?.(); } catch { /* A resource cleanup must not block the runtime. */ }
    recordRuntimeDebug('lifecycle', `theme:${name}`, 'resource deactivated', { resourceCount: resources.size });
  }
  function start() {
    if (active.value || !isEnabled.value) return;
    active.value = true;
    resources.forEach(activateResource);
    recordRuntimeDebug('lifecycle', `theme:${name}`, 'theme runtime started');
  }
  function stop() {
    if (!active.value) return;
    resources.forEach(deactivateResource);
    for (const cleanup of cleanups) { try { cleanup(); } catch { /* A cleanup must not prevent the remaining ones. */ } }
    cleanups.clear();
    active.value = false;
    recordRuntimeDebug('lifecycle', `theme:${name}`, 'theme runtime stopped');
  }
  function register(cleanup: () => void) {
    cleanups.add(cleanup);
    return () => cleanups.delete(cleanup);
  }
  function registerResource(setup: RuntimeResourceSetup) {
    const resource: RuntimeResource = { setup };
    resources.add(resource);
    activateResource(resource);
    return () => {
      resources.delete(resource);
      deactivateResource(resource);
    };
  }
  onMounted(start);
  watch(isEnabled, value => { if (value) start(); else stop(); }, { flush: 'sync' });
  onBeforeUnmount(stop);
  const runtime = { name, active, start, stop, register, registerResource } satisfies ThemeRuntime;
  runtimeRegistry.set(name, runtime);
  onBeforeUnmount(() => {
    if (runtimeRegistry.get(name) === runtime) runtimeRegistry.delete(name);
  });
  return runtime;
}
