import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import ts from 'typescript';
import { createI18n } from 'vue-i18n';

const localeSource = readFileSync(new URL('../src/i18n/locales/zh-CN.ts', import.meta.url), 'utf8');
const localeJs = ts.transpileModule(localeSource, { compilerOptions: { module: ts.ModuleKind.ESNext } }).outputText;
const locale = (await import(`data:text/javascript;base64,${Buffer.from(localeJs).toString('base64')}`)).default;
globalThis.__taskProgressI18n = createI18n({ legacy: false, locale: 'zh-CN', messages: { 'zh-CN': locale } });
const source = readFileSync(new URL('../src/themes/pcl/composables/useTaskProgress.ts', import.meta.url), 'utf8')
  .replace("import { i18n } from '../../../i18n';", 'const i18n = globalThis.__taskProgressI18n;');
const compiled = ts.transpileModule(source, { compilerOptions: { module: ts.ModuleKind.ESNext, target: ts.ScriptTarget.ES2022 } }).outputText;
const { flattenTaskItems, taskActiveItems, taskProgressStats, taskItemProgressText, isTaskItemFinished, taskDownloadStats, taskRowsEqual } =
  await import(`data:text/javascript;base64,${Buffer.from(compiled).toString('base64')}`);

test('root count progress agrees with Qt even when visible file bytes have a different ratio', () => {
  const result = taskProgressStats({ state: 'running', progress: 4, total: 10,
    children: [{ id: 'small', state: 'running', progressUnit: 'bytes', current: 20, total: 80 }] });
  assert.equal(result.percent, 40);
  assert.equal(result.byteSummary, '20 B / 80 B');
});

test('truncated rows never replace full queue counts or byte totals', () => {
  const result = taskProgressStats({ state: 'running', progress: 128, total: 130, childrenTruncated: true,
    summary: { total: 130, succeeded: 128, running: 1, waiting: 1, failed: 0,
      hasBytes: true, currentBytes: 20, totalBytes: 80, unknownByteTotals: 1 },
    children: [{ id: 'active', state: 'running', current: 20, total: 80 }] });
  assert.equal(result.itemCount, 130);
  assert.equal(result.completed, 128);
  assert.equal(result.active, 1);
  assert.equal(result.waiting, 1);
  assert.match(result.byteSummary, /总大小待确定/);
});

test('mod download totals exclude metadata requests and survive row truncation', () => {
  const stats = taskDownloadStats({ summary: { total: 155,
    downloads: { total: 154, succeeded: 120, running: 12, waiting: 21, failed: 1 } },
    childrenTruncated: true, children: [{ id: 'file', state: 'running', progressUnit: 'bytes', current: 8, total: -1 }] });
  assert.deepEqual(stats, { total: 154, completed: 120, active: 12, waiting: 21, failed: 1, percent: 78 });
});

test('older snapshots count only file downloads, not wrapper or API tasks', () => {
  const stats = taskDownloadStats({ children: [
    { id: 'metadata', state: 'succeeded', progressUnit: 'progress' },
    { id: 'metadata-bytes', state: 'running', progressUnit: 'bytes', source: { kind: 'metadata' } },
    { id: 'a', state: 'succeeded', progressUnit: 'bytes' },
    { id: 'b', state: 'waiting', progressUnit: 'bytes' },
  ] });
  assert.equal(stats.total, 2);
  assert.equal(stats.completed, 1);
  assert.equal(stats.percent, 50);
});

test('metadata source changes refresh a waiting row after cache fallback', () => {
  const row = { id: 'forge', state: 'running', current: 0, total: -1,
    source: { kind: 'metadata', url: 'https://origin/forge/index.json', requestUrl: 'http://127.0.0.1/cache/forge/index.json', requestStartedAt: 1000 } };
  assert.equal(taskRowsEqual([row], [{ ...row, source: { ...row.source } }]), true);
  assert.equal(taskRowsEqual([row], [{ ...row, source: { ...row.source, requestUrl: row.source.url, requestStartedAt: 5000 } }]), false);
});

test('failure, cancellation and full byte transfer do not imply success', () => {
  for (const state of ['failed', 'cancelled']) {
    const result = taskProgressStats({ state, finished: true, progress: 1, total: 4,
      children: [{ state, current: 1, total: 4 }] });
    assert.equal(result.percent, 25);
    assert.equal(result.completed, 0);
  }
  assert.equal(isTaskItemFinished({ state: 'running', current: 100, total: 100 }), false);
  assert.equal(isTaskItemFinished({ state: 'waiting', running: false }), false);
});

test('nested legacy snapshots deduplicate aliases without losing equally named files', () => {
  const leaves = [{ id: 'a', label: 'same.jar' }, { id: 'b', label: 'same.jar' }];
  const snapshot = { children: [{ id: 'wrapper', children: leaves, steps: leaves }], steps: leaves };
  assert.deepEqual(flattenTaskItems(snapshot).map(row => row.id), ['a', 'b']);
});

test('active rows beat completed and queued rows even after the old display limit', () => {
  const rows = Array.from({ length: 140 }, (_, i) => ({ id: String(i), state: i < 120 ? 'succeeded' : 'waiting' }));
  rows.push({ id: 'active', state: 'running' });
  assert.equal(taskActiveItems(rows, 8)[0].id, 'active');
});

test('progress units never depend on magnitude, speed or translated labels', () => {
  assert.equal(taskItemProgressText({ state: 'running', current: 4, total: 10, transferRate: 5000 }), '40%');
  assert.equal(taskItemProgressText({ state: 'running', current: 4, total: 10, progressUnit: 'bytes' }), '4 B / 10 B');
  assert.match(taskItemProgressText({ state: 'running', current: 4, total: -1, progressUnit: 'bytes' }), /大小未知/);
});

test('completion during initial status read cannot strand the launch dialog', async () => {
  let listener, removed = 0;
  const order = [];
  globalThis.__taskEventsTestApi = {
    async onEvent(callback) { order.push('listen'); listener = callback; return () => { removed++; }; },
    async execute() {
      order.push('read');
      listener({ kind: 'task', task: { taskId: 'launch', state: 'succeeded', finished: true } });
      return { taskId: 'launch', state: 'running' }; // An older in-flight snapshot.
    },
  };
  const eventSource = readFileSync(new URL('../src/themes/pcl/composables/useTaskEvents.ts', import.meta.url), 'utf8')
    .replace(/import \{ launcher \} from '[^']*boot\/launcher-api';/, 'const launcher = globalThis.__taskEventsTestApi;')
    .replace("import { i18n } from '../../../i18n';", 'const i18n = globalThis.__taskProgressI18n;');
  const eventJs = ts.transpileModule(eventSource, { compilerOptions: { module: ts.ModuleKind.ESNext } }).outputText;
  const { waitForTask } = await import(`data:text/javascript;base64,${Buffer.from(eventJs).toString('base64')}`);
  const updates = [];
  const result = await waitForTask('launch', task => updates.push(task.state), 1000);
  assert.deepEqual(order, ['listen', 'read']);
  assert.deepEqual(updates, ['succeeded']);
  assert.equal(result.state, 'succeeded');
  assert.equal(removed, 1);
  delete globalThis.__taskEventsTestApi;
});
