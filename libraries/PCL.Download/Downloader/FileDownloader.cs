using System.Net.Http;
using Downloader;

namespace PCL.Download;

/// <summary>
/// Core file downloader with multi-URL fallback, chunked parallel download, and resume support.
/// Wraps the Downloader NuGet library.
/// </summary>
public static class FileDownloader
{
    private static SocketsHttpHandler SharedHttpHandler = CreateHttpHandler();

    private static SocketsHttpHandler CreateHttpHandler()
    {
        var handler = new SocketsHttpHandler
        {
            MaxConnectionsPerServer = 64,
            PooledConnectionLifetime = TimeSpan.FromMinutes(10),
            PooledConnectionIdleTimeout = TimeSpan.FromMinutes(2),
            EnableMultipleHttp2Connections = true,
        };
        DownloadConfig.ApplyProxy(handler);
        return handler;
    }

    internal static void ReloadProxyHandler()
    {
        // Active downloads retain their handler. New downloads atomically pick up the new proxy.
        Interlocked.Exchange(ref SharedHttpHandler, CreateHttpHandler());
    }

    /// <summary>
    /// Optional: provide a custom HttpClient factory. If null, uses default HttpClient.
    /// Signature: (url) => HttpClient
    /// </summary>
    public static Func<string, HttpClient>? HttpClientFactory { get; set; }

    public static async Task DownloadAsync(string url, string localPath,
        bool useBrowserUserAgent = false, string customUserAgent = "",
        CancellationToken cancellationToken = default,
        bool enableParallelChunks = true, DownloadFile? trackedFile = null)
    {
        await DownloadCoreAsync([url], localPath, useBrowserUserAgent, customUserAgent,
            cancellationToken, enableParallelChunks, trackedFile).ConfigureAwait(false);
    }

    public static async Task DownloadAsync(IEnumerable<string> urls, string localPath,
        bool useBrowserUserAgent = false, string customUserAgent = "",
        CancellationToken cancellationToken = default,
        bool enableParallelChunks = true, DownloadFile? trackedFile = null)
    {
        await DownloadCoreAsync(urls, localPath, useBrowserUserAgent, customUserAgent,
            cancellationToken, enableParallelChunks, trackedFile).ConfigureAwait(false);
    }

    public static void DownloadSync(string url, string localPath,
        bool useBrowserUserAgent = false, string customUserAgent = "")
    {
        DownloadAsync(url, localPath, useBrowserUserAgent, customUserAgent).GetAwaiter().GetResult();
    }

    public static void DownloadSync(IEnumerable<string> urls, string localPath,
        bool useBrowserUserAgent = false, string customUserAgent = "")
    {
        DownloadAsync(urls, localPath, useBrowserUserAgent, customUserAgent).GetAwaiter().GetResult();
    }

    private static async Task DownloadCoreAsync(IEnumerable<string> urls, string localPath,
        bool useBrowserUserAgent, string customUserAgent,
        CancellationToken cancellationToken, bool enableParallelChunks, DownloadFile? trackedFile)
    {
        var urlList = urls.Where(url => !string.IsNullOrWhiteSpace(url)).Distinct().ToList();
        if (urlList.Count == 0)
            throw new ArgumentException("未提供可用的下载地址", nameof(urls));

        Directory.CreateDirectory(Path.GetDirectoryName(localPath)
            ?? throw new ArgumentException("下载路径无效", nameof(localPath)));

        Exception? lastException = null;
        foreach (var url in urlList)
        {
            try
            {
                await DownloadSingleAsync(url, localPath, useBrowserUserAgent, customUserAgent,
                    cancellationToken, enableParallelChunks, trackedFile).ConfigureAwait(false);
                return;
            }
            catch (OperationCanceledException)
            {
                CleanupTempFiles(localPath);
                throw;
            }
            catch (Exception ex)
            {
                lastException = ex;
                LogHelper.Log(ex, $"[Download] 下载失败，尝试下一个源：{url}");
            }
        }

        CleanupTempFiles(localPath);
        throw new IOException($"下载失败：{localPath}", lastException);
    }

    private static async Task DownloadSingleAsync(string url, string localPath,
        bool useBrowserUserAgent, string customUserAgent,
        CancellationToken cancellationToken, bool enableParallelChunks, DownloadFile? trackedFile)
    {
        LogHelper.Log($"[Download] 开始下载：{url} -> {localPath}");
        trackedFile?.BeginProgressNotifications();

        var perFileThreadLimit = enableParallelChunks ? Math.Max(1, DownloadConfig.NetTaskThreadLimit) : 1;
        var chunkCount = Math.Min(perFileThreadLimit, 4);
        var configuration = new DownloadConfiguration
        {
            ChunkCount = chunkCount,
            ParallelCount = chunkCount,
            ParallelDownload = chunkCount > 1,
            MaximumBytesPerSecond = DownloadConfig.NetTaskSpeedLimitHigh > 0 ? DownloadConfig.NetTaskSpeedLimitHigh : 0,
            MaxTryAgainOnFailure = 2,
            BlockTimeout = 60000,
            DownloadFileExtension = DownloadConfig.NetDownloadEnd,
            EnableAutoResumeDownload = true,
            CustomHttpClientFactory = () => GetHttpClient(url),
            MinimumSizeOfChunking = 1024 * 1024L,
            MaximumMemoryBufferBytes = 256L * 1024 * 1024,
        };

        using var downloader = new DownloadService(configuration);
        using var cancelReg = cancellationToken.Register(() =>
        {
            try { downloader.CancelAsync(); } catch { }
        });

        var tcs = new TaskCompletionSource<bool>();
        var speedWatch = System.Diagnostics.Stopwatch.StartNew();

        void UpdateDownloadStat(DownloadProgressChangedEventArgs args)
        {
            if (trackedFile is null) return;
            trackedFile.State = NetState.Downloading;

            // Downloader may report TotalBytesToReceive = 0/-1 before the response
            // headers are fully processed; Package.TotalFileSize is authoritative
            // once known.
            var total = downloader.Package?.TotalFileSize ?? 0;
            if (total <= 0)
                total = args.TotalBytesToReceive;
            if (total > 0)
            {
                trackedFile.TotalSize = Math.Max(trackedFile.TotalSize, total);
                trackedFile.IsUnknownSize = false;
            }

            var received = Math.Max(trackedFile.DownloadedBytes, args.ReceivedBytesSize);
            trackedFile.DownloadedBytes = received;

            // BytesPerSecondSpeed is 0 on early ticks; fall back to our own average.
            var speed = args.BytesPerSecondSpeed;
            if (speed <= 0)
            {
                var elapsed = speedWatch.Elapsed.TotalSeconds;
                if (elapsed > 0.05)
                    speed = received / elapsed;
            }
            trackedFile.Speed = Math.Max(0L, (long)Math.Round(speed));
            trackedFile.ActiveThreads = Math.Max(0, args.ActiveChunks);
            trackedFile.NotifyProgress();
        }

        downloader.DownloadStarted += (_, args) =>
        {
            if (trackedFile is null) return;
            trackedFile.State = NetState.Reading;
            var total = args.TotalBytesToReceive;
            if (total > 0)
            {
                trackedFile.TotalSize = Math.Max(trackedFile.TotalSize, total);
                trackedFile.IsUnknownSize = false;
            }
            trackedFile.DownloadedBytes = 0;
            trackedFile.Speed = 0;
            trackedFile.ActiveThreads = 0;
            speedWatch.Restart();
        };
        downloader.DownloadProgressChanged += (_, args) => UpdateDownloadStat(args);
        downloader.ChunkDownloadProgressChanged += (_, args) => UpdateDownloadStat(args);
        downloader.DownloadFileCompleted += (_, args) =>
        {
            if (trackedFile is not null)
            {
                trackedFile.ActiveThreads = 0;
                trackedFile.DownloadedBytes = Math.Max(trackedFile.DownloadedBytes, trackedFile.TotalSize);
                if (trackedFile.Speed <= 0)
                {
                    var elapsed = speedWatch.Elapsed.TotalSeconds;
                    if (elapsed > 0.01 && trackedFile.DownloadedBytes > 0)
                        trackedFile.Speed = (long)Math.Round(trackedFile.DownloadedBytes / elapsed);
                }
            }

            if (args.Cancelled)
                tcs.TrySetCanceled();
            else if (args.Error != null)
                tcs.TrySetException(args.Error);
            else
                tcs.TrySetResult(true);
        };

        try
        {
            await downloader.DownloadFileTaskAsync(url, localPath, cancellationToken).ConfigureAwait(false);
            await tcs.Task.ConfigureAwait(false);

            var tempPath = localPath + DownloadConfig.NetDownloadEnd;
            if (!File.Exists(localPath) && File.Exists(tempPath))
            {
                for (var retry = 0; retry < 5; retry++)
                {
                    try { File.Move(tempPath, localPath, true); break; }
                    catch (IOException) { Thread.Sleep(100); }
                }
            }
            if (!File.Exists(localPath))
                throw new IOException($"下载未产生任何文件：{localPath}");

            LogHelper.Log($"[Download] 下载成功：{localPath}");
        }
        catch (TaskCanceledException) when (cancellationToken.IsCancellationRequested)
        {
            throw new OperationCanceledException(cancellationToken);
        }
        catch (TaskCanceledException ex)
        {
            throw new TimeoutException($"下载超时（{url}）", ex);
        }
        catch (OperationCanceledException)
        {
            throw;
        }
        catch (Exception ex)
        {
            throw new IOException($"下载失败：{url}", ex);
        }
    }

    private static void CleanupTempFiles(string localPath)
    {
        TryDeleteFile(localPath);
        TryDeleteFile(localPath + DownloadConfig.NetDownloadEnd);
    }

    private static void TryDeleteFile(string path)
    {
        for (var retry = 0; retry < 5; retry++)
        {
            try
            {
                if (File.Exists(path)) File.Delete(path);
                return;
            }
            catch (IOException) { Thread.Sleep(100); }
        }
    }

    private static HttpClient GetHttpClient(string url)
    {
        if (HttpClientFactory is not null)
            return HttpClientFactory(url);

        return new HttpClient(Volatile.Read(ref SharedHttpHandler), disposeHandler: false)
        {
            DefaultRequestVersion = System.Net.HttpVersion.Version20,
            DefaultVersionPolicy = HttpVersionPolicy.RequestVersionOrLower,
        };
    }
}
