#include "iec103/asdu/ASDU10_21.h"
#include "iec103/types/Constants.h"
#include <QDebug>
#include <cstring>

namespace IEC103 {

// ========== GenericDataItem 解析方法实现 ==========

float GenericDataItem::toFloat() const {
    if (gdd.dataType == static_cast<uint8_t>(DataType::R32_23) && gid.size() >= 4) {
        uint32_t val = 0;
        memcpy(&val, gid.data(), 4);
        float result;
        memcpy(&result, &val, 4);
        return result;
    }
    return 0.0f;
}

int32_t GenericDataItem::toInt32() const {
    if (gdd.dataType == static_cast<uint8_t>(DataType::I) && gid.size() >= 4) {
        int32_t val = 0;
        memcpy(&val, gid.data(), 4);
        return val;
    }
    return 0;
}

uint32_t GenericDataItem::toUInt32() const {
    if (gdd.dataType == static_cast<uint8_t>(DataType::UI) && gid.size() >= 4) {
        uint32_t val = 0;
        memcpy(&val, gid.data(), 4);
        return val;
    }
    return 0;
}

QString GenericDataItem::toAsciiString() const {
    if (gdd.dataType == static_cast<uint8_t>(DataType::OS8ASCII)) {
        return QString::fromLatin1(reinterpret_cast<const char*>(gid.data()), static_cast<int>(gid.size()));
    }
    return QString();
}

DoublePointValue GenericDataItem::toDPI() const {
    if (gdd.dataType == static_cast<uint8_t>(DataType::DPI) && gid.size() >= 1) {
        return static_cast<DoublePointValue>(gid[0] & 0x03);
    }
    return DoublePointValue::Indeterminate0;
}

CP56Time2a GenericDataItem::toCP56Time2a() const {
    CP56Time2a ts;
    if (gid.size() >= 7) {
        memcpy(&ts, gid.data(), 7);
    }
    return ts;
}

// ========== 南网扩展数据类型解析方法 (DataType 213-217) ==========
// 参考：南网规范 图表29 故障量数据上传定义

QDateTime GenericDataItem::absoluteTime() const {
    // DataType 213 (0xD5): 带绝对时间七字节时标报文
    // 格式: DPI(1字节,低2位) + RES(6位) + CP56Time2a(7字节) + CP56Time2a(7字节) + SIN(1字节) = 17字节
    // 实际时标从偏移1开始
    if (gdd.dataType == static_cast<uint8_t>(DataType::TimeTagMsg7) && gid.size() >= 15) {
        CP56Time2a ts;
        memcpy(&ts, gid.data() + 1, 7);  // 实际发生时间
        return ts.toDateTime();
    }
    return QDateTime();
}

QDateTime GenericDataItem::relativeTimeTag() const {
    // DataType 214 (0xD6): 带相对时间七字节时标报文
    // 格式: DPI(1) + RES(6位) + RET(2) + NOF(2) + CP56Time2a(7) + CP56Time2a(7) + SIN(1) = 21字节
    if (gdd.dataType == static_cast<uint8_t>(DataType::TimeTagMsgRel7) && gid.size() >= 14) {
        CP56Time2a ts;
        memcpy(&ts, gid.data() + 7, 7);  // 实际发生时间，偏移7
        return ts.toDateTime();
    }
    return QDateTime();
}

// ========== DataType 215: 带相对时间七字节时标的浮点值 ==========
// 格式: VAL(4) + RET(2) + NOF(2) + TIME(7) + RECV_TIME(7) = 22字节
// 参考：南网规范 图表29

float GenericDataItem::floatWithTime() const {
    if (gdd.dataType == static_cast<uint8_t>(DataType::FloatTimeTag7) && gid.size() >= 4) {
        uint32_t val = 0;
        memcpy(&val, gid.data(), 4);  // 浮点值在偏移0-3
        float result;
        memcpy(&result, &val, 4);
        return result;
    }
    return 0.0f;
}

uint16_t GenericDataItem::relativeTimeMs() const {
    // 相对时间RET在偏移4-5 (仅对DataType 215/216/217有效)
    if ((gdd.dataType == static_cast<uint8_t>(DataType::FloatTimeTag7) ||
         gdd.dataType == static_cast<uint8_t>(DataType::IntTimeTag7) ||
         gdd.dataType == static_cast<uint8_t>(DataType::CharTimeTag7)) && gid.size() >= 6) {
        return gid[4] | (gid[5] << 8);
    }
    return 0;
}

uint16_t GenericDataItem::faultSequenceNo() const {
    // 电网故障序号NOF在偏移6-7 (仅对DataType 215/216/217有效)
    if ((gdd.dataType == static_cast<uint8_t>(DataType::FloatTimeTag7) ||
         gdd.dataType == static_cast<uint8_t>(DataType::IntTimeTag7) ||
         gdd.dataType == static_cast<uint8_t>(DataType::CharTimeTag7)) && gid.size() >= 8) {
        return gid[6] | (gid[7] << 8);
    }
    return 0;
}

CP56Time2a GenericDataItem::eventTimeTag() const {
    // 故障时间CP56Time2a在偏移8-14 (仅对DataType 215/216/217有效)
    CP56Time2a ts;
    if ((gdd.dataType == static_cast<uint8_t>(DataType::FloatTimeTag7) ||
         gdd.dataType == static_cast<uint8_t>(DataType::IntTimeTag7) ||
         gdd.dataType == static_cast<uint8_t>(DataType::CharTimeTag7)) && gid.size() >= 15) {
        memcpy(&ts, gid.data() + 8, 7);
    }
    return ts;
}

CP56Time2a GenericDataItem::recvTimeTag() const {
    // 子站接收时间CP56Time2a在偏移15-21 (仅对DataType 215/216有效)
    CP56Time2a ts;
    if ((gdd.dataType == static_cast<uint8_t>(DataType::FloatTimeTag7) ||
         gdd.dataType == static_cast<uint8_t>(DataType::IntTimeTag7)) && gid.size() >= 22) {
        memcpy(&ts, gid.data() + 15, 7);
    }
    return ts;
}

// ========== DataType 216: 带相对时间七字节时标的整形值 ==========
// 格式: VAL(4, 其中低字节FPT, 高字节JPT) + RET(2) + NOF(2) + TIME(7) + RECV_TIME(7) = 22字节

int32_t GenericDataItem::intWithTime() const {
    if (gdd.dataType == static_cast<uint8_t>(DataType::IntTimeTag7) && gid.size() >= 4) {
        int32_t val = 0;
        memcpy(&val, gid.data(), 4);  // 整形值在偏移0-3
        return val;
    }
    return 0;
}

uint8_t GenericDataItem::fptValue() const {
    // FPT(故障相别及类型)在VAL的低位字节(偏移0)
    if (gdd.dataType == static_cast<uint8_t>(DataType::IntTimeTag7) && gid.size() >= 1) {
        return gid[0];
    }
    return 0;
}

uint8_t GenericDataItem::jptValue() const {
    // JPT(跳闸相别)在VAL的第二字节(偏移1)
    if (gdd.dataType == static_cast<uint8_t>(DataType::IntTimeTag7) && gid.size() >= 2) {
        return gid[1];
    }
    return 0;
}

// ========== DataType 217: 带相对时间七字节时标的字符值 ==========
// 格式: VAL(40字节ASCII) + RET(2) + NOF(2) + TIME(7) + RECV_TIME(7) = 58字节

QString GenericDataItem::stringWithTime() const {
    if (gdd.dataType == static_cast<uint8_t>(DataType::CharTimeTag7) && gid.size() >= 40) {
        // 字符值在偏移0-39，共40字节
        return QString::fromLatin1(reinterpret_cast<const char*>(gid.data()), 40);
    }
    return QString();
}

// ========== 兼容旧接口(已废弃) ==========

QDateTime GenericDataItem::relativeTime() const {
    // 保持向后兼容，实际应使用 relativeTimeTag()
    return relativeTimeTag();
}

// ========== Asdu10Parser 实现 ==========

bool Asdu10Parser::parse(const Asdu& asdu) {
    m_data = GenericDataStruct{};
    m_deviceAddr = asdu.addr();

    if (asdu.ti() != 10) {
        m_lastError = "Invalid TI for ASDU10";
        return false;
    }

    const QByteArray& info = asdu.infoObjects();
    const uint8_t* data = reinterpret_cast<const uint8_t*>(info.constData());
    size_t len = info.size();

    if (len < 4) {
        m_lastError = "ASDU10 info object too short";
        return false;
    }

    size_t offset = 0;

    // FUN + INF
    m_inf = data[1];
    offset = 2;

    // RII
    m_data.rii = data[offset++];

    // NGD (包含CONT位)
    m_data.ngd = data[offset] & 0x3F;
    m_data.cont = (data[offset++] & 0x80) != 0;

    // 解析数据项
    for (uint8_t i = 0; i < m_data.ngd && offset < len; ++i) {
        GenericDataItem item;

        // GIN (2字节)
        if (offset + 2 > len) break;
        item.gin.group = data[offset++];
        item.gin.entry = data[offset++];

        // KOD (1字节)
        if (offset >= len) break;
        item.kod = data[offset++];

        // GDD (3字节)
        if (offset + 3 > len) break;
        item.gdd.dataType = data[offset++];
        item.gdd.dataSize = data[offset++];
        item.gdd.number = data[offset] & 0x7F;
        item.gdd.cont = (data[offset++] & 0x80) != 0;

        // GID
        uint16_t gidLen = item.gdd.totalSize();
        if (offset + gidLen <= len) {
            item.gid.resize(gidLen);
            memcpy(item.gid.data(), data + offset, gidLen);
            offset += gidLen;
        }

        m_data.items.push_back(item);
    }

    return true;
}

// ========== Asdu10Builder 实现 ==========

Asdu Asdu10Builder::build(uint16_t deviceAddr, uint8_t inf, const GenericDataStruct& data) {
    Asdu asdu;
    asdu.setTi(10);
    asdu.setVsq(false, 1);
    asdu.setCot(static_cast<uint16_t>(COT::GenReadValid));
    asdu.setAddr(deviceAddr);

    QByteArray info;
    info.append(static_cast<char>(FUN::Generic));
    info.append(static_cast<char>(inf));
    info.append(static_cast<char>(data.rii));

    uint8_t ngd = (data.cont ? 0x80 : 0x00) | (data.ngd & 0x3F);
    info.append(static_cast<char>(ngd));

    for (const auto& item : data.items) {
        info.append(static_cast<char>(item.gin.group));
        info.append(static_cast<char>(item.gin.entry));
        info.append(static_cast<char>(item.kod));
        info.append(static_cast<char>(item.gdd.dataType));
        info.append(static_cast<char>(item.gdd.dataSize));
        uint8_t gdd3 = (item.gdd.cont ? 0x80 : 0x00) | (item.gdd.number & 0x7F);
        info.append(static_cast<char>(gdd3));
        info.append(reinterpret_cast<const char*>(item.gid.data()), static_cast<int>(item.gid.size()));
    }

    asdu.infoObjects() = info;
    return asdu;
}

// ========== Asdu21Parser 实现 ==========

bool Asdu21Parser::parse(const Asdu& asdu) {
    m_data = GenericDataStruct{};
    m_deviceAddr = asdu.addr();

    if (asdu.ti() != 21) {
        m_lastError = "Invalid TI for ASDU21";
        return false;
    }

    const QByteArray& info = asdu.infoObjects();
    const uint8_t* data = reinterpret_cast<const uint8_t*>(info.constData());
    size_t len = info.size();

    if (len < 4) {
        m_lastError = "ASDU21 info object too short";
        return false;
    }

    size_t offset = 0;
    m_inf = data[1];
    offset = 2;

    m_data.rii = data[offset++];
    m_data.ngd = data[offset] & 0x3F;
    m_data.cont = (data[offset++] & 0x80) != 0;

    for (uint8_t i = 0; i < m_data.ngd && offset < len; ++i) {
        GenericDataItem item;

        if (offset + 2 > len) break;
        item.gin.group = data[offset++];
        item.gin.entry = data[offset++];

        if (offset >= len) break;
        item.kod = data[offset++];

        if (offset + 3 > len) break;
        item.gdd.dataType = data[offset++];
        item.gdd.dataSize = data[offset++];
        item.gdd.number = data[offset] & 0x7F;
        item.gdd.cont = (data[offset++] & 0x80) != 0;

        uint16_t gidLen = item.gdd.totalSize();
        if (offset + gidLen <= len) {
            item.gid.resize(gidLen);
            memcpy(item.gid.data(), data + offset, gidLen);
            offset += gidLen;
        }

        m_data.items.push_back(item);
    }

    return true;
}

// ========== Asdu21Builder 实现 ==========

Asdu Asdu21Builder::build(uint16_t deviceAddr, uint8_t inf, const GenericDataStruct& data) {
    Asdu asdu;
    asdu.setTi(21);
    asdu.setVsq(false, 1);
    asdu.setCot(static_cast<uint16_t>(COT::GenReadValid));
    asdu.setAddr(deviceAddr);

    QByteArray info;
    info.append(static_cast<char>(FUN::Generic));
    info.append(static_cast<char>(inf));
    info.append(static_cast<char>(data.rii));

    uint8_t ngd = (data.cont ? 0x80 : 0x00) | (data.ngd & 0x3F);
    info.append(static_cast<char>(ngd));

    for (const auto& item : data.items) {
        info.append(static_cast<char>(item.gin.group));
        info.append(static_cast<char>(item.gin.entry));
        info.append(static_cast<char>(item.kod));
        info.append(static_cast<char>(item.gdd.dataType));
        info.append(static_cast<char>(item.gdd.dataSize));
        uint8_t gdd3 = (item.gdd.cont ? 0x80 : 0x00) | (item.gdd.number & 0x7F);
        info.append(static_cast<char>(gdd3));
        info.append(reinterpret_cast<const char*>(item.gid.data()), static_cast<int>(item.gid.size()));
    }

    asdu.infoObjects() = info;
    return asdu;
}

Asdu Asdu21Builder::buildReadGroup(uint16_t deviceAddr, uint8_t group, KOD kod) {
    GenericDataStruct data;
    data.rii = 1;
    data.ngd = 1;

    GenericDataItem item;
    item.gin = GIN(group, 0xFF);
    item.kod = static_cast<uint8_t>(kod);
    item.gdd.dataType = 0;
    item.gdd.dataSize = 0;
    item.gdd.number = 0;

    data.items.push_back(item);
    return build(deviceAddr, INF_READ_GROUP_ENTRIES, data);
}

Asdu Asdu21Builder::buildReadEntry(uint16_t deviceAddr, const GIN& gin, KOD kod) {
    GenericDataStruct data;
    data.rii = 1;
    data.ngd = 1;

    GenericDataItem item;
    item.gin = gin;
    item.kod = static_cast<uint8_t>(kod);
    item.gdd.dataType = 0;
    item.gdd.dataSize = 0;
    item.gdd.number = 0;

    data.items.push_back(item);
    return build(deviceAddr, INF_READ_ENTRY_VALUE, data);
}

Asdu Asdu21Builder::buildReadAllGroups(uint16_t deviceAddr) {
    GenericDataStruct data;
    data.rii = 1;
    data.ngd = 0;
    return build(deviceAddr, INF_READ_ALL_GROUPS, data);
}

Asdu Asdu21Builder::buildGenericGI(uint16_t deviceAddr) {
    GenericDataStruct data;
    data.rii = 1;
    data.ngd = 0;
    return build(deviceAddr, INF_GI_GENERIC, data);
}

} // namespace IEC103