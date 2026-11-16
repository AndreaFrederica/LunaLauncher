// SPDX-License-Identifier: MPL-2.0
/** Tauri IPC rejects with { message }, while API operations can use { error }. */
export function formatError(error: unknown): string {
  const seen = new Set<object>();
  function read(value: unknown): string {
    if (typeof value === 'string') return value;
    if (value instanceof Error) return value.message;
    if (value && typeof value === 'object') {
      if (seen.has(value)) return 'Unknown error';
      seen.add(value);
      const object = value as Record<string, unknown>;
      if (object.message) return read(object.message);
      if (object.error) return read(object.error);
      try { return JSON.stringify(value); } catch { return 'Unknown error'; }
    }
    return String(value ?? 'Unknown error');
  }
  return read(error);
}
