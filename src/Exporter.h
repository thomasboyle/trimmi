#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

#include "EncoderCapabilities.h"

struct ExportRequest {
    QString inputPath;
    QString outputPath;
    qint64 startMs = 0;
    qint64 endMs = 0;
    EncoderOption encoder;
    FormatOption format;
};

class Exporter : public QObject {
    Q_OBJECT
public:
    explicit Exporter(QObject* parent = nullptr);
    ~Exporter() override;

    bool isRunning() const;
    void exportVideo(const ExportRequest& request);
    void cancel();

signals:
    void progress(double percent, const QString& status);
    void finished(bool success, const QString& message);

private:
    QStringList buildArgs(const ExportRequest& request, bool forceCpuFallback) const;
    QString pickCpuAv1Encoder() const;
    void startProcess(const QStringList& args, qint64 durationMs);
    void onReadyRead();
    void onFinished(int exitCode, QProcess::ExitStatus status);

    QProcess* m_process = nullptr;
    ExportRequest m_request;
    qint64 m_durationMs = 0;
    bool m_triedCpuFallback = false;
    QString m_lastError;
};
