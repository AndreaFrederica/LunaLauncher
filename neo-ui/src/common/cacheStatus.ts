// SPDX-License-Identifier: MPL-2.0
/** Disk totals use persisted entries; unavailable responses must stay unknown. */
export function readCacheStatus(http: any, resources: any) {
  const number = (value: unknown): number | undefined =>
    typeof value === 'number' && Number.isFinite(value) && value >= 0 ? value : undefined;
  return {
    httpEntries: number(http?.entries),
    httpBytes: number(http?.bytes),
    artifactEntries: number(http?.artifacts?.entries),
    artifactBytes: number(http?.artifacts?.bytes),
    resourceEntries: number(resources?.stats?.entries),
    resourceBytes: number(resources?.stats?.bytes),
    httpIndexPath: String(http?.indexPath || ''),
    resourceDirectory: String(resources?.dataDirectory || ''),
  };
}

export function formatCacheSize(bytes: number | undefined): string {
  if (bytes === undefined) return '—';
  const units = ['B', 'KiB', 'MiB', 'GiB', 'TiB'];
  const unit = bytes > 0 ? Math.max(0, Math.min(units.length - 1, Math.floor(Math.log(bytes) / Math.log(1024)))) : 0;
  return `${Number((bytes / 1024 ** unit).toFixed(unit ? 2 : 0))} ${units[unit]}`;
}
