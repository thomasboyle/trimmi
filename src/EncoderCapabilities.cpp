#include "EncoderCapabilities.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#  include <dxgi.h>
#  include <wrl/client.h>
#  pragma comment(lib, "dxgi.lib")
#endif

namespace {

QString lookBesideApp(const QString& name)
{
    const QDir appDir(QCoreApplication::applicationDirPath());
    const QString candidate = appDir.filePath(name);
    if (QFileInfo::exists(candidate))
        return candidate;
#ifdef Q_OS_WIN
    const QString candidateExe = appDir.filePath(name + QStringLiteral(".exe"));
    if (QFileInfo::exists(candidateExe))
        return candidateExe;
#endif
    return {};
}

} // namespace

EncoderCapabilities::EncoderCapabilities(QObject* parent)
    : QObject(parent)
{
}

QString EncoderCapabilities::findFfmpegExecutable()
{
    if (const QString local = lookBesideApp(QStringLiteral("ffmpeg")); !local.isEmpty())
        return local;
    return QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
}

QString EncoderCapabilities::findFfprobeExecutable()
{
    if (const QString local = lookBesideApp(QStringLiteral("ffprobe")); !local.isEmpty())
        return local;
    return QStandardPaths::findExecutable(QStringLiteral("ffprobe"));
}

void EncoderCapabilities::detect()
{
    m_gpu = GpuInfo{};
    m_availableEncoders.clear();
    m_encoders.clear();
    m_formats.clear();

    detectGpuName();
    detectFfmpegEncoders();
    buildOptions();
    emit detectionFinished();
}

void EncoderCapabilities::detectGpuName()
{
#ifdef Q_OS_WIN
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
        return;

    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc{};
        if (FAILED(adapter->GetDesc1(&desc)))
            continue;
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        const QString name = QString::fromWCharArray(desc.Description).trimmed();
        if (name.isEmpty())
            continue;

        m_gpu.name = name;
        const quint32 vendor = desc.VendorId;
        if (vendor == 0x10DE)
            m_gpu.vendor = QStringLiteral("NVIDIA");
        else if (vendor == 0x1002 || vendor == 0x1022)
            m_gpu.vendor = QStringLiteral("AMD");
        else if (vendor == 0x8086)
            m_gpu.vendor = QStringLiteral("Intel");
        else
            m_gpu.vendor = QStringLiteral("Unknown");

        // Prefer discrete NVIDIA/AMD over first adapter when possible
        if (m_gpu.vendor == QLatin1String("NVIDIA") || m_gpu.vendor == QLatin1String("AMD"))
            break;
    }
#else
    Q_UNUSED(this);
#endif
}

void EncoderCapabilities::detectFfmpegEncoders()
{
    const QString ffmpeg = findFfmpegExecutable();
    if (ffmpeg.isEmpty())
        return;

    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(ffmpeg, {QStringLiteral("-hide_banner"), QStringLiteral("-encoders")});
    if (!proc.waitForFinished(15000)) {
        proc.kill();
        return;
    }

    const QString output = QString::fromUtf8(proc.readAllStandardOutput());
    const QStringList lines = output.split(QLatin1Char('\n'));
    for (QString line : lines) {
        line = line.trimmed();
        // Encoder lines look like: " V....D av1_nvenc           NVIDIA NVENC av1 encoder"
        if (line.size() < 10)
            continue;
        if (!(line.startsWith(QLatin1Char('V')) || line.startsWith(QLatin1Char('A'))
              || line.startsWith(QLatin1Char('S'))))
            continue;

        const QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")),
                                             Qt::SkipEmptyParts);
        if (parts.size() < 2)
            continue;
        // parts[0] is capability flags, parts[1] is encoder name
        const QString name = parts.size() >= 2 && parts[0].size() <= 8 ? parts[1] : parts[0];
        if (name.contains(QLatin1Char('.')) || name == QLatin1Char('=') || name.size() < 2)
            continue;
        if (!name.contains(QRegularExpression(QStringLiteral("^[A-Za-z0-9_\\-]+$"))))
            continue;
        m_availableEncoders.push_back(name);
    }

    auto has = [this](const char* enc) {
        return m_availableEncoders.contains(QLatin1String(enc));
    };

    if (has("av1_nvenc") || has("h264_nvenc") || has("hevc_nvenc"))
        m_gpu.hwEncoders << QStringLiteral("NVENC");
    if (has("av1_amf") || has("h264_amf") || has("hevc_amf"))
        m_gpu.hwEncoders << QStringLiteral("AMF");
    if (has("av1_qsv") || has("h264_qsv") || has("hevc_qsv"))
        m_gpu.hwEncoders << QStringLiteral("QSV");

    m_gpu.available = !m_gpu.hwEncoders.isEmpty();
    if (m_gpu.available && m_gpu.name.isEmpty()) {
        if (m_gpu.hwEncoders.contains(QStringLiteral("NVENC")))
            m_gpu.name = QStringLiteral("NVIDIA GPU (NVENC)");
        else if (m_gpu.hwEncoders.contains(QStringLiteral("AMF")))
            m_gpu.name = QStringLiteral("AMD GPU (AMF)");
        else if (m_gpu.hwEncoders.contains(QStringLiteral("QSV")))
            m_gpu.name = QStringLiteral("Intel GPU (QSV)");
    }
}

void EncoderCapabilities::buildOptions()
{
    auto has = [this](const char* enc) {
        return m_availableEncoders.contains(QLatin1String(enc));
    };

    if (has("av1_nvenc")) {
        m_encoders.push_back({QStringLiteral("gpu_av1_nvenc"),
                              QStringLiteral("Use GPU (AV1)"),
                              QStringLiteral("av1_nvenc"),
                              true,
                              QStringLiteral("GPU")});
    }
    if (has("av1_amf")) {
        m_encoders.push_back({QStringLiteral("gpu_av1_amf"),
                              QStringLiteral("Use GPU AMF (AV1)"),
                              QStringLiteral("av1_amf"),
                              true,
                              QStringLiteral("GPU")});
    }
    if (has("av1_qsv")) {
        m_encoders.push_back({QStringLiteral("gpu_av1_qsv"),
                              QStringLiteral("Use GPU QSV (AV1)"),
                              QStringLiteral("av1_qsv"),
                              true,
                              QStringLiteral("GPU")});
    }

    // Preferred GPU entry even if AV1 HW missing — UI can fall back at export time
    if (m_encoders.isEmpty() && m_gpu.available) {
        if (has("hevc_nvenc")) {
            m_encoders.push_back({QStringLiteral("gpu_hevc_nvenc"),
                                  QStringLiteral("Use GPU (HEVC)"),
                                  QStringLiteral("hevc_nvenc"),
                                  true,
                                  QStringLiteral("GPU")});
        } else if (has("h264_nvenc")) {
            m_encoders.push_back({QStringLiteral("gpu_h264_nvenc"),
                                  QStringLiteral("Use GPU (H.264)"),
                                  QStringLiteral("h264_nvenc"),
                                  true,
                                  QStringLiteral("GPU")});
        }
    }

    if (has("libsvtav1")) {
        m_encoders.push_back({QStringLiteral("cpu_svtav1"),
                              QStringLiteral("CPU (SVT-AV1)"),
                              QStringLiteral("libsvtav1"),
                              false,
                              QStringLiteral("CPU")});
    }
    if (has("libaom-av1")) {
        m_encoders.push_back({QStringLiteral("cpu_aom"),
                              QStringLiteral("CPU (libaom AV1)"),
                              QStringLiteral("libaom-av1"),
                              false,
                              QStringLiteral("CPU")});
    }
    if (has("libx265")) {
        m_encoders.push_back({QStringLiteral("cpu_x265"),
                              QStringLiteral("CPU (x265 HEVC)"),
                              QStringLiteral("libx265"),
                              false,
                              QStringLiteral("CPU")});
    }
    if (has("libx264")) {
        m_encoders.push_back({QStringLiteral("cpu_x264"),
                              QStringLiteral("CPU (x264 H.264)"),
                              QStringLiteral("libx264"),
                              false,
                              QStringLiteral("CPU")});
    }

    if (m_encoders.isEmpty()) {
        m_encoders.push_back({QStringLiteral("copy"),
                              QStringLiteral("Stream copy (no re-encode)"),
                              QStringLiteral("copy"),
                              false,
                              QStringLiteral("COPY")});
    }

    m_formats.push_back({QStringLiteral("mp4_av1"),
                         QStringLiteral("MP4 (AV1)"),
                         QStringLiteral("mp4"),
                         QStringLiteral("av1"),
                         QStringLiteral("Modern AV1 in MP4 — best quality/size for new devices."),
                         QStringLiteral("mp4")});
    m_formats.push_back({QStringLiteral("mp4_hevc"),
                         QStringLiteral("MP4 (HEVC)"),
                         QStringLiteral("mp4"),
                         QStringLiteral("hevc"),
                         QStringLiteral("HEVC/H.265 in MP4 — wide compatibility with smaller files."),
                         QStringLiteral("mp4")});
    m_formats.push_back({QStringLiteral("mp4_h264"),
                         QStringLiteral("MP4 (H.264)"),
                         QStringLiteral("mp4"),
                         QStringLiteral("h264"),
                         QStringLiteral("H.264 in MP4 — maximum compatibility."),
                         QStringLiteral("mp4")});
    m_formats.push_back({QStringLiteral("mkv_av1"),
                         QStringLiteral("MKV (AV1)"),
                         QStringLiteral("mkv"),
                         QStringLiteral("av1"),
                         QStringLiteral("AV1 in Matroska — flexible container, open formats."),
                         QStringLiteral("mkv")});
    m_formats.push_back({QStringLiteral("webm_av1"),
                         QStringLiteral("WebM (AV1)"),
                         QStringLiteral("webm"),
                         QStringLiteral("av1"),
                         QStringLiteral("AV1 in WebM — good for web delivery."),
                         QStringLiteral("webm")});
    m_formats.push_back({QStringLiteral("mp4_copy"),
                         QStringLiteral("MP4 (stream copy)"),
                         QStringLiteral("mp4"),
                         QStringLiteral("copy"),
                         QStringLiteral("Lossless cut when codecs fit MP4 — fastest, no re-encode."),
                         QStringLiteral("mp4")});
}

EncoderOption EncoderCapabilities::encoderById(const QString& id) const
{
    for (const auto& e : m_encoders) {
        if (e.id == id)
            return e;
    }
    return m_encoders.isEmpty() ? EncoderOption{} : m_encoders.first();
}

FormatOption EncoderCapabilities::formatById(const QString& id) const
{
    for (const auto& f : m_formats) {
        if (f.id == id)
            return f;
    }
    return m_formats.isEmpty() ? FormatOption{} : m_formats.first();
}
