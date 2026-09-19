// SPDX-License-Identifier: GPL-3.0-only
// Framework-independent client for an installed `lunalauncher-cli --mcp` sidecar.
// Connect write() to the child process stdin; pass each stdout line to acceptLine().

export type Parameters = Record<string, unknown>;
export type ApiResult<T = unknown> =
  | { ok: true; apiVersion: number; operation: string; data: T; exitCode?: number }
  | { ok: false; apiVersion: number; operation: string; error: string; exitCode: number };
export type ApiEvent = {
  requestId: string | number;
  kind: "status" | "device_code" | "task" | "input";
  message?: string;
  data?: Record<string, unknown>;
  url?: string;
  code?: string;
  expiresIn?: number;
  interactionId?: string;
  prompt?: string;
  secret?: boolean;
  choices?: unknown[];
};
export type Operation = {
  name: string;
  description: string;
  apiVersion: number;
  inputSchema?: Record<string, unknown>;
  destructive: boolean;
  capability?: string;
};

export class LauncherClient {
  private nextId = 0;
  private closed: Error | undefined;
  private writes = Promise.resolve();
  private readonly pending = new Map<number, {
    resolve: (value: unknown) => void;
    reject: (error: Error) => void;
  }>();
  private readonly write: (line: string) => Promise<void>;
  private readonly onEvent: (event: ApiEvent) => void;

  constructor(write: (line: string) => Promise<void>, onEvent: (event: ApiEvent) => void = () => {}) {
    this.write = write;
    this.onEvent = onEvent;
  }

  // Tauri shell stdout callbacks already deliver complete lines. For a raw Rust
  // pipe, decode UTF-8 and buffer through each newline before calling this.
  acceptLine(line: string): void {
    const message = JSON.parse(line);
    if (message.jsonrpc !== "2.0") throw new Error("Invalid launcher JSON-RPC message");
    if (message.method === "launcher/event") {
      this.onEvent(message.params as ApiEvent);
      return;
    }
    const request = this.pending.get(message.id);
    if (!request) return;
    this.pending.delete(message.id);
    if (message.error) {
      const error = new Error(message.error.message);
      Object.assign(error, { code: message.error.code });
      request.reject(error);
    } else {
      request.resolve(message.result);
    }
  }

  // Call on child exit or a broken pipe; no promises should survive the process.
  close(error = new Error("Launcher sidecar exited")): void {
    this.closed = error;
    for (const request of this.pending.values()) request.reject(error);
    this.pending.clear();
  }

  private send(message: Record<string, unknown>): Promise<void> {
    const line = JSON.stringify({ jsonrpc: "2.0", ...message }) + "\n";
    this.writes = this.writes.then(async () => {
      if (this.closed) throw this.closed;
      await this.write(line);
    });
    return this.writes.catch((error: Error) => { this.close(error); throw error; });
  }

  private request<T>(method: string, params: Parameters = {}): { id: number; result: Promise<T> } {
    const id = ++this.nextId;
    const result = new Promise<T>((resolve, reject) => {
      if (this.closed) { reject(this.closed); return; }
      this.pending.set(id, { resolve: value => resolve(value as T), reject });
      void this.send({ id, method, params }).catch(reject);
    });
    return { id, result };
  }

  catalog(): Promise<{ apiVersion: number; operations: Operation[] }> {
    return this.request<{ apiVersion: number; operations: Operation[] }>("launcher/catalog").result;
  }

  // Mutations are serialized by the backend. Await the current operation before
  // beginning another; task.list/status/cancel and interaction replies may overlap.
  begin<T = unknown>(operation: string, parameters: Parameters = {}) {
    const request = this.request<ApiResult<T>>("launcher/execute", { operation, parameters });
    return {
      requestId: request.id,
      result: request.result,
      cancel: () => this.send({ method: "notifications/cancelled", params: { requestId: request.id } }),
    };
  }

  execute<T = unknown>(operation: string, parameters: Parameters = {}): Promise<ApiResult<T>> {
    return this.begin<T>(operation, parameters).result;
  }

  reply(interactionId: string, value: string | number | null): Promise<{ accepted: boolean }> {
    return this.request<{ accepted: boolean }>("launcher/respond",
      value === null ? { interactionId, cancel: true } : { interactionId, value }).result;
  }
}
