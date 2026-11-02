using Microsoft.UI.Xaml;
using Windows.Storage;

namespace Viksen;

/// <summary>Small per-user preferences.</summary>
public static class Settings
{
    private static ApplicationDataContainer? Store
    {
        get { try { return ApplicationData.Current.LocalSettings; } catch (InvalidOperationException) { return null; } }
    }

    public static ElementTheme Appearance
    {
        get => Store?.Values["appearance"] is string s && Enum.TryParse<ElementTheme>(s, out var t) ? t : ElementTheme.Dark;
        set { if (Store != null) Store.Values["appearance"] = value.ToString(); }
    }
}
