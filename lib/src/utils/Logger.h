#ifndef IEC103_LOGGER_H
#define IEC103_LOGGER_H

#include "../include/iec103/callback/ILogHandler.h"
#include <QString>
#include <QDateTime>

namespace IEC103 {

class Logger {
public:
    static LogLevel level() { return s_level; }
    static void setLevel(LogLevel level) { s_level = level; }
    
    // 设置日志回调处理器
    static void setHandler(ILogHandler* handler) { s_handler = handler; }
    static ILogHandler* handler() { return s_handler; }

    static void debug(const QString& msg);
    static void info(const QString& msg);
    static void warning(const QString& msg);
    static void error(const QString& msg);

private:
    static void log(LogLevel level, const QString& msg);
    
    static LogLevel s_level;
    static ILogHandler* s_handler;
};

} // namespace IEC103

#endif // IEC103_LOGGER_H
