#include "TitleBar.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QStyle>

TitleBar::TitleBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(36);
    setObjectName(QStringLiteral("titleBar"));

    auto* icon = new QLabel(this);
    icon->setFixedSize(18, 18);
    icon->setPixmap([&] {
        QPixmap px(18, 18);
        px.fill(Qt::transparent);
        QPainter p(&px);
        p.setRenderHint(QPainter::Antialiasing);
        QPen pen(QColor(QLatin1String(Theme::Accent)), 2.2, Qt::SolidLine, Qt::RoundCap);
        p.setPen(pen);
        p.drawLine(QPointF(4, 4), QPointF(14, 14));
        p.drawLine(QPointF(14, 4), QPointF(4, 14));
        return px;
    }());

    m_titleLabel = new QLabel(QStringLiteral("Simple Video Trimmer"), this);
    m_titleLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; font-weight: 500;")
                                    .arg(QLatin1String(Theme::TextPrimary)));

    m_minBtn = new QPushButton(QStringLiteral("—"), this);
    m_maxBtn = new QPushButton(QStringLiteral("□"), this);
    m_closeBtn = new QPushButton(QStringLiteral("✕"), this);

    for (auto* btn : {m_minBtn, m_maxBtn, m_closeBtn}) {
        btn->setObjectName(QStringLiteral("titleButton"));
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setCursor(Qt::ArrowCursor);
    }
    m_closeBtn->setObjectName(QStringLiteral("closeButton"));
    m_closeBtn->setProperty("class", "titleButton");
    // Keep both object names for stylesheet: close uses closeButton hover
    m_closeBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; border: none; min-width: 46px; max-width: 46px;"
        " min-height: 36px; color: %1; font-size: 11px; }"
        "QPushButton:hover { background-color: #e81123; color: white; }")
                                  .arg(QLatin1String(Theme::TextPrimary)));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 0, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(icon, 0, Qt::AlignVCenter);
    layout->addWidget(m_titleLabel, 0, Qt::AlignVCenter);
    layout->addStretch(1);
    layout->addWidget(m_minBtn);
    layout->addWidget(m_maxBtn);
    layout->addWidget(m_closeBtn);

    connect(m_minBtn, &QPushButton::clicked, this, &TitleBar::minimizeClicked);
    connect(m_maxBtn, &QPushButton::clicked, this, &TitleBar::maximizeClicked);
    connect(m_closeBtn, &QPushButton::clicked, this, &TitleBar::closeClicked);
}

void TitleBar::setMaximized(bool maximized)
{
    m_maximized = maximized;
    m_maxBtn->setText(maximized ? QStringLiteral("❐") : QStringLiteral("□"));
}

void TitleBar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), QColor(QLatin1String(Theme::WindowBg)));
    p.setPen(QColor(QLatin1String(Theme::Border)));
    p.drawLine(0, height() - 1, width(), height() - 1);
}

void TitleBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !m_maximized) {
        if (auto* w = window()) {
            m_dragOffset = event->globalPosition().toPoint() - w->frameGeometry().topLeft();
            m_dragging = true;
        }
    }
    QWidget::mousePressEvent(event);
}

void TitleBar::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton) && !m_maximized) {
        if (auto* w = window())
            w->move(event->globalPosition().toPoint() - m_dragOffset);
    }
    QWidget::mouseMoveEvent(event);
}

void TitleBar::mouseReleaseEvent(QMouseEvent* event)
{
    m_dragging = false;
    QWidget::mouseReleaseEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        emit maximizeClicked();
    QWidget::mouseDoubleClickEvent(event);
}
