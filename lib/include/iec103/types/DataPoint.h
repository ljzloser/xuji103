#ifndef IEC103_DATAPOINT_H
#define IEC103_DATAPOINT_H

#include "Types.h"
#include "TimeStamp.h"
#include <QString>
#include <QDateTime>
#include <vector>

namespace IEC103 {

// 遥信数据点 (双点信息)
struct DigitalPoint {
    uint16_t deviceAddr = 0;      // 设备地址
    uint8_t fun = 0;              // 功能类型
    uint8_t inf = 0;              // 信息序号
    uint16_t infoAddr = 0;        // 信息体地址 (FUN<<8 | INF)
    DoublePointValue value = DoublePointValue::Indeterminate0;
    Quality quality;
    QDateTime eventTime;          // 事件发生时间
    QDateTime recvTime;           // 子站接收时间
    uint8_t sin = 0;              // 附加信息

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

// 遥测数据点 (模拟量)
struct AnalogPoint {
    uint16_t deviceAddr = 0;      // 设备地址
    uint8_t group = 0;            // 组号
    uint8_t entry = 0;            // 条目号
    float value = 0.0f;           // 工程值
    Quality quality;
    QString unit;                 // 单位
    QString description;          // 描述
    QDateTime timestamp;          // 时间戳 (如果有)

    bool isValid() const { return quality.isGood(); }
};

// 遥脉数据点 (积分总量)
struct CounterPoint {
    uint16_t deviceAddr = 0;      // 设备地址
    uint8_t group = 0;            // 组号
    uint8_t entry = 0;            // 条目号
    uint32_t value = 0;           // 积分值
    Quality quality;
    QString unit;                 // 单位
    QString description;          // 描述
    QDateTime timestamp;          // 时间戳

    bool isValid() const { return quality.isGood(); }
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

    // 便捷解析方法
    float toFloat() const;
    int32_t toInt32() const;
    uint32_t toUInt32() const;
    QString toAsciiString() const;
    DoublePointValue toDPI() const;
    CP56Time2a toCP56Time2a() const;
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
    uint16_t address = 0;         // 设备地址
    QString name;                 // 设备名称
    QString manufacturer;         // 制造商
    QString model;                // 型号
    QString firmware;             // 固件版本
    uint8_t compatibility = 0;    // 兼容级别
};

}

#endif // IEC103_DATAPOINT_H
