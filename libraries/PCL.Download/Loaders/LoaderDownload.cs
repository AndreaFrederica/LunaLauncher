using System.Collections.Concurrent;

namespace PCL.Download;

/// <summary>
/// Batch download orchestrator with parallel file downloads, retry, and progress aggregation.
/// </summary>
public class LoaderDownload : LoaderBase
{
    public SafeList<DownloadFile> Files;
    private int _fileRemain;
    private readonly object _fileRemainLock = new();
    private CancellationTokenSource? _cts;
    public int FailCount { get; set; }

    public override double Progress
    {
        get => State >= LoadState.Finished ? 1 : (Files.Any() ? Files.Average(f => f.Progress) : 0);
        set => throw new InvalidOperationException("文件下载不允许指定进度");
    }

    public LoaderDownload(string name, List<DownloadFile> fileTasks) : base(name)
    {
        Files = new SafeList<DownloadFile>(fileTasks ?? new List<DownloadFile>());
    }

    public override void Start(object? input = null, bool isForceRestart = false)
    {
        if (input is List<DownloadFile> inputFiles)
            Files = new SafeList<DownloadFile>(inputFiles);

        lock (LockState)
        {
            if (State == LoadState.Loading) return;
            State = LoadState.Loading;
        }

        _cts = new CancellationTokenSource();
        lock (_fileRemainLock) { _fileRemain = Files.Count; }

        NetManager.Instance.Start(this);

        _ = Task.Run(() => RunAsync(_cts.Token));
    }

    private async Task RunAsync(CancellationToken cancellationToken)
    {
        try
        {
            if (!Files.Any())
            {
                OnFinish();
                return;
            }

            var exceptions = new ConcurrentQueue<Exception>();
            using var semaphore = new SemaphoreSlim(GetMaxParallelFiles());
            var tasks = Files.Select(async file =>
            {
                var entered = false;
                try
                {
                    await semaphore.WaitAsync(cancellationToken).ConfigureAwait(false);
                    entered = true;
                    await ProcessFileAsync(file, cancellationToken).ConfigureAwait(false);
                }
                catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested) { }
                catch (Exception ex)
                {
                    file.AddError(ex);
                    file.State = NetState.Interrupted;
                    exceptions.Enqueue(ex);
                    _cts?.Cancel();
                }
                finally
                {
                    if (entered) semaphore.Release();
                }
            }).ToList();

            await Task.WhenAll(tasks).ConfigureAwait(false);
            if (!exceptions.IsEmpty)
                OnFail(exceptions.ToList());
        }
        catch (OperationCanceledException)
        {
            Abort();
        }
        catch (Exception ex)
        {
            OnFail(new List<Exception> { ex });
        }
    }

    private int GetMaxParallelFiles()
    {
        return Math.Max(1, Math.Min(Files.Count, Math.Clamp(DownloadConfig.NetTaskThreadLimit, 1, 64)));
    }

    private async Task ProcessFileAsync(DownloadFile file, CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        file.RegisterLoader(this);

        if (State >= LoadState.Finished) return;

        Directory.CreateDirectory(Path.GetDirectoryName(file.LocalPath)
            ?? throw new IOException("下载路径无效"));

        // Skip if existing file passes integrity check
        if (file.Check?.CanUseExistsFile == true && file.Check.Check(file.LocalPath) is null)
        {
            file.IsCopy = true;
            file.State = NetState.Finished;
            try { file.TotalSize = new FileInfo(file.LocalPath).Length; }
            catch (IOException) { file.TotalSize = -1; }
            file.DownloadedBytes = file.TotalSize;
            file.Speed = 0;
            file.ActiveThreads = 0;
            OnFileFinish(file);
            return;
        }

        file.State = NetState.Connecting;
        var enableParallelChunks = Files.Count <= 1;

        for (var retry = 0; retry < 4; retry++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            try
            {
                await FileDownloader.DownloadAsync(file.Urls, file.LocalPath,
                    file.UseBrowserUserAgent, file.CustomUserAgent,
                    cancellationToken, enableParallelChunks, file).ConfigureAwait(false);
                break;
            }
            catch (OperationCanceledException) { throw; }
            catch (Exception ex) when (retry < 3)
            {
                LogHelper.Log(ex, $"[Download] 重试 {retry + 1}/3：{file.LocalPath}");
                await Task.Delay(Random.Shared.Next(300, 500 + retry * 300), cancellationToken).ConfigureAwait(false);
            }
        }

        try { file.TotalSize = new FileInfo(file.LocalPath).Length; }
        catch (IOException) { file.TotalSize = -1; }
        file.IsUnknownSize = file.TotalSize < 0;
        file.DownloadedBytes = Math.Max(0, file.TotalSize);
        file.Speed = 0;
        file.ActiveThreads = 0;
        file.State = NetState.Finished;
        OnFileFinish(file);
    }

    public void OnFileFinish(DownloadFile file)
    {
        lock (_fileRemainLock)
        {
            _fileRemain -= 1;
            if (_fileRemain > 0) return;
        }
        OnFinish();
    }

    public void OnFinish()
    {
        RaisePreviewFinish();
        lock (LockState)
        {
            if (State > LoadState.Loading) return;
            State = LoadState.Finished;
        }
        NetManager.Instance.Finish(this);
    }

    public void OnFail(List<Exception> exList)
    {
        lock (LockState)
        {
            if (State > LoadState.Loading) return;
            Error = exList.FirstOrDefault() ?? new Exception("未知下载错误");
            State = LoadState.Failed;
        }

        FailCount += exList.Count;
        foreach (var file in Files.Where(f => f.State < NetState.Finished))
        {
            file.State = NetState.Interrupted;
            file.Speed = 0;
            file.ActiveThreads = 0;
            file.AddErrors(exList);
        }

        NetManager.Instance.Finish(this);
    }

    public override void Abort()
    {
        lock (LockState)
        {
            if (State >= LoadState.Finished) return;
            State = LoadState.Aborted;
        }

        _cts?.Cancel();
        foreach (var file in Files.Where(f => f.State < NetState.Finished))
        {
            file.State = NetState.Interrupted;
            file.Speed = 0;
            file.ActiveThreads = 0;
        }

        NetManager.Instance.Finish(this);
    }
}
