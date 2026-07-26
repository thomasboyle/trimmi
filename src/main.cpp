#include "MainWindow.h"
#include "Theme.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QString>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

#ifndef TRIMMI_VERSION_STR
#  define TRIMMI_VERSION_STR "0.0.0"
#endif

static void loadAppFonts()
{
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/PixelifySans-Regular.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/PixelifySans-Bold.ttf"));
}

static QIcon makeAppIcon()
{
    QPixmap px(64, 64);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setBrush(Theme::color(Theme::PanelBg));
    p.setPen(QPen(Theme::color(Theme::SurfaceBorder), 2));
    p.drawRoundedRect(QRectF(4, 4, 56, 56), 10, 10);
    p.setPen(QPen(Theme::color(Theme::Accent), 6.0, Qt::SolidLine, Qt::SquareCap));
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
    QApplication::setApplicationVersion(QLatin1String(TRIMMI_VERSION_STR));

    loadAppFonts();
    QApplication::setWindowIcon(makeAppIcon());

    QFont font;
    font.setFamily(QString::fromUtf8(Theme::UiFontFamily));
    font.setPixelSize(14);
    font.setStyleStrategy(QFont::NoAntialias);
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
