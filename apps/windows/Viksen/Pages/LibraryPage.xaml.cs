using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using Viksen.Shared;
using Windows.Storage;
using Windows.Storage.Pickers;

namespace Viksen.Pages;

public sealed class RecordingItem
{
    public string Path { get; init; } = "";
    public string Name => System.IO.Path.GetFileNameWithoutExtension(Path);
    public string Detail { get; init; } = "";
}

public sealed partial class LibraryPage : Page
{
    private List<RecordingItem> _all = new();

    public LibraryPage() => InitializeComponent();

    protected override void OnNavigatedTo(NavigationEventArgs e) => Reload();

    private void Reload()
    {
        _all = Directory.Exists(AppPaths.Recordings)
            ? new DirectoryInfo(AppPaths.Recordings).GetFiles("*.wav")
                .OrderByDescending(f => f.LastWriteTime)
                .Select(f => new RecordingItem { Path = f.FullName, Detail = $"{f.LastWriteTime:g} · {f.Length / 1048576.0:0.0} MB" })
                .ToList()
            : new List<RecordingItem>();
        ApplyFilter();
    }

    private void ApplyFilter()
    {
        var q = Search.Text?.Trim() ?? "";
        Recordings.ItemsSource = q.Length == 0 ? _all : _all.Where(r => r.Name.Contains(q, StringComparison.OrdinalIgnoreCase)).ToList();
    }

    private void OnSearch(AutoSuggestBox sender, AutoSuggestBoxTextChangedEventArgs args) => ApplyFilter();

    private RecordingItem? Selected => Recordings.SelectedItem as RecordingItem;

    private async void OnPlay(object sender, RoutedEventArgs e)
    {
        if (Selected is not { } r) return;
        try
        {
            var file = await StorageFile.GetFileFromPathAsync(r.Path);
            await Windows.System.Launcher.LaunchFileAsync(file);
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or System.Runtime.InteropServices.COMException)
        {
            AppLog.Warning($"Play failed: {ex.Message}");
            Reload();
        }
    }

    private async void OnSaveCopy(object sender, RoutedEventArgs e)
    {
        if (Selected is not { } r) return;
        var picker = new FileSavePicker { SuggestedFileName = r.Name, SuggestedStartLocation = PickerLocationId.MusicLibrary };
        picker.FileTypeChoices.Add("WAV audio", new List<string> { ".wav" });
        WinRT.Interop.InitializeWithWindow.Initialize(picker, WinRT.Interop.WindowNative.GetWindowHandle(App.Window));
        var dest = await picker.PickSaveFileAsync();
        if (dest == null) return;
        try
        {
            File.Copy(r.Path, dest.Path, overwrite: true);
            AppLog.Info("Recording copied out");
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            AppLog.Warning($"Copy failed: {ex.Message}");
            await new ContentDialog { Title = "Could not save the copy", Content = ex.Message, CloseButtonText = "OK", XamlRoot = XamlRoot }.ShowAsync();
        }
    }

    private async void OnDelete(object sender, RoutedEventArgs e)
    {
        if (Selected is not { } r) return;
        var d = new ContentDialog
        {
            Title = $"Delete “{r.Name}”?", Content = "This cannot be undone.",
            PrimaryButtonText = "Delete", CloseButtonText = "Cancel", DefaultButton = ContentDialogButton.Close, XamlRoot = XamlRoot,
        };
        if (await d.ShowAsync() != ContentDialogResult.Primary) return;
        try { File.Delete(r.Path); }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { AppLog.Warning($"Delete failed: {ex.Message}"); }
        Reload();
    }
}
