namespace Trimmi.Core.Models;

public sealed class AppSettings
{
    public bool UiSoundsEnabled { get; set; } = true;

    /// <summary>UI sound loudness from 0 (mute) to 100 (3× the original default level). Default 50 matches the original level.</summary>
    public int UiSoundVolume { get; set; } = 50;
}
