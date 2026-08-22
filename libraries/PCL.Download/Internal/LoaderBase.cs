namespace PCL.Download;

/// <summary>
/// Base class for all loaders (download tasks). Simplified from PCL-CE ModLoader.LoaderBase.
/// Provides state machine, progress tracking, and event hooks.
/// </summary>
public abstract class LoaderBase
{
    private static int _uuidCounter;
    private static readonly object _uuidLock = new();

    public static int GetUuid()
    {
        lock (_uuidLock) { return ++_uuidCounter; }
    }

    public delegate void StateChangedHandler(LoaderBase loader, LoadState newState, LoadState oldState);
    public delegate void PreviewFinishHandler(LoaderBase loader);

    public readonly object LockState = new();
    public int Uuid { get; } = GetUuid();
    public string Name { get; set; }
    public bool Show { get; set; } = true;

    public event StateChangedHandler? StateChanged;
    public event PreviewFinishHandler? PreviewFinish;

    private LoadState _state = LoadState.Waiting;
    public LoadState State
    {
        get => _state;
        set
        {
            if (_state == value) return;
            var old = _state;
            _state = value;
            LogHelper.Log($"[Loader] {Name} state: {old} -> {value}");
            StateChanged?.Invoke(this, value, old);
        }
    }

    public Exception? Error { get; set; }

    private double _progressValue = -1;
    public virtual double Progress
    {
        get => _state switch
        {
            LoadState.Waiting => 0,
            LoadState.Loading => _progressValue < 0 ? 0.02 : _progressValue,
            _ => 1
        };
        set
        {
            if (_progressValue == value) return;
            _progressValue = value;
        }
    }

    public double ProgressWeight { get; set; } = 1;

    protected void RaisePreviewFinish() => PreviewFinish?.Invoke(this);

    public abstract void Start(object? input = null, bool isForceRestart = false);
    public abstract void Abort();

    protected LoaderBase() => Name = $"Task {Uuid}#";
    protected LoaderBase(string name) => Name = name;
}
