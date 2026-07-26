#include "VideoPlayer.h"
#include "EncoderCapabilities.h"

#include <QAudioOutput>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMediaPlayer>
#include <QProcess>
#include <QUrl>
#include <QVideoWidget>

VideoPlayer::VideoPlayer(QObject* parent)
    : QObject(parent)
{
    m_videoWidget = new QVideoWidget;
    m_videoWidget->setAspectRatioMode(Qt::KeepAspectRatio);
    m_videoWidget->setStyleSheet(QStringLiteral("background-color: #000000;"));

    m_audio = new QAudioOutput(this);
    m_audio->setVolume(0.8f);

    m_player = new QMediaPlayer(this);
    m_player->setAudioOutput(m_audio);
    m_player->setVideoOutput(m_videoWidget);

    attachPlayerSignals();
}

VideoPlayer::~VideoPlayer() = default;

QWidget* VideoPlayer::videoWidget() const
{
    return m_videoWidget;
}

void VideoPlayer::attachPlayerSignals()
{
    connect(m_player, &QMediaPlayer::positionChanged, this, &VideoPlayer::positionChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, [this](qint64 d) {
        if (d > 0 && m_meta.durationMs <= 0)
            m_meta.durationMs = d;
        emit durationChanged(d);
    });
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState s) {
        emit playingChanged(s == QMediaPlayer::PlayingState);
    });
    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString& errorString) {
                emit loadFailed(errorString.isEmpty() ? QStringLiteral("Failed to play media")
                                                      : errorString);
            });
}

VideoMetadata VideoPlayer::probeMetadata(const QString& path)
{
    VideoMetadata meta;
    meta.path = path;
    meta.fileName = QFileInfo(path).fileName();

    const QString ffprobe = EncoderCapabilities::findFfprobeExecutable();
    if (ffprobe.isEmpty())
        return meta;

    QProcess proc;
    proc.start(ffprobe,
               {QStringLiteral("-v"), QStringLiteral("quiet"), QStringLiteral("-print_format"),
                QStringLiteral("json"), QStringLiteral("-show_format"),
                QStringLiteral("-show_streams"), path});
    if (!proc.waitForFinished(20000)) {
        proc.kill();
        return meta;
    }

    const auto doc = QJsonDocument::fromJson(proc.readAllStandardOutput());
    const QJsonObject root = doc.object();
    const double durSec = root.value(QStringLiteral("format"))
                              .toObject()
                              .value(QStringLiteral("duration"))
                              .toString()
                              .toDouble();
    meta.durationMs = static_cast<qint64>(durSec * 1000.0);
    for (const auto& streamVal : root.value(QStringLiteral("streams")).toArray()) {
        const QJsonObject s = streamVal.toObject();
        if (s.value(QStringLiteral("codec_type")).toString() == QLatin1String("video")
            && meta.width == 0) {
            meta.width = s.value(QStringLiteral("width")).toInt();
            meta.height = s.value(QStringLiteral("height")).toInt();
            meta.videoCodec = s.value(QStringLiteral("codec_name")).toString();
            const QString rate = s.value(QStringLiteral("avg_frame_rate")).toString();
            const auto parts = rate.split(QLatin1Char('/'));
            if (parts.size() == 2 && parts[1].toDouble() > 0)
                meta.frameRate = parts[0].toDouble() / parts[1].toDouble();
        } else if (s.value(QStringLiteral("codec_type")).toString() == QLatin1String("audio")
                   && meta.audioCodec.isEmpty()) {
            meta.audioCodec = s.value(QStringLiteral("codec_name")).toString();
        }
    }
    meta.valid = meta.durationMs > 0 || (meta.width > 0 && meta.height > 0);
    return meta;
}

bool VideoPlayer::load(const QString& path)
{
    if (!QFileInfo::exists(path)) {
        emit loadFailed(QStringLiteral("File does not exist."));
        return false;
    }

    m_meta = probeMetadata(path);
    if (!m_meta.valid) {
        emit loadFailed(QStringLiteral("Unsupported or unreadable video file."));
        return false;
    }

    if (m_meta.frameRate > 1.0)
        m_frameStepMs = qMax<qint64>(1, static_cast<qint64>(1000.0 / m_meta.frameRate));
    else
        m_frameStepMs = 33;

    m_player->setSource(QUrl::fromLocalFile(path));
    emit loaded(m_meta);
    if (m_meta.durationMs > 0)
        emit durationChanged(m_meta.durationMs);
    return true;
}

void VideoPlayer::play()
{
    m_player->play();
}

void VideoPlayer::pause()
{
    m_player->pause();
}

void VideoPlayer::togglePlayPause()
{
    if (isPlaying())
        pause();
    else
        play();
}

void VideoPlayer::stop()
{
    m_player->stop();
}

void VideoPlayer::seek(qint64 positionMs)
{
    positionMs = qBound(positionMs, 0LL, duration());
    m_player->setPosition(positionMs);
}

void VideoPlayer::stepFrame(int direction)
{
    pause();
    seek(position() + direction * m_frameStepMs);
}

void VideoPlayer::jumpToEnd()
{
    pause();
    seek(duration());
}

void VideoPlayer::setVolume(float volume01)
{
    m_audio->setVolume(qBound(0.0f, volume01, 1.0f));
}

float VideoPlayer::volume() const
{
    return m_audio->volume();
}

void VideoPlayer::setMuted(bool muted)
{
    m_audio->setMuted(muted);
}

bool VideoPlayer::isPlaying() const
{
    return m_player->playbackState() == QMediaPlayer::PlayingState;
}

qint64 VideoPlayer::position() const
{
    return m_player->position();
}

qint64 VideoPlayer::duration() const
{
    const qint64 d = m_player->duration();
    return d > 0 ? d : m_meta.durationMs;
}
