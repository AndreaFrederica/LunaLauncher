namespace PCL.Download;

/// <summary>
/// Simple logging facade. Assign <see cref="Logger"/> to redirect logs to your framework.
/// </summary>
public static class LogHelper
{
    public enum LogLevel
    {
        Debug,
        Normal,
        Feedback,
        Critical
    }

    /// <summary>
    /// Custom logger delegate. Signature: (message, level).
    /// Defaults to Console.WriteLine.
    /// </summary>
    public static Action<string, LogLevel> Logger { get; set; } = (msg, _) => Console.WriteLine(msg);

    public static void Log(string message, LogLevel level = LogLevel.Normal)
    {
        try { Logger(message, level); } catch { /* never let logging break download */ }
    }

    public static void Log(Exception ex, string message, LogLevel level = LogLevel.Debug)
    {
        try { Logger($"{message}: {ex}", level); } catch { }
    }
}
