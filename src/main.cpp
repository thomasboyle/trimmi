#include "MainWindow.h"
#include "Theme.h"

#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QPainter>
#include <QPixmap>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

static QIcon makeAppIcon()
{
    QPixmap px(64, 64);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(QStringLiteral("#1e1e1e")));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRectF(4, 4, 56, 56), 14, 14);
    p.setPen(QPen(QColor(QStringLiteral("#2f7ff0")), 6.0, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(20, 20), QPointF(44, 44));
    p.drawLine(QPointF(44, 20), QPointF(20, 44));
    return QIcon(px);
}

int main(int argc, char* argv[])
{
#if defined(Q_OS_WIN)
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Trimmi"));
    QApplication::setOrganizationName(QStringLiteral("Trimmi"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    QApplication::setWindowIcon(makeAppIcon());

    QFont font(QStringLiteral("Segoe UI"));
    font.setPixelSize(13);
    app.setFont(font);
    app.setStyleSheet(Theme::appStyleSheet());

    MainWindow window;
    window.show();

    if (argc >= 2) {
        const QString path = QString::fromLocal8Bit(argv[1]);
        QMetaObject::invokeMethod(&window, "loadVideo", Qt::QueuedConnection, Q_ARG(QString, path));
    }

    return app.exec();
}
