using System.Security.Cryptography;

namespace PCL.Download;

/// <summary>
/// File integrity checker. Supports MD5/SHA1/SHA256 hash verification and size checks.
/// </summary>
public class FileChecker
{
    public long MinSize { get; set; } = -1;
    public long ActualSize { get; set; } = -1;
    public string? Hash { get; set; }
    public bool CanUseExistsFile { get; set; } = true;

    public FileChecker(long minSize = -1, long actualSize = -1, string? hash = null, bool canUseExistsFile = true)
    {
        MinSize = minSize;
        ActualSize = actualSize;
        Hash = hash;
        CanUseExistsFile = canUseExistsFile;
    }

    /// <summary>
    /// Check file integrity. Returns null on success, error description on failure.
    /// </summary>
    public string? Check(string localPath)
    {
        try
        {
            var info = new FileInfo(localPath);
            if (!info.Exists)
                return "文件不存在：" + localPath;

            var fileSize = info.Length;
            var errors = new List<string>();
            var hashPassed = false;

            if (!string.IsNullOrEmpty(Hash))
            {
                string? computed = Hash!.Length switch
                {
                    < 35 => GetFileMD5(localPath),
                    64 => GetFileSHA256(localPath),
                    _ => GetFileSHA1(localPath)
                };

                if (!string.Equals(Hash, computed, StringComparison.OrdinalIgnoreCase))
                    errors.Add($"Hash mismatch: expected {Hash}, got {computed}");
                else
                    hashPassed = true;
            }

            if (ActualSize >= 0 && ActualSize != fileSize && !hashPassed)
                errors.Add($"Size mismatch: expected {ActualSize}, got {fileSize}");

            if (MinSize >= 0 && fileSize < MinSize)
                errors.Add($"File too small: {fileSize} < {MinSize}");

            return errors.Count > 0 ? string.Join("; ", errors) : null;
        }
        catch (Exception ex)
        {
            return $"校验异常：{ex.Message}";
        }
    }

    private static string GetFileMD5(string path)
    {
        using var stream = File.OpenRead(path);
        return Convert.ToHexStringLower(MD5.HashData(stream));
    }

    private static string GetFileSHA1(string path)
    {
        using var stream = File.OpenRead(path);
        return Convert.ToHexStringLower(SHA1.HashData(stream));
    }

    private static string GetFileSHA256(string path)
    {
        using var stream = File.OpenRead(path);
        return Convert.ToHexStringLower(SHA256.HashData(stream));
    }
}
