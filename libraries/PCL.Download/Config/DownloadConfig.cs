namespace PCL.Download;

/// <summary>
/// Global download configuration. Replace with your own settings provider if needed.
/// </summary>
public static class DownloadConfig
{
    /// <summary>Max concurrent threads per download task.</summary>
    public static int NetTaskThreadLimit { get; set; } = 16;

    /// <summary>Speed limit in bytes/sec. -1 = unlimited.</summary>
    public static long NetTaskSpeedLimitHigh { get; set; } = -1;

    /// <summary>Temp file extension during download.</summary>
    public const string NetDownloadEnd = ".PCLDownloading";
}
