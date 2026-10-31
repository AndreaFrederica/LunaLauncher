// SPDX-License-Identifier: MPL-2.0
import assert from 'node:assert/strict';
import { test } from 'node:test';
import vm from 'node:vm';
import { build } from 'esbuild';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));

async function loadBridge() {
  const result = await build({
    entryPoints: [resolve(root, 'src/client/bridge.ts')],
    bundle: true,
    write: false,
    format: 'iife',
    platform: 'browser',
    target: 'es2020',
    minify: false,
    logLevel: 'silent',
  });
  const calls = [];
  const listeners = new Map();
  const invoke = async (command, args) => {
    calls.push({ command, args });
    if (command === 'launcher_request') {
      if (args.method === 'launcher/catalog') return { apiVersion: 1, operations: [] };
      if (args.method === 'launcher/respond') return { accepted: true };
      if (args.method === 'launcher/execute') {
        const operation = args.parameters.operation;
        if (operation === 'instance.list') return { ok: true, data: [{ id: 'i1', name: 'Instance 1' }] };
        if (operation === 'account.list') return { ok: true, data: [{ id: 'a1', name: 'Account 1', default: true }] };
        if (operation === 'instance.icons') return { ok: true, data: { icons: [{ id: 'i1', dataUrl: 'data:image/png;base64,AA==' }] } };
        if (operation === 'instance.import') { await delay(60); return { ok: true, data: {} }; }
        return { ok: true, data: {} };
      }
    }
    return {};
  };
  const context = {
    window: {
      __TAURI__: {
        core: { invoke },
        event: {
          listen: async (event, handler) => {
            listeners.set(event, handler);
            return () => listeners.delete(event);
          },
        },
      },
    },
    document: { addEventListener: () => {} },
    console,
    setTimeout,
    clearTimeout,
    queueMicrotask,
    performance,
    Date,
    JSON,
    Map,
    Set,
    Promise,
    Object,
    Array,
    String,
    Number,
    Math,
  };
  context.globalThis = context;
  vm.createContext(context);
  vm.runInContext(result.outputFiles[0].text, context);
  return { luna: context.window.luna, calls, listeners };
}

function count(calls, operation) {
  return calls.filter(call => call.command === 'launcher_request'
    && call.args?.method === 'launcher/execute'
    && call.args?.parameters?.operation === operation).length;
}

test('generated bridge keeps the documented API version', async () => {
  const { luna } = await loadBridge();
  assert.equal(luna.apiVersion, 1);
});

test('neo.task.* controls bypass the normal request queue', async () => {
  const { luna } = await loadBridge();
  const order = [];
  const slow = luna.execute('instance.import', {}).then(() => order.push('import'));
  const cancel = luna.execute('neo.task.cancel', { taskId: 'task-1' }).then(() => order.push('cancel'));
  await cancel;
  assert.deepEqual(order, ['cancel']);
  await slow;
  assert.deepEqual(order, ['cancel', 'import']);
});

test('onExit receives the backend payload rather than the Tauri event wrapper', async () => {
  const { luna, listeners } = await loadBridge();
  let reason;
  const unlisten = await luna.onExit(value => { reason = value; });
  listeners.get('launcher-exit')({ payload: { message: 'backend gone' } });
  assert.equal(reason.message, 'backend gone');
  unlisten();
});

test('runtime refreshes instances, accounts and selection from one shared model', async () => {
  const { luna } = await loadBridge();
  await luna.runtime.start();
  const snapshot = luna.runtime.getSnapshot();
  assert.equal(snapshot.instances[0].id, 'i1');
  assert.equal(snapshot.accounts.length, 1);
  assert.equal(snapshot.selectedAccount.id, 'a1');
  assert.equal(snapshot.ready, true);
});

test('read-only requests are cached between calls', async () => {
  const { luna, calls } = await loadBridge();
  await luna.runtime.start();
  const before = count(calls, 'instance.list');
  await luna.execute('instance.list');
  await luna.execute('instance.list');
  assert.equal(count(calls, 'instance.list'), before);
});
