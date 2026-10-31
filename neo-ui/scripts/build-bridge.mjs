// SPDX-License-Identifier: MPL-2.0
// Generate the injected Web UI bridge from the shared TypeScript client.
import { mkdirSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { build } from 'esbuild';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const outfile = resolve(root, 'src-tauri/generated/webui-bridge.js');
mkdirSync(dirname(outfile), { recursive: true });

await build({
  entryPoints: [resolve(root, 'src/client/bridge.ts')],
  outfile,
  bundle: true,
  format: 'iife',
  platform: 'browser',
  target: 'es2020',
  minify: true,
  sourcemap: false,
  legalComments: 'none',
  logLevel: 'info',
  banner: {
    js: '// Generated from src/client/bridge.ts. Do not edit by hand.',
  },
});
