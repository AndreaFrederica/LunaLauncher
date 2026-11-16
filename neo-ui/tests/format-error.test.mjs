import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import ts from 'typescript';
const source = readFileSync(new URL('../src/common/formatError.ts', import.meta.url), 'utf8');
const js = ts.transpileModule(source, { compilerOptions: { module: ts.ModuleKind.ESNext } }).outputText;
const { formatError } = await import(`data:text/javascript;base64,${Buffer.from(js).toString('base64')}`);
test('Tauri backend errors retain their actual message', () => {
  assert.equal(formatError({ message: 'Backend exited: HTTP 403' }), 'Backend exited: HTTP 403');
  assert.equal(formatError({ error: { message: 'Download failed' } }), 'Download failed');
  assert.equal(formatError(new Error('failure')), 'failure');
  assert.equal(formatError('plain error'), 'plain error');
});
test('unknown and cyclic errors cannot hide the original failure behind a formatter exception', () => {
  const cycle = {}; cycle.message = cycle;
  assert.equal(formatError(cycle), 'Unknown error');
  assert.equal(formatError({ code: 403 }), '{"code":403}');
});
