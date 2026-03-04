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