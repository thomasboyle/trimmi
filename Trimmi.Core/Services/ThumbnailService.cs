using System.Diagnostics;
using System.Globalization;

namespace Trimmi.Core.Services;

public sealed class ThumbnailService
{
    public async Task<IReadOnlyList<string>> GenerateAsync(
        string path,
        long durationMs,
        int count,
        IProgress<(int index, string pngPath)>? progress = null,
        CancellationToken cancellationToken = default)
    {
        if (count <= 0 || durationMs <= 0)
            return Array.Empty<string>();

        var ffmpeg = FfmpegToolPaths.FindFfmpeg();
        if (string.IsNullOrEmpty(ffmpeg))
            throw new InvalidOperationException("ffmpeg.exe was not found for thumbnails.");

        var tmpDir = Path.Combine(Path.GetTempPath(), "TrimmiThumbs_" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(tmpDir);

        var results = new List<string>(count);

        try
        {
            for (var i = 0; i < count; i++)
            {
                cancellationToken.ThrowIfCancellationRequested();

                var targetMs = count == 1
                    ? 0L
                    : (long)(durationMs * i / (double)count);
                var outPath = Path.Combine(tmpDir, $"thumb_{i}.png");

                using var proc = new Process();
                proc.StartInfo = new ProcessStartInfo
                {
                    FileName = ffmpeg,
                    UseShellExecute = false,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    CreateNoWindow = true,
                };
                foreach (var a in new[]
                         {
                             "-hide_banner", "-loglevel", "error",
                             "-ss", (targetMs / 1000.0).ToString("F3", CultureInfo.InvariantCulture),
                             "-i", path,
                             "-frames:v", "1",
                             "-vf", "scale=160:-1",
                             "-y", outPath,
                         })
                {
                    proc.StartInfo.ArgumentList.Add(a);
                }

                await using var reg = cancellationToken.Register(() => TryKill(proc));

                try
                {
                    if (!proc.Start())
                        continue;
                }
                catch
                {
                    continue;
                }

                _ = proc.StandardOutput.ReadToEndAsync(cancellationToken);
                _ = proc.StandardError.ReadToEndAsync(cancellationToken);

                try
                {
                    using var timeout = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
                    timeout.CancelAfter(TimeSpan.FromSeconds(30));
                    await proc.WaitForExitAsync(timeout.Token).ConfigureAwait(false);
                }
                catch (OperationCanceledException) when (!cancellationToken.IsCancellationRequested)
                {
                    TryKill(proc);
                    continue;
                }
                catch (OperationCanceledException)
                {
                    TryKill(proc);
                    throw;
                }

                if (proc.ExitCode == 0 && File.Exists(outPath))
                {
                    results.Add(outPath);
                    progress?.Report((i, outPath));
                }
            }
        }
        catch
        {
            TryCleanup(tmpDir, results);
            throw;
        }

        return results;
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

    private static void TryCleanup(string tmpDir, List<string> results)
    {
        foreach (var f in results)
        {
            try { File.Delete(f); } catch { /* ignore */ }
        }

        try { Directory.Delete(tmpDir, recursive: true); } catch { /* ignore */ }
    }
}
