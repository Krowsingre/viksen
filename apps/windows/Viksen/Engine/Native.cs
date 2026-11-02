using System.Runtime.InteropServices;

namespace Viksen.Engine;

/// <summary>P/Invoke surface of platform/capi/ViksenEngine.h. Keep in sync.</summary>
public static class Native
{
    private const string Dll = "viksen_engine";
    public const int ApiVersion = 1;

    public enum Param
    {
        BusGainMusic = 0, BusGainAtmosphere = 1, BusGainMicRaw = 2, BusGainMicTransformed = 3,
        BusMuteMusic = 4, BusMuteAtmosphere = 5, BusMuteMicRaw = 6, BusMuteMicTransformed = 7,
        MasterGain = 8, LimiterCeilingDb = 9,
        BedEnabled = 10, BedLevel = 11, BedColourHz = 12,
        MicToneHz = 13, MicEchoSeconds = 14, MicFeedback = 15, MicWet = 16,
        MicShiftHz = 17, MicShiftMix = 18,
        RoomMemoryEnabled = 19, RoomMemoryLevel = 20, RoomMemoryPitch = 21,
        Looping = 22,
    }

    public enum RecordMode { FullMix = 1, WithoutMic = 2, AtmosphereOnly = 3, EnvironmentOnly = 4 }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct Meters
    {
        public float OutputPeakL, OutputPeakR;
        public float BusMusic, BusAtmosphere, BusMicRaw, BusMicTransformed;
        public float LimiterGain;
        public double PositionSeconds;
        public double DurationSeconds;
        public int Playing, MicActive, Panicked;
        public ulong Underruns;
        public float EnvironmentLevelDb;
        public int Quiet, Loud, Still, Moving;
        public int Loudness;
        public int RoomMemoryVoices;
        public int LunarDay;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 24)] public string MoonPhase;
    }

    [DllImport(Dll)] public static extern int vk_api_version();
    [DllImport(Dll)] public static extern IntPtr vk_create();
    [DllImport(Dll)] public static extern void vk_destroy(IntPtr e);
    [DllImport(Dll)] public static extern IntPtr vk_last_error(IntPtr e);

    [DllImport(Dll)] public static extern int vk_start_audio(IntPtr e);
    [DllImport(Dll)] public static extern void vk_stop_audio(IntPtr e);
    [DllImport(Dll)] public static extern int vk_audio_running(IntPtr e);
    [DllImport(Dll)] public static extern double vk_sample_rate(IntPtr e);
    [DllImport(Dll)] public static extern IntPtr vk_device_name(IntPtr e);
    [DllImport(Dll)] public static extern int vk_set_mic_enabled(IntPtr e, int enabled);
    [DllImport(Dll)] public static extern int vk_mic_enabled(IntPtr e);
    [DllImport(Dll)] public static extern void vk_panic(IntPtr e);
    [DllImport(Dll)] public static extern void vk_clear_panic(IntPtr e);

    [DllImport(Dll)] public static extern void vk_set_param(IntPtr e, Param p, float value);
    [DllImport(Dll)] public static extern float vk_get_param(IntPtr e, Param p);
    [DllImport(Dll)] public static extern void vk_set_listener_yaw(IntPtr e, float degrees);
    [DllImport(Dll)] public static extern void vk_tick(IntPtr e, double intensity, out Meters meters);

    [DllImport(Dll)] public static extern IntPtr vk_dimension_info(IntPtr e, [MarshalAs(UnmanagedType.LPUTF8Str)] string folder);
    [DllImport(Dll)] public static extern int vk_load_dimension(IntPtr e, [MarshalAs(UnmanagedType.LPUTF8Str)] string folder);
    [DllImport(Dll)] public static extern int vk_load_studio_stems(IntPtr e, IntPtr[] paths, int count);
    [DllImport(Dll)] public static extern void vk_world_start(IntPtr e, int quick);
    [DllImport(Dll)] public static extern void vk_world_stop(IntPtr e, int quick);
    [DllImport(Dll)] public static extern void vk_world_seek(IntPtr e, double seconds);
    [DllImport(Dll)] public static extern void vk_world_unload(IntPtr e);
    [DllImport(Dll)] public static extern void vk_world_set_stem(IntPtr e, int stem, float gainDb, int muted);
    [DllImport(Dll)] public static extern void vk_world_set_spatial(IntPtr e, int enabled);
    [DllImport(Dll)] public static extern void vk_world_move_stem(IntPtr e, int stem, float azimuth, float elevation, float distance);
    [DllImport(Dll)] public static extern void vk_world_reset_positions(IntPtr e);
    [DllImport(Dll)] public static extern int vk_binaural_hrtf();
    [DllImport(Dll)] public static extern IntPtr vk_world_status(IntPtr e);

    [DllImport(Dll)] public static extern int vk_start_recording(IntPtr e, [MarshalAs(UnmanagedType.LPUTF8Str)] string path, RecordMode mode);
    [DllImport(Dll)] public static extern double vk_stop_recording(IntPtr e);
    [DllImport(Dll)] public static extern int vk_recording(IntPtr e);
    [DllImport(Dll)] public static extern double vk_recording_seconds(IntPtr e);

    public static string Str(IntPtr p) => p == IntPtr.Zero ? "" : Marshal.PtrToStringUTF8(p) ?? "";
}
