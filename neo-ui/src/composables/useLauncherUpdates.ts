import { onMounted, onUnmounted } from 'vue';

/** Check only; installing always requires the user's action on the notification. */
export function useLauncherUpdates(options: {
  canCheck: () => boolean;
  execute: (operation: string) => Promise<any>;
  available: (release: any) => void;
}) {
  let timer: ReturnType<typeof setInterval> | undefined;
  let checking = false;
  let checkedOnLaunch = false;
  let nextAttempt = 0;
  let notified = '';
  let disposed = false;
  async function poll() {
    if (disposed || checking || !options.canCheck() || Date.now() < nextAttempt) return;
    checking = true;
    nextAttempt = Date.now() + 60_000;
    try {
      const status = await options.execute('launcher.update.status');
      if (!status?.supported || !status.automatic) return;
      const interval = Math.max(0, Number(status.intervalSeconds ?? 86400)) * 1000;
      const last = Date.parse(status.lastCheck || '') || 0;
      if (interval === 0 ? checkedOnLaunch : last > 0 && Date.now() - last < interval) return;
      const release = await options.execute('launcher.update.check');
      checkedOnLaunch = true;
      if (!disposed && release?.available && release.versionTag !== notified) {
        notified = release.versionTag;
        options.available(release);
      }
    } catch {
      // Background network failure should not interrupt game/instance workflows.
      // Manual checks still display their error; retry with a bounded interval.
      nextAttempt = Date.now() + 300_000;
    } finally { checking = false; }
  }
  onMounted(() => { timer = setInterval(() => { void poll(); }, 15_000); });
  onUnmounted(() => { disposed = true; if (timer) clearInterval(timer); });
}
