using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Navigation;
using Viksen.Engine;
using Windows.Storage.Pickers;

namespace Viksen.Pages;

public sealed partial class StudioPage : Page
{
    private readonly EngineService _engine = EngineService.Shared;
    private readonly List<(Slider Gain, ToggleButton Mute, ToggleButton Solo)> _rows = new();
    private bool _updating;

    public StudioPage()
    {
        InitializeComponent();
        HrtfText.Text = EngineService.BinauralHrtf
            ? "Steam Audio HRTF: each stem is a sound source around your head."
            : "Simplified 3D panner in this build.";
    }

    protected override void OnNavigatedTo(NavigationEventArgs e)
    {
        if (_engine.StudioLoaded) BuildMixer();
        _engine.Ticked += Refresh;
        Refresh();
    }

    protected override void OnNavigatedFrom(NavigationEventArgs e) => _engine.Ticked -= Refresh;

    private async void OnAddStems(object sender, RoutedEventArgs e)
    {
        var picker = new FileOpenPicker { SuggestedStartLocation = PickerLocationId.MusicLibrary };
        foreach (var ext in new[] { ".wav", ".flac", ".mp3" }) picker.FileTypeFilter.Add(ext);
        WinRT.Interop.InitializeWithWindow.Initialize(picker, WinRT.Interop.WindowNative.GetWindowHandle(App.Window));
        var files = await picker.PickMultipleFilesAsync();
        if (files == null || files.Count == 0) return;
        var paths = files.Select(f => f.Path).Take(8).ToList();
        if (!_engine.LoadStudio(paths))
        {
            await new ContentDialog { Title = "Could not open these stems", Content = _engine.LastError, CloseButtonText = "OK", XamlRoot = XamlRoot }.ShowAsync();
            return;
        }
        BuildMixer();
    }

    private void BuildMixer()
    {
        Mixer.Children.Clear();
        _rows.Clear();
        var names = _engine.StudioStemPaths.Select(Path.GetFileNameWithoutExtension).ToList();
        for (var i = 0; i < names.Count; i++)
        {
            var index = i;
            var grid = new Grid { ColumnSpacing = 8 };
            grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(160) });
            grid.ColumnDefinitions.Add(new ColumnDefinition());
            grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
            grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
            var label = new TextBlock { Text = names[i], VerticalAlignment = VerticalAlignment.Center, TextTrimming = TextTrimming.CharacterEllipsis };
            var gain = new Slider { Minimum = -60, Maximum = 6, Value = _engine.StudioGainDb[i], StepFrequency = 0.5, VerticalAlignment = VerticalAlignment.Center };
            var mute = new ToggleButton { Content = "M", IsChecked = _engine.StudioMuted[i] };
            var solo = new ToggleButton { Content = "S", IsChecked = _engine.StudioSolo == i };
            ToolTipService.SetToolTip(mute, "Mute");
            ToolTipService.SetToolTip(solo, "Solo");
            gain.ValueChanged += (_, _) => _engine.SetStudioStem(index, (float)gain.Value, mute.IsChecked == true);
            mute.Click += (_, _) => _engine.SetStudioStem(index, (float)gain.Value, mute.IsChecked == true);
            solo.Click += (_, _) =>
            {
                _engine.StudioSolo = solo.IsChecked == true ? index : null;
                for (var j = 0; j < _rows.Count; j++) _rows[j].Solo.IsChecked = _engine.StudioSolo == j;
                _engine.ApplyStudioMix();
            };
            Grid.SetColumn(gain, 1); Grid.SetColumn(mute, 2); Grid.SetColumn(solo, 3);
            grid.Children.Add(label); grid.Children.Add(gain); grid.Children.Add(mute); grid.Children.Add(solo);
            Mixer.Children.Add(grid);
            _rows.Add((gain, mute, solo));
        }
        EmptyText.Visibility = Visibility.Collapsed;
        LoadedPanel.Visibility = Visibility.Visible;
        _updating = true;
        SpatialSwitch.IsOn = _engine.World.Spatial || !_engine.World.Loaded;
        _updating = false;
    }

    private void Refresh()
    {
        var w = _engine.World;
        if (!_engine.StudioLoaded || w.Id != "studio") return;
        PlayIcon.Glyph = w.Playing ? "" : "";
        _updating = true;
        Position.Maximum = Math.Max(0.1, w.Length);
        Position.Value = Math.Min(w.Position, Position.Maximum);
        _updating = false;
        TimeText.Text = $"{TimeSpan.FromSeconds(w.Position):m\\:ss} / {TimeSpan.FromSeconds(w.Length):m\\:ss}";
        SpaceCanvas.Update(w);
    }

    private void OnPlay(object sender, RoutedEventArgs e)
    {
        if (_engine.World.Playing) _engine.StudioPause();
        else
        {
            if (!_engine.AudioRunning) _engine.StartAudio();
            _engine.StudioPlay();
        }
    }

    // User moves only (ticks update the slider with _updating set).
    private void OnPositionChanged(object sender, RangeBaseValueChangedEventArgs e)
    {
        if (!_updating) _engine.Seek(e.NewValue);
    }

    private void OnSpatial(object sender, RoutedEventArgs e) { if (!_updating) _engine.SetSpatial(SpatialSwitch.IsOn); }
    private void OnReset(object sender, RoutedEventArgs e) => _engine.ResetPositions();
}
