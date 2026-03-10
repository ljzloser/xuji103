#ifndef IEC103_LOGGER_H
#define IEC103_LOGGER_H

#include "../include/iec103/callback/ILogHandler.h"
#include <QString>
#include <QDateTime>
#include <memory>

namespace IEC103 {

class Logger {
public:
    Logger();
    ~Logger() = default;
    
    // 设置日志级别
    void setLevel(LogLevel level) { m_level = level; }
    LogLevel level() const { return m_level; }
    
    // 设置日志回调处理器（每个Logger实例独立）
    void setHandler(ILogHandler* handler) { m_handler = handler; }
    ILogHandler* handler() const { return m_handler; }

    void debug(const QString& msg);
    void info(const QString& msg);
    void warning(const QString& msg);
    void error(const QString& msg);

private:
    void log(LogLevel level, const QString& msg);
    
    LogLevel m_level;
    ILogHandler* m_handler = nullptr;
};

} // namespace IEC103

#endif // IEC103_LOGGER_H