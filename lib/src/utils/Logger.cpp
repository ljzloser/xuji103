#include "Logger.h"
#include <QDebug>
#include <QDateTime>

namespace IEC103 {

LogLevel Logger::s_level = LogLevel::Info;

static QString timestamp() {
    return QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
}

void Logger::debug(const QString& msg) {
    if (s_level <= LogLevel::Debug) {
        qDebug().noquote() << QString("[%1] [DEBUG] %2").arg(timestamp(), msg);
    }
}

void Logger::info(const QString& msg) {
    if (s_level <= LogLevel::Info) {
        qDebug().noquote() << QString("[%1] [INFO] %2").arg(timestamp(), msg);
    }
}

void Logger::warning(const QString& msg) {
    if (s_level <= LogLevel::Warning) {
        qWarning().noquote() << QString("[%1] [WARN] %2").arg(timestamp(), msg);
    }
}

void Logger::error(const QString& msg) {
    if (s_level <= LogLevel::Error) {
        qCritical().noquote() << QString("[%1] [ERROR] %2").arg(timestamp(), msg);
    }
}

} // namespace IEC103
