using System.Runtime.InteropServices;
using System.Text.RegularExpressions;
using Trimmi.Core.Models;

namespace Trimmi.Core.Services;

public sealed class EncoderCapabilities
{
    private GpuInfo _gpu = new();
    private readonly List<string> _availableEncoders = [];
    private List<EncoderOption> _encoders = [];
    private List<FormatOption> _formats = [];

    public GpuInfo Gpu => _gpu;
    public IReadOnlyList<EncoderOption> Encoders => _encoders;
    public IReadOnlyList<FormatOption> Formats => _formats;

    public async Task DetectAsync(CancellationToken cancellationToken = default)
    {
        _gpu = new GpuInfo();
        _availableEncoders.Clear();
        _encoders = [];
        _formats = [];

        DetectGpuName();
        await DetectFfmpegEncodersAsync(cancellationToken).ConfigureAwait(false);
        BuildOptions();
    }

    public EncoderOption EncoderById(string id)
    {
        foreach (var e in _encoders)
        {
            if (e.Id == id)
                return e;
        }

        return _encoders.Count == 0 ? new EncoderOption() : _encoders[0];
    }

    public FormatOption FormatById(string id)
    {
        foreach (var f in _formats)
        {
            if (f.Id == id)
                return f;
        }

        return _formats.Count == 0 ? new FormatOption() : _formats[0];
    }

    private void DetectGpuName()
    {
        if (!OperatingSystem.IsWindows())
            return;

        try
        {
            var hr = NativeMethods.CreateDXGIFactory1(typeof(IDXGIFactory1).GUID, out var factoryUnk);
            if (hr < 0 || factoryUnk is null)
                return;

            try
            {
                var factory = (IDXGIFactory1)factoryUnk;
                for (uint i = 0; ; i++)
                {
                    hr = factory.EnumAdapters1(i, out var adapter);
                    if (hr == NativeMethods.DXGI_ERROR_NOT_FOUND || adapter is null)
                        break;

                    try
                    {
                        adapter.GetDesc1(out var desc);
                        if ((desc.Flags & NativeMethods.DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
                            continue;

                        var name = desc.Description?.Trim() ?? "";
                        if (string.IsNullOrEmpty(name))
                            continue;

                        _gpu = new GpuInfo
                        {
                            Name = name,
                            Vendor = VendorFromId(desc.VendorId),
                            Available = _gpu.Available,
                            HwEncoders = _gpu.HwEncoders,
                        };

                        if (_gpu.Vendor is "NVIDIA" or "AMD")
                            break;
                    }
                    finally
                    {
                        Marshal.ReleaseComObject(adapter);
                    }
                }
            }
            finally
            {
                Marshal.ReleaseComObject(factoryUnk);
            }
        }
        catch
        {
            // DXGI unavailable — ffmpeg encoder detection still fills GPU info.
        }
    }

    private static string VendorFromId(uint vendor) => vendor switch
    {
        0x10DE => "NVIDIA",
        0x1002 or 0x1022 => "AMD",
        0x8086 => "Intel",
        _ => "Unknown",
    };

    private async Task DetectFfmpegEncodersAsync(CancellationToken cancellationToken)
    {
        var ffmpeg = FfmpegToolPaths.FindFfmpeg();
        if (string.IsNullOrEmpty(ffmpeg))
            return;

        using var proc = new System.Diagnostics.Process();
        proc.StartInfo = new System.Diagnostics.ProcessStartInfo
        {
            FileName = ffmpeg,
            Arguments = "-hide_banner -encoders",
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
        };

        try
        {
            if (!proc.Start())
                return;
        }
        catch
        {
            return;
        }

        var outputTask = proc.StandardOutput.ReadToEndAsync(cancellationToken);
        var errorTask = proc.StandardError.ReadToEndAsync(cancellationToken);
        try
        {
            using var timeout = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
            timeout.CancelAfter(TimeSpan.FromSeconds(15));
            await proc.WaitForExitAsync(timeout.Token).ConfigureAwait(false);
        }
        catch (OperationCanceledException) when (!cancellationToken.IsCancellationRequested)
        {
            TryKill(proc);
            return;
        }
        catch (OperationCanceledException)
        {
            TryKill(proc);
            throw;
        }

        var output = await outputTask.ConfigureAwait(false);
        _ = await errorTask.ConfigureAwait(false);

        foreach (var rawLine in output.Split('\n'))
        {
            var line = rawLine.Trim();
            if (line.Length < 10)
                continue;
            if (!(line.StartsWith('V') || line.StartsWith('A') || line.StartsWith('S')))
                continue;

            var parts = Regex.Split(line, @"\s+").Where(p => p.Length > 0).ToArray();
            if (parts.Length < 2)
                continue;

            var name = parts[0].Length <= 8 ? parts[1] : parts[0];
            if (name.Contains('.') || name == "=" || name.Length < 2)
                continue;
            if (!Regex.IsMatch(name, @"^[A-Za-z0-9_\-]+$"))
                continue;

            _availableEncoders.Add(name);
        }

        var vendor = _gpu.Vendor;
        var hw = new List<string>();

        // Only count the API that matches the detected GPU as available.
        var matchedApi = vendor switch
        {
            "NVIDIA" when Has("av1_nvenc") || Has("h264_nvenc") || Has("hevc_nvenc") => "NVENC",
            "AMD" when Has("av1_amf") || Has("h264_amf") || Has("hevc_amf") => "AMF",
            "Intel" when Has("av1_qsv") || Has("h264_qsv") || Has("hevc_qsv") => "QSV",
            _ => null,
        };

        if (matchedApi is null && (string.IsNullOrEmpty(vendor) || vendor == "Unknown"))
        {
            if (Has("av1_nvenc") || Has("h264_nvenc") || Has("hevc_nvenc"))
                matchedApi = "NVENC";
            else if (Has("av1_amf") || Has("h264_amf") || Has("hevc_amf"))
                matchedApi = "AMF";
            else if (Has("av1_qsv") || Has("h264_qsv") || Has("hevc_qsv"))
                matchedApi = "QSV";
        }

        if (matchedApi is not null)
            hw.Add(matchedApi);

        var available = hw.Count > 0;
        var gpuName = _gpu.Name;
        if (available && string.IsNullOrEmpty(gpuName))
        {
            gpuName = matchedApi switch
            {
                "NVENC" => "NVIDIA GPU (NVENC)",
                "AMF" => "AMD GPU (AMF)",
                "QSV" => "Intel GPU (QSV)",
                _ => gpuName,
            };
        }

        _gpu = new GpuInfo
        {
            Available = available,
            Name = gpuName,
            Vendor = vendor,
            HwEncoders = hw,
        };
    }

    private bool Has(string enc) => _availableEncoders.Contains(enc);

    private void BuildOptions()
    {
        TryAddBestGpuEncoder();

        if (Has("libsvtav1"))
        {
            _encoders.Add(new EncoderOption
            {
                Id = "cpu_av1",
                Label = "AV1 (CPU)",
                FfmpegEncoder = "libsvtav1",
                IsGpu = false,
                Badge = "CPU",
            });
        }
        else if (Has("libaom-av1"))
        {
            _encoders.Add(new EncoderOption
            {
                Id = "cpu_av1",
                Label = "AV1 (CPU)",
                FfmpegEncoder = "libaom-av1",
                IsGpu = false,
                Badge = "CPU",
            });
        }

        if (Has("libx264"))
        {
            _encoders.Add(new EncoderOption
            {
                Id = "cpu_h264",
                Label = "H.264 (CPU)",
                FfmpegEncoder = "libx264",
                IsGpu = false,
                Badge = "CPU",
            });
        }

        if (Has("libx265"))
        {
            _encoders.Add(new EncoderOption
            {
                Id = "cpu_h265",
                Label = "H.265 (CPU)",
                FfmpegEncoder = "libx265",
                IsGpu = false,
                Badge = "CPU",
            });
        }

        if (_encoders.Count == 0)
        {
            _encoders.Add(new EncoderOption
            {
                Id = "copy",
                Label = "Stream copy (no re-encode)",
                FfmpegEncoder = "copy",
                IsGpu = false,
                Badge = "COPY",
            });
        }

        _formats.Add(new FormatOption
        {
            Id = "mp4",
            Label = "MP4",
            Container = "mp4",
            HelperText = "Widely supported container — best for sharing and playback.",
            DefaultExtension = "mp4",
        });
        _formats.Add(new FormatOption
        {
            Id = "mkv",
            Label = "MKV",
            Container = "mkv",
            HelperText = "Flexible Matroska container — open formats, fewer limits.",
            DefaultExtension = "mkv",
        });
        _formats.Add(new FormatOption
        {
            Id = "webm",
            Label = "WebM",
            Container = "webm",
            HelperText = "Web-friendly container — best with AV1 for browsers.",
            DefaultExtension = "webm",
        });
    }

    private void TryAddBestGpuEncoder()
    {
        foreach (var candidate in GpuEncoderCandidates(_gpu.Vendor))
        {
            if (!Has(candidate.FfmpegEncoder))
                continue;

            _encoders.Add(new EncoderOption
            {
                Id = candidate.Id,
                Label = candidate.Label,
                FfmpegEncoder = candidate.FfmpegEncoder,
                IsGpu = true,
                Badge = "GPU",
            });
            return;
        }
    }

    private static IEnumerable<(string Id, string Label, string FfmpegEncoder)> GpuEncoderCandidates(string vendor)
    {
        // Only expose the API that matches the detected GPU. Unknown vendor: try in priority order.
        var apis = vendor switch
        {
            "NVIDIA" => new[] { "nvenc" },
            "AMD" => new[] { "amf" },
            "Intel" => new[] { "qsv" },
            _ => new[] { "nvenc", "amf", "qsv" },
        };

        var codecs = new (string Suffix, string LabelPrefix, string IdPrefix)[]
        {
            ("av1", "AV1", "gpu_av1"),
            ("hevc", "H.265", "gpu_h265"),
            ("h264", "H.264", "gpu_h264"),
        };

        foreach (var api in apis)
        {
            foreach (var (suffix, labelPrefix, idPrefix) in codecs)
            {
                yield return ($"{idPrefix}_{api}", $"{labelPrefix} (GPU)", $"{suffix}_{api}");
            }
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

    private static class NativeMethods
    {
        public const int DXGI_ERROR_NOT_FOUND = unchecked((int)0x887A0002);
        public const uint DXGI_ADAPTER_FLAG_SOFTWARE = 2;

        [DllImport("dxgi.dll", ExactSpelling = true)]
        public static extern int CreateDXGIFactory1(
            [In] ref Guid riid,
            [MarshalAs(UnmanagedType.IUnknown)] out object? ppFactory);

        public static int CreateDXGIFactory1(Guid riid, out object? ppFactory)
            => CreateDXGIFactory1(ref riid, out ppFactory);
    }

    [ComImport]
    [Guid("770aae78-f26f-4dba-a829-253c83d1b387")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface IDXGIFactory1
    {
        // IDXGIObject
        void SetPrivateData(ref Guid name, uint dataSize, IntPtr pData);
        void SetPrivateDataInterface(ref Guid name, [MarshalAs(UnmanagedType.IUnknown)] object pUnknown);
        void GetPrivateData(ref Guid name, ref uint pDataSize, IntPtr pData);
        void GetParent(ref Guid riid, [MarshalAs(UnmanagedType.IUnknown)] out object ppParent);

        // IDXGIFactory
        void EnumAdapters(uint adapter, out IDXGIAdapter ppAdapter);
        void MakeWindowAssociation(IntPtr windowHandle, uint flags);
        void GetWindowAssociation(out IntPtr pWindowHandle);
        void CreateSwapChain([MarshalAs(UnmanagedType.IUnknown)] object pDevice, IntPtr pDesc, out IntPtr ppSwapChain);
        void CreateSoftwareAdapter(IntPtr module, out IDXGIAdapter ppAdapter);

        // IDXGIFactory1
        [PreserveSig]
        int EnumAdapters1(uint adapter, out IDXGIAdapter1? ppAdapter);
        [return: MarshalAs(UnmanagedType.Bool)]
        bool IsCurrent();
    }

    [ComImport]
    [Guid("2411e7e1-12ac-4ccf-bd14-9798e8534dc0")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface IDXGIAdapter
    {
        void SetPrivateData(ref Guid name, uint dataSize, IntPtr pData);
        void SetPrivateDataInterface(ref Guid name, [MarshalAs(UnmanagedType.IUnknown)] object pUnknown);
        void GetPrivateData(ref Guid name, ref uint pDataSize, IntPtr pData);
        void GetParent(ref Guid riid, [MarshalAs(UnmanagedType.IUnknown)] out object ppParent);
        void EnumOutputs(uint output, out IntPtr ppOutput);
        void GetDesc(out DXGI_ADAPTER_DESC pDesc);
        void CheckInterfaceSupport(ref Guid interfaceName, out long pUmdVersion);
    }

    [ComImport]
    [Guid("29038f61-3839-4626-91fd-086879011a05")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface IDXGIAdapter1
    {
        void SetPrivateData(ref Guid name, uint dataSize, IntPtr pData);
        void SetPrivateDataInterface(ref Guid name, [MarshalAs(UnmanagedType.IUnknown)] object pUnknown);
        void GetPrivateData(ref Guid name, ref uint pDataSize, IntPtr pData);
        void GetParent(ref Guid riid, [MarshalAs(UnmanagedType.IUnknown)] out object ppParent);
        void EnumOutputs(uint output, out IntPtr ppOutput);
        void GetDesc(out DXGI_ADAPTER_DESC pDesc);
        void CheckInterfaceSupport(ref Guid interfaceName, out long pUmdVersion);
        void GetDesc1(out DXGI_ADAPTER_DESC1 pDesc);
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct DXGI_ADAPTER_DESC
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
        public string Description;
        public uint VendorId;
        public uint DeviceId;
        public uint SubSysId;
        public uint Revision;
        public UIntPtr DedicatedVideoMemory;
        public UIntPtr DedicatedSystemMemory;
        public UIntPtr SharedSystemMemory;
        public long AdapterLuid;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct DXGI_ADAPTER_DESC1
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
        public string Description;
        public uint VendorId;
        public uint DeviceId;
        public uint SubSysId;
        public uint Revision;
        public UIntPtr DedicatedVideoMemory;
        public UIntPtr DedicatedSystemMemory;
        public UIntPtr SharedSystemMemory;
        public long AdapterLuid;
        public uint Flags;
    }
}
