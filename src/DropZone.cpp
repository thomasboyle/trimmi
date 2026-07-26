#include "DropZone.h"
#include "Theme.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
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
    setMinimumHeight(168);
    setFrameShape(QFrame::NoFrame);
    setObjectName(QStringLiteral("dropZone"));

    auto* icon = new QLabel(this);
    icon->setAlignment(Qt::AlignCenter);
    icon->setPixmap(QPixmap(QStringLiteral(":/ui/icon-camera-add.png"))
                        .scaled(48, 48, Qt::KeepAspectRatio, Qt::FastTransformation));
    icon->setStyleSheet(QStringLiteral("background: transparent;"));

    auto* dropText = new QLabel(QStringLiteral("Drop a video file here"), this);
    dropText->setAlignment(Qt::AlignCenter);
    dropText->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 14px; font-weight: 700; background: transparent;")
                                .arg(QLatin1String(Theme::TextPrimary)));

    auto* orText = new QLabel(QStringLiteral("or"), this);
    orText->setAlignment(Qt::AlignCenter);
    orText->setObjectName(QStringLiteral("helperText"));

    auto* selectBtn = new QPushButton(QStringLiteral("Select File"), this);
    selectBtn->setCursor(Qt::PointingHandCursor);
    selectBtn->setFixedWidth(140);

    auto* leaf = new QLabel(this);
    leaf->setObjectName(QStringLiteral("leafSprig"));
    leaf->setAttribute(Qt::WA_TransparentForMouseEvents);
    leaf->setPixmap(QPixmap(QStringLiteral(":/ui/deco-leaf-sprig.png"))
                        .scaled(56, 56, Qt::KeepAspectRatio, Qt::FastTransformation));
    leaf->setStyleSheet(QStringLiteral("background: transparent;"));

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

    QColor wash = Theme::color(Theme::AccentLight);
    wash.setAlpha(m_dragActive ? 56 : 36);
    const QColor border = Theme::color(Theme::Subtitle);

    p.setBrush(wash);
    QPen pen(border, 2.5, Qt::DashLine);
    pen.setDashPattern({4, 3});
    p.setPen(pen);
    p.drawRoundedRect(rect().adjusted(2, 2, -3, -3), 12, 12);

    if (auto* leaf = findChild<QLabel*>(QStringLiteral("leafSprig"))) {
        leaf->move(width() - leaf->width() - 8, 8);
        leaf->raise();
    }
}
