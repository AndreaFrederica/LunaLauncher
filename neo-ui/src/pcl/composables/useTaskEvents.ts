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
      // Hybrid start: one authoritative snapshot avoids missing a task that
      // completed before the event listener was attached.
      try {
        const current: any = await launcher.execute('neo.task.status', { taskId });
        if (current) {
          onUpdate?.(current);
          if (current.finished || ['succeeded', 'failed', 'cancelled', 'interrupted'].includes(String(current.state || '').toLowerCase())) {
            finish(current);
            return;
          }
        }
      } catch {
        // The task may be queued and not visible through the legacy status API.
      }
      unlisten = await launcher.onEvent((event: any) => {
        if (event?.kind !== 'task' || (!event.task && !event.data)) return;
        const snapshot = event.task || event.data;
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


/** Replay the current task list and then receive pushed snapshots. */
export async function subscribeTaskSnapshots(
  onSnapshot: (snapshot: any) => void,
  taskId = '',
): Promise<() => void> {
  const result: any = await launcher.execute('neo.task.subscribe', taskId ? { taskId } : {});
  const subscriptionId = String(result?.subscriptionId || '');
  if (!subscriptionId) return () => {};
  const unlisten = await launcher.onStream((batch: any) => {
    if (!batch || batch.subscriptionId !== subscriptionId) return;
    for (const event of batch.events || []) {
      if (event.kind === 'task.snapshot' && event.task) onSnapshot(event.task);
    }
  });
  return () => {
    unlisten();
    void launcher.execute('event.unsubscribe', { subscriptionId }).catch(() => {});
  };
}
