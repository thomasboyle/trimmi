namespace Trimmi.Core.Services;

public static class FfmpegToolPaths
{
    public static string? FindFfmpeg() => Resolve("ffmpeg");

    public static string? FindFfprobe() => Resolve("ffprobe");

    private static string? Resolve(string tool)
    {
        var baseDir = AppContext.BaseDirectory;
        var candidates = new[]
        {
            Path.Combine(baseDir, "Assets", "ffmpeg", tool + ".exe"),
            Path.Combine(baseDir, "Assets", "ffmpeg", tool),
            Path.Combine(baseDir, tool + ".exe"),
            Path.Combine(baseDir, tool),
        };

        foreach (var candidate in candidates)
        {
            if (File.Exists(candidate))
                return candidate;
        }

        var beside = LookBesideApp(tool);
        if (beside is not null)
            return beside;

        return FindOnPath(tool);
    }

    private static string? LookBesideApp(string name)
    {
        var appDir = AppContext.BaseDirectory;
        var candidate = Path.Combine(appDir, name);
        if (File.Exists(candidate))
            return candidate;

        if (OperatingSystem.IsWindows())
        {
            var candidateExe = Path.Combine(appDir, name + ".exe");
            if (File.Exists(candidateExe))
                return candidateExe;
        }

        return null;
    }

    private static string? FindOnPath(string tool)
    {
        if (OperatingSystem.IsWindows())
        {
            try
            {
                using var proc = new System.Diagnostics.Process();
                proc.StartInfo = new System.Diagnostics.ProcessStartInfo
                {
                    FileName = "where.exe",
                    Arguments = tool,
                    UseShellExecute = false,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    CreateNoWindow = true,
                };
                if (!proc.Start())
                    return null;
                var output = proc.StandardOutput.ReadToEnd();
                proc.WaitForExit(5000);
                foreach (var line in output.Split(['\r', '\n'], StringSplitOptions.RemoveEmptyEntries))
                {
                    var path = line.Trim();
                    if (File.Exists(path))
                        return path;
                }
            }
            catch
            {
                // Fall through to PATH scan.
            }
        }

        var pathEnv = Environment.GetEnvironmentVariable("PATH");
        if (string.IsNullOrEmpty(pathEnv))
            return null;

        var extensions = OperatingSystem.IsWindows()
            ? new[] { ".exe", ".cmd", ".bat", "" }
            : new[] { "" };

        foreach (var dir in pathEnv.Split(Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries))
        {
            foreach (var ext in extensions)
            {
                var candidate = Path.Combine(dir.Trim(), tool + ext);
                if (File.Exists(candidate))
                    return candidate;
            }
        }

        return null;
    }
}
