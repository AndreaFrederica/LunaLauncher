using System.Net;
using System.Net.Http;

namespace PCL.Download;

/// <summary>
/// Global download configuration. Replace with your own settings provider if needed.
/// </summary>
public static class DownloadConfig
{
    private sealed record ProxySettings(string Type, string Host, int Port, string User, string Password);

    private static ProxySettings _proxySettings = new("Default", "", 0, "", "");

    /// <summary>Max concurrent threads per download task.</summary>
    public static int NetTaskThreadLimit { get; set; } = 16;

    /// <summary>Speed limit in bytes/sec. -1 = unlimited.</summary>
    public static long NetTaskSpeedLimitHigh { get; set; } = -1;

    /// <summary>Temp file extension during download.</summary>
    public const string NetDownloadEnd = ".PCLDownloading";

    public static void ConfigureProxy(string type, string host, int port, string user, string password)
    {
        var normalizedType = type.ToUpperInvariant();
        if (normalizedType is not ("DEFAULT" or "NONE" or "HTTP" or "SOCKS5"))
            normalizedType = "DEFAULT";
        if ((normalizedType is "HTTP" or "SOCKS5") && (string.IsNullOrWhiteSpace(host) || port is <= 0 or > 65535))
            normalizedType = "NONE";

        Volatile.Write(ref _proxySettings,
            new ProxySettings(normalizedType, host.Trim(), port, user, password));
    }

    internal static void ApplyProxy(SocketsHttpHandler handler)
    {
        var settings = Volatile.Read(ref _proxySettings);
        if (settings.Type == "NONE")
        {
            handler.UseProxy = false;
            return;
        }
        if (settings.Type == "DEFAULT")
        {
            handler.UseProxy = true;
            handler.Proxy = null;
            return;
        }

        var scheme = settings.Type == "SOCKS5" ? "socks5" : "http";
        var proxy = new WebProxy(new UriBuilder(scheme, settings.Host, settings.Port).Uri);
        if (!string.IsNullOrEmpty(settings.User))
            proxy.Credentials = new NetworkCredential(settings.User, settings.Password);
        handler.UseProxy = true;
        handler.Proxy = proxy;
    }
}
