#ifndef IEC103_LOGGER_H
#define IEC103_LOGGER_H

#include <QString>

namespace IEC103 {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class Logger {
public:
    static LogLevel level() { return s_level; }
    static void setLevel(LogLevel level) { s_level = level; }

    static void debug(const QString& msg);
    static void info(const QString& msg);
    static void warning(const QString& msg);
    static void error(const QString& msg);

private:
    static LogLevel s_level;
};

} // namespace IEC103

#endif // IEC103_LOGGER_H