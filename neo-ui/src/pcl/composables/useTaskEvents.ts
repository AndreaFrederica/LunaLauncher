import { launcher } from '../../boot/launcher-api';

/**
 * Wait for a task to finish using the launcher event stream instead of
 * repeatedly asking task.status. The stream carries the same snapshot shape.
 */
export async function waitForTask(
  taskId: string,
  onUpdate?: (snapshot: any) => void,
  timeoutMs = 20 * 60 * 1000,
): Promise<any> {
  if (!taskId) throw new Error('Task ID is empty.');
  let unlisten: (() => void) | undefined;
  let timer: ReturnType<typeof setTimeout> | undefined;
  return new Promise(async (resolve, reject) => {
    let settled = false;
    const finish = (snapshot: any, error?: Error) => {
      if (settled) return;
      settled = true;
      unlisten?.();
      if (timer) clearTimeout(timer);
      if (error) reject(error);
      else resolve(snapshot);
    };
    try {
      unlisten = await launcher.onEvent((event: any) => {
        if (event?.kind !== 'task' || !event.data) return;
        const snapshot = event.data;
        if (String(snapshot.taskId || '') !== taskId) return;
        onUpdate?.(snapshot);
        if (snapshot.finished || ['succeeded', 'failed', 'cancelled', 'interrupted'].includes(String(snapshot.state || '').toLowerCase())) {
          finish(snapshot);
        }
      });
    } catch (error) {
      finish(null, error instanceof Error ? error : new Error(String(error)));
      return;
    }
    timer = setTimeout(() => finish(null, new Error('等待任务超时。')), timeoutMs);
  });
}
