using System.Text.Json.Serialization;

namespace Viksen.Engine;

public sealed class DimensionInfo
{
    [JsonPropertyName("id")] public string Id { get; set; } = "";
    [JsonPropertyName("name")] public string Name { get; set; } = "";
    [JsonPropertyName("artist")] public string Artist { get; set; } = "";
    [JsonPropertyName("key")] public string Key { get; set; } = "";
    [JsonPropertyName("tier")] public string Tier { get; set; } = "free";
    [JsonPropertyName("tempo")] public double Tempo { get; set; }
    [JsonPropertyName("stems")] public int StemCount { get; set; }
    [JsonPropertyName("audioAvailable")] public bool AudioAvailable { get; set; }
    [JsonIgnore] public string Folder { get; set; } = "";
    [JsonIgnore] public bool UserMade { get; set; }
    [JsonIgnore] public string Subtitle =>
        $"{(string.IsNullOrEmpty(Artist) ? "" : Artist + " · ")}{Tempo:0} BPM · {Key} · {StemCount} stems" +
        (AudioAvailable ? "" : " · music not in this build");
}

public sealed class StemStatus
{
    [JsonPropertyName("title")] public string Title { get; set; } = "";
    [JsonPropertyName("azimuth")] public float Azimuth { get; set; }
    [JsonPropertyName("elevation")] public float Elevation { get; set; }
    [JsonPropertyName("distance")] public float Distance { get; set; }
    [JsonPropertyName("width")] public float Width { get; set; }
    [JsonPropertyName("gainDb")] public float GainDb { get; set; }
    [JsonPropertyName("peak")] public float Peak { get; set; }
    [JsonPropertyName("reasons")] public List<string> Reasons { get; set; } = new();
}

public sealed class WorldStatus
{
    [JsonPropertyName("loaded")] public bool Loaded { get; set; }
    [JsonPropertyName("playing")] public bool Playing { get; set; }
    [JsonPropertyName("id")] public string Id { get; set; } = "";
    [JsonPropertyName("name")] public string Name { get; set; } = "";
    [JsonPropertyName("position")] public double Position { get; set; }
    [JsonPropertyName("length")] public double Length { get; set; }
    [JsonPropertyName("master")] public float Master { get; set; }
    [JsonPropertyName("spatial")] public bool Spatial { get; set; }
    [JsonPropertyName("hrtf")] public bool Hrtf { get; set; }
    [JsonPropertyName("stems")] public List<StemStatus> Stems { get; set; } = new();
    [JsonPropertyName("adaptive")] public bool Adaptive { get; set; }
    [JsonPropertyName("composed")] public bool Composed { get; set; } = true;
    [JsonPropertyName("section")] public string Section { get; set; } = "";
    [JsonPropertyName("why")] public string Why { get; set; } = "";
}
