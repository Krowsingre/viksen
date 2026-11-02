using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Viksen.Engine;
using Viksen.Shared;

namespace Viksen.Pages;

public sealed partial class SettingsPage : Page
{
    private readonly EngineService _engine = EngineService.Shared;
    private bool _loading = true;

    public SettingsPage()
    {
        InitializeComponent();
        AppearanceChoice.SelectedIndex = Settings.Appearance switch { ElementTheme.Light => 1, ElementTheme.Default => 2, _ => 0 };
        Ceiling.Value = _engine.Get(Native.Param.LimiterCeilingDb);
        EngineInfo.Text = _engine.Available
            ? $"Engine: {(EngineService.BinauralHrtf ? "Steam Audio HRTF" : "simplified panner")} · " +
              (_engine.AudioRunning ? $"{_engine.DeviceName} @ {_engine.SampleRate:0} Hz" : "sound off")
            : _engine.LastError ?? "Engine unavailable";
        VersionText.Text = $"Version {typeof(App).Assembly.GetName().Version}";
        _loading = false;
    }

    private void OnAppearance(object sender, SelectionChangedEventArgs e)
    {
        if (_loading) return;
        var theme = AppearanceChoice.SelectedIndex switch { 1 => ElementTheme.Light, 2 => ElementTheme.Default, _ => ElementTheme.Dark };
        Settings.Appearance = theme;
        App.Window?.ApplyTheme(theme);
    }

    private void OnCeiling(object sender, RangeBaseValueChangedEventArgs e)
    {
        if (!_loading) _engine.Set(Native.Param.LimiterCeilingDb, (float)e.NewValue);
    }

    private void OnShowLog(object sender, RoutedEventArgs e)
    {
        LogText.Text = AppLog.ReadAll();
        LogText.Visibility = Visibility.Visible;
    }

    private void OnClearLog(object sender, RoutedEventArgs e)
    {
        AppLog.Clear();
        LogText.Text = "";
    }
}
