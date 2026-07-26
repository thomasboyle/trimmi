#include "MainWindow.h"
#include "Theme.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QProxyStyle>
#include <QString>
#include <QtGlobal>
#include <QWidget>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

#ifndef TRIMMI_VERSION_STR
#  define TRIMMI_VERSION_STR "0.0.0"
#endif

namespace {

void configurePixelFontPlatform()
{
#if defined(Q_OS_WIN)
    // DirectWrite ignores NoAntialias for many faces; FreeType honors crisp pixel fonts.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "windows:fontengine=freetype");
    }
#endif
    // Fractional DPI (125%/150%) blurs pixel grids; snap to integer scale factors.
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::Round);
}

void applyCrispPixelFont(QFont& font)
{
    font.setHintingPreference(QFont::PreferFullHinting);
    font.setStyleStrategy(static_cast<QFont::StyleStrategy>(
        QFont::NoAntialias | QFont::NoSubpixelAntialias));
}

bool loadAppFonts()
{
    const int regularId =
        QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/PixelifySans-Regular.ttf"));
    const int boldId =
        QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/PixelifySans-Bold.ttf"));
    return regularId >= 0 && boldId >= 0;
}

class CrispPixelStyle final : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;

    void polish(QWidget* widget) override
    {
        QProxyStyle::polish(widget);
        if (!widget) {
            return;
        }

        QFont font = widget->font();
        const auto strategy = static_cast<QFont::StyleStrategy>(
            QFont::NoAntialias | QFont::NoSubpixelAntialias);
        if (font.styleStrategy() == strategy
            && font.hintingPreference() == QFont::PreferFullHinting) {
            return;
        }

        applyCrispPixelFont(font);
        widget->setFont(font);
    }
};

QIcon makeAppIcon()
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

} // namespace

int main(int argc, char* argv[])
{
    configurePixelFontPlatform();

#if defined(Q_OS_WIN)
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Trimmi"));
    QApplication::setOrganizationName(QStringLiteral("Trimmi"));
    QApplication::setApplicationVersion(QLatin1String(TRIMMI_VERSION_STR));
    app.setStyle(new CrispPixelStyle);

    if (!loadAppFonts()) {
        qWarning("Failed to load bundled Pixelify Sans fonts");
    }
    QApplication::setWindowIcon(makeAppIcon());

    QFont font;
    font.setFamily(QString::fromUtf8(Theme::UiFontFamily));
    font.setPixelSize(14);
    applyCrispPixelFont(font);
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
