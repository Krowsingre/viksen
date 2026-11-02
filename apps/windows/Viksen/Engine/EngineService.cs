using System.Runtime.InteropServices;
using System.Text.Json;
using Microsoft.UI.Dispatching;
using Viksen.Shared;

namespace Viksen.Engine;

/// <summary>
/// The one engine for the app (UI thread only). Mirrors the iPhone EngineController:
/// it owns the native engine, ticks it ~30 times a second and remembers what is loaded.
/// </summary>
public sealed class EngineService : IDisposable
{
    public static EngineService Shared { get; } = new();

    private IntPtr _e;
    private DispatcherQueueTimer? _timer;

    public Native.Meters Meters { get; private set; }
    public WorldStatus World { get; private set; } = new();
    public string? LoadedDimensionId { get; private set; }
    public bool StudioLoaded { get; private set; }
    public IReadOnlyList<string> StudioStemPaths { get; private set; } = Array.Empty<string>();
    public string? LastError { get; private set; }
    public bool Available => _e != IntPtr.Zero;

    /// <summary>Raised on every tick (UI thread).</summary>
    public event Action? Ticked;

    private EngineService()
    {
        try
        {
            if (Native.vk_api_version() != Native.ApiVersion)
            {
                LastError = "Engine library version mismatch.";
                AppLog.Error(LastError);
                return;
            }
            _e = Native.vk_create();
            AppLog.Info($"Engine created (HRTF: {(Native.vk_binaural_hrtf() == 1 ? "Steam Audio" : "simplified panner")})");
        }
        catch (DllNotFoundException ex)
        {
            LastError = "The audio engine (viksen_engine.dll) is missing from this build.";
            AppLog.Error($"{LastError} {ex.Message}");
        }
    }

    public void StartTicking(DispatcherQueue queue)
    {
        if (_timer != null) return;
        _timer = queue.CreateTimer();
        _timer.Interval = TimeSpan.FromMilliseconds(33);
        _timer.Tick += (_, _) => Tick();
        _timer.Start();
    }

    private void Tick()
    {
        if (!Available) return;
        Native.vk_tick(_e, -1.0, out var m);
        Meters = m;
        World = ReadWorld();
        Ticked?.Invoke();
    }

    private string Error() => Native.Str(Native.vk_last_error(_e));

    // ---------------------------------------------------------------- audio

    public bool AudioRunning => Available && Native.vk_audio_running(_e) == 1;
    public double SampleRate => Available ? Native.vk_sample_rate(_e) : 0;
    public string DeviceName => Available ? Native.Str(Native.vk_device_name(_e)) : "";

    public bool StartAudio()
    {
        if (!Available) return false;
        if (Native.vk_start_audio(_e) == 1)
        {
            AppLog.Info($"Audio started: {DeviceName} @ {SampleRate:0} Hz");
            return true;
        }
        LastError = Error();
        AppLog.Error($"Audio start failed: {LastError}");
        return false;
    }

    public void StopAudio()
    {
        if (!Available) return;
        Native.vk_stop_audio(_e);
        AppLog.Info("Audio stopped");
    }

    public bool MicEnabled => Available && Native.vk_mic_enabled(_e) == 1;

    /// <summary>Only ever called from an explicit user action.</summary>
    public bool SetMicEnabled(bool on)
    {
        if (!Available) return false;
        if (Native.vk_set_mic_enabled(_e, on ? 1 : 0) == 1)
        {
            AppLog.Info(on ? "Microphone on" : "Microphone off");
            return true;
        }
        LastError = Error();
        AppLog.Warning($"Microphone change failed: {LastError}");
        return false;
    }

    public void Panic()
    {
        if (!Available) return;
        Native.vk_panic(_e);
        AppLog.Warning("Panic: silenced");
    }

    public void ClearPanic() { if (Available) Native.vk_clear_panic(_e); }

    public float Get(Native.Param p) => Available ? Native.vk_get_param(_e, p) : 0f;
    public void Set(Native.Param p, float v) { if (Available) Native.vk_set_param(_e, p, v); }
    public bool GetFlag(Native.Param p) => Get(p) != 0f;
    public void SetFlag(Native.Param p, bool v) => Set(p, v ? 1f : 0f);

    // ---------------------------------------------------------------- Dimensions

    public static string BuiltInFolder => Path.Combine(AppContext.BaseDirectory, "Worlds");
    public static string UserFolder => Path.Combine(AppPaths.Data, "Dimensions");

    public List<DimensionInfo> AvailableDimensions()
    {
        var list = new List<DimensionInfo>();
        if (!Available) return list;
        var seen = new HashSet<string>();
        foreach (var (root, user) in new[] { (BuiltInFolder, false), (UserFolder, true) })
        {
            if (!Directory.Exists(root)) continue;
            foreach (var dir in Directory.GetDirectories(root).OrderBy(d => d, StringComparer.OrdinalIgnoreCase))
            {
                var json = Native.Str(Native.vk_dimension_info(_e, dir));
                if (json.Length == 0) continue;
                var info = JsonSerializer.Deserialize<DimensionInfo>(json);
                if (info == null || !seen.Add(info.Id)) continue;   // built-ins can never be shadowed
                info.Folder = dir;
                info.UserMade = user;
                list.Add(info);
            }
        }
        return list;
    }

    public bool EnterDimension(DimensionInfo d)
    {
        if (!Available) return false;
        if (!AudioRunning && !StartAudio()) return false;
        if (Native.vk_load_dimension(_e, d.Folder) != 1)
        {
            LastError = Error();
            AppLog.Error($"Dimension {d.Id} failed: {LastError}");
            return false;
        }
        Native.vk_world_start(_e, 0);
        LoadedDimensionId = d.Id;
        StudioLoaded = false;
        AppLog.Info($"Entered Dimension {d.Id}");
        return true;
    }

    public void LeaveDimension()
    {
        if (!Available) return;
        Native.vk_world_stop(_e, 0);
        AppLog.Info($"Left Dimension {LoadedDimensionId}");
        LoadedDimensionId = null;
    }

    // ---------------------------------------------------------------- Studio

    public bool LoadStudio(IReadOnlyList<string> paths)
    {
        if (!Available) return false;
        if (!AudioRunning && !StartAudio()) return false;
        var ptrs = paths.Select(p => Marshal.StringToCoTaskMemUTF8(p)).ToArray();
        try
        {
            if (Native.vk_load_studio_stems(_e, ptrs, ptrs.Length) != 1)
            {
                LastError = Error();
                AppLog.Error($"Studio load failed: {LastError}");
                return false;
            }
        }
        finally
        {
            foreach (var p in ptrs) Marshal.FreeCoTaskMem(p);
        }
        StudioLoaded = true;
        StudioStemPaths = paths.ToArray();
        LoadedDimensionId = null;
        AppLog.Info($"Studio: {paths.Count} stems loaded");
        return true;
    }

    public void StudioPlay() { if (Available) Native.vk_world_start(_e, 1); }
    public void StudioPause() { if (Available) Native.vk_world_stop(_e, 1); }
    public void Seek(double seconds) { if (Available) Native.vk_world_seek(_e, seconds); }
    public void SetStem(int index, float gainDb, bool muted) { if (Available) Native.vk_world_set_stem(_e, index, gainDb, muted ? 1 : 0); }

    // ---------------------------------------------------------------- 3D

    public static bool BinauralHrtf { get { try { return Native.vk_binaural_hrtf() == 1; } catch (DllNotFoundException) { return false; } } }
    public void SetSpatial(bool on) { if (Available) Native.vk_world_set_spatial(_e, on ? 1 : 0); }
    public void MoveStem(int i, float azimuth, float distance) { if (Available) Native.vk_world_move_stem(_e, i, azimuth, 0, distance); }
    public void ResetPositions() { if (Available) Native.vk_world_reset_positions(_e); }

    private WorldStatus ReadWorld()
    {
        var json = Native.Str(Native.vk_world_status(_e));
        return (json.Length > 0 ? JsonSerializer.Deserialize<WorldStatus>(json) : null) ?? new WorldStatus();
    }

    // ---------------------------------------------------------------- recording

    public bool Recording => Available && Native.vk_recording(_e) == 1;
    public double RecordingSeconds => Available ? Native.vk_recording_seconds(_e) : 0;

    public string? StartRecording(Native.RecordMode mode)
    {
        if (!Available) return null;
        if (!AudioRunning && !StartAudio()) return null;
        Directory.CreateDirectory(AppPaths.Recordings);
        var path = Path.Combine(AppPaths.Recordings, $"Viksen {DateTime.Now:yyyy-MM-dd HH.mm.ss}.wav");
        if (Native.vk_start_recording(_e, path, mode) != 1)
        {
            LastError = Error();
            AppLog.Error($"Recording failed: {LastError}");
            return null;
        }
        AppLog.Info($"Recording started ({mode})");
        return path;
    }

    public double StopRecording()
    {
        if (!Available) return 0;
        var s = Native.vk_stop_recording(_e);
        AppLog.Info($"Recording stopped: {s:0.0} s");
        return s;
    }

    public void Dispose()
    {
        _timer?.Stop();
        if (_e != IntPtr.Zero) Native.vk_destroy(_e);
        _e = IntPtr.Zero;
    }
}
