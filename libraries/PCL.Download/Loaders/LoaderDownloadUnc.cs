namespace PCL.Download;

/// <summary>
/// Loader for UNC path file copy operations.
/// </summary>
public class LoaderDownloadUnc : LoaderBase
{
    public string UncPath { get; set; }
    public string SavePath { get; set; }
    private CancellationTokenSource? _cts;

    public LoaderDownloadUnc(string name, string uncPath, string savePath) : base(name)
    {
        UncPath = uncPath;
        SavePath = savePath;
    }

    public override void Start(object? input = null, bool isForceRestart = false)
    {
        if (input is Tuple<string, string> tuple)
        {
            UncPath = tuple.Item1;
            SavePath = tuple.Item2;
        }

        lock (LockState)
        {
            if (State == LoadState.Loading) return;
            State = LoadState.Loading;
        }

        _cts = new CancellationTokenSource();
        var thread = new Thread(() => Run(_cts.Token))
        {
            Name = $"UNC/{Uuid}",
            IsBackground = true
        };
        thread.Start();
    }

    private void Run(CancellationToken cancellationToken)
    {
        try
        {
            cancellationToken.ThrowIfCancellationRequested();
            Directory.CreateDirectory(Path.GetDirectoryName(SavePath)
                ?? throw new IOException("下载路径无效"));
            File.Copy(UncPath, SavePath, true);
            State = LoadState.Finished;
        }
        catch (OperationCanceledException)
        {
            Abort();
        }
        catch (Exception ex)
        {
            Error = ex;
            State = LoadState.Failed;
        }
    }

    public override void Abort()
    {
        if (State >= LoadState.Finished) return;
        State = LoadState.Aborted;
        _cts?.Cancel();
    }
}
