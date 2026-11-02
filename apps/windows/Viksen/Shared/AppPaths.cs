namespace Viksen.Shared;

/// <summary>Per-user data folders (inside the package's LocalAppData when installed from the Store).</summary>
public static class AppPaths
{
    /// <summary>The package's real LocalState folder when installed (no path virtualisation surprises
    /// when files are handed to other apps); %LOCALAPPDATA%\Viksen when run unpackaged.</summary>
    public static string Data { get; } = Resolve();

    private static string Resolve()
    {
        try { return Windows.Storage.ApplicationData.Current.LocalFolder.Path; }
        catch (InvalidOperationException) { }
        catch (System.Runtime.InteropServices.COMException) { }
        return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Viksen");
    }
    public static string Recordings => Path.Combine(Data, "Recordings");
    public static string Logs => Path.Combine(Data, "Logs");
}
