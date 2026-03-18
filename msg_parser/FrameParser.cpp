#include "FrameParser.h"
#include <iec103/asdu/ASDU.h>
#include <iec103/asdu/ASDU1.h>
#include <iec103/asdu/ASDU7_8.h>
#include <iec103/asdu/ASDU42.h>
#include <iec103/asdu/ASDU10_21.h>
#include <iec103/asdu/ASDU11.h>
#include <iec103/types/Constants.h>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#include <cstring>

using namespace IEC103;

namespace {

// U格式控制域名称
QString uFormatName(uint8_t byte1) {
    switch (byte1) {
        case 0x07: return "STARTDT_act";
        case 0x0B: return "STARTDT_con";
        case 0x13: return "STOPDT_act";
        case 0x23: return "STOPDT_con";
        case 0x43: return "TESTFR_act";
        case 0x83: return "TESTFR_con";
        default: return QString("未知U帧(%1)").arg(byte1, 2, 16, QChar('0'));
    }
}

// ASDU类型名称
QString asduTypeName(uint8_t ti) {
    switch (ti) {
        case 1: return "ASDU1-带时标报文(双点遥信)";
        case 2: return "ASDU2-带相对时间时标报文(保护事件)";
        case 7: return "ASDU7-总召唤命令";
        case 8: return "ASDU8-总召唤结束";
        case 10: return "ASDU10-通用分类数据";
        case 11: return "ASDU11-通用分类标识";
        case 21: return "ASDU21-通用分类读命令";
        case 42: return "ASDU42-双点信息状态";
        default: return QString("未知类型%1").arg(ti);
    }
}

// 传输原因名称
QString cotName(uint8_t cot) {
    switch (cot) {
        case 1: return "自发/突发";
        case 2: return "循环";
        case 3: return "复位FCB";
        case 4: return "复位CU";
        case 5: return "启动/重启";
        case 6: return "电源合上";
        case 7: return "测试模式";
        case 8: return "时间同步";
        case 9: return "总召唤";
        case 10: return "总召唤终止";
        case 11: return "当地操作";
        case 12: return "远方操作";
        case 20: return "命令肯定认可";
        case 21: return "命令否定认可";
        case 31: return "扰动数据传送";
        case 40: return "写命令肯定";
        case 41: return "写命令否定";
        case 42: return "读命令有效响应";
        case 43: return "读命令无效响应";
        default: return QString("未知原因%1").arg(cot);
    }
}

// 功能类型名称
QString funName(uint8_t fun) {
    switch (fun) {
        case 0xFE: return "GEN(通用分类)";
        case 0xFF: return "GLB(全局)";
        case 128: return "t(Z)距离保护";
        case 160: return "I>>过流保护";
        case 176: return "IT变压器差动";
        case 192: return "IL线路差动";
        default: return QString("功能类型%1").arg(fun);
    }
}

// 通用服务INF名称
QString infGenericName(uint8_t inf) {
    switch (inf) {
        case 0xF0: return "读全部组标题";
        case 0xF1: return "读一组全部条目";
        case 0xF3: return "读单条目录";
        case 0xF4: return "读单条值";
        case 0xF5: return "总召唤中止";
        default: return QString("INF=0x%1").arg(inf, 2, 16, QChar('0'));
    }
}

// 数据类型名称
QString dataTypeName(uint8_t dt) {
    switch (dt) {
        case 1: return "OS8ASCII字符串";
        case 2: return "BS16位串";
        case 3: return "UINT无符号整数";
        case 5: return "INT有符号整数";
        case 7: return "R32.23浮点数";
        case 8: return "CP56Time2a时标";
        case 22: return "失败回答码";
        case 23: return "数据结构";
        case 213: return "带绝对时标报文";
        case 214: return "带相对时标报文";
        case 215: return "带相对时标浮点";
        case 216: return "带相对时标整数";
        case 217: return "带相对时标字符";
        default: return QString("类型%1").arg(dt);
    }
}

// KOD名称
QString kodName(uint8_t kod) {
    switch (kod) {
        case 0x01: return "实际值(从装置取)";
        case 0x0A: return "描述字符串";
        case 0x67: return "属性结构(南网扩展)";
        case 0xA8: return "装置实际值(南网)";
        case 0xA9: return "数据库值(南网)";
        default: return QString("KOD=0x%1").arg(kod, 2, 16, QChar('0'));
    }
}

} // namespace

FrameParser::FrameParser() {
}

QList<FrameInfo> FrameParser::parseLogFile(const QString& filepath) {
    QList<FrameInfo> frames;

    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return frames;
    }

    QTextStream in(&file);
    in.setCodec("UTF-8");

    while (!in.atEnd()) {
        QString line = in.readLine();
        QString timestamp, direction;
        QByteArray data;

        if (parseLogLine(line, timestamp, direction, data)) {
            FrameInfo frame = parseFrame(data, timestamp, direction);
            frame.rawLine = line;
            frames.append(frame);
        } else {
            // 日志行解析失败，创建一个失败的 FrameInfo
            FrameInfo frame;
            frame.failed = true;
            frame.rawLine = line;
            frame.error = "无法解析日志行格式";
            // 尝试提取时间戳
            QRegularExpression tsRe(R"(\[(.*?)\])");
            QRegularExpressionMatch tsMatch = tsRe.match(line);
            if (tsMatch.hasMatch()) {
                frame.timestamp = tsMatch.captured(1);
            }
            frames.append(frame);
        }
    }

    file.close();
    return frames;
}

bool FrameParser::parseLogLine(const QString& line, QString& timestamp,
                               QString& direction, QByteArray& data) {
    // 格式: [timestamp] [xuji103_msg] [info] recv/send: hex_data
    QRegularExpression re(R"(\[(.*?)\].*?\[info\]\s+(recv|send):\s+(.*))");
    QRegularExpressionMatch match = re.match(line);

    if (!match.hasMatch()) {
        return false;
    }

    timestamp = match.captured(1);
    direction = match.captured(2);
    QString hexStr = match.captured(3).replace(" ", "");

    // 双重解码：日志中的hex是ASCII编码的
    QByteArray asciiBytes = QByteArray::fromHex(hexStr.toLatin1());
    QString realHex = QString::fromLatin1(asciiBytes).replace(" ", "");
    data = QByteArray::fromHex(realHex.toLatin1());

    return !data.isEmpty();
}

QString FrameParser::formatCP56Time2a(const uint8_t* data) {
    uint16_t ms = data[0] | (data[1] << 8);
    uint8_t minute = data[2] & 0x3F;
    uint8_t hour = data[3] & 0x1F;
    uint8_t day = data[4] & 0x1F;
    uint8_t month = data[5] & 0x0F;
    uint8_t year = data[6];

    return QString("%1-%2-%3 %4:%5:%6.%7")
        .arg(2000 + year, 4, 10, QChar('0'))
        .arg(month, 2, 10, QChar('0'))
        .arg(day, 2, 10, QChar('0'))
        .arg(hour, 2, 10, QChar('0'))
        .arg(minute, 2, 10, QChar('0'))
        .arg(ms / 1000, 2, 10, QChar('0'))
        .arg(ms % 1000, 3, 10, QChar('0'));
}

QString FrameParser::formatDPI(uint8_t dpi) {
    switch (dpi & 0x03) {
        case 0: return "不确定(00)";
        case 1: return "分/OFF";
        case 2: return "合/ON";
        case 3: return "不确定(11)";
        default: return "??";
    }
}

QString FrameParser::formatFPT(uint8_t fpt) {
    if (!(fpt & 0x80)) {
        return QString("无效");
    }
    
    QStringList parts;
    if (fpt & 0x01) parts << QString("A相");
    if (fpt & 0x02) parts << QString("B相");
    if (fpt & 0x04) parts << QString("C相");
    if (fpt & 0x08) parts << QString("接地");
    if (fpt & 0x10) parts << QString("区内");
    
    return parts.isEmpty() ? QString("未知") : parts.join(QString("+"));
}

FrameInfo FrameParser::parseFrame(const QByteArray& data,
                                  const QString& timestamp,
                                  const QString& direction) {
    FrameInfo frame;
    frame.timestamp = timestamp;
    frame.direction = direction;
    frame.raw = data;

    if (data.isEmpty()) {
        frame.failed = true;
        frame.error = "数据为空";
        return frame;
    }

    if (data.size() < 6) {
        frame.failed = true;
        frame.error = QString("数据长度不足: %1字节").arg(data.size());
        return frame;
    }

    if ((uint8_t)data[0] != 0x68) {
        frame.failed = true;
        frame.error = QString("启动字节错误: 0x%1").arg((uint8_t)data[0], 2, 16, QChar('0'));
        return frame;
    }

    int length = (uint8_t)data[1] | ((uint8_t)data[2] << 8);

    // 启动字节
    frame.fields.append(FieldInfo("启动字符", "68H", 0, 1));

    // 长度域
    frame.fields.append(FieldInfo("长度L", QString("%1 (0x%2)")
        .arg(length).arg(length, 4, 16, QChar('0')), 1, 3));

    // APCI
    int apciStart = 3;
    uint8_t byte1 = data[3];

    if ((byte1 & 0x01) == 0) {  // I帧
        frame.frameType = 0;

        uint16_t sendSeq = (byte1 >> 1) | (((uint8_t)data[4] & 0xFE) << 7);
        uint16_t recvSeq = ((uint8_t)data[5] >> 1) | (((uint8_t)data[6] & 0xFE) << 7);

        FieldInfo apci("APCI (I格式)", "", apciStart, 7);
        apci.children.append(FieldInfo("控制域[0]", QString("%1").arg((uint8_t)data[3], 2, 16, QChar('0')), 3, 4));
        apci.children.append(FieldInfo("控制域[1]", QString("%1").arg((uint8_t)data[4], 2, 16, QChar('0')), 4, 5));
        apci.children.append(FieldInfo("控制域[2]", QString("%1").arg((uint8_t)data[5], 2, 16, QChar('0')), 5, 6));
        apci.children.append(FieldInfo("控制域[3]", QString("%1").arg((uint8_t)data[6], 2, 16, QChar('0')), 6, 7));
        apci.children.append(FieldInfo("N(S)", QString::number(sendSeq), 3, 5));
        apci.children.append(FieldInfo("N(R)", QString::number(recvSeq), 5, 7));
        frame.fields.append(apci);

        // 使用lib解析ASDU
        if (length > 4) {
            QByteArray asduData = data.mid(7, length - 4);
            Asdu asdu;
            if (asdu.parse(asduData)) {
                int asduStart = 7;
                FieldInfo asduField("ASDU", "", asduStart, 7 + asduData.size());

                asduField.children.append(FieldInfo("类型标识TI", 
                    QString("%1 (%2)").arg(asdu.ti(), 2, 16, QChar('0')).arg(asduTypeName(asdu.ti())), 
                    asduStart, asduStart + 1));
                asduField.children.append(FieldInfo("VSQ", 
                    QString("%1 (SQ=%2, 数目=%3)").arg(asdu.vsq(), 2, 16, QChar('0'))
                        .arg(asdu.sq() ? "1" : "0").arg(asdu.count()), 
                    asduStart + 1, asduStart + 2));
                uint8_t cotVal = asdu.cot();
                asduField.children.append(FieldInfo("传输原因COT", 
                    QString("%1 (%2)").arg(cotVal).arg(cotName(cotVal)), 
                    asduStart + 2, asduStart + 3));
                asduField.children.append(FieldInfo("ASDU地址", 
                    QString("%1 (设备=%2 CPU=%3)")
                        .arg(asdu.addr())
                        .arg((asdu.addr() >> 8) & 0xFF)
                        .arg(asdu.addr() & 0x07), 
                    asduStart + 3, asduStart + 5));

                // 解析信息对象
                parseInfoObject(asdu.infoObjects(), asduStart + 5, asdu.ti(), asdu.vsq(), asduField.children);

                frame.fields.append(asduField);
            }
        }
    }
    else if (byte1 == 0x01) {  // S帧
        frame.frameType = 1;

        uint16_t recvSeq = ((uint8_t)data[5] >> 1) | (((uint8_t)data[6] & 0xFE) << 7);

        FieldInfo apci("APCI (S格式)", "", apciStart, 7);
        apci.children.append(FieldInfo("控制域[0]", "01 (S格式标识)", 3, 4));
        apci.children.append(FieldInfo("控制域[1]", "00 (保留)", 4, 5));
        apci.children.append(FieldInfo("控制域[2]", QString("%1").arg((uint8_t)data[5], 2, 16, QChar('0')), 5, 6));
        apci.children.append(FieldInfo("控制域[3]", QString("%1").arg((uint8_t)data[6], 2, 16, QChar('0')), 6, 7));
        apci.children.append(FieldInfo("N(R)", QString::number(recvSeq), 5, 7));
        frame.fields.append(apci);
    }
    else if ((byte1 & 0x03) == 0x03) {  // U帧
        frame.frameType = 3;

        QString uName = uFormatName(byte1);

        FieldInfo apci("APCI (U格式)", "", apciStart, 7);
        apci.children.append(FieldInfo("控制域[0]", 
            QString("%1 (%2)").arg(byte1, 2, 16, QChar('0')).arg(uName), 3, 4));
        apci.children.append(FieldInfo("控制域[1]", "00 (保留)", 4, 5));
        apci.children.append(FieldInfo("控制域[2]", "00 (保留)", 5, 6));
        apci.children.append(FieldInfo("控制域[3]", "00 (保留)", 6, 7));
        frame.fields.append(apci);
    }
    else {
        // 未知帧类型
        FieldInfo apci("APCI (未知格式)", "", apciStart, 7);
        apci.children.append(FieldInfo("控制域", 
            QString("%1 %2 %3 %4").arg((uint8_t)data[3], 2, 16, QChar('0'))
                .arg((uint8_t)data[4], 2, 16, QChar('0'))
                .arg((uint8_t)data[5], 2, 16, QChar('0'))
                .arg((uint8_t)data[6], 2, 16, QChar('0')), 3, 7));
        frame.fields.append(apci);
    }

    return frame;
}

void FrameParser::parseInfoObject(const QByteArray& infoData, int infoStart,
                                  uint8_t typeId, uint8_t vsq,
                                  QList<FieldInfo>& fields) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(infoData.constData());
    int len = infoData.size();

    FieldInfo info("信息对象", "", infoStart, infoStart + len);

    // 使用lib中的解析器
    Asdu asdu;
    asdu.setTi(typeId);
    asdu.setVsq(vsq);
    asdu.infoObjects() = infoData;

    if (typeId == 0x07) {  // ASDU7 总召唤命令
        if (len >= 3) {
            info.children.append(FieldInfo("FUN", 
                QString("%1 (%2)").arg(data[0], 2, 16, QChar('0')).arg(funName(data[0])), 
                infoStart, infoStart + 1));
            info.children.append(FieldInfo("INF", QString("%1").arg(data[1], 2, 16, QChar('0')), 
                infoStart + 1, infoStart + 2));
            info.children.append(FieldInfo("SCN", QString("%1 (扫描序号)").arg(data[2], 2, 16, QChar('0')), 
                infoStart + 2, infoStart + 3));
        }
    }
    else if (typeId == 0x08) {  // ASDU8 总召唤结束
        if (len >= 3) {
            info.children.append(FieldInfo("FUN", 
                QString("%1 (%2)").arg(data[0], 2, 16, QChar('0')).arg(funName(data[0])), 
                infoStart, infoStart + 1));
            info.children.append(FieldInfo("INF", QString("%1").arg(data[1], 2, 16, QChar('0')), 
                infoStart + 1, infoStart + 2));
            info.children.append(FieldInfo("SCN", QString("%1 (总召唤结束标志)").arg(data[2], 2, 16, QChar('0')), 
                infoStart + 2, infoStart + 3));
        }
    }
    else if (typeId == 0x01) {  // ASDU1 带时标报文
        // 使用lib中的Asdu1Parser
        Asdu1Parser parser;
        if (parser.parse(asdu) && !parser.results().empty()) {
            const auto& result = parser.results()[0];
            info.children.append(FieldInfo("FUN", 
                QString("%1 (%2)").arg(result.fun, 2, 16, QChar('0')).arg(funName(result.fun)), 
                infoStart, infoStart + 1));
            info.children.append(FieldInfo("INF", QString("%1").arg(result.inf, 2, 16, QChar('0')), 
                infoStart + 1, infoStart + 2));
            info.children.append(FieldInfo("DPI", 
                QString("%1 (%2)").arg(static_cast<int>(result.dpi)).arg(formatDPI(static_cast<uint8_t>(result.dpi))), 
                infoStart + 2, infoStart + 3));
            info.children.append(FieldInfo("事件时间", 
                formatCP56Time2a(reinterpret_cast<const uint8_t*>(&result.eventTime)), 
                infoStart + 3, infoStart + 10));
            info.children.append(FieldInfo("接收时间", 
                formatCP56Time2a(reinterpret_cast<const uint8_t*>(&result.recvTime)), 
                infoStart + 10, infoStart + 17));
        }
    }
    else if (typeId == 0x02) {  // ASDU2 带相对时间时标报文
        // 使用lib中的Asdu2Parser
        Asdu2Parser parser;
        if (parser.parse(asdu) && !parser.results().empty()) {
            const auto& result = parser.results()[0];
            info.children.append(FieldInfo("FUN", 
                QString("%1 (%2)").arg(result.fun, 2, 16, QChar('0')).arg(funName(result.fun)), 
                infoStart, infoStart + 1));
            info.children.append(FieldInfo("INF", QString("%1").arg(result.inf, 2, 16, QChar('0')), 
                infoStart + 1, infoStart + 2));
            info.children.append(FieldInfo("DPI", 
                QString("%1 (%2)").arg(static_cast<int>(result.dpi)).arg(formatDPI(static_cast<uint8_t>(result.dpi))), 
                infoStart + 2, infoStart + 3));
            info.children.append(FieldInfo("相对时间RET", QString("%1 ms").arg(result.relativeTime), 
                infoStart + 3, infoStart + 5));
            info.children.append(FieldInfo("故障序号FAN", QString::number(result.faultNo), 
                infoStart + 5, infoStart + 7));
            info.children.append(FieldInfo("事件时间", 
                formatCP56Time2a(reinterpret_cast<const uint8_t*>(&result.eventTime)), 
                infoStart + 7, infoStart + 14));
            info.children.append(FieldInfo("接收时间", 
                formatCP56Time2a(reinterpret_cast<const uint8_t*>(&result.recvTime)), 
                infoStart + 14, infoStart + 21));
            info.children.append(FieldInfo("FPT故障相别", 
                QString("%1 (%2)").arg(result.fpt, 2, 16, QChar('0')).arg(formatFPT(result.fpt)), 
                infoStart + 21, infoStart + 22));
        }
    }
    else if (typeId == 0x2A) {  // ASDU42 双点信息状态
        // 使用lib中的Asdu42Parser
        Asdu42Parser parser;
        if (parser.parse(asdu)) {
            const auto& objects = parser.infoObjects();
            int offset = infoStart;
            for (size_t i = 0; i < objects.size(); ++i) {
                const auto& obj = objects[i];
                FieldInfo item(QString("信息对象[%1]").arg(i + 1), "", offset, offset + 3);
                item.children.append(FieldInfo("FUN", 
                    QString("%1 (%2)").arg(obj.fun, 2, 16, QChar('0')).arg(funName(obj.fun)), 
                    offset, offset + 1));
                item.children.append(FieldInfo("INF", QString("%1").arg(obj.inf, 2, 16, QChar('0')), 
                    offset + 1, offset + 2));
                item.children.append(FieldInfo("DPI", 
                    QString("%1 (%2)").arg(static_cast<int>(obj.dpi)).arg(formatDPI(static_cast<uint8_t>(obj.dpi))), 
                    offset + 2, offset + 3));
                info.children.append(item);
                offset += 3;
            }
            info.children.append(FieldInfo("SIN/SCN", QString("%1").arg(parser.scn(), 2, 16, QChar('0')), 
                offset, offset + 1));
        }
    }
    else if (typeId == 0x0A) {  // ASDU10 通用分类数据
        // 使用lib中的Asdu10Parser
        Asdu10Parser parser;
        if (parser.parse(asdu)) {
            const auto& gdata = parser.data();
            
            info.children.append(FieldInfo("FUN", 
                QString("%1 (%2)").arg(static_cast<uint8_t>(FUN::Generic), 2, 16, QChar('0')).arg(funName(static_cast<uint8_t>(FUN::Generic))), 
                infoStart, infoStart + 1));
            info.children.append(FieldInfo("INF", 
                QString("%1 (%2)").arg(parser.inf(), 2, 16, QChar('0')).arg(infGenericName(parser.inf())), 
                infoStart + 1, infoStart + 2));
            info.children.append(FieldInfo("RII", QString("%1 (返回信息标识)").arg(gdata.rii, 2, 16, QChar('0')), 
                infoStart + 2, infoStart + 3));
            info.children.append(FieldInfo("NGD", QString("数目=%1 CONT=%2").arg(gdata.ngd).arg(gdata.cont ? 1 : 0), 
                infoStart + 3, infoStart + 4));

            // 解析数据项
            int offset = 4;
            for (size_t i = 0; i < gdata.items.size(); ++i) {
                const auto& item = gdata.items[i];
                int itemStart = infoStart + offset;
                int gidLen = item.gdd.totalSize();

                FieldInfo itemField(QString("条目[%1]").arg(i + 1), "", itemStart, itemStart + 6 + gidLen);

                itemField.children.append(FieldInfo("GIN组号", QString::number(item.gin.group), itemStart, itemStart + 1));
                itemField.children.append(FieldInfo("GIN条目", QString::number(item.gin.entry), itemStart + 1, itemStart + 2));
                itemField.children.append(FieldInfo("KOD", 
                    QString("%1 (%2)").arg(item.kod, 2, 16, QChar('0')).arg(kodName(item.kod)), 
                    itemStart + 2, itemStart + 3));
                itemField.children.append(FieldInfo("GDD类型", 
                    QString("%1 (%2)").arg(item.gdd.dataType).arg(dataTypeName(item.gdd.dataType)), 
                    itemStart + 3, itemStart + 4));
                itemField.children.append(FieldInfo("GDD长度", QString("%1 字节").arg(item.gdd.dataSize), 
                    itemStart + 4, itemStart + 5));
                itemField.children.append(FieldInfo("GDD数目", QString::number(item.gdd.number), 
                    itemStart + 5, itemStart + 6));

                // 解析GID值
                QString valStr;
                if (item.gdd.dataType == 7 && item.gid.size() >= 4) {  // 浮点数
                    float val = item.toFloat();
                    valStr = QString::number(val, 'f', 6);
                }
                else if (item.gdd.dataType == 3 && item.gid.size() >= 4) {  // 无符号整数
                    valStr = QString::number(item.toUInt32());
                }
                else if (item.gdd.dataType == 5 && item.gid.size() >= 4) {  // 有符号整数
                    valStr = QString::number(item.toInt32());
                }
                else if (item.gdd.dataType == 1) {  // ASCII字符串
                    QString s = item.toAsciiString();
                    QString clean;
                    for (QChar c : s) {
                        if (c.isPrint() || c == '\t' || c == '\n' || c == '\r') {
                            clean += c;
                        } else if (c != '\0') {
                            clean += '.';
                        }
                    }
                    valStr = QString("\"%1\"").arg(clean.trimmed());
                }
                else if (item.gdd.dataType == 8 && item.gid.size() >= 7) {  // CP56Time2a
                    CP56Time2a ts = item.toCP56Time2a();
                    valStr = formatCP56Time2a(reinterpret_cast<const uint8_t*>(&ts));
                }
                else if (item.gdd.dataType == 215 && item.gid.size() >= 4) {  // 带相对时标浮点
                    float val = item.floatWithTime();
                    QString ts = formatCP56Time2a(reinterpret_cast<const uint8_t*>(&item.eventTimeTag()));
                    valStr = QString("%1 @ %2").arg(val, 0, 'f', 4).arg(ts);
                }
                else if (item.gdd.dataType == 216 && item.gid.size() >= 4) {  // 带相对时标整数
                    int32_t val = item.intWithTime();
                    QString ts = formatCP56Time2a(reinterpret_cast<const uint8_t*>(&item.eventTimeTag()));
                    valStr = QString("%1 @ %2").arg(val).arg(ts);
                }
                else {
                    QString hex = QByteArray(reinterpret_cast<const char*>(item.gid.data()), item.gid.size()).toHex();
                    if (hex.length() > 32) hex = hex.left(32) + "...";
                    valStr = QString("0x%1").arg(hex);
                }

                itemField.children.append(FieldInfo("GID值", valStr, itemStart + 6, itemStart + 6 + gidLen));
                info.children.append(itemField);
                offset += 6 + gidLen;
            }
        }
    }
    else if (typeId == 0x0B) {  // ASDU11 通用分类标识
        // 使用lib中的Asdu11Parser
        Asdu11Parser parser;
        if (parser.parse(asdu)) {
            const auto& cdata = parser.data();
            
            info.children.append(FieldInfo("FUN", 
                QString("%1 (%2)").arg(static_cast<uint8_t>(FUN::Generic), 2, 16, QChar('0')).arg(funName(static_cast<uint8_t>(FUN::Generic))), 
                infoStart, infoStart + 1));
            info.children.append(FieldInfo("INF", 
                QString("%1 (%2)").arg(parser.inf(), 2, 16, QChar('0')).arg(infGenericName(parser.inf())), 
                infoStart + 1, infoStart + 2));
            info.children.append(FieldInfo("RII", QString("%1").arg(cdata.rii, 2, 16, QChar('0')), 
                infoStart + 2, infoStart + 3));
            info.children.append(FieldInfo("GIN组号", QString::number(cdata.gin.group), 
                infoStart + 3, infoStart + 4));
            info.children.append(FieldInfo("GIN条目", QString::number(cdata.gin.entry), 
                infoStart + 4, infoStart + 5));
            info.children.append(FieldInfo("NGD", QString("%1 个目录条目").arg(cdata.ngd), 
                infoStart + 5, infoStart + 6));

            // 解析目录条目
            int offset = 6;
            for (int i = 0; i < cdata.entries.size(); ++i) {
                const auto& entry = cdata.entries[i];
                int entryStart = infoStart + offset;
                int gidLen = entry.gdd.dataSize * entry.gdd.number;

                FieldInfo entryField(QString("目录[%1]").arg(i + 1), "", entryStart, entryStart + 4 + gidLen);
                entryField.children.append(FieldInfo("KOD", 
                    QString("%1 (%2)").arg(entry.kod, 2, 16, QChar('0')).arg(entry.kodDescription()), 
                    entryStart, entryStart + 1));
                entryField.children.append(FieldInfo("GDD类型", 
                    QString("%1 (%2)").arg(entry.gdd.dataType).arg(dataTypeName(entry.gdd.dataType)), 
                    entryStart + 1, entryStart + 2));
                entryField.children.append(FieldInfo("GDD长度", QString::number(entry.gdd.dataSize), 
                    entryStart + 2, entryStart + 3));
                entryField.children.append(FieldInfo("GDD数目", QString::number(entry.gdd.number), 
                    entryStart + 3, entryStart + 4));

                // GID
                QString hex = entry.gid.toHex();
                if (hex.length() > 32) hex = hex.left(32) + "...";
                entryField.children.append(FieldInfo("GID", QString("0x%1").arg(hex), 
                    entryStart + 4, entryStart + 4 + gidLen));

                info.children.append(entryField);
                offset += 4 + gidLen;
            }
        }
    }
    else if (typeId == 0x15) {  // ASDU21 通用分类读命令
        // 使用lib中的Asdu21Parser
        Asdu21Parser parser;
        if (parser.parse(asdu)) {
            const auto& gdata = parser.data();
            
            info.children.append(FieldInfo("FUN", 
                QString("%1 (%2)").arg(static_cast<uint8_t>(FUN::Generic), 2, 16, QChar('0')).arg(funName(static_cast<uint8_t>(FUN::Generic))), 
                infoStart, infoStart + 1));
            info.children.append(FieldInfo("INF", 
                QString("%1 (%2)").arg(parser.inf(), 2, 16, QChar('0')).arg(infGenericName(parser.inf())), 
                infoStart + 1, infoStart + 2));
            info.children.append(FieldInfo("RII", QString("%1").arg(gdata.rii, 2, 16, QChar('0')), 
                infoStart + 2, infoStart + 3));
            info.children.append(FieldInfo("NGD", QString("数目=%1 CONT=%2").arg(gdata.ngd).arg(gdata.cont ? 1 : 0), 
                infoStart + 3, infoStart + 4));

            // 解析数据项 (ASDU21只包含GIN + KOD，不包含GDD + GID)
            int offset = 4;
            for (size_t i = 0; i < gdata.items.size(); ++i) {
                const auto& item = gdata.items[i];
                int itemStart = infoStart + offset;

                FieldInfo itemField(QString("请求项[%1]").arg(i + 1), "", itemStart, itemStart + 3);
                itemField.children.append(FieldInfo("GIN组号", QString::number(item.gin.group), itemStart, itemStart + 1));
                itemField.children.append(FieldInfo("GIN条目", QString::number(item.gin.entry), itemStart + 1, itemStart + 2));
                itemField.children.append(FieldInfo("KOD", 
                    QString("%1 (%2)").arg(item.kod, 2, 16, QChar('0')).arg(kodName(item.kod)), 
                    itemStart + 2, itemStart + 3));

                info.children.append(itemField);
                offset += 3;  // ASDU21每项只有3字节: GIN(2) + KOD(1)
            }
        }
    }
    else {
        // 其他类型显示原始数据
        info.children.append(FieldInfo("原始数据", infoData.toHex(), infoStart, infoStart + len));
    }

    fields.append(info);
}