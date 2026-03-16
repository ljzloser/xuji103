#ifndef FRAME_PARSER_H
#define FRAME_PARSER_H

#include <QString>
#include <QByteArray>
#include <QList>
#include <QDateTime>

// 字段信息结构
struct FieldInfo {
    QString name;           // 字段名称
    QString value;          // 字段值
    int start;              // 起始字节位置
    int end;                // 结束字节位置
    QList<FieldInfo> children;

    FieldInfo() : start(0), end(0) {}
    FieldInfo(const QString& n, const QString& v, int s, int e)
        : name(n), value(v), start(s), end(e) {}
};

// 帧信息
struct FrameInfo {
    QByteArray raw;         // 原始数据
    QString timestamp;      // 时间戳
    QString direction;      // 方向: "recv" 或 "send"
    int frameType;          // 帧类型: 0=I, 1=S, 3=U, -1=解析失败
    uint8_t typeId;         // ASDU类型标识
    bool failed;            // 解析失败标志
    QString error;          // 错误信息
    QString rawLine;        // 原始日志行
    QList<FieldInfo> fields;

    FrameInfo() : frameType(-1), typeId(0), failed(false) {}
    
    // 便捷方法
    QString typeName() const {
        if (failed) return "解析失败";
        switch (frameType) {
            case 0: return "I帧";
            case 1: return "S帧";
            case 3: return "U帧";
            default: return "?";
        }
    }
};

// 报文解析器 - 复用lib中的解析逻辑
class FrameParser {
public:
    FrameParser();

    // 解析日志文件
    QList<FrameInfo> parseLogFile(const QString& filepath);

    // 解析单行日志
    static bool parseLogLine(const QString& line, QString& timestamp, 
                             QString& direction, QByteArray& data);

    // 解析帧
    static FrameInfo parseFrame(const QByteArray& data, 
                                const QString& timestamp = "", 
                                const QString& direction = "");

private:
    // 格式化CP56Time2a时标
    static QString formatCP56Time2a(const uint8_t* data);

    // 格式化双点信息
    static QString formatDPI(uint8_t dpi);

    // 格式化FPT故障相别
    static QString formatFPT(uint8_t fpt);

    // 解析ASDU信息对象
    static void parseInfoObject(const QByteArray& infoData, int infoStart,
                                uint8_t typeId, uint8_t vsq, 
                                QList<FieldInfo>& fields);
};

#endif // FRAME_PARSER_H