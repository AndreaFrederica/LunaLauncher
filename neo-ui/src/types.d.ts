// SPDX-License-Identifier: MPL-2.0
declare module '*.vue';
interface ImportMeta {
  glob: (pattern: string, options?: Record<string, unknown>) => Record<string, any>;
}
declare module '*.webp' { const source: string; export default source; }
declare module '*.png' { const source: string; export default source; }
declare module '*.svg' { const source: string; export default source; }
declare module '*.svg?raw' { const source: string; export default source; }
