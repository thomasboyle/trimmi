#include "ThumbnailGenerator.h"

#include <QMetaObject>
#include <QtConcurrent>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

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
    QtConcurrent::run([this, path, durationMs, count, generation]() {
        runGenerate(path, durationMs, count, generation);
    });
}

void ThumbnailGenerator::runGenerate(QString path, qint64 durationMs, int count, int generation)
{
    if (count <= 0 || durationMs <= 0) {
        QMetaObject::invokeMethod(this, [this]() { emit finished(); }, Qt::QueuedConnection);
        return;
    }

    AVFormatContext* fmt = nullptr;
    const QByteArray pathUtf8 = path.toUtf8();
    if (avformat_open_input(&fmt, pathUtf8.constData(), nullptr, nullptr) < 0) {
        QMetaObject::invokeMethod(
            this,
            [this]() { emit failed(QStringLiteral("Could not open video for thumbnails.")); },
            Qt::QueuedConnection);
        return;
    }
    if (avformat_find_stream_info(fmt, nullptr) < 0) {
        avformat_close_input(&fmt);
        QMetaObject::invokeMethod(
            this, [this]() { emit failed(QStringLiteral("Could not read stream info.")); },
            Qt::QueuedConnection);
        return;
    }

    int videoStream = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoStream < 0) {
        avformat_close_input(&fmt);
        QMetaObject::invokeMethod(
            this, [this]() { emit failed(QStringLiteral("No video stream found.")); },
            Qt::QueuedConnection);
        return;
    }

    AVStream* st = fmt->streams[videoStream];
    const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!codec) {
        avformat_close_input(&fmt);
        QMetaObject::invokeMethod(
            this, [this]() { emit failed(QStringLiteral("Unsupported video codec.")); },
            Qt::QueuedConnection);
        return;
    }

    AVCodecContext* dec = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(dec, st->codecpar);
    if (avcodec_open2(dec, codec, nullptr) < 0) {
        avcodec_free_context(&dec);
        avformat_close_input(&fmt);
        QMetaObject::invokeMethod(
            this, [this]() { emit failed(QStringLiteral("Failed to open decoder.")); },
            Qt::QueuedConnection);
        return;
    }

    AVFrame* frame = av_frame_alloc();
    AVFrame* rgb = av_frame_alloc();
    AVPacket* pkt = av_packet_alloc();

    const int thumbW = 160;
    const int thumbH = qMax(1, static_cast<int>(thumbW * (dec->height / static_cast<double>(qMax(1, dec->width)))));

    SwsContext* sws = sws_getContext(dec->width, dec->height, dec->pix_fmt, thumbW, thumbH,
                                     AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);

    const int bufSize = av_image_get_buffer_size(AV_PIX_FMT_RGB24, thumbW, thumbH, 1);
    QByteArray buffer(bufSize, 0);
    av_image_fill_arrays(rgb->data, rgb->linesize, reinterpret_cast<uint8_t*>(buffer.data()),
                         AV_PIX_FMT_RGB24, thumbW, thumbH, 1);

    for (int i = 0; i < count; ++i) {
        if (m_generation.load() != generation)
            break;

        const qint64 targetMs = (count == 1)
                                    ? 0
                                    : static_cast<qint64>((durationMs * i) / static_cast<double>(count));
        const int64_t ts = av_rescale_q(targetMs, AVRational{1, 1000}, st->time_base);
        av_seek_frame(fmt, videoStream, ts, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(dec);

        bool got = false;
        while (av_read_frame(fmt, pkt) >= 0) {
            if (pkt->stream_index != videoStream) {
                av_packet_unref(pkt);
                continue;
            }
            if (avcodec_send_packet(dec, pkt) == 0) {
                while (avcodec_receive_frame(dec, frame) == 0) {
                    sws_scale(sws, frame->data, frame->linesize, 0, dec->height, rgb->data,
                              rgb->linesize);
                    QImage img(rgb->data[0], thumbW, thumbH, rgb->linesize[0], QImage::Format_RGB888);
                    const QImage copy = img.copy();
                    QMetaObject::invokeMethod(
                        this,
                        [this, i, copy]() { emit thumbnailReady(i, copy); },
                        Qt::QueuedConnection);
                    got = true;
                    break;
                }
            }
            av_packet_unref(pkt);
            if (got)
                break;
        }
        if (!got) {
            QImage blank(thumbW, thumbH, QImage::Format_RGB888);
            blank.fill(Qt::black);
            QMetaObject::invokeMethod(
                this, [this, i, blank]() { emit thumbnailReady(i, blank); }, Qt::QueuedConnection);
        }
    }

    sws_freeContext(sws);
    av_packet_free(&pkt);
    av_frame_free(&rgb);
    av_frame_free(&frame);
    avcodec_free_context(&dec);
    avformat_close_input(&fmt);

    if (m_generation.load() == generation) {
        QMetaObject::invokeMethod(this, [this]() { emit finished(); }, Qt::QueuedConnection);
    }
}
