#ifndef IEC103_ILOGHANDLER_H
#define IEC103_ILOGHANDLER_H

#include <QString>
#include <QDateTime>

namespace IEC103 {

// 日志级别
enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

// 日志记录结构
struct LogRecord {
    LogLevel level;             // 日志级别
    QString message;            // 日志消息
    QString timestamp;          // 时间戳字符串
    QDateTime dateTime;         // 时间戳对象
};

// 日志回调接口
// 用户需要继承此接口并实现onLog方法来自定义日志输出
class ILogHandler {
public:
    virtual ~ILogHandler() = default;
    
    // 日志回调
    // record: 日志记录信息
    virtual void onLog(const LogRecord& record) = 0;
};

} // namespace IEC103

#endif // IEC103_ILOGHANDLER_H
