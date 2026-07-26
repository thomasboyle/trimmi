#pragma once

#include <QString>

namespace TimeFormat {

QString formatMs(qint64 milliseconds, bool includeMillis = true);
qint64 parseToMs(const QString& text, bool* ok = nullptr);

} // namespace TimeFormat
