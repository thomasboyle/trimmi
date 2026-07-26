#include "GpuStatusCard.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

namespace {

QPixmap makeStatusIcon(bool ok)
{
    QPixmap px(22, 22);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    if (ok) {
        p.setBrush(QColor(QLatin1String(Theme::Success)));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QRectF(1, 1, 20, 20));
        p.setPen(QPen(Qt::white, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(QPointF(6, 11.5), QPointF(9.5, 15));
        p.drawLine(QPointF(9.5, 15), QPointF(16, 7.5));
    } else {
        p.setBrush(QColor(QLatin1String(Theme::TextMuted)));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QRectF(1, 1, 20, 20));
        p.setPen(QPen(Qt::white, 2.0, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(7, 7), QPointF(15, 15));
        p.drawLine(QPointF(15, 7), QPointF(7, 15));
    }
    return px;
}

} // namespace

GpuStatusCard::GpuStatusCard(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("gpuCard"));
    setMinimumHeight(72);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(22, 22);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setStyleSheet(QStringLiteral("font-weight: 600; font-size: 12px;"));

    m_nameLabel = new QLabel(this);
    m_nameLabel->setObjectName(QStringLiteral("helperText"));
    m_nameLabel->setWordWrap(true);

    auto* textCol = new QVBoxLayout;
    textCol->setSpacing(2);
    textCol->setContentsMargins(0, 0, 0, 0);
    textCol->addWidget(m_titleLabel);
    textCol->addWidget(m_nameLabel);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);
    layout->addWidget(m_iconLabel, 0, Qt::AlignTop);
    layout->addLayout(textCol, 1);

    setGpuInfo({});
}

void GpuStatusCard::setGpuInfo(const GpuInfo& info)
{
    m_iconLabel->setPixmap(makeStatusIcon(info.available));
    if (info.available) {
        m_titleLabel->setText(QStringLiteral("GPU Acceleration: Available"));
        m_titleLabel->setStyleSheet(
            QStringLiteral("font-weight: 600; font-size: 12px; color: %1;")
                .arg(QLatin1String(Theme::Success)));
        m_nameLabel->setText(info.name.isEmpty() ? QStringLiteral("Hardware encoder detected")
                                                 : info.name);
    } else {
        m_titleLabel->setText(QStringLiteral("GPU Acceleration: Unavailable"));
        m_titleLabel->setStyleSheet(
            QStringLiteral("font-weight: 600; font-size: 12px; color: %1;")
                .arg(QLatin1String(Theme::TextSecondary)));
        m_nameLabel->setText(info.name.isEmpty()
                                 ? QStringLiteral("Will use CPU encoders")
                                 : info.name + QStringLiteral(" (no HW encoder)"));
    }
}
