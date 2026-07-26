#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

struct EncoderOption {
    QString id;
    QString label;
    QString ffmpegEncoder; // e.g. av1_nvenc, libaom-av1, libsvtav1
    bool isGpu = false;
    QString badge;         // "GPU" / "CPU"
};

struct FormatOption {
    QString id;
    QString label;
    QString container;     // mp4, mkv, webm
    QString videoCodecHint;
    QString helperText;
    QString defaultExtension;
};

struct GpuInfo {
    bool available = false;
    QString name;
    QString vendor; // NVIDIA, AMD, Intel, Unknown
    QStringList hwEncoders;
};

class EncoderCapabilities : public QObject {
    Q_OBJECT
public:
    explicit EncoderCapabilities(QObject* parent = nullptr);

    void detect();

    const GpuInfo& gpuInfo() const { return m_gpu; }
    QVector<EncoderOption> encoderOptions() const { return m_encoders; }
    QVector<FormatOption> formatOptions() const { return m_formats; }

    EncoderOption encoderById(const QString& id) const;
    FormatOption formatById(const QString& id) const;

    static QString findFfmpegExecutable();
    static QString findFfprobeExecutable();

signals:
    void detectionFinished();

private:
    void detectGpuName();
    void detectFfmpegEncoders();
    void buildOptions();

    GpuInfo m_gpu;
    QStringList m_availableEncoders;
    QVector<EncoderOption> m_encoders;
    QVector<FormatOption> m_formats;
};
