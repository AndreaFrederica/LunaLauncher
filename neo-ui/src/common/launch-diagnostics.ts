// SPDX-License-Identifier: MPL-2.0
export interface LaunchCompletion {
  instance: string; name: string; launchId: string; startedAt: string; finishedAt?: string;
  succeeded: boolean; aborted: boolean; gameStarted: boolean; exitCode?: number;
  reason: string; console: string; consoleTruncated?: boolean;
}

// Receive only structured lifecycle events, never infer crashes from WARNs,
// stale crash reports, localized log text, or a running -> stopped transition.
export class LaunchFailureQueue {
  private seen = new Set<string>();
  accept(event: LaunchCompletion): boolean {
    if (!event?.launchId || !event.instance || event.succeeded !== false || event.aborted !== false) return false;
    if (this.seen.has(event.launchId)) return false;
    this.seen.add(event.launchId);
    if (this.seen.size > 256) this.seen.delete(this.seen.values().next().value!);
    return true;
  }
}

export function isGameCrash(event: LaunchCompletion): boolean {
  return event.gameStarted && (event.exitCode === undefined || event.exitCode !== 0);
}
