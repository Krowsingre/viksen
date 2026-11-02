namespace Viksen.Shared;

/// <summary>Per-user data folders (inside the package's LocalAppData when installed from the Store).</summary>
public static class AppPaths
{
    public static string Data { get; } = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Viksen");
    public static string Recordings => Path.Combine(Data, "Recordings");
    public static string Logs => Path.Combine(Data, "Logs");
}
