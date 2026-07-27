using System.Diagnostics;
using System.Globalization;
using System.Text;
using System.Text.RegularExpressions;
using Trimmi.Core.Models;

namespace Trimmi.Core.Services;

public sealed partial class ExportService
{
    [GeneratedRegex(@"time=(\d+):(\d+):(\d+(?:\.\d+)?)", RegexOptions.CultureInvariant)]
    private static partial Regex TimeProgressRegex();

    public async Task ExportAsync(
        ExportRequest request,
        IProgress<ExportProgress>? progress = null,
        CancellationToken cancellationToken = default)
    {
        var ffmpeg = FfmpegToolPaths.FindFfmpeg();
        if (string.IsNullOrEmpty(ffmpeg))
            throw new InvalidOperationException(
                "ffmpeg.exe was not found next to Trimmi. Reinstall the application.");

        if (!File.Exists(request.InputPath))
            throw new FileNotFoundException("Input file is missing.", request.InputPath);

        var durationMs = Math.Max(1, request.EndMs - request.StartMs);
        var lastError = new StringBuilder();

        var (ok, error) = await RunOnceAsync(
            ffmpeg,
            BuildArgs(request, forceCpuFallback: false),
            durationMs,
            lastError,
            progress,
            cancellationToken).ConfigureAwait(false);

        if (ok)
        {
            progress?.Report(new ExportProgress { Percent = 100.0, Status = "Done" });
            return;
        }

        if (request.Encoder.IsGpu)
        {
            lastError.Clear();
            progress?.Report(new ExportProgress
            {
                Percent = 0.0,
                Status = "GPU encode failed — falling back to CPU…",
            });

            (ok, error) = await RunOnceAsync(
                ffmpeg,
                BuildArgs(request, forceCpuFallback: true),
                durationMs,
                lastError,
                progress,
                cancellationToken).ConfigureAwait(false);

            if (ok)
            {
                progress?.Report(new ExportProgress { Percent = 100.0, Status = "Done" });
                return;
            }
        }

        var msg = "Export failed.";
        var errText = lastError.ToString();
        if (errText.Contains("No NVENC capable devices", StringComparison.Ordinal)
            || errText.Contains("Cannot load nvcuda", StringComparison.Ordinal))
        {
            msg = "GPU encoder unavailable. Try a CPU encoder.";
        }
        else
        {
            var lines = errText.Split(['\r', '\n'], StringSplitOptions.RemoveEmptyEntries);
            if (lines.Length > 0)
                msg = lines[^1].Trim();
            else if (!string.IsNullOrWhiteSpace(error))
                msg = error;
        }

        throw new InvalidOperationException(msg);
    }

    private static async Task<(bool Ok, string Error)> RunOnceAsync(
        string ffmpeg,
        IReadOnlyList<string> args,
        long durationMs,
        StringBuilder lastError,
        IProgress<ExportProgress>? progress,
        CancellationToken cancellationToken)
    {
        progress?.Report(new ExportProgress { Percent = 0.0, Status = "Starting export…" });

        using var proc = new Process();
        proc.StartInfo = new ProcessStartInfo
        {
            FileName = ffmpeg,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
        };
        foreach (var a in args)
            proc.StartInfo.ArgumentList.Add(a);

        try
        {
            if (!proc.Start())
                return (false, "Failed to start ffmpeg.");
        }
        catch (Exception ex)
        {
            return (false, "Failed to start ffmpeg: " + ex.Message);
        }

        await using var reg = cancellationToken.Register(() => TryKill(proc));

        var stderrDone = ReadStreamAsync(proc.StandardError, lastError, durationMs, progress, cancellationToken);
        var stdoutDone = ReadStreamAsync(proc.StandardOutput, lastError, durationMs, progress, cancellationToken);

        try
        {
            await proc.WaitForExitAsync(cancellationToken).ConfigureAwait(false);
        }
        catch (OperationCanceledException)
        {
            TryKill(proc);
            throw;
        }

        await Task.WhenAll(stderrDone, stdoutDone).ConfigureAwait(false);

        if (cancellationToken.IsCancellationRequested)
            throw new OperationCanceledException(cancellationToken);

        if (proc.ExitCode == 0)
            return (true, "");

        return (false, $"ffmpeg exited with code {proc.ExitCode}.");
    }

    private static async Task ReadStreamAsync(
        StreamReader reader,
        StringBuilder lastError,
        long durationMs,
        IProgress<ExportProgress>? progress,
        CancellationToken cancellationToken)
    {
        var buffer = new char[4096];
        while (true)
        {
            var read = await reader.ReadAsync(buffer.AsMemory(0, buffer.Length), cancellationToken)
                .ConfigureAwait(false);
            if (read <= 0)
                break;

            var text = new string(buffer, 0, read);
            lock (lastError)
                lastError.Append(text);

            ParseProgress(text, durationMs, progress);
        }
    }

    private static void ParseProgress(string text, long durationMs, IProgress<ExportProgress>? progress)
    {
        Match? last = null;
        foreach (Match m in TimeProgressRegex().Matches(text))
            last = m;

        if (last is null)
            return;

        var hours = double.Parse(last.Groups[1].Value, CultureInfo.InvariantCulture);
        var mins = double.Parse(last.Groups[2].Value, CultureInfo.InvariantCulture);
        var secs = double.Parse(last.Groups[3].Value, CultureInfo.InvariantCulture);
        var currentMs = (hours * 3600.0 + mins * 60.0 + secs) * 1000.0;
        var pct = Math.Clamp((currentMs / durationMs) * 100.0, 0.0, 99.0);
        progress?.Report(new ExportProgress
        {
            Percent = pct,
            Status = $"Encoding… {(int)pct}%",
        });
    }

    internal static List<string> BuildArgs(ExportRequest request, bool forceCpuFallback)
    {
        var args = new List<string>
        {
            "-y",
            "-hide_banner",
            "-ss",
            (request.StartMs / 1000.0).ToString("F3", CultureInfo.InvariantCulture),
            "-i",
            request.InputPath,
            "-t",
            (Math.Max(1, request.EndMs - request.StartMs) / 1000.0)
                .ToString("F3", CultureInfo.InvariantCulture),
        };

        var streamCopy = request.Format.VideoCodecHint == "copy"
                         || request.Encoder.FfmpegEncoder == "copy";

        if (streamCopy)
        {
            args.AddRange(["-c", "copy", "-avoid_negative_ts", "make_zero"]);
        }
        else
        {
            var vEncoder = request.Encoder.FfmpegEncoder;
            if (forceCpuFallback && request.Encoder.IsGpu)
                vEncoder = PickCpuAv1Encoder();

            var hint = request.Format.VideoCodecHint;
            if (!forceCpuFallback && !request.Encoder.IsGpu)
            {
                if (hint == "h264" && vEncoder.Contains("av1", StringComparison.Ordinal))
                    vEncoder = "libx264";
                else if (hint == "hevc" && vEncoder.Contains("av1", StringComparison.Ordinal))
                    vEncoder = "libx265";
            }

            args.AddRange(["-c:v", vEncoder]);

            if (vEncoder == "av1_nvenc")
            {
                args.AddRange(["-preset", "p4", "-cq", "28", "-b:v", "0"]);
            }
            else if (vEncoder == "av1_amf")
            {
                args.AddRange(["-quality", "balanced", "-rc", "cqp", "-qp_i", "28"]);
            }
            else if (vEncoder == "av1_qsv")
            {
                args.AddRange(["-global_quality", "28"]);
            }
            else if (vEncoder == "libsvtav1")
            {
                args.AddRange(["-crf", "28", "-preset", "6"]);
            }
            else if (vEncoder == "libaom-av1")
            {
                args.AddRange(["-crf", "30", "-b:v", "0", "-cpu-used", "6"]);
            }
            else if (vEncoder == "libx265" || vEncoder.Contains("hevc", StringComparison.Ordinal))
            {
                args.AddRange(["-crf", "23"]);
            }
            else if (vEncoder == "libx264" || vEncoder.Contains("h264", StringComparison.Ordinal))
            {
                args.AddRange(["-crf", "20", "-preset", "medium"]);
            }

            if (request.Format.Container == "webm")
                args.AddRange(["-c:a", "libopus", "-b:a", "128k"]);
            else
                args.AddRange(["-c:a", "aac", "-b:a", "192k"]);
        }

        if (request.Format.Container == "mp4")
            args.AddRange(["-movflags", "+faststart"]);

        args.Add(request.OutputPath);
        return args;
    }

    private static string PickCpuAv1Encoder()
    {
        var ffmpeg = FfmpegToolPaths.FindFfmpeg();
        if (string.IsNullOrEmpty(ffmpeg))
            return "libaom-av1";

        try
        {
            using var probe = new Process();
            probe.StartInfo = new ProcessStartInfo
            {
                FileName = ffmpeg,
                Arguments = "-hide_banner -encoders",
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                CreateNoWindow = true,
            };
            if (!probe.Start())
                return "libaom-av1";

            var output = probe.StandardOutput.ReadToEnd();
            if (!probe.WaitForExit(10_000))
            {
                TryKill(probe);
                return "libaom-av1";
            }

            if (output.Contains("libsvtav1", StringComparison.Ordinal))
                return "libsvtav1";
            if (output.Contains("libaom-av1", StringComparison.Ordinal))
                return "libaom-av1";
            return "libx264";
        }
        catch
        {
            return "libaom-av1";
        }
    }

    private static void TryKill(Process proc)
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
