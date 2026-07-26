#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QAudioOutput;
class QMediaPlayer;
class QVideoWidget;
class QWidget;

struct VideoMetadata {
    QString path;
    QString fileName;
    qint64 durationMs = 0;
    int width = 0;
    int height = 0;
    QString videoCodec;
    QString audioCodec;
    double frameRate = 0.0;
    bool valid = false;
};

class VideoPlayer : public QObject {
    Q_OBJECT
public:
    explicit VideoPlayer(QObject* parent = nullptr);
    ~VideoPlayer() override;

    QWidget* videoWidget() const;

    bool load(const QString& path);
    void play();
    void pause();
    void togglePlayPause();
    void stop();
    void seek(qint64 positionMs);
    void stepFrame(int direction); // -1 previous, +1 next
    void jumpToEnd();
    void setVolume(float volume01);
    float volume() const;
    void setMuted(bool muted);

    bool isPlaying() const;
    qint64 position() const;
    qint64 duration() const;
    const VideoMetadata& metadata() const { return m_meta; }

    static VideoMetadata probeMetadata(const QString& path);

signals:
    void loaded(const VideoMetadata& meta);
    void loadFailed(const QString& message);
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
    void playingChanged(bool playing);
    void mediaStatusChanged();

private:
    void attachPlayerSignals();

    QMediaPlayer* m_player = nullptr;
    QAudioOutput* m_audio = nullptr;
    QVideoWidget* m_videoWidget = nullptr;
    VideoMetadata m_meta;
    qint64 m_frameStepMs = 33;
};
