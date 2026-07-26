#pragma once

#include <QImage>
#include <QVector>
#include <QWidget>

class TimelineWidget : public QWidget {
    Q_OBJECT
public:
    explicit TimelineWidget(QWidget* parent = nullptr);

    void setDuration(qint64 durationMs);
    void setPosition(qint64 positionMs);
    void setTrimRange(qint64 startMs, qint64 endMs);
    void setThumbnails(const QVector<QImage>& thumbs);
    void setThumbnail(int index, const QImage& image, int totalHint = -1);

    qint64 startMs() const { return m_startMs; }
    qint64 endMs() const { return m_endMs; }
    qint64 positionMs() const { return m_positionMs; }
    qint64 durationMs() const { return m_durationMs; }

signals:
    void trimChanged(qint64 startMs, qint64 endMs);
    void seekRequested(qint64 positionMs);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    enum class DragTarget { None, Start, End, Playhead, Selection };

    QRect trackRect() const;
    QRect filmstripRect() const;
    QRect rulerRect() const;
    int msToX(qint64 ms) const;
    qint64 xToMs(int x) const;
    DragTarget hitTest(const QPoint& pos) const;
    void emitTrim();
    void drawGrip(QPainter& p, int x, const QString& label, bool active) const;

    qint64 m_durationMs = 0;
    qint64 m_positionMs = 0;
    qint64 m_startMs = 0;
    qint64 m_endMs = 0;
    QVector<QImage> m_thumbs;

    DragTarget m_drag = DragTarget::None;
    qint64 m_dragOffsetMs = 0;
    bool m_hoverStart = false;
    bool m_hoverEnd = false;
};
