namespace PCL.Download;

/// <summary>
/// Global download manager singleton. Tracks all active download files and tasks.
/// </summary>
public sealed class NetManager
{
    private static readonly Lazy<NetManager> _instance = new(() => new NetManager());
    public static NetManager Instance => _instance.Value;

    public Dictionary<string, DownloadFile> Files { get; } = new();
    public object LockFiles { get; } = new();
    public SafeList<LoaderDownload> Tasks { get; } = new();

    private int _activeFiles;
    private int _activeLargeFiles;
    private readonly object _slotLock = new();

    public async ValueTask<IDisposable> AcquireFileSlotAsync(long expectedSize, CancellationToken cancellationToken)
    {
        var large = expectedSize < 0 || expectedSize > DownloadConfig.SmallFileSize;
        while (true)
        {
            cancellationToken.ThrowIfCancellationRequested();
            lock (_slotLock)
            {
                var smart = DownloadConfig.SmartConcurrency;
                var limit = smart ? 32 : Math.Clamp(DownloadConfig.NetTaskThreadLimit, 1, 64);
                if (_activeFiles < limit && (!smart || !large || _activeLargeFiles < 8))
                {
                    _activeFiles++;
                    if (large) _activeLargeFiles++;
                    return new FileSlot(this, large);
                }
            }
            await Task.Delay(25, cancellationToken).ConfigureAwait(false);
        }
    }

    private sealed class FileSlot : IDisposable
    {
        private NetManager? _owner;
        private readonly bool _large;
        public FileSlot(NetManager owner, bool large) { _owner = owner; _large = large; }
        public void Dispose()
        {
            var owner = Interlocked.Exchange(ref _owner, null);
            if (owner is not null)
                lock (owner._slotLock)
                {
                    owner._activeFiles--;
                    if (_large) owner._activeLargeFiles--;
                }
        }
    }

    public int FileRemain
    {
        get
        {
            lock (LockFiles)
                return Files.Values.Count(f => f.State != NetState.Finished);
        }
    }

    private long _downloadDone;
    private readonly object _lockDone = new();
    public long DownloadDone
    {
        get { lock (_lockDone) return _downloadDone; }
        set { lock (_lockDone) _downloadDone = value; }
    }

    public long Speed
    {
        get
        {
            lock (LockFiles)
                return Files.Values.Sum(f => f.Speed);
        }
    }

    public int ThreadCount
    {
        get
        {
            lock (LockFiles)
                return Files.Values.Sum(f => f.ActiveThreads);
        }
    }

    public void Start(LoaderDownload task)
    {
        lock (LockFiles)
        {
            Tasks.Remove(task);
            Tasks.Add(task);
            foreach (var file in task.Files)
                Files[file.LocalPath] = file;
        }
    }

    public void Finish(LoaderDownload task)
    {
        lock (LockFiles)
        {
            Tasks.Remove(task);
            foreach (var file in task.Files)
                Files.Remove(file.LocalPath);
        }
    }
}
