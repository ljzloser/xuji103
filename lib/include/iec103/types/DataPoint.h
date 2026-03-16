#ifndef IEC103_DATAPOINT_H
#define IEC103_DATAPOINT_H

#include "Types.h"
#include "TimeStamp.h"
#include <QString>
#include <QDateTime>
#include <QVariant>
#include <vector>

namespace IEC103 {

// 遥信数据点 (双点信息)
struct DigitalPoint {
    uint16_t asduAddr = 0;        // ASDU公共地址 (完整2字节)
    uint8_t fun = 0;              // 功能类型
    uint8_t inf = 0;              // 信息序号
    uint16_t infoAddr = 0;        // 信息体地址 (FUN<<8 | INF)
    DoublePointValue value = DoublePointValue::Indeterminate0;
    Quality quality;
    QDateTime eventTime;          // 事件发生时间
    QDateTime recvTime;           // 子站接收时间
    uint8_t sin = 0;              // 附加信息

    // ========== 地址解析方法 (南网规范) ==========

    // 设备地址 (高字节)
    uint8_t deviceAddr() const { return (asduAddr >> 8) & 0xFF; }

    // CPU号 (低3位)
    uint8_t cpuNo() const { return asduAddr & 0x07; }

    // 定值区号 (D7-D3)
    uint8_t settingZone() const { return (asduAddr >> 3) & 0x1F; }

    // 设置地址
    void setAddr(uint8_t device, uint8_t cpu = 0) {
        asduAddr = (static_cast<uint16_t>(device) << 8) | (cpu & 0x07);
    }

    // 便捷方法
    bool isOn() const { return value == DoublePointValue::On; }
    bool isOff() const { return value == DoublePointValue::Off; }
    bool isValid() const { return quality.isGood(); }
    QString valueString() const {
        switch (value) {
            case DoublePointValue::Off: return "分";
            case DoublePointValue::On: return "合";
            default: return "不确定";
        }
    }
};

// 保护动作事件 (ASDU2 - 带相对时间的时标报文)
// 用于保护动作事件上送，包含故障序号、故障相别等信息
struct ProtectionEvent
{
    uint16_t asduAddr = 0;                                     // ASDU公共地址 (完整2字节)
    uint8_t fun = 0;                                           // 功能类型
    uint8_t inf = 0;                                           // 信息序号
    DoublePointValue value = DoublePointValue::Indeterminate0; // 动作状态
    uint16_t relativeTime = 0;                                 // 相对时间
    uint16_t faultNo = 0;                                      // 故障序号
    uint8_t faultPhase = 0;                                    // 故障相别 (FPT)
    QDateTime eventTime;                                       // 事件发生时间
    QDateTime recvTime;                                        // 子站接收时间

    // ========== 地址解析方法 (南网规范) ==========

    uint8_t deviceAddr() const { return (asduAddr >> 8) & 0xFF; }
    uint8_t cpuNo() const { return asduAddr & 0x07; }

    // ========== 故障相别解析 (FPT) ==========
    // D7: 有效位 (1=有效, 0=无效)
    // D4: 区内(1)/区外(0)故障
    // D3: 接地(1)/非接地(0)故障
    // D2: C相故障
    // D1: B相故障
    // D0: A相故障

    bool isValid() const { return faultPhase & 0x80; }
    bool isInternalFault() const { return faultPhase & 0x10; } // 区内故障
    bool isGroundFault() const { return faultPhase & 0x08; }   // 接地故障
    bool isPhaseA() const { return faultPhase & 0x01; }
    bool isPhaseB() const { return faultPhase & 0x02; }
    bool isPhaseC() const { return faultPhase & 0x04; }

    QString faultPhaseString() const
    {
        if (!isValid())
            return "无效";
        QString phases;
        if (isPhaseA())
            phases += "A";
        if (isPhaseB())
            phases += "B";
        if (isPhaseC())
            phases += "C";
        if (phases.isEmpty())
            phases = "未知";
        if (isGroundFault())
            phases += "接地";
        if (isInternalFault())
            phases += "(区内)";
        else
            phases += "(区外)";
        return phases;
    }

    bool isOn() const { return value == DoublePointValue::On; }
    bool isOff() const { return value == DoublePointValue::Off; }
};

// 通用服务数据点 (遥测/遥脉统一)
// 数据类型由GDD.DataType决定: 7=浮点数(遥测), 3=无符号整数(遥脉)
struct GenericPoint {
    uint16_t asduAddr = 0;        // ASDU公共地址 (完整2字节)
    uint8_t group = 0;            // 组号
    uint8_t entry = 0;            // 条目号
    uint8_t dataType = 0;         // GDD.DataType: 7=浮点, 3=整数
    QVariant value;               // 根据dataType解析的值
    Quality quality;
    QString unit;                 // 单位
    QString description;          // 描述
    QDateTime timestamp;          // 时间戳

    // ========== 地址解析方法 (南网规范) ==========

    // 设备地址 (高字节)
    uint8_t deviceAddr() const { return (asduAddr >> 8) & 0xFF; }

    // CPU号 (低3位)
    uint8_t cpuNo() const { return asduAddr & 0x07; }

    // 定值区号 (D7-D3)
    uint8_t settingZone() const { return (asduAddr >> 3) & 0x1F; }

    // 设置地址
    void setAddr(uint8_t device, uint8_t cpu = 0) {
        asduAddr = (static_cast<uint16_t>(device) << 8) | (cpu & 0x07);
    }

    bool isValid() const { return quality.isGood(); }
    bool isFloat() const { return dataType == 7; }
    bool isInteger() const { return dataType == 3; }
    
    // 便捷方法
    float toFloat() const { return value.toFloat(); }
    uint32_t toUInt32() const { return value.toUInt(); }
    int32_t toInt32() const { return value.toInt(); }
    
    QString valueString() const {
        if (dataType == 7) {
            return QString::number(value.toFloat(), 'f', 2);
        } else if (dataType == 3) {
            return QString::number(value.toUInt());
        }
        return value.toString();
    }
};

// 通用分类标识序号
struct GIN {
    uint8_t group = 0;            // 组号
    uint8_t entry = 0;            // 条目号 (0xFF表示组标题)

    GIN() = default;
    GIN(uint8_t g, uint8_t e) : group(g), entry(e) {}

    bool isGroupHeader() const { return entry == 0xFF; }
    uint16_t toUint16() const { return (static_cast<uint16_t>(group) << 8) | entry; }
    static GIN fromUint16(uint16_t v) { return GIN(v >> 8, v & 0xFF); }

    bool operator==(const GIN& other) const {
        return group == other.group && entry == other.entry;
    }
};

// 通用分类数据描述
struct GDD {
    uint8_t dataType = 0;         // 数据类型
    uint8_t dataSize = 0;         // 数据宽度 (字节数)
    uint8_t number = 0;           // 数据元素数目
    bool cont = false;            // 后续状态位

    uint16_t totalSize() const { return dataSize * number; }
};

// 通用分类数据项
struct GenericDataItem {
    GIN gin;                      // 标识序号
    uint8_t kod = 0;              // 描述类别
    GDD gdd;                      // 数据描述
    std::vector<uint8_t> gid;     // 数据内容

    // ========== 基本数据类型解析方法 ==========
    float toFloat() const;              // DataType 7: R32.23浮点数
    int32_t toInt32() const;            // DataType 4: 有符号整数
    uint32_t toUInt32() const;          // DataType 3: 无符号整数
    QString toAsciiString() const;      // DataType 1: ASCII字符串
    DoublePointValue toDPI() const;     // DataType 9: 双点信息
    CP56Time2a toCP56Time2a() const;    // 7字节时标

    // ========== 南网扩展数据类型解析方法 (DataType 213-217) ==========
    // 参考：南网规范 图表29 故障量数据上传定义
    
    // DataType 213: 带绝对时间七字节时标报文
    // 格式: DPI + RES + CP56Time2a + CP56Time2a + SIN = 17字节
    QDateTime absoluteTime() const;
    
    // DataType 214: 带相对时间七字节时标报文
    // 格式: DPI + RES + RET(2) + NOF(2) + CP56Time2a + CP56Time2a + SIN = 21字节
    QDateTime relativeTimeTag() const;  // 返回实际发生时间
    QDateTime relativeTime() const;     // 兼容旧接口(已废弃)
    
    // DataType 215: 带相对时间七字节时标的浮点值
    // 格式: VAL(4) + RET(2) + NOF(2) + TIME(7) + RECV_TIME(7) = 22字节
    float floatWithTime() const;        // 浮点值(偏移0-3)
    uint16_t relativeTimeMs() const;    // 相对时间ms(偏移4-5)
    uint16_t faultSequenceNo() const;   // 故障序号(偏移6-7)
    CP56Time2a eventTimeTag() const;    // 故障时间(偏移8-14)
    CP56Time2a recvTimeTag() const;     // 子站接收时间(偏移15-21)
    
    // DataType 216: 带相对时间七字节时标的整形值
    // 格式: VAL(4,FPT+JPT) + RET(2) + NOF(2) + TIME(7) + RECV_TIME(7) = 22字节
    int32_t intWithTime() const;        // 整形值(偏移0-3)
    uint8_t fptValue() const;           // FPT故障相别及类型(偏移0)
    uint8_t jptValue() const;           // JPT跳闸相别(偏移1)
    
    // DataType 217: 带相对时间七字节时标的字符值
    // 格式: VAL(40) + RET(2) + NOF(2) + TIME(7) + RECV_TIME(7) = 58字节
    QString stringWithTime() const;     // 字符值(偏移0-39)
};

// 通用分类数据集
struct GenericDataSet {
    uint8_t rii = 0;              // 返回信息标识符
    uint8_t count = 0;            // 数据集数目
    bool cont = false;            // 后续状态位
    std::vector<GenericDataItem> items;

    void addItem(const GenericDataItem& item) {
        items.push_back(item);
        count = static_cast<uint8_t>(items.size());
    }
};

// 设备信息
struct DeviceInfo {
    uint16_t asduAddr = 0;        // ASDU公共地址 (完整2字节)
    QString name;                 // 设备名称
    QString manufacturer;         // 制造商
    QString model;                // 型号
    QString firmware;             // 固件版本
    uint8_t compatibility = 0;    // 兼容级别

    // 设备地址 (高字节)
    uint8_t deviceAddr() const { return (asduAddr >> 8) & 0xFF; }
};

}

#endif // IEC103_DATAPOINT_H
