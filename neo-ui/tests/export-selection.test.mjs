// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import { test } from 'node:test';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import { build } from 'esbuild';
import { reactive, ref, nextTick, effectScope } from 'vue';

const require = createRequire(import.meta.url);
const compiled = await build({ entryPoints: [fileURLToPath(new URL('../src/themes/pcl/composables/usePclExportFiles.ts', import.meta.url))], bundle: true, write: false, format: 'cjs', platform: 'node', external: ['vue'] });
const module = { exports: {} };
new Function('require', 'module', 'exports', compiled.outputFiles[0].text)(require, module, module.exports);
const { usePclExportFiles } = module.exports;
const file = (path, category, defaultSelected = true) => ({ path, name: path.split('/').at(-1), isDir: true, category, defaultSelected });

function setup(execute) {
  const props = reactive({ instance: 'one', section: 'export', execute });
  const selected = ref(['mod', 'gameProfile']);
  const format = ref('zip');
  const scope = effectScope();
  const files = scope.run(() => usePclExportFiles(props, selected, format));
  return { props, selected, format, files, scope };
}

test('manual subfolder exclusion and descendant selection retain the intended rules', async () => {
  const state = setup(async (_op, params) => ({ gamePrefix: '.minecraft', entries: params.path ? [
    file('.minecraft/mods', 'mod'), file('.minecraft/config', 'gameProfile'), file('.minecraft/.git', 'gameProfile', false),
    file('.minecraft/saves', 'saves'),
  ] : [file('.minecraft', 'gameProfile'), file('lunaui', 'gameProfile')] }));
  try {
    await state.files.load();
    assert.equal(state.files.checked('.minecraft/.git'), false);
    state.files.toggle('.minecraft/config/dev', false);
    state.files.toggle('.minecraft/config/dev/keep.txt', true);
    assert.equal(state.files.checked('.minecraft/config/normal.txt'), true);
    assert.equal(state.files.checked('.minecraft/config/dev/junk.txt'), false);
    assert.equal(state.files.checked('.minecraft/config/dev/keep.txt'), true);
    assert.equal(state.files.partial('.minecraft/config'), true);
    state.files.category('mod', false);
    assert.equal(state.files.checked('.minecraft/mods/file.jar'), false);
    state.files.category('saves', true);
    assert.equal(state.files.checked('.minecraft/saves/world/level.dat'), true);
    state.format.value = 'modrinth';
    assert.equal(state.files.selection.value.lunaui, true);
    assert.equal('.minecraft' in state.files.selection.value, false);
  } finally { state.scope.stop(); }
});

test('a stale directory response cannot replace the newly selected instance', async () => {
  let release;
  const stale = new Promise(resolve => { release = resolve; });
  const state = setup(async (_op, params) => params.instance === 'one' ? stale : ({ gamePrefix: '.', entries: [file('mods', 'mod')] }));
  try {
    state.props.instance = 'two';
    await nextTick();
    await nextTick();
    assert.equal(state.files.ready.value, true);
    release({ gamePrefix: '.', entries: [file('old', 'gameProfile')] });
    await nextTick();
    assert.deepEqual(Object.keys(state.files.selection.value), ['mods']);
    assert.equal(state.files.loading.value, false);
  } finally { state.scope.stop(); }
});

test('switching away during a read does not leave the next instance permanently loading', async () => {
  let release;
  const state = setup(async () => new Promise(resolve => { release = resolve; }));
  try {
    state.props.section = 'install';
    state.props.instance = 'two';
    await nextTick();
    release({ gamePrefix: '.', entries: [] });
    await nextTick();
    assert.equal(state.files.loading.value, false);
    assert.equal(state.files.ready.value, false);
  } finally { state.scope.stop(); }
});
