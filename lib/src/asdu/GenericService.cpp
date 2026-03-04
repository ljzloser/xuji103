#include "iec103/asdu/ASDU10_21.h"
#include "iec103/types/Constants.h"
#include <QDebug>

namespace IEC103 {

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

    if (len < 2) {
        m_lastError = "ASDU10 info object too short";
        return false;
    }

    // 解析信息体标识符
    // FUN + INF + RII + NGD
    size_t offset = 0;
    // FUN = data[0], INF = data[1]
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
        if (!parseItem(data, offset, len, item)) {
            qWarning() << "Failed to parse generic data item" << i;
            break;
        }
        m_data.items.push_back(item);
    }

    return !m_data.items.empty() || m_data.ngd == 0;
}

bool Asdu10Parser::parseItem(const uint8_t* data, size_t& offset, size_t maxLen, GenericDataItem& item) {
    if (offset + 7 > maxLen) {
        return false;
    }

    // GIN (2字节)
    item.gin.group = data[offset++];
    item.gin.entry = data[offset++];

    // KOD (1字节)
    item.kod = data[offset++];

    // GDD (3字节)
    item.gdd.dataType = data[offset++];
    item.gdd.dataSize = data[offset++];
    item.gdd.number = data[offset] & 0x7F;
    item.gdd.cont = (data[offset++] & 0x80) != 0;

    // GID (可变长度)
    uint16_t gidLen = item.gdd.totalSize();
    if (offset + gidLen > maxLen) {
        return false;
    }

    item.gid.resize(gidLen);
    memcpy(item.gid.data(), data + offset, gidLen);
    offset += gidLen;

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
    info.append(static_cast<char>(FUN::Generic));  // FUN
    info.append(static_cast<char>(inf));           // INF

    // RII
    info.append(static_cast<char>(data.rii));

    // NGD
    uint8_t ngd = (data.cont ? 0x80 : 0x00) | (data.ngd & 0x3F);
    info.append(static_cast<char>(ngd));

    // 数据项
    for (const auto& item : data.items) {
        // GIN
        info.append(static_cast<char>(item.gin.group));
        info.append(static_cast<char>(item.gin.entry));

        // KOD
        info.append(static_cast<char>(item.kod));

        // GDD
        info.append(static_cast<char>(item.gdd.dataType));
        info.append(static_cast<char>(item.gdd.dataSize));
        uint8_t gdd3 = (item.gdd.cont ? 0x80 : 0x00) | (item.gdd.number & 0x7F);
        info.append(static_cast<char>(gdd3));

        // GID
        info.append(reinterpret_cast<const char*>(item.gid.data()), item.gid.size());
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
    // FUN + INF
    m_inf = data[1];
    offset = 2;

    // RII
    m_data.rii = data[offset++];

    // NGD
    m_data.ngd = data[offset] & 0x3F;
    m_data.cont = (data[offset++] & 0x80) != 0;

    // 解析数据项
    for (uint8_t i = 0; i < m_data.ngd && offset < len; ++i) {
        GenericDataItem item;
        if (offset + 7 > len) break;

        item.gin.group = data[offset++];
        item.gin.entry = data[offset++];
        item.kod = data[offset++];
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
        info.append(reinterpret_cast<const char*>(item.gid.data()), item.gid.size());
    }

    asdu.infoObjects() = info;
    return asdu;
}

Asdu Asdu21Builder::buildReadGroup(uint16_t deviceAddr, uint8_t group, KOD kod) {
    GenericDataStruct data;
    data.rii = 1;
    data.ngd = 1;

    GenericDataItem item;
    item.gin = GIN(group, 0xFF);  // 组标题
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
    data.ngd = 0;  // 无数据项
    return build(deviceAddr, INF_READ_ALL_GROUPS, data);
}

Asdu Asdu21Builder::buildGenericGI(uint16_t deviceAddr) {
    GenericDataStruct data;
    data.rii = 1;
    data.ngd = 0;
    return build(deviceAddr, INF_GI_GENERIC, data);
}

} // namespace IEC103
