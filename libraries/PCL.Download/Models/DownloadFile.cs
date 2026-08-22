namespace PCL.Download;

/// <summary>
/// Represents a single downloadable file with state tracking.
/// </summary>
public class DownloadFile
{
    public int Id { get; } = LoaderBase.GetUuid();
    public string LocalPath { get; set; }
    public string LocalName { get; }
    public List<string> Urls { get; }
    public FileChecker? Check { get; }
    public bool UseBrowserUserAgent { get; }
    public string CustomUserAgent { get; }
    public NetState State { get; set; } = NetState.WaitingToCheck;
    public long TotalSize { get; set; } = -1;
    public bool IsUnknownSize { get; set; } = true;
    public long DownloadedBytes { get; set; }
    public bool IsCopy { get; set; }
    public long Speed { get; set; }
    public int ActiveThreads { get; set; }

    private readonly object _sync = new();
    private readonly List<Exception> _errors = new();
    private readonly List<LoaderDownload> _loaders = new();

    public IReadOnlyList<Exception> Errors
    {
        get { lock (_sync) return _errors.ToArray(); }
    }

    public IReadOnlyList<LoaderDownload> Loaders
    {
        get { lock (_sync) return _loaders.ToArray(); }
    }

    public void AddError(Exception error)
    {
        lock (_sync) _errors.Add(error);
    }

    public void AddErrors(IEnumerable<Exception> errors)
    {
        lock (_sync) _errors.AddRange(errors);
    }

    public bool RegisterLoader(LoaderDownload loader)
    {
        lock (_sync)
        {
            if (_loaders.Contains(loader)) return false;
            _loaders.Add(loader);
            return true;
        }
    }

    public double Progress
    {
        get
        {
            return State switch
            {
                NetState.WaitingToCheck => 0,
                NetState.WaitingToDownload => 0.01,
                NetState.Connecting => 0.02,
                NetState.Reading => 0.04,
                NetState.Downloading when TotalSize > 0 => Math.Clamp((double)DownloadedBytes / TotalSize, 0.05, 1),
                NetState.Downloading => 0.5,
                NetState.Merging => 0.99,
                NetState.Finished or NetState.Interrupted => 1,
                _ => 0
            };
        }
    }

    /// <summary>
    /// Raised for sustained downloads only. Short transfers skip intermediate
    /// progress events and report their terminal state directly.
    /// Used by the native export layer to push events to the host process.
    /// </summary>
    public event Action<DownloadFile>? ProgressChanged;

    private long _lastProgressNotify;
    private long _progressNotifyStarted;
    private const long IntermediateProgressMinimumBytes = 1024 * 1024;

    internal void BeginProgressNotifications()
    {
        var now = Environment.TickCount64;
        Interlocked.Exchange(ref _progressNotifyStarted, now);
        Interlocked.Exchange(ref _lastProgressNotify, now);
    }

    internal void NotifyProgress()
    {
        var handler = ProgressChanged;
        if (handler is null) return;

        // Asset-heavy downloads contain thousands of tiny files. Their terminal
        // event already carries the final byte count, so an intermediate native
        // callback only adds cross-runtime and UI-thread scheduling overhead.
        if (!IsUnknownSize && TotalSize >= 0 && TotalSize <= IntermediateProgressMinimumBytes)
            return;

        var now = Environment.TickCount64;
        var started = Interlocked.Read(ref _progressNotifyStarted);
        if (started > 0 && now - started < 250) return;
        var last = Interlocked.Read(ref _lastProgressNotify);
        if (now - last < 200) return;
        if (Interlocked.CompareExchange(ref _lastProgressNotify, now, last) != last) return;
        try { handler(this); } catch { /* never break the download */ }
    }

    public DownloadFile(IEnumerable<string> urls, string localPath, FileChecker? checker = null,
        bool useBrowserUserAgent = false, string customUserAgent = "")
    {
        Urls = urls.Where(url => !string.IsNullOrWhiteSpace(url)).Distinct().ToList();
        LocalPath = localPath;
        LocalName = Path.GetFileName(localPath);
        Check = checker;
        UseBrowserUserAgent = useBrowserUserAgent;
        CustomUserAgent = customUserAgent;
    }
}
