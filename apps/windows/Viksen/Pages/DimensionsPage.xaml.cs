using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using Viksen.Engine;

namespace Viksen.Pages;

public sealed partial class DimensionsPage : Page
{
    private readonly EngineService _engine = EngineService.Shared;
    private DimensionInfo? _selected;
    private bool _updating;

    public DimensionsPage() => InitializeComponent();

    protected override void OnNavigatedTo(NavigationEventArgs e)
    {
        var all = _engine.AvailableDimensions();
        DimensionList.ItemsSource = all;
        var current = all.FirstOrDefault(d => d.Id == _engine.LoadedDimensionId) ?? all.FirstOrDefault();
        if (current != null) DimensionList.SelectedItem = current;
        _engine.Ticked += Refresh;
    }

    protected override void OnNavigatedFrom(NavigationEventArgs e) => _engine.Ticked -= Refresh;

    private void OnSelect(object sender, SelectionChangedEventArgs e)
    {
        _selected = DimensionList.SelectedItem as DimensionInfo;
        Detail.Visibility = _selected == null ? Visibility.Collapsed : Visibility.Visible;
        if (_selected == null) return;
        NameText.Text = _selected.Name;
        Subtitle.Text = _selected.Subtitle;
        Refresh();
    }

    private bool IsInside => _selected != null && _engine.LoadedDimensionId == _selected.Id && _engine.World.Id == _selected.Id;

    private void Refresh()
    {
        if (_selected == null) return;
        var w = _engine.World;
        var inside = IsInside && w.Playing;
        EnterButton.IsEnabled = _selected.AudioAvailable && !inside;
        LeaveButton.IsEnabled = inside;
        StatusText.Text = !_selected.AudioAvailable ? "This build does not include this Dimension's music yet."
            : inside ? $"{TimeSpan.FromSeconds(w.Position):m\\:ss} · {(w.Spatial ? (w.Hrtf ? "3D (Steam Audio HRTF)" : "3D (simplified)") : "Stereo")}"
            : "";
        if (!IsInside) { Layers.Children.Clear(); return; }
        _updating = true;
        SpatialSwitch.IsOn = w.Spatial;
        _updating = false;
        SpaceCanvas.Update(w);
        while (Layers.Children.Count > w.Stems.Count) Layers.Children.RemoveAt(Layers.Children.Count - 1);
        for (var i = 0; i < w.Stems.Count; i++)
        {
            if (i >= Layers.Children.Count) Layers.Children.Add(new TextBlock { TextWrapping = TextWrapping.Wrap });
            var s = w.Stems[i];
            var audible = s.GainDb > -40;
            ((TextBlock)Layers.Children[i]).Text =
                $"{(audible ? "●" : "○")} {s.Title}{(s.Reasons.Count > 0 ? " — " + string.Join(", ", s.Reasons) : "")}";
            ((TextBlock)Layers.Children[i]).Opacity = audible ? 1.0 : 0.5;
        }
    }

    private async void OnEnter(object sender, RoutedEventArgs e)
    {
        if (_selected == null) return;
        if (!_engine.EnterDimension(_selected))
            await new ContentDialog { Title = "Could not enter", Content = _engine.LastError, CloseButtonText = "OK", XamlRoot = XamlRoot }.ShowAsync();
        else _engine.SetSpatial(SpatialSwitch.IsOn);
    }

    private void OnLeave(object sender, RoutedEventArgs e) => _engine.LeaveDimension();
    private void OnSpatial(object sender, RoutedEventArgs e) { if (!_updating && IsInside) _engine.SetSpatial(SpatialSwitch.IsOn); }
    private void OnReset(object sender, RoutedEventArgs e) { if (IsInside) _engine.ResetPositions(); }
}
