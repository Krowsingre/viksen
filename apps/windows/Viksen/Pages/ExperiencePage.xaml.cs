using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Navigation;
using Viksen.Engine;
using P = Viksen.Engine.Native.Param;

namespace Viksen.Pages;

public sealed partial class ExperiencePage : Page
{
    private readonly EngineService _engine = EngineService.Shared;
    private bool _loading = true;   // sliders fire ValueChanged while the page is built

    public ExperiencePage() => InitializeComponent();

    protected override void OnNavigatedTo(NavigationEventArgs e)
    {
        _loading = true;
        AudioSwitch.IsOn = _engine.AudioRunning;
        Master.Value = _engine.Get(P.MasterGain);
        BedSwitch.IsOn = _engine.GetFlag(P.BedEnabled);
        BedLevel.Value = _engine.Get(P.BedLevel);
        BedColour.Value = _engine.Get(P.BedColourHz);
        MicSwitch.IsOn = _engine.MicEnabled;
        RawLevel.Value = _engine.Get(P.BusGainMicRaw);
        Wet.Value = _engine.Get(P.MicWet);
        Echo.Value = _engine.Get(P.MicEchoSeconds);
        Feedback.Value = _engine.Get(P.MicFeedback);
        Tone.Value = _engine.Get(P.MicToneHz);
        Shift.Value = _engine.Get(P.MicShiftHz);
        ShiftMix.Value = _engine.Get(P.MicShiftMix);
        MemorySwitch.IsOn = _engine.GetFlag(P.RoomMemoryEnabled);
        MemoryLevel.Value = _engine.Get(P.RoomMemoryLevel);
        MemoryPitch.Value = _engine.Get(P.RoomMemoryPitch);
        _loading = false;
        _engine.Ticked += Refresh;
        Refresh();
    }

    protected override void OnNavigatedFrom(NavigationEventArgs e) => _engine.Ticked -= Refresh;

    private void Refresh()
    {
        var m = _engine.Meters;
        MeterMusic.Value = Math.Min(1, m.BusMusic);
        MeterAtmosphere.Value = Math.Min(1, m.BusAtmosphere);
        MeterMicRaw.Value = Math.Min(1, m.BusMicRaw);
        MeterMicTransformed.Value = Math.Min(1, m.BusMicTransformed);
        RouteText.Text = _engine.AudioRunning ? $"{_engine.DeviceName} · {_engine.SampleRate:0} Hz" : "Sound is off.";
        EnvText.Text = m.MicActive != 0
            ? $"{(m.Quiet != 0 ? "Quiet" : m.Loud != 0 ? "Loud" : "Moderate")} · {m.EnvironmentLevelDb:0} dB" +
              (m.RoomMemoryVoices > 0 ? $" · {m.RoomMemoryVoices} memories" : "")
            : "";
        if (AudioSwitch.IsOn != _engine.AudioRunning) { _loading = true; AudioSwitch.IsOn = _engine.AudioRunning; _loading = false; }
    }

    private async Task Say(string title, string text)
    {
        var d = new ContentDialog { Title = title, Content = text, CloseButtonText = "OK", XamlRoot = XamlRoot };
        await d.ShowAsync();
    }

    private async void OnAudioToggled(object sender, RoutedEventArgs e)
    {
        if (_loading) return;
        if (AudioSwitch.IsOn)
        {
            if (!_engine.StartAudio()) { AudioSwitch.IsOn = false; await Say("Sound could not start", _engine.LastError ?? ""); }
        }
        else _engine.StopAudio();
    }

    private async void OnMic(object sender, RoutedEventArgs e)
    {
        if (_loading) return;
        if (!_engine.SetMicEnabled(MicSwitch.IsOn))
        {
            _loading = true; MicSwitch.IsOn = false; _loading = false;
            await Say("Microphone unavailable",
                $"{_engine.LastError}\n\nCheck Settings > Privacy & security > Microphone and that a microphone is connected.");
        }
    }

    private void OnPanic(object sender, RoutedEventArgs e)
    {
        _engine.Panic();
        _engine.ClearPanic();   // ready again, but paused and silent until the user plays
    }

    private void Set(P p, RangeBaseValueChangedEventArgs e) { if (!_loading) _engine.Set(p, (float)e.NewValue); }
    private void OnMaster(object s, RangeBaseValueChangedEventArgs e) => Set(P.MasterGain, e);
    private void OnBed(object s, RoutedEventArgs e) { if (!_loading) _engine.SetFlag(P.BedEnabled, BedSwitch.IsOn); }
    private void OnBedLevel(object s, RangeBaseValueChangedEventArgs e) => Set(P.BedLevel, e);
    private void OnBedColour(object s, RangeBaseValueChangedEventArgs e) => Set(P.BedColourHz, e);
    private void OnRawLevel(object s, RangeBaseValueChangedEventArgs e) => Set(P.BusGainMicRaw, e);
    private void OnWet(object s, RangeBaseValueChangedEventArgs e) => Set(P.MicWet, e);
    private void OnEcho(object s, RangeBaseValueChangedEventArgs e) => Set(P.MicEchoSeconds, e);
    private void OnFeedback(object s, RangeBaseValueChangedEventArgs e) => Set(P.MicFeedback, e);
    private void OnTone(object s, RangeBaseValueChangedEventArgs e) => Set(P.MicToneHz, e);
    private void OnShift(object s, RangeBaseValueChangedEventArgs e) => Set(P.MicShiftHz, e);
    private void OnShiftMix(object s, RangeBaseValueChangedEventArgs e) => Set(P.MicShiftMix, e);
    private void OnMemory(object s, RoutedEventArgs e) { if (!_loading) _engine.SetFlag(P.RoomMemoryEnabled, MemorySwitch.IsOn); }
    private void OnMemoryLevel(object s, RangeBaseValueChangedEventArgs e) => Set(P.RoomMemoryLevel, e);
    private void OnMemoryPitch(object s, RangeBaseValueChangedEventArgs e) => Set(P.RoomMemoryPitch, e);
}
