using Microsoft.UI.Xaml;
using Viksen.Engine;
using Viksen.Shared;

namespace Viksen;

public partial class App : Application
{
    public static MainWindow? Window { get; private set; }

    public App()
    {
        InitializeComponent();
        UnhandledException += (_, e) => AppLog.Error($"Unhandled: {e.Exception}");
    }

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        AppLog.Info(AppLog.LaunchBanner());
        Window = new MainWindow();
        Window.Closed += (_, _) =>
        {
            if (EngineService.Shared.Recording) EngineService.Shared.StopRecording();
            EngineService.Shared.Dispose();
            AppLog.Info("Closed");
        };
        Window.Activate();
    }
}
