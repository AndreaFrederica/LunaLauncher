namespace PCL.Download;

/// <summary>
/// Thread-safe list with ReaderWriterLockSlim. Extracted from PCL-CE ModBase.SafeList.
/// </summary>
public class SafeList<T> : IEnumerable<T>, IDisposable, ICollection<T>
{
    private readonly List<T> _internalList;
    private readonly ReaderWriterLockSlim _lock = new();

    public SafeList() => _internalList = new List<T>();
    public SafeList(IEnumerable<T> data) => _internalList = new List<T>(data);

    public T this[int index]
    {
        get => _internalList[index];
        set => _internalList[index] = value;
    }

    public void Add(T item)
    {
        _lock.EnterWriteLock();
        try { _internalList.Add(item); }
        finally { _lock.ExitWriteLock(); }
    }

    public bool Remove(T item)
    {
        _lock.EnterWriteLock();
        try { return _internalList.Remove(item); }
        finally { _lock.ExitWriteLock(); }
    }

    public void Clear()
    {
        _lock.EnterWriteLock();
        try { _internalList.Clear(); }
        finally { _lock.ExitWriteLock(); }
    }

    public int Count
    {
        get
        {
            _lock.EnterReadLock();
            try { return _internalList.Count; }
            finally { _lock.ExitReadLock(); }
        }
    }

    public bool IsReadOnly => ((ICollection<T>)_internalList).IsReadOnly;
    public bool Contains(T item) => ((ICollection<T>)_internalList).Contains(item);
    public void CopyTo(T[] array, int arrayIndex) => ((ICollection<T>)_internalList).CopyTo(array, arrayIndex);
    public void Dispose() => _lock.Dispose();

    public IEnumerator<T> GetEnumerator() => ToList().GetEnumerator();
    System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator() => GetEnumerator();

    public List<T> ToList()
    {
        _lock.EnterReadLock();
        try { return new List<T>(_internalList); }
        finally { _lock.ExitReadLock(); }
    }
}
