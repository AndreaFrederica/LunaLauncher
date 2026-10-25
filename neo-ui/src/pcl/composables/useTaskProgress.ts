/** Shared presentation of legacy and Neo task snapshots. Units come from the backend. */
export function taskState(item: any): string {
  const state = String(item?.state || '').toLowerCase();
  const aliases: Record<string, string> = { active: 'running', 'in-progress': 'running', in_progress: 'running',
    queued: 'waiting', pending: 'waiting', inactive: 'waiting', created: 'waiting',
    success: 'succeeded', done: 'succeeded', completed: 'succeeded', complete: 'succeeded',
    error: 'failed', canceled: 'cancelled', aborted: 'cancelled' };
  if (state) return aliases[state] || state;
  if (item?.finished) return item.successful === false ? 'failed' : item.successful === true ? 'succeeded' : 'finished';
  return item?.running === true ? 'running' : 'waiting';
}
const number = (value: unknown): number => Number.isFinite(Number(value)) ? Math.max(0, Number(value)) : 0;
export const taskItemCurrent = (item: any): number => number(item?.current ?? item?.progress);
export const taskItemTotal = (item: any): number => number(item?.total ?? item?.totalProgress);
export const isTaskItemFinished = (item: any): boolean => Boolean(item?.finished) ||
  ['succeeded', 'failed', 'cancelled', 'interrupted', 'finished', 'skipped'].includes(taskState(item));
export const isTaskItemActive = (item: any): boolean => !isTaskItemFinished(item) && taskState(item) === 'running';
export const isTaskItemWaiting = (item: any): boolean => !isTaskItemFinished(item) && taskState(item) === 'waiting';
export const taskItemLabel = (item: any): string => String(item?.source?.fileName || item?.label || item?.name || item?.status || item?.details || item?.type || '任务');
export const taskItemIsBytes = (item: any): boolean => item?.progressUnit === 'bytes' || item?.source?.kind === 'download';

/** Prefer children to duplicated step summaries, and never deduplicate by a display name. */
export function flattenTaskItems(input: any): any[] {
  const rows = new Map<string, any>();
  const children = (item: any): any[] => Array.isArray(item?.children) && item.children.length
    ? item.children : Array.isArray(item?.steps) ? item.steps : [];
  const visit = (item: any, path: string) => {
    if (!item || typeof item !== 'object') return;
    const nested = children(item);
    if (nested.length) { nested.forEach((child, i) => visit(child, `${path}.${i}`)); return; }
    const id = String(item.id || item.taskId || path);
    rows.set(id, { ...item, id });
  };
  (Array.isArray(input) ? input : children(input)).forEach((item, i) => visit(item, String(i)));
  return [...rows.values()];
}
export function taskActiveItems(input: any, limit = 8): any[] {
  const rows = flattenTaskItems(input);
  return [...rows.filter(isTaskItemActive), ...rows.filter(row => taskState(row) === 'failed'),
    ...rows.filter(isTaskItemWaiting)].slice(0, limit);
}
export function taskItemPercent(item: any): number | null {
  if (taskState(item) === 'succeeded') return 100;
  const total = taskItemTotal(item);
  return total > 0 ? Math.min(100, taskItemCurrent(item) / total * 100) : null;
}
export function formatTaskSize(value: unknown): string {
  let bytes = number(value), index = 0;
  const units = ['B', 'KiB', 'MiB', 'GiB', 'TiB'];
  while (bytes >= 1024 && index < units.length - 1) { bytes /= 1024; index++; }
  return `${bytes.toFixed(index ? 1 : 0)} ${units[index]}`;
}
export const formatTaskRate = (value: unknown): string => number(value) > 0 ? `${formatTaskSize(value)}/s` : '';
const states: Record<string, string> = { succeeded: '已完成', failed: '失败', cancelled: '已取消', interrupted: '已中断', waiting: '等待中', running: '进行中', finished: '已结束' };
export function taskItemProgressText(item: any): string {
  const state = taskState(item);
  if (state !== 'running') return states[state] || state;
  const total = taskItemTotal(item), current = taskItemCurrent(item);
  if (taskItemIsBytes(item)) return total > 0 ? `${formatTaskSize(current)} / ${formatTaskSize(total)}` : `${formatTaskSize(current)} / 大小未知`;
  return total > 0 ? `${Math.round(Math.min(1, current / total) * 100)}%` : '进行中';
}
export function taskProgressStats(task: any, inputRows?: any[]) {
  const rows = inputRows || flattenTaskItems(task);
  const authoritative = task?.summary && typeof task.summary.total === 'number' ? task.summary : null;
  const completed = authoritative ? number(authoritative.succeeded) : rows.filter(row => taskState(row) === 'succeeded').length;
  const failed = authoritative ? number(authoritative.failed) : rows.filter(row => taskState(row) === 'failed').length;
  const cancelled = authoritative ? number(authoritative.cancelled) : rows.filter(row => ['cancelled', 'interrupted'].includes(taskState(row))).length;
  const active = authoritative ? number(authoritative.running) : rows.filter(isTaskItemActive).length;
  const waiting = authoritative ? number(authoritative.waiting) : rows.filter(isTaskItemWaiting).length;
  const itemCount = authoritative ? number(authoritative.total) : rows.length;
  const byteRows = rows.filter(taskItemIsBytes);
  const currentBytes = authoritative ? number(authoritative.currentBytes) : byteRows.reduce((sum, row) => sum + taskItemCurrent(row), 0);
  const totalBytes = authoritative ? number(authoritative.totalBytes) : byteRows.reduce((sum, row) => sum + taskItemTotal(row), 0);
  const unknownByteTotals = authoritative ? number(authoritative.unknownByteTotals) : byteRows.filter(row => !taskItemTotal(row)).length;
  const hasBytes = authoritative ? Boolean(authoritative.hasBytes) : byteRows.length > 0;
  // Match Qt ProgressDialog: the root's progress describes the whole current
  // stage. A bounded visible list is never a replacement denominator.
  const total = taskItemTotal(task), current = taskItemCurrent(task);
  const percent = taskState(task) === 'succeeded' ? 100 : total > 0 ? Math.min(100, current / total * 100) : null;
  const byteSummary = hasBytes ? `${formatTaskSize(currentBytes)} / ${unknownByteTotals ? '总大小待确定' : formatTaskSize(totalBytes)}` : '';
  let summary = itemCount ? `已完成 ${completed} / ${itemCount}${authoritative || !task?.childrenTruncated ? '' : '（已显示项目）'}` : '等待任务进度…';
  if (failed) summary += ` · 失败 ${failed}`;
  if (cancelled) summary += ` · 已取消 ${cancelled}`;
  return { percent, current, total, completed, failed, cancelled, active, waiting, itemCount,
    currentBytes, totalBytes, unknownByteTotals, hasBytes, byteSummary, summary };
}
