import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import ts from 'typescript';

const source = readFileSync(new URL('../src/common/cacheStatus.ts', import.meta.url), 'utf8');
const js = ts.transpileModule(source, { compilerOptions: { module: ts.ModuleKind.ESNext, target: ts.ScriptTarget.ES2022 } }).outputText;
const { readCacheStatus, formatCacheSize } = await import(`data:text/javascript;base64,${Buffer.from(js).toString('base64')}`);

test('reads the actual HTTP and embedded-resource API schemas', () => {
  const result = readCacheStatus({ entries: 57, bytes: 4886289, indexPath: 'profile/metacache' },
    { stats: { entries: 29, bytes: 566181 }, dataDirectory: 'profile/yggdrasil' });
  assert.deepEqual(result, { httpEntries: 57, httpBytes: 4886289, resourceEntries: 29, resourceBytes: 566181,
    artifactEntries: undefined, artifactBytes: undefined, httpIndexPath: 'profile/metacache', resourceDirectory: 'profile/yggdrasil' });
  assert.equal(formatCacheSize(result.httpBytes), '4.66 MiB');
  assert.equal(formatCacheSize(result.resourceBytes), '552.91 KiB');
});

test('missing or failed statistics stay unknown and do not masquerade as zero', () => {
  const result = readCacheStatus(undefined, {});
  assert.equal(result.httpEntries, undefined);
  assert.equal(result.resourceBytes, undefined);
  assert.equal(formatCacheSize(result.httpBytes), '—');
});

test('a genuinely empty cache displays zero', () => {
  const result = readCacheStatus({ entries: 0, bytes: 0 }, { stats: { entries: 0, bytes: 0 } });
  assert.equal(result.httpEntries, 0);
  assert.equal(result.resourceEntries, 0);
  assert.equal(formatCacheSize(result.resourceBytes), '0 B');
});

test('unavailable and invalid fields do not contaminate the other cache result', () => {
  const result = readCacheStatus({ entries: -1, bytes: NaN }, { stats: { entries: 2, bytes: 2048 } });
  assert.equal(result.httpEntries, undefined);
  assert.equal(result.httpBytes, undefined);
  assert.equal(result.resourceEntries, 2);
  assert.equal(formatCacheSize(result.resourceBytes), '2 KiB');
});

test('download copies are reported separately from metadata and resource totals', () => {
  const result = readCacheStatus({ entries: 3, bytes: 100, artifacts: { entries: 2042, bytes: 90000000 } }, { stats: { entries: 25, bytes: 6000 } });
  assert.equal(result.httpEntries, 3);
  assert.equal(result.artifactEntries, 2042);
  assert.equal(result.artifactBytes, 90000000);
  assert.equal(result.resourceEntries, 25);
});
