namespace Trimmi.Core.Models;

public sealed class VideoMetadata
{
    public string Path { get; init; } = "";
    public string FileName { get; init; } = "";
    public long DurationMs { get; init; }
    public int Width { get; init; }
    public int Height { get; init; }
    public string VideoCodec { get; init; } = "";
    public string AudioCodec { get; init; } = "";
    public double FrameRate { get; init; }
    public bool Valid { get; init; }
}

public sealed class EncoderOption
{
    public string Id { get; init; } = "";
    public string Label { get; init; } = "";
    public string FfmpegEncoder { get; init; } = "";
    public bool IsGpu { get; init; }
    public string Badge { get; init; } = "";
}

public sealed class FormatOption
{
    public string Id { get; init; } = "";
    public string Label { get; init; } = "";
    public string Container { get; init; } = "";
    public string HelperText { get; init; } = "";
    public string DefaultExtension { get; init; } = "";
}

public sealed class GpuInfo
{
    public bool Available { get; init; }
    public string Name { get; init; } = "";
    public string Vendor { get; init; } = "";
    public IReadOnlyList<string> HwEncoders { get; init; } = Array.Empty<string>();
}

public sealed class ExportRequest
{
    public required string InputPath { get; init; }
    public required string OutputPath { get; init; }
    public long StartMs { get; init; }
    public long EndMs { get; init; }
    public required EncoderOption Encoder { get; init; }
    public required FormatOption Format { get; init; }
}

public sealed class ExportProgress
{
    public double Percent { get; init; }
    public string Status { get; init; } = "";
}
