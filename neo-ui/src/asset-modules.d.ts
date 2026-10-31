// Vite asset module declarations. Kept in a script-style declaration file so
// they apply globally while src/types.d.ts can augment Vue's module safely.
declare module '*.webp' { const source: string; export default source; }
declare module '*.png' { const source: string; export default source; }
declare module '*.jpg' { const source: string; export default source; }
declare module '*.svg' { const source: string; export default source; }
declare module '*.svg?raw' { const source: string; export default source; }
interface ImportMeta {
  glob: (pattern: string, options?: Record<string, unknown>) => Record<string, any>;
}
