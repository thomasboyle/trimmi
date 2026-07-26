#include "TimelineWidget.h"
#include "Theme.h"
#include "TimeFormat.h"

#include <QFont>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace {
constexpr int kHandleHalf = 7;
constexpr int kRulerH = 22;
constexpr int kFilmH = 56;
constexpr int kPadX = 12;
}

TimelineWidget::TimelineWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(kRulerH + kFilmH + 18);
    setMouseTracking(true);
    setCursor(Qt::ArrowCursor);
}

void TimelineWidget::setDuration(qint64 durationMs)
{
    m_durationMs = qMax<qint64>(0, durationMs);
    m_startMs = 0;
    m_endMs = m_durationMs;
    m_positionMs = 0;
    update();
}

void TimelineWidget::setPosition(qint64 positionMs)
{
    m_positionMs = qBound(positionMs, 0LL, m_durationMs);
    update();
}

void TimelineWidget::setTrimRange(qint64 startMs, qint64 endMs)
{
    m_startMs = qBound(startMs, 0LL, m_durationMs);
    m_endMs = qBound(endMs, m_startMs, m_durationMs);
    update();
}

void TimelineWidget::setThumbnails(const QVector<QImage>& thumbs)
{
    m_thumbs = thumbs;
    update();
}

void TimelineWidget::setThumbnail(int index, const QImage& image, int totalHint)
{
    if (totalHint > 0 && m_thumbs.size() != totalHint)
        m_thumbs.resize(totalHint);
    if (index < 0)
        return;
    if (index >= m_thumbs.size())
        m_thumbs.resize(index + 1);
    m_thumbs[index] = image;
    update();
}

QRect TimelineWidget::rulerRect() const
{
    return QRect(kPadX, 0, width() - 2 * kPadX, kRulerH);
}

QRect TimelineWidget::filmstripRect() const
{
    return QRect(kPadX, kRulerH, width() - 2 * kPadX, kFilmH);
}

QRect TimelineWidget::trackRect() const
{
    return filmstripRect();
}

int TimelineWidget::msToX(qint64 ms) const
{
    const QRect r = trackRect();
    if (m_durationMs <= 0 || r.width() <= 0)
        return r.left();
    const double t = qBound(0.0, ms / static_cast<double>(m_durationMs), 1.0);
    return r.left() + static_cast<int>(t * r.width());
}

qint64 TimelineWidget::xToMs(int x) const
{
    const QRect r = trackRect();
    if (m_durationMs <= 0 || r.width() <= 0)
        return 0;
    const double t = qBound(0.0, (x - r.left()) / static_cast<double>(r.width()), 1.0);
    return static_cast<qint64>(t * m_durationMs);
}

TimelineWidget::DragTarget TimelineWidget::hitTest(const QPoint& pos) const
{
    const int startX = msToX(m_startMs);
    const int endX = msToX(m_endMs);
    const int playX = msToX(m_positionMs);
    const QRect track = trackRect().adjusted(0, -4, 0, 8);

    if (QRect(startX - kHandleHalf - 2, track.top(), kHandleHalf * 2 + 4, track.height()).contains(pos))
        return DragTarget::Start;
    if (QRect(endX - kHandleHalf - 2, track.top(), kHandleHalf * 2 + 4, track.height()).contains(pos))
        return DragTarget::End;
    if (QRect(playX - 4, track.top() - 6, 8, track.height() + 10).contains(pos))
        return DragTarget::Playhead;
    if (track.contains(pos) && pos.x() >= startX && pos.x() <= endX)
        return DragTarget::Selection;
    if (track.contains(pos))
        return DragTarget::Playhead;
    return DragTarget::None;
}

void TimelineWidget::emitTrim()
{
    emit trimChanged(m_startMs, m_endMs);
}

void TimelineWidget::drawGrip(QPainter& p, int x, const QString& label, bool active) const
{
    const QRect track = filmstripRect();
    const QColor fill = active ? Theme::color(Theme::AccentHover) : Theme::color(Theme::Accent);
    const QColor ink = Theme::color(Theme::TextPrimary);

    QRect handle(x - kHandleHalf, track.top() - 2, kHandleHalf * 2, track.height() + 4);
    p.setPen(QPen(ink, 1));
    p.setBrush(fill);
    p.drawRoundedRect(handle, 3, 3);

    p.setPen(QPen(ink, 1.4));
    const int cx = handle.center().x();
    const int cy = handle.center().y();
    p.drawLine(cx - 2, cy - 6, cx - 2, cy + 6);
    p.drawLine(cx + 2, cy - 6, cx + 2, cy + 6);

    p.setPen(Theme::color(Theme::Subtitle));
    QFont f = font();
    f.setPixelSize(11);
    f.setBold(true);
    f.setHintingPreference(QFont::PreferFullHinting);
    f.setStyleStrategy(static_cast<QFont::StyleStrategy>(
        QFont::NoAntialias | QFont::NoSubpixelAntialias));
    p.setFont(f);
    const QRect labelRect(x - 24, track.bottom() + 2, 48, 14);
    p.drawText(labelRect, Qt::AlignHCenter | Qt::AlignTop, label);
}

void TimelineWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRect ruler = rulerRect();
    const QRect film = filmstripRect();

    // Ruler ticks
    p.setPen(Theme::color(Theme::TextMuted));
    QFont rf = font();
    rf.setPixelSize(11);
    rf.setHintingPreference(QFont::PreferFullHinting);
    rf.setStyleStrategy(static_cast<QFont::StyleStrategy>(
        QFont::NoAntialias | QFont::NoSubpixelAntialias));
    p.setFont(rf);

    if (m_durationMs > 0) {
        const int tickCount = qBound(2, width() / 140, 8);
        for (int i = 0; i <= tickCount; ++i) {
            const qint64 ms = (m_durationMs * i) / tickCount;
            const int x = msToX(ms);
            p.setPen(Theme::color(Theme::PanelBorder));
            p.drawLine(x, ruler.bottom() - 5, x, ruler.bottom());
            p.setPen(Theme::color(Theme::TextMuted));
            const QString label = TimeFormat::formatMs(ms, true);
            const int align = (i == 0)                   ? Qt::AlignLeft
                              : (i == tickCount)         ? Qt::AlignRight
                                                         : Qt::AlignHCenter;
            QRect tr(x - 60, ruler.top(), 120, ruler.height() - 4);
            if (i == 0)
                tr = QRect(x, ruler.top(), 120, ruler.height() - 4);
            else if (i == tickCount)
                tr = QRect(x - 120, ruler.top(), 120, ruler.height() - 4);
            p.drawText(tr, align | Qt::AlignVCenter, label);
        }
    }

    // Filmstrip background — warm dark olive paper, not pure black
    p.setPen(QPen(Theme::color(Theme::SurfaceBorder), 1));
    p.setBrush(QColor(QStringLiteral("#2A2924")));
    p.drawRoundedRect(film, 10, 10);

    if (!m_thumbs.isEmpty() && film.width() > 0) {
        const int n = m_thumbs.size();
        for (int i = 0; i < n; ++i) {
            const int x0 = film.left() + (film.width() * i) / n;
            const int x1 = film.left() + (film.width() * (i + 1)) / n;
            QRect cell(x0, film.top(), qMax(1, x1 - x0), film.height());
            if (!m_thumbs[i].isNull()) {
                const QImage scaled =
                    m_thumbs[i].scaled(cell.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                QRect src = QRect(0, 0, scaled.width(), scaled.height());
                if (scaled.width() > cell.width())
                    src.moveLeft((scaled.width() - cell.width()) / 2);
                if (scaled.height() > cell.height())
                    src.moveTop((scaled.height() - cell.height()) / 2);
                src.setSize(cell.size());
                p.drawImage(cell, scaled, src);
            }
            p.setPen(QColor(42, 50, 32, 50));
            p.drawLine(x1, film.top(), x1, film.bottom());
        }
    }

    // Dim outside selection
    if (m_durationMs > 0) {
        const int sx = msToX(m_startMs);
        const int ex = msToX(m_endMs);
        p.fillRect(QRect(film.left(), film.top(), sx - film.left(), film.height()),
                   QColor(30, 29, 26, 140));
        p.fillRect(QRect(ex, film.top(), film.right() - ex + 1, film.height()),
                   QColor(30, 29, 26, 140));

        // Selection overlay — sage stipple wash
        QColor sel = Theme::color(Theme::Accent);
        sel.setAlpha(80);
        p.fillRect(QRect(sx, film.top(), qMax(1, ex - sx), film.height()), sel);
        p.setPen(QPen(Theme::color(Theme::AccentLight), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRect(sx, film.top(), qMax(1, ex - sx), film.height() - 1));
    }

    // Playhead
    if (m_durationMs > 0) {
        const int px = msToX(m_positionMs);
        const QColor ink = Theme::color(Theme::Surface);
        p.setPen(QPen(ink, 1.5));
        p.drawLine(px, film.top() - 4, px, film.bottom() + 2);
        QPainterPath tri;
        tri.moveTo(px, film.top() - 1);
        tri.lineTo(px - 5, film.top() - 8);
        tri.lineTo(px + 5, film.top() - 8);
        tri.closeSubpath();
        p.setBrush(ink);
        p.setPen(Qt::NoPen);
        p.drawPath(tri);
    }

    // Handles
    if (m_durationMs > 0) {
        drawGrip(p, msToX(m_startMs), QStringLiteral("Start"), m_hoverStart || m_drag == DragTarget::Start);
        drawGrip(p, msToX(m_endMs), QStringLiteral("End"), m_hoverEnd || m_drag == DragTarget::End);
    }
}

void TimelineWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || m_durationMs <= 0)
        return;

    m_drag = hitTest(event->pos());
    if (m_drag == DragTarget::Selection) {
        m_dragOffsetMs = xToMs(event->pos().x()) - m_startMs;
    } else if (m_drag == DragTarget::Playhead || m_drag == DragTarget::None) {
        m_drag = DragTarget::Playhead;
        const qint64 ms = xToMs(event->pos().x());
        m_positionMs = ms;
        emit seekRequested(ms);
        update();
    }
}

void TimelineWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_durationMs <= 0)
        return;

    if (m_drag == DragTarget::None) {
        const DragTarget hit = hitTest(event->pos());
        m_hoverStart = hit == DragTarget::Start;
        m_hoverEnd = hit == DragTarget::End;
        if (hit == DragTarget::Start || hit == DragTarget::End)
            setCursor(Qt::SizeHorCursor);
        else if (hit == DragTarget::Playhead)
            setCursor(Qt::PointingHandCursor);
        else
            setCursor(Qt::ArrowCursor);
        update();
        return;
    }

    const qint64 ms = xToMs(event->pos().x());
    if (m_drag == DragTarget::Start) {
        m_startMs = qBound(ms, 0LL, m_endMs);
        emitTrim();
    } else if (m_drag == DragTarget::End) {
        m_endMs = qBound(ms, m_startMs, m_durationMs);
        emitTrim();
    } else if (m_drag == DragTarget::Playhead) {
        m_positionMs = ms;
        emit seekRequested(ms);
    } else if (m_drag == DragTarget::Selection) {
        const qint64 span = m_endMs - m_startMs;
        qint64 newStart = ms - m_dragOffsetMs;
        newStart = qBound(newStart, 0LL, m_durationMs - span);
        m_startMs = newStart;
        m_endMs = newStart + span;
        emitTrim();
    }
    update();
}

void TimelineWidget::mouseReleaseEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    m_drag = DragTarget::None;
}

void TimelineWidget::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    m_hoverStart = false;
    m_hoverEnd = false;
    update();
}

void TimelineWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    update();
}
