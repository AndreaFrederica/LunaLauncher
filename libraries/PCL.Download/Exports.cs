using System.Collections.Concurrent;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;

namespace PCL.Download;

/// <summary>
/// Native C API exports for the PCL.Download NativeAOT shared library.
/// All exported methods are synchronous, use only blittable parameter types,
/// and never let exceptions escape into native code.
/// </summary>
public static class Exports
{
    private sealed class TaskHandle
    {
        public required LoaderDownload Loader { get; init; }
        public ManualResetEventSlim Done { get; } = new(false);
        public object EventSync { get; } = new();
        public string? ErrorMessage;
        public bool TerminalEventSent;
    }

    private static readonly ConcurrentDictionary<int, TaskHandle> Tasks = new();
    private static int _initialized;

    // ---- 事件推送 ----
    // eventType: 0=progress, 1=finished, 2=failed, 3=aborted
    // 回调签名 (Cdecl): void (*)(int taskId, int eventType, long downloaded, long total, long speed, int threads)
    public const int EventProgress = 0;
    public const int EventFinished = 1;
    public const int EventFailed = 2;
    public const int EventAborted = 3;

    private static IntPtr _eventCallback;

    /// <summary>
    /// Register a global event callback (native function pointer, Cdecl).
    /// The callback is invoked from .NET threadpool threads — the host must
    /// marshal to its own UI thread. Pass IntPtr.Zero to unregister.
    /// </summary>
    [UnmanagedCallersOnly(EntryPoint = "pcl_download_set_event_callback")]
    public static void SetEventCallback(IntPtr callback)
    {
        _eventCallback = callback;
    }

    private static unsafe void RaiseEvent(int taskId, int eventType, long downloaded, long total, long speed, int threads)
    {
        var callback = _eventCallback;
        if (callback == IntPtr.Zero)
            return;
        var fn = (delegate* unmanaged[Cdecl]<int, int, long, long, long, int, void>)callback;
        try { fn(taskId, eventType, downloaded, total, speed, threads); }
        catch { /* host callback must never break the download */ }
    }

    // ========== 初始化/销毁 ==========

    [UnmanagedCallersOnly(EntryPoint = "pcl_download_init")]
    public static int Init()
    {
        try
        {
            if (Interlocked.Exchange(ref _initialized, 1) == 1)
                return 0;
            _ = NetManager.Instance;
            return 0;
        }
        catch
        {
            return -1;
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "pcl_download_shutdown")]
    public static void Shutdown()
    {
        try
        {
            _eventCallback = IntPtr.Zero;
            foreach (var pair in Tasks)
            {
                try { pair.Value.Loader.Abort(); } catch { /* best effort */ }
                pair.Value.Done.Dispose();
            }
            Tasks.Clear();
            Interlocked.Exchange(ref _initialized, 0);
        }
        catch { /* never throw across the ABI boundary */ }
    }

    // ========== 单文件下载 ==========

    /// <summary>
    /// Start a single-file download.
    /// urlsJson: JSON array of URL strings (tried in order, failover).
    /// localPath: absolute target file path (UTF-8).
    /// hash: expected MD5/SHA1/SHA256 hex string, or null/empty to skip hash check.
    /// expectedSize: expected file size in bytes, or -1 if unknown.
    /// Returns task id (&gt; 0) or -1 on immediate error.
    /// </summary>
    [UnmanagedCallersOnly(EntryPoint = "pcl_download_file")]
    public static int DownloadFile(IntPtr urlsJsonPtr, IntPtr localPathPtr, IntPtr hashPtr, long expectedSize)
    {
        try
        {
            var urls = ParseStringArray(Marshal.PtrToStringUTF8(urlsJsonPtr));
            var localPath = Marshal.PtrToStringUTF8(localPathPtr) ?? "";
            var hash = Marshal.PtrToStringUTF8(hashPtr);
            if (urls.Length == 0 || localPath.Length == 0)
                return -1;

            FileChecker? checker = !string.IsNullOrEmpty(hash) || expectedSize >= 0
                ? new FileChecker(actualSize: expectedSize, hash: string.IsNullOrEmpty(hash) ? null : hash)
                : null;
            var file = new DownloadFile(urls, localPath, checker);
            var loader = new LoaderDownload($"Download {file.LocalName}", new List<DownloadFile> { file });
            var id = Register(loader);
            loader.Start();
            return id;
        }
        catch (Exception ex)
        {
            LogHelper.Log(ex, "[Export] pcl_download_file failed");
            return -1;
        }
    }

    /// <summary>Block until the task completes. Returns 0=success, 1=failed, 2=cancelled.</summary>
    [UnmanagedCallersOnly(EntryPoint = "pcl_download_file_wait")]
    public static int DownloadFileWait(int taskId) => WaitForTask(taskId);

    [UnmanagedCallersOnly(EntryPoint = "pcl_download_file_abort")]
    public static void DownloadFileAbort(int taskId) => AbortTask(taskId);

    [UnmanagedCallersOnly(EntryPoint = "pcl_download_release")]
    public static void ReleaseTask(int taskId)
    {
        try
        {
            if (Tasks.TryRemove(taskId, out var handle))
                handle.Done.Dispose();
        }
        catch { /* best effort */ }
    }

    // ========== 批量下载 ==========

    /// <summary>
    /// Start a batch download. filesJson: JSON array of
    /// {"urls": ["..."], "localPath": "...", "hash": "...", "expectedSize": -1}.
    /// Returns batch id (&gt; 0) or -1 on immediate error.
    /// </summary>
    [UnmanagedCallersOnly(EntryPoint = "pcl_download_batch_start")]
    public static int BatchStart(IntPtr filesJsonPtr)
    {
        try
        {
            var files = ParseBatchFileList(Marshal.PtrToStringUTF8(filesJsonPtr));
            if (files.Count == 0)
                return -1;
            var loader = new LoaderDownload($"Batch download ({files.Count} files)", files);
            var id = Register(loader);
            loader.Start();
            return id;
        }
        catch (Exception ex)
        {
            LogHelper.Log(ex, "[Export] pcl_download_batch_start failed");
            return -1;
        }
    }

    /// <summary>0=running, 1=finished, 2=failed, 3=aborted, -1=unknown task.</summary>
    [UnmanagedCallersOnly(EntryPoint = "pcl_download_batch_get_state")]
    public static int BatchGetState(int batchId) => GetTaskState(batchId);

    /// <summary>Aggregate progress, 0.0 ~ 1.0.</summary>
    [UnmanagedCallersOnly(EntryPoint = "pcl_download_batch_get_progress")]
    public static double BatchGetProgress(int batchId) => GetTaskProgress(batchId);

    /// <summary>
    /// Detailed per-file progress as a JSON string.
    /// The caller must free the returned pointer with pcl_free_string.
    /// </summary>
    [UnmanagedCallersOnly(EntryPoint = "pcl_download_batch_get_progress_detail")]
    public static IntPtr BatchGetProgressDetail(int batchId) => GetTaskProgressDetail(batchId);

    /// <summary>Block until the batch completes. Returns 0=success, 1=failed, 2=cancelled.</summary>
    [UnmanagedCallersOnly(EntryPoint = "pcl_download_batch_wait")]
    public static int BatchWait(int batchId) => WaitForTask(batchId);

    [UnmanagedCallersOnly(EntryPoint = "pcl_download_batch_abort")]
    public static void BatchAbort(int batchId) => AbortTask(batchId);

    // ========== 单文件任务状态（与批量共用同一注册表） ==========

    /// <summary>0=running, 1=finished, 2=failed, 3=aborted, -1=unknown task.</summary>
    [UnmanagedCallersOnly(EntryPoint = "pcl_download_get_state")]
    public static int GetState(int taskId) => GetTaskState(taskId);

    /// <summary>Aggregate progress, 0.0 ~ 1.0.</summary>
    [UnmanagedCallersOnly(EntryPoint = "pcl_download_get_progress")]
    public static double GetProgress(int taskId) => GetTaskProgress(taskId);

    /// <summary>
    /// Detailed per-file progress as a JSON string.
    /// The caller must free the returned pointer with pcl_free_string.
    /// </summary>
    [UnmanagedCallersOnly(EntryPoint = "pcl_download_get_progress_detail")]
    public static IntPtr GetProgressDetail(int taskId) => GetTaskProgressDetail(taskId);

    // ========== 全局状态 ==========

    /// <summary>Global download speed in bytes/sec across all active downloads.</summary>
    [UnmanagedCallersOnly(EntryPoint = "pcl_download_get_speed")]
    public static long GetSpeed()
    {
        try { return NetManager.Instance.Speed; }
        catch { return 0; }
    }

    /// <summary>Total number of active download threads/chunks.</summary>
    [UnmanagedCallersOnly(EntryPoint = "pcl_download_get_active_threads")]
    public static int GetActiveThreads()
    {
        try { return NetManager.Instance.ThreadCount; }
        catch { return 0; }
    }

    // ========== 错误处理 ==========

    /// <summary>
    /// Get the error message of a task as a UTF-8 string.
    /// The caller must free the returned pointer with pcl_free_string.
    /// Returns null if there is no error or the task is unknown.
    /// </summary>
    [UnmanagedCallersOnly(EntryPoint = "pcl_download_get_error")]
    public static IntPtr GetError(int taskId)
    {
        try
        {
            if (!Tasks.TryGetValue(taskId, out var handle))
                return IntPtr.Zero;

            var sb = new StringBuilder();
            if (!string.IsNullOrEmpty(handle.ErrorMessage))
                sb.AppendLine(handle.ErrorMessage);
            foreach (var file in handle.Loader.Files)
            {
                foreach (var err in file.Errors)
                    sb.Append('[').Append(file.LocalName).Append("] ").AppendLine(err.Message);
            }
            var msg = sb.ToString().Trim();
            return msg.Length == 0 ? IntPtr.Zero : StringToPtr(msg);
        }
        catch
        {
            return IntPtr.Zero;
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "pcl_free_string")]
    public static void FreeString(IntPtr ptr)
    {
        if (ptr != IntPtr.Zero)
            Marshal.FreeCoTaskMem(ptr);
    }

    // ========== 配置 ==========

    /// <summary>Max concurrent file downloads per batch (clamped to 1..64).</summary>
    [UnmanagedCallersOnly(EntryPoint = "pcl_download_set_thread_limit")]
    public static void SetThreadLimit(int limit)
    {
        try { DownloadConfig.NetTaskThreadLimit = Math.Clamp(limit, 1, 64); }
        catch { /* ignore */ }
    }

    /// <summary>Speed limit in bytes/sec. &lt;= 0 means unlimited.</summary>
    [UnmanagedCallersOnly(EntryPoint = "pcl_download_set_speed_limit")]
    public static void SetSpeedLimit(long bytesPerSec)
    {
        try { DownloadConfig.NetTaskSpeedLimitHigh = bytesPerSec; }
        catch { /* ignore */ }
    }

    /// <summary>Apply the launcher's proxy configuration to newly started downloads.</summary>
    [UnmanagedCallersOnly(EntryPoint = "pcl_download_set_proxy")]
    public static void SetProxy(IntPtr typePtr, IntPtr hostPtr, int port, IntPtr userPtr, IntPtr passwordPtr)
    {
        try
        {
            DownloadConfig.ConfigureProxy(
                Marshal.PtrToStringUTF8(typePtr) ?? "Default",
                Marshal.PtrToStringUTF8(hostPtr) ?? "",
                port,
                Marshal.PtrToStringUTF8(userPtr) ?? "",
                Marshal.PtrToStringUTF8(passwordPtr) ?? "");
            FileDownloader.ReloadProxyHandler();
        }
        catch { /* ignore */ }
    }

    // ========== 内部实现 ==========

    private static int WaitForTask(int taskId)
    {
        try
        {
            if (!Tasks.TryGetValue(taskId, out var handle))
                return 1;
            handle.Done.Wait();
            return WaitResultOf(handle.Loader.State);
        }
        catch
        {
            return 1;
        }
    }

    private static void AbortTask(int taskId)
    {
        try
        {
            if (Tasks.TryGetValue(taskId, out var handle))
                handle.Loader.Abort();
        }
        catch { /* best effort */ }
    }

    private static int Register(LoaderDownload loader)
    {
        var handle = new TaskHandle { Loader = loader };
        var id = loader.Uuid;
        loader.StateChanged += (_, newState, _) =>
        {
            if (newState < LoadState.Finished)
                return;

            lock (handle.EventSync)
            {
                if (handle.TerminalEventSent)
                    return;
                handle.TerminalEventSent = true;
                handle.ErrorMessage = loader.Error?.ToString();
                handle.Done.Set();
                var eventType = newState switch
                {
                    LoadState.Finished => EventFinished,
                    LoadState.Failed => EventFailed,
                    _ => EventAborted
                };
                var (downloaded, total, speed, threads) = AggregateStats(loader);
                RaiseEvent(id, eventType, downloaded, total, speed, threads);
            }
        };
        foreach (var file in loader.Files)
        {
            file.ProgressChanged += f =>
            {
                lock (handle.EventSync)
                {
                    if (handle.TerminalEventSent)
                        return;
                    RaiseEvent(id, EventProgress, f.DownloadedBytes, f.TotalSize, f.Speed, f.ActiveThreads);
                }
            };
        }
        Tasks[id] = handle;
        return id;
    }

    private static (long downloaded, long total, long speed, int threads) AggregateStats(LoaderDownload loader)
    {
        long downloaded = 0, total = 0, speed = 0;
        var threads = 0;
        var totalKnown = true;
        foreach (var file in loader.Files)
        {
            downloaded += file.DownloadedBytes;
            if (file.TotalSize >= 0)
                total += file.TotalSize;
            else
                totalKnown = false;
            speed += file.Speed;
            threads += file.ActiveThreads;
        }
        return (downloaded, totalKnown ? total : -1, speed, threads);
    }

    private static int GetTaskState(int taskId)
    {
        try
        {
            if (!Tasks.TryGetValue(taskId, out var handle))
                return -1;
            return MapState(handle.Loader.State);
        }
        catch
        {
            return -1;
        }
    }

    private static double GetTaskProgress(int taskId)
    {
        try
        {
            if (!Tasks.TryGetValue(taskId, out var handle))
                return 0;
            return Math.Clamp(handle.Loader.Progress, 0, 1);
        }
        catch
        {
            return 0;
        }
    }

    private static IntPtr GetTaskProgressDetail(int taskId)
    {
        try
        {
            if (!Tasks.TryGetValue(taskId, out var handle))
                return IntPtr.Zero;

            var sb = new StringBuilder(256);
            sb.Append("{\"state\":").Append(MapState(handle.Loader.State));
            sb.Append(",\"progress\":").Append(handle.Loader.Progress.ToString("R", CultureInfo.InvariantCulture));
            sb.Append(",\"files\":[");
            var first = true;
            foreach (var file in handle.Loader.Files)
            {
                if (!first) sb.Append(',');
                first = false;
                sb.Append("{\"path\":\"").Append(EscapeJson(file.LocalPath)).Append('\"');
                sb.Append(",\"state\":").Append((int)file.State);
                sb.Append(",\"progress\":").Append(file.Progress.ToString("R", CultureInfo.InvariantCulture));
                sb.Append(",\"totalSize\":").Append(file.TotalSize);
                sb.Append(",\"downloaded\":").Append(file.DownloadedBytes);
                sb.Append(",\"speed\":").Append(file.Speed);
                sb.Append(",\"threads\":").Append(file.ActiveThreads);
                sb.Append(",\"hasError\":").Append(file.Errors.Count > 0 ? "true" : "false");
                sb.Append('}');
            }
            sb.Append("]}");
            return StringToPtr(sb.ToString());
        }
        catch
        {
            return IntPtr.Zero;
        }
    }

    private static int MapState(LoadState state) => state switch
    {
        LoadState.Waiting or LoadState.Loading => 0,
        LoadState.Finished => 1,
        LoadState.Failed => 2,
        LoadState.Aborted => 3,
        _ => -1
    };

    private static int WaitResultOf(LoadState state) => state switch
    {
        LoadState.Finished => 0,
        LoadState.Aborted => 2,
        _ => 1
    };

    private static IntPtr StringToPtr(string value) => Marshal.StringToCoTaskMemUTF8(value);

    private static string EscapeJson(string value)
    {
        var sb = new StringBuilder(value.Length + 8);
        foreach (var c in value)
        {
            switch (c)
            {
                case '\\': sb.Append("\\\\"); break;
                case '"': sb.Append("\\\""); break;
                case '\n': sb.Append("\\n"); break;
                case '\r': sb.Append("\\r"); break;
                case '\t': sb.Append("\\t"); break;
                default:
                    if (c < ' ')
                        sb.Append("\\u").Append(((int)c).ToString("x4", CultureInfo.InvariantCulture));
                    else
                        sb.Append(c);
                    break;
            }
        }
        return sb.ToString();
    }

    private static string[] ParseStringArray(string? json)
    {
        if (string.IsNullOrWhiteSpace(json))
            return [];
        try
        {
            using var doc = JsonDocument.Parse(json);
            if (doc.RootElement.ValueKind != JsonValueKind.Array)
                return [];
            var list = new List<string>();
            foreach (var element in doc.RootElement.EnumerateArray())
            {
                if (element.ValueKind == JsonValueKind.String)
                {
                    var value = element.GetString();
                    if (!string.IsNullOrWhiteSpace(value))
                        list.Add(value!);
                }
            }
            return list.ToArray();
        }
        catch
        {
            return [];
        }
    }

    private static List<DownloadFile> ParseBatchFileList(string? json)
    {
        var result = new List<DownloadFile>();
        if (string.IsNullOrWhiteSpace(json))
            return result;
        try
        {
            using var doc = JsonDocument.Parse(json);
            if (doc.RootElement.ValueKind != JsonValueKind.Array)
                return result;
            foreach (var element in doc.RootElement.EnumerateArray())
            {
                if (element.ValueKind != JsonValueKind.Object)
                    continue;

                var urls = element.TryGetProperty("urls", out var urlsProp)
                    ? ParseStringArray(urlsProp.GetRawText())
                    : [];
                var localPath = element.TryGetProperty("localPath", out var pathProp)
                    ? pathProp.GetString() ?? ""
                    : "";
                var hash = element.TryGetProperty("hash", out var hashProp) && hashProp.ValueKind == JsonValueKind.String
                    ? hashProp.GetString()
                    : null;
                var expectedSize = element.TryGetProperty("expectedSize", out var sizeProp) && sizeProp.ValueKind == JsonValueKind.Number
                    ? sizeProp.GetInt64()
                    : -1;

                if (urls.Length == 0 || localPath.Length == 0)
                    continue;

                FileChecker? checker = !string.IsNullOrEmpty(hash) || expectedSize >= 0
                    ? new FileChecker(actualSize: expectedSize, hash: hash)
                    : null;
                result.Add(new DownloadFile(urls, localPath, checker));
            }
        }
        catch (Exception ex)
        {
            LogHelper.Log(ex, "[Export] ParseBatchFileList failed");
        }
        return result;
    }
}
