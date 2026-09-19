// SPDX-License-Identifier: GPL-3.0-only
// Node 22+: node --experimental-strip-types --test tests/launcher_api_client.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { LauncherClient } from '../examples/neo-ui/launcher-api.ts';

test('responses may finish out of order and interaction events preserve request IDs', async () => {
  const writes = [], events = [];
  const client = new LauncherClient(async line => { writes.push(JSON.parse(line)); }, event => events.push(event));
  const login = client.begin('account.login', { type: 'offline' });
  const tasks = client.begin('task.list');
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(writes.length, 2);
  client.acceptLine(JSON.stringify({ jsonrpc: '2.0', id: tasks.requestId, result: { ok: true, data: [] } }));
  assert.deepEqual((await tasks.result).data, []);
  client.acceptLine(JSON.stringify({ jsonrpc: '2.0', method: 'launcher/event', params: {
    requestId: login.requestId, kind: 'input', interactionId: 'prompt', secret: false, prompt: 'Username',
  } }));
  assert.equal(events[0].requestId, login.requestId);
  const reply = client.reply(events[0].interactionId, 'Neo');
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(writes[2].params.value, 'Neo');
  client.acceptLine(JSON.stringify({ jsonrpc: '2.0', id: writes[2].id, result: { accepted: true } }));
  await reply;
  client.acceptLine(JSON.stringify({ jsonrpc: '2.0', id: login.requestId, result: { ok: true, data: { profileName: 'Neo' } } }));
  assert.equal((await login.result).data.profileName, 'Neo');
});

test('cancellation targets its operation and process exit rejects pending requests', async () => {
  const writes = [];
  const client = new LauncherClient(async line => { writes.push(JSON.parse(line)); });
  const operation = client.begin('resource.install-version');
  await operation.cancel();
  assert.equal(writes[1].method, 'notifications/cancelled');
  assert.equal(writes[1].params.requestId, operation.requestId);
  const rejected = assert.rejects(operation.result, /exited/);
  client.close();
  await rejected;
  await assert.rejects(client.catalog(), /exited/);
});

test('JSON-RPC errors reject while domain failures remain typed API results', async () => {
  const client = new LauncherClient(async () => {});
  const request = client.begin('instance.create');
  const rejected = assert.rejects(request.result, error => error.code === -32000);
  client.acceptLine(JSON.stringify({ jsonrpc: '2.0', id: request.requestId, error: { code: -32000, message: 'busy' } }));
  await rejected;
  const failure = client.begin('server.yaml.read');
  client.acceptLine(JSON.stringify({ jsonrpc: '2.0', id: failure.requestId,
    result: { ok: false, apiVersion: 1, operation: 'server.yaml.read', error: 'not found', exitCode: 2 } }));
  assert.equal((await failure.result).ok, false);
});

test('a broken stdin rejects every pending request', async () => {
  const client = new LauncherClient(async () => { throw new Error('broken pipe'); });
  const requests = [client.begin('task.list').result, client.catalog()];
  const results = await Promise.allSettled(requests);
  assert.ok(results.every(result => result.status === 'rejected' && result.reason.message === 'broken pipe'));
});

test('stream batches preserve cursors, overflow and base64 bytes independently of responses', async () => {
  const batches = [];
  const client = new LauncherClient(async () => {}, undefined, batch => batches.push(batch));
  const request = client.begin('instance.console.subscribe', { instance: 'server' });
  const batch = { subscriptionId: 'sub', nextCursor: 514, dropped: 2, hasMore: true,
    events: [{ subscriptionId: 'sub', instance: 'server', sequence: 514,
      kind: 'console.data', encoding: 'base64', data: Buffer.from('Neo 测试\u001b[0m').toString('base64') }] };
  client.acceptLine(JSON.stringify({ jsonrpc: '2.0', method: 'launcher/stream', params: batch }));
  assert.deepEqual(batches, [batch]);
  assert.equal(Buffer.from(batches[0].events[0].data, 'base64').toString('utf8'), 'Neo 测试\u001b[0m');
  client.acceptLine(JSON.stringify({ jsonrpc: '2.0', id: request.requestId,
    result: { ok: true, data: { subscriptionId: 'sub' } } }));
  assert.equal((await request.result).data.subscriptionId, 'sub');
});
