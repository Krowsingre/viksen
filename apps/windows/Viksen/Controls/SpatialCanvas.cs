using Microsoft.UI;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Automation;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Shapes;
using Viksen.Engine;
using Windows.Foundation;
using Windows.System;

namespace Viksen.Controls;

/// <summary>
/// Top-down view of the listener and the instruments (same as iPhone's spatial canvas):
/// drag an instrument to move it around you; the distance from the centre is its
/// distance in metres (outer ring = 4 m). Arrow keys rotate the focused instrument.
/// </summary>
public sealed class SpatialCanvas : UserControl
{
    private const float MaxDistance = 4f;
    private readonly Canvas _canvas = new();
    private readonly List<(Ellipse Dot, TextBlock Label)> _stems = new();
    private int? _dragging;
    private int _focused;

    public SpatialCanvas()
    {
        Height = 340;
        Content = _canvas;
        _canvas.Background = new SolidColorBrush(Colors.Transparent);   // hit-testable
        SizeChanged += (_, _) => Redraw();
        _canvas.PointerMoved += OnMoved;
        _canvas.PointerReleased += (_, e) => { _dragging = null; _canvas.ReleasePointerCapture(e.Pointer); };
        IsTabStop = true;
        KeyDown += OnKey;
        AutomationProperties.SetName(this, "Spatial canvas");
    }

    private double Radius => Math.Max(40, Math.Min(ActualWidth, ActualHeight) / 2 - 24);
    private Point Centre => new(ActualWidth / 2, ActualHeight / 2);

    private void Redraw()
    {
        _canvas.Children.Clear();
        _stems.Clear();
        var c = Centre;
        foreach (var f in new[] { 1.0, 0.5 })
        {
            var ring = new Ellipse { Width = Radius * 2 * f, Height = Radius * 2 * f, Stroke = new SolidColorBrush(Colors.Gray) { Opacity = 0.35 }, StrokeThickness = 1 };
            Canvas.SetLeft(ring, c.X - Radius * f);
            Canvas.SetTop(ring, c.Y - Radius * f);
            _canvas.Children.Add(ring);
        }
        var front = new TextBlock { Text = "front", FontSize = 11, Opacity = 0.6 };
        Canvas.SetLeft(front, c.X - 14);
        Canvas.SetTop(front, c.Y - Radius - 20);
        _canvas.Children.Add(front);
        var you = new FontIcon { Glyph = "", Foreground = (Brush)Application.Current.Resources["VkAccentBrush"], FontSize = 20 };
        Canvas.SetLeft(you, c.X - 10);
        Canvas.SetTop(you, c.Y - 10);
        _canvas.Children.Add(you);
        Update(EngineService.Shared.World);
    }

    /// <summary>Moves the dots to the engine's current positions.</summary>
    public void Update(WorldStatus w)
    {
        if (w.Stems.Count != _stems.Count)
        {
            foreach (var (d, l) in _stems) { _canvas.Children.Remove(d); _canvas.Children.Remove(l); }
            _stems.Clear();
            for (var i = 0; i < w.Stems.Count; i++)
            {
                var index = i;
                var dot = new Ellipse { Width = 26, Height = 26, Fill = (Brush)Application.Current.Resources["VkMusicBrush"] };
                dot.PointerPressed += (_, e) =>
                {
                    _dragging = index;
                    _focused = index;
                    _canvas.CapturePointer(e.Pointer);
                    Focus(FocusState.Pointer);
                    e.Handled = true;
                };
                var label = new TextBlock { FontSize = 11, IsHitTestVisible = false };
                _canvas.Children.Add(dot);
                _canvas.Children.Add(label);
                _stems.Add((dot, label));
            }
        }
        for (var i = 0; i < w.Stems.Count; i++)
        {
            var s = w.Stems[i];
            var p = PointFor(s.Azimuth, s.Distance);
            var (dot, label) = _stems[i];
            Canvas.SetLeft(dot, p.X - 13);
            Canvas.SetTop(dot, p.Y - 13);
            dot.Opacity = 0.45 + 0.55 * Math.Min(1, s.Peak * 3);
            label.Text = s.Title;
            Canvas.SetLeft(label, p.X - 20);
            Canvas.SetTop(label, p.Y + 14);
            AutomationProperties.SetName(dot, $"{s.Title}, {s.Azimuth:0} degrees, {s.Distance:0.0} metres");
        }
    }

    private Point PointFor(float azimuth, float distance)
    {
        var r = Math.Min(distance, MaxDistance) / MaxDistance * Radius;
        var a = azimuth * Math.PI / 180;
        return new Point(Centre.X + Math.Sin(a) * r, Centre.Y - Math.Cos(a) * r);
    }

    private void OnMoved(object sender, PointerRoutedEventArgs e)
    {
        if (_dragging is not int i) return;
        var p = e.GetCurrentPoint(_canvas).Position;
        double dx = p.X - Centre.X, dy = Centre.Y - p.Y;
        var az = (float)(Math.Atan2(dx, dy) * 180 / Math.PI);
        var dist = (float)Math.Min(1, Math.Max(0.08, Math.Sqrt(dx * dx + dy * dy) / Radius)) * MaxDistance;
        EngineService.Shared.MoveStem(i, az, dist);
    }

    private void OnKey(object sender, KeyRoutedEventArgs e)
    {
        var w = EngineService.Shared.World;
        if (_focused >= w.Stems.Count) return;
        var s = w.Stems[_focused];
        switch (e.Key)
        {
            case VirtualKey.Left: EngineService.Shared.MoveStem(_focused, s.Azimuth - 15, s.Distance); break;
            case VirtualKey.Right: EngineService.Shared.MoveStem(_focused, s.Azimuth + 15, s.Distance); break;
            case VirtualKey.Up: EngineService.Shared.MoveStem(_focused, s.Azimuth, Math.Max(0.3f, s.Distance - 0.25f)); break;
            case VirtualKey.Down: EngineService.Shared.MoveStem(_focused, s.Azimuth, Math.Min(MaxDistance, s.Distance + 0.25f)); break;
            case VirtualKey.Tab: return;
            default:
                if (e.Key >= VirtualKey.Number1 && e.Key <= VirtualKey.Number8) _focused = (int)(e.Key - VirtualKey.Number1);
                else return;
                break;
        }
        e.Handled = true;
    }
}
