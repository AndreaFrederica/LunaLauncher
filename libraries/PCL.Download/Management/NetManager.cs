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
