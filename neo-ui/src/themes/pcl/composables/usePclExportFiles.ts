import { computed, ref, watch, type Ref } from 'vue';

interface ExportFile {
  path: string;
  name: string;
  isDir: boolean;
  category: string;
  defaultSelected: boolean;
}
type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
const within = (parent: string, path: string) => parent === path || path.startsWith(`${parent}/`);

export function usePclExportFiles(props: { instance?: string; section: string; execute?: Execute }, selected: Ref<string[]>, format: Ref<string>) {
  const roots = ref<ExportFile[]>([]);
  const children = ref<Record<string, ExportFile[]>>({});
  const expanded = ref<string[]>([]);
  const rules = ref<Record<string, boolean>>({});
  const loading = ref(false), ready = ref(false), error = ref('');
  const pending = ref<string[]>([]);
  const gamePrefix = ref('');
  let generation = 0;
  function available(node: ExportFile) {
    return format.value === 'zip' || (node.category !== 'game' &&
      (gamePrefix.value === '.' || within(gamePrefix.value, node.path) || within('lunaui', node.path)));
  }
  const visibleRoots = computed(() => roots.value.filter(available));
  const rows = computed(() => {
    const result: (ExportFile & { depth: number })[] = [];
    function visit(nodes: ExportFile[], depth: number) {
      for (const node of nodes) {
        result.push({ ...node, depth });
        if (expanded.value.includes(node.path)) visit(children.value[node.path] || [], depth + 1);
      }
    }
    visit(visibleRoots.value, 0);
    return result;
  });
  function checked(path: string) {
    const ancestors = Object.keys(rules.value).filter(rule => within(rule, path)).sort((a, b) => b.length - a.length);
    return ancestors.length ? Boolean(rules.value[ancestors[0]!]) : false;
  }
  function partial(path: string) {
    const state = checked(path);
    return Object.entries(rules.value).some(([rule, value]) => rule !== path && within(path, rule) && value !== state);
  }
  function toggle(path: string, value: boolean) {
    const next = { ...rules.value };
    for (const rule of Object.keys(next)) if (within(path, rule)) delete next[rule];
    next[path] = value;
    rules.value = next;
  }
  function category(key: string, value: boolean) {
    const next = { ...rules.value };
    for (const node of roots.value.filter(node => node.category === key)) {
      for (const path of Object.keys(next)) if (within(node.path, path)) delete next[path];
      // Quick selection enables the category's standard content. Other folders are opt-in.
      next[node.path] = value && node.defaultSelected;
    }
    rules.value = next;
  }
  async function load() {
    if (!props.execute || !props.instance) return;
    const token = ++generation, instance = props.instance;
    loading.value = true; ready.value = false; error.value = '';
    try {
      const root = await props.execute('instance.export-files', { instance });
      const prefix = root.gamePrefix;
      let entries: ExportFile[] = root.entries;
      if (prefix && prefix !== '.') {
        const game = await props.execute('instance.export-files', { instance, path: prefix });
        entries = [...entries.filter(node => node.path !== prefix), ...game.entries];
      }
      if (token !== generation) return;
      gamePrefix.value = prefix;
      roots.value = entries;
      children.value = {}; expanded.value = []; pending.value = [];
      rules.value = Object.fromEntries(entries.map(node => [node.path, selected.value.includes(node.category) && node.defaultSelected]));
      ready.value = true;
    } catch (e) {
      if (token === generation) error.value = e instanceof Error ? e.message : String(e);
    } finally {
      if (token === generation) loading.value = false;
    }
  }
  async function expand(node: ExportFile) {
    if (expanded.value.includes(node.path)) { expanded.value = expanded.value.filter(path => path !== node.path); return; }
    if (!children.value[node.path] && props.execute && props.instance) {
      const token = generation;
      if (pending.value.includes(node.path)) return;
      pending.value.push(node.path);
      try {
        const result = await props.execute('instance.export-files', { instance: props.instance, path: node.path });
        if (token !== generation) return;
        children.value[node.path] = result.entries;
      } catch (e) {
        if (token === generation) error.value = e instanceof Error ? e.message : String(e);
        return;
      } finally {
        if (token === generation) pending.value = pending.value.filter(path => path !== node.path);
      }
    }
    expanded.value.push(node.path);
  }
  const selection = computed(() => Object.fromEntries(Object.entries(rules.value).filter(([path]) =>
    roots.value.some(node => within(node.path, path) && available(node)))));
  watch(() => props.instance, () => {
    generation++; ready.value = false; loading.value = false; error.value = ''; roots.value = []; rules.value = {};
    if (props.section === 'export') void load();
  });
  watch(() => props.section, section => { if (section === 'export' && !ready.value && !loading.value) void load(); }, { immediate: true });
  return { rows, rules, selection, loading, ready, error, expanded, pending, checked, partial, toggle, category, load, expand };
}
