#include "ThumbnailGenerator.h"
#include "EncoderCapabilities.h"

#include <QMetaObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QtConcurrent>

ThumbnailGenerator::ThumbnailGenerator(QObject* parent)
    : QObject(parent)
{
}

ThumbnailGenerator::~ThumbnailGenerator()
{
    cancel();
}

void ThumbnailGenerator::cancel()
{
    m_generation.fetch_add(1);
}

void ThumbnailGenerator::generate(const QString& path, qint64 durationMs, int count)
{
    cancel();
    const int generation = m_generation.load();
    (void)QtConcurrent::run([this, path, durationMs, count, generation]() {
        runGenerate(path, durationMs, count, generation);
    });
}

void ThumbnailGenerator::runGenerate(QString path, qint64 durationMs, int count, int generation)
{
    if (count <= 0 || durationMs <= 0) {
        QMetaObject::invokeMethod(this, [this]() { emit finished(); }, Qt::QueuedConnection);
        return;
    }

    const QString ffmpeg = EncoderCapabilities::findFfmpegExecutable();
    if (ffmpeg.isEmpty()) {
        QMetaObject::invokeMethod(
            this,
            [this]() { emit failed(QStringLiteral("ffmpeg.exe was not found for thumbnails.")); },
            Qt::QueuedConnection);
        return;
    }

    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        QMetaObject::invokeMethod(
            this,
            [this]() { emit failed(QStringLiteral("Could not create temp directory for thumbnails.")); },
            Qt::QueuedConnection);
        return;
    }

    for (int i = 0; i < count; ++i) {
        if (m_generation.load() != generation)
            break;

        const qint64 targetMs = (count == 1)
                                    ? 0
                                    : static_cast<qint64>((durationMs * i) / static_cast<double>(count));
        const QString outPath = tmp.filePath(QStringLiteral("thumb_%1.jpg").arg(i));

        QProcess proc;
        proc.setProcessChannelMode(QProcess::MergedChannels);
        proc.start(ffmpeg,
                   {QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"),
                    QStringLiteral("error"), QStringLiteral("-ss"),
                    QString::number(targetMs / 1000.0, 'f', 3), QStringLiteral("-i"), path,
                    QStringLiteral("-frames:v"), QStringLiteral("1"), QStringLiteral("-vf"),
                    QStringLiteral("scale=160:-1"), QStringLiteral("-y"), outPath});

        QImage copy;
        if (proc.waitForFinished(30000) && proc.exitStatus() == QProcess::NormalExit
            && proc.exitCode() == 0) {
            copy = QImage(outPath);
        }

        if (copy.isNull()) {
            copy = QImage(160, 90, QImage::Format_RGB888);
            copy.fill(Qt::black);
        }

        if (m_generation.load() != generation)
            break;

        QMetaObject::invokeMethod(
            this, [this, i, copy]() { emit thumbnailReady(i, copy); }, Qt::QueuedConnection);
    }

    if (m_generation.load() == generation) {
        QMetaObject::invokeMethod(this, [this]() { emit finished(); }, Qt::QueuedConnection);
    }
}
