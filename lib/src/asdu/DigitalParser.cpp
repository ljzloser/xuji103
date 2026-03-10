#include "iec103/asdu/ASDU1.h"
#include <QDebug>

namespace IEC103 {

// ========== Asdu1Parser 实现 ==========

bool Asdu1Parser::parse(const Asdu& asdu) {
    m_results.clear();

    if (asdu.ti() != 1) {
        m_lastError = "Invalid TI for ASDU1";
        return false;
    }

    const QByteArray& info = asdu.infoObjects();
    const uint8_t* data = reinterpret_cast<const uint8_t*>(info.constData());
    size_t len = info.size();

    // 南网规范ASDU1信息体结构:
    // FUN(1) + INF(1) + DPI(1) + CP56Time2a(7) + CP56Time2a(7) + SIN(1) = 18字节
    constexpr size_t ITEM_SIZE = 18;

    if (len < ITEM_SIZE) {
        m_lastError = "ASDU1 info object too short";
        return false;
    }

    size_t offset = 0;
    while (offset + ITEM_SIZE <= len) {
        Result result;
        result.fun = data[offset];
        result.inf = data[offset + 1];
        result.dpi = static_cast<DoublePointValue>(data[offset + 2] & 0x03);

        // 拷贝时标
        memcpy(&result.eventTime, data + offset + 3, 7);
        memcpy(&result.recvTime, data + offset + 10, 7);
        result.sin = data[offset + 17];

        m_results.push_back(result);
        offset += ITEM_SIZE;
    }

    return !m_results.empty();
}

DigitalPoint Asdu1Parser::toDigitalPoint(const Result& result, uint16_t asduAddr) const {
    DigitalPoint point;
    point.asduAddr = asduAddr;
    point.fun = result.fun;
    point.inf = result.inf;
    point.infoAddr = (result.fun << 8) | result.inf;
    point.value = result.dpi;
    point.eventTime = result.eventTime.toDateTime();
    point.recvTime = result.recvTime.toDateTime();
    point.sin = result.sin;
    return point;
}

// ========== Asdu2Parser 实现 ==========

bool Asdu2Parser::parse(const Asdu& asdu) {
    m_results.clear();

    if (asdu.ti() != 2) {
        m_lastError = "Invalid TI for ASDU2";
        return false;
    }

    const QByteArray& info = asdu.infoObjects();
    const uint8_t* data = reinterpret_cast<const uint8_t*>(info.constData());
    size_t len = info.size();

    // 南网规范ASDU2信息体结构:
    // FUN(1) + INF(1) + DPI(1) + RET(2) + FAN(2) + CP56Time2a(7) + CP56Time2a(7) + SIN(1) = 22字节
    constexpr size_t ITEM_SIZE = 22;

    if (len < ITEM_SIZE) {
        m_lastError = "ASDU2 info object too short";
        return false;
    }

    size_t offset = 0;
    while (offset + ITEM_SIZE <= len) {
        Result result;
        result.fun = data[offset];
        result.inf = data[offset + 1];
        result.dpi = static_cast<DoublePointValue>(data[offset + 2] & 0x03);
        result.relativeTime = data[offset + 3] | (data[offset + 4] << 8);
        result.faultNo = data[offset + 5] | (data[offset + 6] << 8);

        memcpy(&result.eventTime, data + offset + 7, 7);
        memcpy(&result.recvTime, data + offset + 14, 7);
        result.fpt = data[offset + 21];

        m_results.push_back(result);
        offset += ITEM_SIZE;
    }

    return !m_results.empty();
}

} // namespace IEC103