using System.Text.Json;
using Trimmi.Core.Models;

namespace Trimmi.Core.Services;

public sealed class MediaProbeService
{
    public async Task<VideoMetadata> ProbeAsync(string path, CancellationToken cancellationToken = default)
    {
        var meta = new VideoMetadata
        {
            Path = path,
            FileName = Path.GetFileName(path) ?? "",
        };

        var ffprobe = FfmpegToolPaths.FindFfprobe();
        if (string.IsNullOrEmpty(ffprobe))
            return meta;

        using var proc = new System.Diagnostics.Process();
        proc.StartInfo = new System.Diagnostics.ProcessStartInfo
        {
            FileName = ffprobe,
            ArgumentList =
            {
                "-v", "quiet",
                "-print_format", "json",
                "-show_format",
                "-show_streams",
                path,
            },
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
        };

        try
        {
            if (!proc.Start())
                return meta;
        }
        catch
        {
            return meta;
        }

        var stdoutTask = proc.StandardOutput.ReadToEndAsync(cancellationToken);
        var stderrTask = proc.StandardError.ReadToEndAsync(cancellationToken);

        using var timeout = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        timeout.CancelAfter(TimeSpan.FromSeconds(20));

        try
        {
            await proc.WaitForExitAsync(timeout.Token).ConfigureAwait(false);
        }
        catch (OperationCanceledException) when (!cancellationToken.IsCancellationRequested)
        {
            TryKill(proc);
            return meta;
        }
        catch (OperationCanceledException)
        {
            TryKill(proc);
            throw;
        }

        var json = await stdoutTask.ConfigureAwait(false);
        _ = await stderrTask.ConfigureAwait(false);

        if (string.IsNullOrWhiteSpace(json))
            return meta;

        try
        {
            using var doc = JsonDocument.Parse(json);
            var root = doc.RootElement;

            long durationMs = 0;
            if (root.TryGetProperty("format", out var format)
                && format.TryGetProperty("duration", out var durEl))
            {
                var durStr = durEl.ValueKind == JsonValueKind.String
                    ? durEl.GetString()
                    : durEl.ToString();
                if (double.TryParse(durStr, System.Globalization.NumberStyles.Float,
                        System.Globalization.CultureInfo.InvariantCulture, out var durSec))
                    durationMs = (long)(durSec * 1000.0);
            }

            var width = 0;
            var height = 0;
            var videoCodec = "";
            var audioCodec = "";
            double frameRate = 0;

            if (root.TryGetProperty("streams", out var streams)
                && streams.ValueKind == JsonValueKind.Array)
            {
                foreach (var s in streams.EnumerateArray())
                {
                    var codecType = s.TryGetProperty("codec_type", out var ct) ? ct.GetString() : null;
                    if (codecType == "video" && width == 0)
                    {
                        width = s.TryGetProperty("width", out var w) ? w.GetInt32() : 0;
                        height = s.TryGetProperty("height", out var h) ? h.GetInt32() : 0;
                        videoCodec = s.TryGetProperty("codec_name", out var vc) ? vc.GetString() ?? "" : "";
                        if (s.TryGetProperty("avg_frame_rate", out var rateEl))
                        {
                            var rate = rateEl.GetString() ?? "";
                            var parts = rate.Split('/');
                            if (parts.Length == 2
                                && double.TryParse(parts[0], System.Globalization.NumberStyles.Float,
                                    System.Globalization.CultureInfo.InvariantCulture, out var num)
                                && double.TryParse(parts[1], System.Globalization.NumberStyles.Float,
                                    System.Globalization.CultureInfo.InvariantCulture, out var den)
                                && den > 0)
                            {
                                frameRate = num / den;
                            }
                        }
                    }
                    else if (codecType == "audio" && string.IsNullOrEmpty(audioCodec))
                    {
                        audioCodec = s.TryGetProperty("codec_name", out var ac) ? ac.GetString() ?? "" : "";
                    }
                }
            }

            return new VideoMetadata
            {
                Path = path,
                FileName = Path.GetFileName(path) ?? "",
                DurationMs = durationMs,
                Width = width,
                Height = height,
                VideoCodec = videoCodec,
                AudioCodec = audioCodec,
                FrameRate = frameRate,
                Valid = durationMs > 0 || (width > 0 && height > 0),
            };
        }
        catch (JsonException)
        {
            return meta;
        }
    }

    private static void TryKill(System.Diagnostics.Process proc)
    {
        try
        {
            if (!proc.HasExited)
                proc.Kill(entireProcessTree: true);
        }
        catch
        {
            // Best-effort.
        }
    }
}
