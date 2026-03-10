#include "Logger.h"
#include <QDebug>
#include <QDateTime>

namespace IEC103 {

Logger::Logger()
    : m_level(LogLevel::Info)
    , m_handler(nullptr)
{
}

void Logger::log(LogLevel level, const QString& msg) {
    if (m_level > level) {
        return;  // 日志级别过滤
    }
    
    QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    
    // 如果设置了回调处理器，使用回调
    if (m_handler) {
        LogRecord record;
        record.level = level;
        record.message = msg;
        record.timestamp = ts;
        record.dateTime = QDateTime::currentDateTime();
        m_handler->onLog(record);
        return;
    }
    
    // 默认输出到qDebug
    QString levelStr;
    QtMsgType msgType;
    switch (level) {
        case LogLevel::Debug:
            levelStr = "DEBUG";
            msgType = QtDebugMsg;
            break;
        case LogLevel::Info:
            levelStr = "INFO";
            msgType = QtInfoMsg;
            break;
        case LogLevel::Warning:
            levelStr = "WARN";
            msgType = QtWarningMsg;
            break;
        case LogLevel::Error:
            levelStr = "ERROR";
            msgType = QtCriticalMsg;
            break;
    }
    
    QString formatted = QString("[%1] [%2] %3").arg(ts, levelStr, msg);
    
    switch (msgType) {
        case QtDebugMsg:
            qDebug().noquote() << formatted;
            break;
        case QtInfoMsg:
            qInfo().noquote() << formatted;
            break;
        case QtWarningMsg:
            qWarning().noquote() << formatted;
            break;
        case QtCriticalMsg:
            qCritical().noquote() << formatted;
            break;
        default:
            break;
    }
}

void Logger::debug(const QString& msg) {
    log(LogLevel::Debug, msg);
}

void Logger::info(const QString& msg) {
    log(LogLevel::Info, msg);
}

void Logger::warning(const QString& msg) {
    log(LogLevel::Warning, msg);
}

void Logger::error(const QString& msg) {
    log(LogLevel::Error, msg);
}

} // namespace IEC103
