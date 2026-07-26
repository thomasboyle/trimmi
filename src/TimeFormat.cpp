#include "TimeFormat.h"

#include <QRegularExpression>
#include <QtMath>

namespace TimeFormat {

QString formatMs(qint64 milliseconds, bool includeMillis)
{
    if (milliseconds < 0)
        milliseconds = 0;

    const qint64 totalMs = milliseconds;
    const qint64 hours = totalMs / 3600000;
    const qint64 minutes = (totalMs % 3600000) / 60000;
    const qint64 seconds = (totalMs % 60000) / 1000;
    const qint64 millis = totalMs % 1000;

    if (includeMillis) {
        return QStringLiteral("%1:%2:%3.%4")
            .arg(hours, 2, 10, QLatin1Char('0'))
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'))
            .arg(millis, 3, 10, QLatin1Char('0'));
    }

    return QStringLiteral("%1:%2:%3")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

qint64 parseToMs(const QString& text, bool* ok)
{
    const QString trimmed = text.trimmed();
    static const QRegularExpression re(
        QStringLiteral(R"(^(?:(\d+):)?(\d{1,2}):(\d{1,2})(?:\.(\d{1,3}))?$)"));

    const auto match = re.match(trimmed);
    if (!match.hasMatch()) {
        if (ok)
            *ok = false;
        return 0;
    }

    const qint64 hours = match.captured(1).isEmpty() ? 0 : match.captured(1).toLongLong();
    const qint64 minutes = match.captured(2).toLongLong();
    const qint64 seconds = match.captured(3).toLongLong();
    QString millisStr = match.captured(4);
    if (millisStr.isEmpty())
        millisStr = QStringLiteral("0");
    while (millisStr.size() < 3)
        millisStr.append(QLatin1Char('0'));

    const qint64 millis = millisStr.left(3).toLongLong();
    if (ok)
        *ok = true;
    return hours * 3600000 + minutes * 60000 + seconds * 1000 + millis;
}

} // namespace TimeFormat
