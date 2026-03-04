#include "Logger.h"
#include <QDebug>

namespace IEC103 {

LogLevel Logger::s_level = LogLevel::Info;

void Logger::debug(const QString& msg) {
    if (s_level <= LogLevel::Debug) {
        qDebug() << "[DEBUG]" << msg;
    }
}

void Logger::info(const QString& msg) {
    if (s_level <= LogLevel::Info) {
        qDebug() << "[INFO]" << msg;
    }
}

void Logger::warning(const QString& msg) {
    if (s_level <= LogLevel::Warning) {
        qWarning() << "[WARN]" << msg;
    }
}

void Logger::error(const QString& msg) {
    if (s_level <= LogLevel::Error) {
        qCritical() << "[ERROR]" << msg;
    }
}

} // namespace IEC103
