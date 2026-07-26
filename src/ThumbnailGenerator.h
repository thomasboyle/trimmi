#pragma once

#include <QImage>
#include <QObject>
#include <QString>
#include <QVector>
#include <atomic>

class ThumbnailGenerator : public QObject {
    Q_OBJECT
public:
    explicit ThumbnailGenerator(QObject* parent = nullptr);
    ~ThumbnailGenerator() override;

    void generate(const QString& path, qint64 durationMs, int count);
    void cancel();

signals:
    void thumbnailReady(int index, const QImage& image);
    void finished();
    void failed(const QString& message);

private:
    void runGenerate(QString path, qint64 durationMs, int count, int generation);

    std::atomic<int> m_generation{0};
};
