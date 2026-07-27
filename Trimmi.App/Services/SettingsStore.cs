using System.Text.Json;
using Trimmi.Core.Models;

namespace Trimmi_App.Services;

public sealed class SettingsStore
{
    private readonly string _settingsPath;
    private readonly object _gate = new();
    private AppSettings? _snapshot;

    public SettingsStore(string? settingsPath = null)
    {
        _settingsPath = settingsPath ?? Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "Trimmi",
            "settings.json");
        Directory.CreateDirectory(Path.GetDirectoryName(_settingsPath)!);
    }

    public AppSettings Load()
    {
        lock (_gate)
        {
            if (_snapshot is not null)
            {
                return Clone(_snapshot);
            }

            if (!File.Exists(_settingsPath))
            {
                _snapshot = CreateDefault();
                return Clone(_snapshot);
            }

            try
            {
                var json = File.ReadAllText(_settingsPath);
                _snapshot = JsonSerializer.Deserialize(json, SettingsJsonContext.Default.AppSettings) ?? CreateDefault();
            }
            catch
            {
                _snapshot = CreateDefault();
            }

            return Clone(_snapshot);
        }
    }

    public void Save(AppSettings settings)
    {
        lock (_gate)
        {
            var json = JsonSerializer.Serialize(settings, SettingsJsonContext.Default.AppSettings);
            File.WriteAllText(_settingsPath, json);
            _snapshot = Clone(settings);
        }
    }

    private static AppSettings CreateDefault() => new();

    private static AppSettings Clone(AppSettings source) => new()
    {
        UiSoundsEnabled = source.UiSoundsEnabled,
        UiSoundVolume = source.UiSoundVolume,
    };
}
