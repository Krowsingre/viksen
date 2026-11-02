namespace Viksen.Shared;

/// <summary>Names for the engine's moon phase ids (computed on-device by the engine).</summary>
public static class Lunar
{
    public static string Title(string id) => id switch
    {
        "new" => "New Moon",
        "waxingCrescent" => "Waxing Crescent",
        "firstQuarter" => "First Quarter",
        "waxingGibbous" => "Waxing Gibbous",
        "full" => "Full Moon",
        "waningGibbous" => "Waning Gibbous",
        "lastQuarter" => "Last Quarter",
        "waningCrescent" => "Waning Crescent",
        _ => "Moon",
    };

    public static string Glyph(string id) => id switch
    {
        "new" => "🌑", "waxingCrescent" => "🌒", "firstQuarter" => "🌓", "waxingGibbous" => "🌔",
        "full" => "🌕", "waningGibbous" => "🌖", "lastQuarter" => "🌗", "waningCrescent" => "🌘",
        _ => "🌙",
    };
}
