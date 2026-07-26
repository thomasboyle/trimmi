#include "DropZone.h"
#include "Theme.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QSet>
#include <QVBoxLayout>

DropZone::DropZone(QWidget* parent)
    : QFrame(parent)
{
    setAcceptDrops(true);
    setMinimumHeight(160);
    setObjectName(QStringLiteral("dropZone"));

    auto* icon = new QLabel(this);
    icon->setAlignment(Qt::AlignCenter);
    icon->setPixmap([&] {
        QPixmap px(40, 40);
        px.fill(Qt::transparent);
        QPainter p(&px);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor(QLatin1String(Theme::TextSecondary)), 1.8));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(8, 10, 24, 22), 3, 3);
        p.drawLine(QPointF(20, 6), QPointF(20, 22));
        p.drawLine(QPointF(14, 14), QPointF(20, 6));
        p.drawLine(QPointF(26, 14), QPointF(20, 6));
        return px;
    }());

    auto* dropText = new QLabel(QStringLiteral("Drop a video file here"), this);
    dropText->setAlignment(Qt::AlignCenter);
    dropText->setStyleSheet(QStringLiteral("color: %1; font-size: 13px;")
                                .arg(QLatin1String(Theme::TextPrimary)));

    auto* orText = new QLabel(QStringLiteral("or"), this);
    orText->setAlignment(Qt::AlignCenter);
    orText->setObjectName(QStringLiteral("helperText"));

    auto* selectBtn = new QPushButton(QStringLiteral("Select File"), this);
    selectBtn->setObjectName(QStringLiteral("accentButton"));
    selectBtn->setCursor(Qt::PointingHandCursor);
    selectBtn->setFixedWidth(140);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 18, 16, 18);
    layout->setSpacing(8);
    layout->addStretch();
    layout->addWidget(icon, 0, Qt::AlignHCenter);
    layout->addWidget(dropText);
    layout->addWidget(orText);
    layout->addWidget(selectBtn, 0, Qt::AlignHCenter);
    layout->addStretch();

    connect(selectBtn, &QPushButton::clicked, this, &DropZone::selectFileClicked);
}

bool DropZone::isVideoFile(const QString& path) const
{
    static const QSet<QString> exts = {
        QStringLiteral("mp4"),  QStringLiteral("mov"), QStringLiteral("mkv"),
        QStringLiteral("webm"), QStringLiteral("avi"), QStringLiteral("m4v"),
        QStringLiteral("wmv"),  QStringLiteral("flv"), QStringLiteral("ts"),
        QStringLiteral("mts"),  QStringLiteral("m2ts"), QStringLiteral("mpg"),
        QStringLiteral("mpeg"), QStringLiteral("3gp")};
    return exts.contains(QFileInfo(path).suffix().toLower());
}

void DropZone::dragEnterEvent(QDragEnterEvent* event)
{
    if (!event->mimeData()->hasUrls())
        return;
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile() && isVideoFile(url.toLocalFile())) {
            m_dragActive = true;
            event->acceptProposedAction();
            update();
            return;
        }
    }
}

void DropZone::dragLeaveEvent(QDragLeaveEvent* event)
{
    Q_UNUSED(event);
    m_dragActive = false;
    update();
}

void DropZone::dropEvent(QDropEvent* event)
{
    m_dragActive = false;
    update();
    for (const QUrl& url : event->mimeData()->urls()) {
        if (!url.isLocalFile())
            continue;
        const QString path = url.toLocalFile();
        if (isVideoFile(path)) {
            emit fileDropped(path);
            event->acceptProposedAction();
            return;
        }
    }
}

void DropZone::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QColor fill = m_dragActive ? QColor(47, 127, 240, 28)
                                     : QColor(QLatin1String(Theme::PanelBgRaised));
    const QColor border = m_dragActive ? QColor(QLatin1String(Theme::Accent))
                                       : QColor(QLatin1String(Theme::BorderMuted));

    p.setBrush(fill);
    QPen pen(border, 1.5, Qt::DashLine);
    pen.setDashPattern({4, 3});
    p.setPen(pen);
    p.drawRoundedRect(rect().adjusted(1, 1, -2, -2), 10, 10);
}
