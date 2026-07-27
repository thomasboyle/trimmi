namespace Trimmi.Core.Models;

public sealed class AppUpdateInfo
{
    public required string Version { get; init; }
    public required string DownloadUrl { get; init; }
    public string ReleaseNotes { get; init; } = "";
    public string ReleasePageUrl { get; init; } = "";
}
