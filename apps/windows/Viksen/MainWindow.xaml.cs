using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Viksen.Engine;
using Viksen.Pages;

namespace Viksen;

public sealed partial class MainWindow : Window
{
    public MainWindow()
    {
        InitializeComponent();
        ExtendsContentIntoTitleBar = false;
        AppWindow.Resize(new Windows.Graphics.SizeInt32(1100, 780));
        Root.RequestedTheme = Settings.Appearance;
        EngineService.Shared.StartTicking(DispatcherQueue);
        Nav.SelectedItem = Nav.MenuItems[0];
        var timer = DispatcherQueue.CreateTimer();
        timer.Interval = TimeSpan.FromMilliseconds(1200);
        timer.IsRepeating = false;
        timer.Tick += (_, _) => Splash.Visibility = Visibility.Collapsed;
        timer.Start();
    }

    public void ApplyTheme(ElementTheme theme) => Root.RequestedTheme = theme;

    public void Navigate(string tag)
    {
        foreach (var item in Nav.MenuItems.OfType<NavigationViewItem>())
            if ((string)item.Tag == tag) { Nav.SelectedItem = item; return; }
    }

    private void OnNavSelectionChanged(NavigationView sender, NavigationViewSelectionChangedEventArgs args)
    {
        if (args.IsSettingsSelected) { ContentFrame.Navigate(typeof(SettingsPage)); return; }
        var page = ((args.SelectedItem as NavigationViewItem)?.Tag as string) switch
        {
            "experience" => typeof(ExperiencePage),
            "studio" => typeof(StudioPage),
            "dimensions" => typeof(DimensionsPage),
            "library" => typeof(LibraryPage),
            _ => typeof(HomePage),
        };
        ContentFrame.Navigate(page);
    }
}
