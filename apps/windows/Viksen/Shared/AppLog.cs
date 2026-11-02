using System.Diagnostics;

namespace Viksen.Shared;

/// <summary>
/// Runtime log (same idea as iPhone): app events only, never audio, location or file
/// contents. Rotates at 512 KB; stays on this PC unless the user shares it.
/// </summary>
public static class AppLog
{
    private const long MaxBytes = 512 * 1024;
    private static readonly object Gate = new();
    public static string FilePath => Path.Combine(AppPaths.Logs, "runtime.log");

    public static void Info(string m) => Write("INFO", m);
    public static void Warning(string m) => Write("WARN", m);
    public static void Error(string m) => Write("ERROR", m);

    private static void Write(string level, string message)
    {
        var line = $"{DateTime.Now:yyyy-MM-dd HH:mm:ss.fff} [{level}] {message}";
        Debug.WriteLine(line);
        lock (Gate)
        {
            try
            {
                Directory.CreateDirectory(AppPaths.Logs);
                var info = new FileInfo(FilePath);
                if (info.Exists && info.Length > MaxBytes)
                    File.Move(FilePath, Path.Combine(AppPaths.Logs, "runtime.1.log"), overwrite: true);
                File.AppendAllText(FilePath, line + Environment.NewLine);
            }
            catch (IOException) { }
            catch (UnauthorizedAccessException) { }
        }
    }

    public static string ReadAll()
    {
        lock (Gate)
        {
            try { return File.Exists(FilePath) ? File.ReadAllText(FilePath) : ""; }
            catch (IOException) { return ""; }
        }
    }

    public static void Clear()
    {
        lock (Gate)
        {
            try { if (File.Exists(FilePath)) File.Delete(FilePath); } catch (IOException) { }
        }
    }

    public static string LaunchBanner()
    {
        var v = typeof(AppLog).Assembly.GetName().Version;
        return $"Launch v{v} on Windows {Environment.OSVersion.Version}, {Environment.ProcessorCount} cores";
    }
}
