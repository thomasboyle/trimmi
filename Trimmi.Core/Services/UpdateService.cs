using System.Net.Http.Headers;
using System.Reflection;
using System.Text.Json;
using System.Text.RegularExpressions;
using Trimmi.Core.Models;

namespace Trimmi.Core.Services;

public sealed partial class UpdateService
{
    private const string GitHubOwner = "thomasboyle";
    private const string GitHubRepo = "trimmi";
    private const string LatestReleaseApi =
        $"https://api.github.com/repos/{GitHubOwner}/{GitHubRepo}/releases/latest";

    private static readonly HttpClient Http = CreateClient();

    public string CurrentVersion { get; } = ReadCurrentVersion();

    public async Task<AppUpdateInfo?> CheckForUpdateAsync(CancellationToken cancellationToken = default)
    {
        using var request = new HttpRequestMessage(HttpMethod.Get, LatestReleaseApi);
        using var response = await Http.SendAsync(request, cancellationToken).ConfigureAwait(false);
        if (!response.IsSuccessStatusCode)
            return null;

        await using var stream = await response.Content.ReadAsStreamAsync(cancellationToken)
            .ConfigureAwait(false);
        using var doc = await JsonDocument.ParseAsync(stream, cancellationToken: cancellationToken)
            .ConfigureAwait(false);

        var root = doc.RootElement;
        if (!root.TryGetProperty("tag_name", out var tagEl))
            return null;

        var tag = tagEl.GetString() ?? "";
        var remoteVersion = NormalizeVersion(tag);
        if (string.IsNullOrEmpty(remoteVersion))
            return null;

        if (!IsNewer(remoteVersion, CurrentVersion))
            return null;

        var downloadUrl = FindSetupAssetUrl(root, remoteVersion);
        if (string.IsNullOrEmpty(downloadUrl))
            return null;

        var notes = root.TryGetProperty("body", out var bodyEl) ? bodyEl.GetString() ?? "" : "";
        var pageUrl = root.TryGetProperty("html_url", out var htmlEl)
            ? htmlEl.GetString() ?? ""
            : $"https://github.com/{GitHubOwner}/{GitHubRepo}/releases/tag/v{remoteVersion}";

        return new AppUpdateInfo
        {
            Version = remoteVersion,
            DownloadUrl = downloadUrl,
            ReleaseNotes = notes,
            ReleasePageUrl = pageUrl,
        };
    }

    public async Task DownloadUpdateAsync(
        AppUpdateInfo update,
        string destinationPath,
        IProgress<double>? progress = null,
        CancellationToken cancellationToken = default)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(destinationPath)!);

        using var response = await Http.GetAsync(
                update.DownloadUrl,
                HttpCompletionOption.ResponseHeadersRead,
                cancellationToken)
            .ConfigureAwait(false);
        response.EnsureSuccessStatusCode();

        var total = response.Content.Headers.ContentLength ?? -1L;
        await using var input = await response.Content.ReadAsStreamAsync(cancellationToken)
            .ConfigureAwait(false);
        await using var output = new FileStream(
            destinationPath,
            FileMode.Create,
            FileAccess.Write,
            FileShare.None,
            81920,
            useAsync: true);

        var buffer = new byte[81920];
        long readTotal = 0;
        while (true)
        {
            var read = await input.ReadAsync(buffer.AsMemory(0, buffer.Length), cancellationToken)
                .ConfigureAwait(false);
            if (read <= 0)
                break;

            await output.WriteAsync(buffer.AsMemory(0, read), cancellationToken).ConfigureAwait(false);
            readTotal += read;
            if (total > 0)
                progress?.Report(Math.Clamp(100.0 * readTotal / total, 0, 100));
        }

        progress?.Report(100);
    }

    public static string DefaultInstallerPath(string version) =>
        Path.Combine(Path.GetTempPath(), "Trimmi", $"TrimmiSetup-{version}.exe");

    private static string? FindSetupAssetUrl(JsonElement root, string version)
    {
        if (!root.TryGetProperty("assets", out var assets) || assets.ValueKind != JsonValueKind.Array)
            return null;

        var preferred = $"TrimmiSetup-{version}.exe";
        string? fallback = null;

        foreach (var asset in assets.EnumerateArray())
        {
            var name = asset.TryGetProperty("name", out var nameEl) ? nameEl.GetString() ?? "" : "";
            var url = asset.TryGetProperty("browser_download_url", out var urlEl)
                ? urlEl.GetString()
                : null;
            if (string.IsNullOrEmpty(url))
                continue;

            if (string.Equals(name, preferred, StringComparison.OrdinalIgnoreCase))
                return url;

            if (fallback is null
                && name.StartsWith("TrimmiSetup-", StringComparison.OrdinalIgnoreCase)
                && name.EndsWith(".exe", StringComparison.OrdinalIgnoreCase))
            {
                fallback = url;
            }
        }

        return fallback;
    }

    private static string ReadCurrentVersion()
    {
        var asm = Assembly.GetEntryAssembly() ?? Assembly.GetExecutingAssembly();
        var info = asm.GetCustomAttribute<AssemblyInformationalVersionAttribute>()?.InformationalVersion;
        if (!string.IsNullOrWhiteSpace(info))
        {
            var normalized = NormalizeVersion(info);
            if (!string.IsNullOrEmpty(normalized))
                return normalized;
        }

        var ver = asm.GetName().Version;
        if (ver is not null)
            return $"{ver.Major}.{ver.Minor}.{ver.Build}";

        return "0.0.0";
    }

    internal static string NormalizeVersion(string raw)
    {
        if (string.IsNullOrWhiteSpace(raw))
            return "";

        var match = SemVerRegex().Match(raw.Trim().TrimStart('v', 'V'));
        return match.Success ? match.Value : "";
    }

    internal static bool IsNewer(string remote, string current)
    {
        if (!TryParseVersion(remote, out var r) || !TryParseVersion(current, out var c))
            return false;

        if (r.Major != c.Major) return r.Major > c.Major;
        if (r.Minor != c.Minor) return r.Minor > c.Minor;
        return r.Patch > c.Patch;
    }

    private static bool TryParseVersion(string text, out (int Major, int Minor, int Patch) version)
    {
        version = default;
        var parts = text.Split('.');
        if (parts.Length < 3)
            return false;
        if (!int.TryParse(parts[0], out var major)
            || !int.TryParse(parts[1], out var minor)
            || !int.TryParse(parts[2], out var patch))
        {
            return false;
        }

        version = (major, minor, patch);
        return true;
    }

    private static HttpClient CreateClient()
    {
        var client = new HttpClient { Timeout = TimeSpan.FromMinutes(10) };
        client.DefaultRequestHeaders.UserAgent.Add(
            new ProductInfoHeaderValue("Trimmi", ReadCurrentVersion()));
        client.DefaultRequestHeaders.Accept.Add(
            new MediaTypeWithQualityHeaderValue("application/vnd.github+json"));
        return client;
    }

    [GeneratedRegex(@"^\d+\.\d+\.\d+", RegexOptions.CultureInvariant)]
    private static partial Regex SemVerRegex();
}
