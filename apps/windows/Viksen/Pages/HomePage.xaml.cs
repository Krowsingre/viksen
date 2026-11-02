using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using Viksen.Engine;
using Viksen.Shared;

namespace Viksen.Pages;

public sealed partial class HomePage : Page
{
    private readonly EngineService _engine = EngineService.Shared;

    public HomePage()
    {
        InitializeComponent();
        EngineNote.Text = _engine.Available
            ? (EngineService.BinauralHrtf ? "3D audio: Steam Audio HRTF." : "3D audio: simplified panner in this build.")
            : _engine.LastError ?? "";
    }

    protected override void OnNavigatedTo(NavigationEventArgs e)
    {
        _engine.Ticked += Refresh;
        Refresh();
    }

    protected override void OnNavigatedFrom(NavigationEventArgs e) => _engine.Ticked -= Refresh;

    private void Refresh()
    {
        var m = _engine.Meters;
        var phase = string.IsNullOrEmpty(m.MoonPhase) ? "new" : m.MoonPhase;
        MoonGlyph.Text = Lunar.Glyph(phase);
        MoonTitle.Text = Lunar.Title(phase);
        LunarDayText.Text = m.LunarDay > 0 ? $"Lunar day {m.LunarDay} of 30" : "";
    }

    private void OnDimensions(object sender, RoutedEventArgs e) => App.Window?.Navigate("dimensions");
    private void OnExperience(object sender, RoutedEventArgs e) => App.Window?.Navigate("experience");
    private void OnStudio(object sender, RoutedEventArgs e) => App.Window?.Navigate("studio");
}
