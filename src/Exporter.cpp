#include "Exporter.h"

#include <QFileInfo>
#include <QRegularExpression>

Exporter::Exporter(QObject* parent)
    : QObject(parent)
{
    m_process = new QProcess(this);
    connect(m_process, &QProcess::readyReadStandardError, this, &Exporter::onReadyRead);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &Exporter::onReadyRead);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &Exporter::onFinished);
}

Exporter::~Exporter()
{
    cancel();
}

bool Exporter::isRunning() const
{
    return m_process->state() != QProcess::NotRunning;
}

void Exporter::cancel()
{
    if (isRunning()) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}

QString Exporter::pickCpuAv1Encoder() const
{
    // Prefer SVT-AV1, then libaom
    const QString ffmpeg = EncoderCapabilities::findFfmpegExecutable();
    if (ffmpeg.isEmpty())
        return QStringLiteral("libaom-av1");

    QProcess probe;
    probe.start(ffmpeg, {QStringLiteral("-hide_banner"), QStringLiteral("-encoders")});
    if (!probe.waitForFinished(10000)) {
        probe.kill();
        return QStringLiteral("libaom-av1");
    }
    const QString out = QString::fromUtf8(probe.readAllStandardOutput());
    if (out.contains(QLatin1String("libsvtav1")))
        return QStringLiteral("libsvtav1");
    if (out.contains(QLatin1String("libaom-av1")))
        return QStringLiteral("libaom-av1");
    return QStringLiteral("libx264");
}

QStringList Exporter::buildArgs(const ExportRequest& request, bool forceCpuFallback) const
{
    QStringList args;
    args << QStringLiteral("-y")
         << QStringLiteral("-hide_banner")
         << QStringLiteral("-ss")
         << QString::number(request.startMs / 1000.0, 'f', 3)
         << QStringLiteral("-i")
         << request.inputPath
         << QStringLiteral("-t")
         << QString::number(qMax<qint64>(1, request.endMs - request.startMs) / 1000.0, 'f', 3);

    const bool streamCopy = request.format.videoCodecHint == QLatin1String("copy")
                            || request.encoder.ffmpegEncoder == QLatin1String("copy");

    if (streamCopy) {
        args << QStringLiteral("-c") << QStringLiteral("copy")
             << QStringLiteral("-avoid_negative_ts") << QStringLiteral("make_zero");
    } else {
        QString vEncoder = request.encoder.ffmpegEncoder;
        if (forceCpuFallback && request.encoder.isGpu)
            vEncoder = pickCpuAv1Encoder();

        // Align encoder with format hint when possible
        const QString hint = request.format.videoCodecHint;
        if (!forceCpuFallback && !request.encoder.isGpu) {
            if (hint == QLatin1String("h264") && vEncoder.contains(QLatin1String("av1")))
                vEncoder = QStringLiteral("libx264");
            else if (hint == QLatin1String("hevc") && vEncoder.contains(QLatin1String("av1")))
                vEncoder = QStringLiteral("libx265");
        }

        args << QStringLiteral("-c:v") << vEncoder;

        if (vEncoder == QLatin1String("av1_nvenc")) {
            args << QStringLiteral("-preset") << QStringLiteral("p4")
                 << QStringLiteral("-cq") << QStringLiteral("28")
                 << QStringLiteral("-b:v") << QStringLiteral("0");
        } else if (vEncoder == QLatin1String("av1_amf")) {
            args << QStringLiteral("-quality") << QStringLiteral("balanced")
                 << QStringLiteral("-rc") << QStringLiteral("cqp")
                 << QStringLiteral("-qp_i") << QStringLiteral("28");
        } else if (vEncoder == QLatin1String("av1_qsv")) {
            args << QStringLiteral("-global_quality") << QStringLiteral("28");
        } else if (vEncoder == QLatin1String("libsvtav1")) {
            args << QStringLiteral("-crf") << QStringLiteral("28")
                 << QStringLiteral("-preset") << QStringLiteral("6");
        } else if (vEncoder == QLatin1String("libaom-av1")) {
            args << QStringLiteral("-crf") << QStringLiteral("30")
                 << QStringLiteral("-b:v") << QStringLiteral("0")
                 << QStringLiteral("-cpu-used") << QStringLiteral("6");
        } else if (vEncoder == QLatin1String("libx265") || vEncoder.contains(QLatin1String("hevc"))) {
            args << QStringLiteral("-crf") << QStringLiteral("23");
        } else if (vEncoder == QLatin1String("libx264") || vEncoder.contains(QLatin1String("h264"))) {
            args << QStringLiteral("-crf") << QStringLiteral("20")
                 << QStringLiteral("-preset") << QStringLiteral("medium");
        }

        if (request.format.container == QLatin1String("webm")) {
            args << QStringLiteral("-c:a") << QStringLiteral("libopus")
                 << QStringLiteral("-b:a") << QStringLiteral("128k");
        } else {
            args << QStringLiteral("-c:a") << QStringLiteral("aac")
                 << QStringLiteral("-b:a") << QStringLiteral("192k");
        }
    }

    if (request.format.container == QLatin1String("mp4"))
        args << QStringLiteral("-movflags") << QStringLiteral("+faststart");

    args << request.outputPath;
    return args;
}

void Exporter::exportVideo(const ExportRequest& request)
{
    if (isRunning()) {
        emit finished(false, QStringLiteral("An export is already in progress."));
        return;
    }

    const QString ffmpeg = EncoderCapabilities::findFfmpegExecutable();
    if (ffmpeg.isEmpty()) {
        emit finished(false, QStringLiteral("ffmpeg.exe was not found next to Trimmi. Reinstall the application."));
        return;
    }
    if (!QFileInfo::exists(request.inputPath)) {
        emit finished(false, QStringLiteral("Input file is missing."));
        return;
    }

    m_request = request;
    m_durationMs = qMax<qint64>(1, request.endMs - request.startMs);
    m_triedCpuFallback = false;
    m_lastError.clear();

    m_process->setProgram(ffmpeg);
    startProcess(buildArgs(request, false), m_durationMs);
}

void Exporter::startProcess(const QStringList& args, qint64 durationMs)
{
    Q_UNUSED(durationMs);
    emit progress(0.0, QStringLiteral("Starting export…"));
    m_process->setArguments(args);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    m_process->start();
    if (!m_process->waitForStarted(5000)) {
        emit finished(false, QStringLiteral("Failed to start ffmpeg."));
    }
}

void Exporter::onReadyRead()
{
    const QByteArray data = m_process->readAllStandardError() + m_process->readAllStandardOutput();
    const QString text = QString::fromUtf8(data);
    m_lastError += text;

    // Parse time=HH:MM:SS.xx from ffmpeg progress
    static const QRegularExpression re(QStringLiteral(R"(time=(\d+):(\d+):(\d+(?:\.\d+)?))"));
    auto it = re.globalMatch(text);
    QRegularExpressionMatch last;
    bool found = false;
    while (it.hasNext()) {
        last = it.next();
        found = true;
    }
    if (found) {
        const double hours = last.captured(1).toDouble();
        const double mins = last.captured(2).toDouble();
        const double secs = last.captured(3).toDouble();
        const double currentMs = (hours * 3600.0 + mins * 60.0 + secs) * 1000.0;
        const double pct = qBound(0.0, (currentMs / m_durationMs) * 100.0, 99.0);
        emit progress(pct, QStringLiteral("Encoding… %1%").arg(static_cast<int>(pct)));
    }
}

void Exporter::onFinished(int exitCode, QProcess::ExitStatus status)
{
    if (status == QProcess::CrashExit) {
        emit finished(false, QStringLiteral("ffmpeg crashed during export."));
        return;
    }

    if (exitCode == 0) {
        emit progress(100.0, QStringLiteral("Done"));
        emit finished(true, QStringLiteral("Export completed successfully."));
        return;
    }

    // GPU failure → retry with CPU AV1 once
    if (m_request.encoder.isGpu && !m_triedCpuFallback) {
        m_triedCpuFallback = true;
        emit progress(0.0, QStringLiteral("GPU encode failed — falling back to CPU…"));
        m_lastError.clear();
        startProcess(buildArgs(m_request, true), m_durationMs);
        return;
    }

    QString msg = QStringLiteral("Export failed.");
    if (m_lastError.contains(QLatin1String("No NVENC capable devices"))
        || m_lastError.contains(QLatin1String("Cannot load nvcuda")))
        msg = QStringLiteral("GPU encoder unavailable. Try a CPU encoder.");
    else {
        const QStringList lines = m_lastError.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        if (!lines.isEmpty())
            msg = lines.last().trimmed();
    }
    emit finished(false, msg);
}
