#include "iec103/asdu/ASDU7_8.h"
#include "iec103/asdu/ASDU42.h"
#include <QDebug>

namespace IEC103 {

// ========== Asdu7Builder 实现 ==========

Asdu Asdu7Builder::build(uint16_t deviceAddr, uint8_t scn) {
    Asdu asdu;
    asdu.setTi(7);
    asdu.setVsq(false, 1);
    asdu.setCot(static_cast<uint8_t>(COT::GeneralInterrogation));
    asdu.setAddr(deviceAddr);

    QByteArray info;
    info.append(static_cast<char>(FUN::Global));  // FUN
    info.append(static_cast<char>(0));            // INF
    info.append(static_cast<char>(scn));          // SCN
    asdu.infoObjects() = info;

    return asdu;
}

Asdu Asdu7Builder::buildBroadcast(uint8_t scn) {
    return build(ASDU_ADDR_BROADCAST, scn);
}

// ========== Asdu8Parser 实现 ==========

bool Asdu8Parser::parse(const Asdu& asdu) {
    m_result = Result();

    if (asdu.ti() != 8) {
        m_lastError = "Invalid TI for ASDU8";
        return false;
    }

    const QByteArray& info = asdu.infoObjects();
    if (info.size() < 3) {
        m_lastError = "ASDU8 info object too short";
        return false;
    }

    m_result.asduAddr = asdu.addr();
    m_result.scn = static_cast<uint8_t>(info[2]);

    return true;
}

// ========== Asdu8Builder 实现 ==========

Asdu Asdu8Builder::build(uint16_t deviceAddr, uint8_t scn) {
    Asdu asdu;
    asdu.setTi(8);
    asdu.setVsq(false, 1);
    asdu.setCot(static_cast<uint8_t>(COT::GITermination));
    asdu.setAddr(deviceAddr);

    QByteArray info;
    info.append(static_cast<char>(FUN::Global));
    info.append(static_cast<char>(0));
    info.append(static_cast<char>(scn));
    asdu.infoObjects() = info;

    return asdu;
}

// ========== Asdu42Parser 实现 ==========

bool Asdu42Parser::parse(const Asdu& asdu) {
    m_infoObjects.clear();

    if (asdu.ti() != 42) {
        m_lastError = "Invalid TI for ASDU42";
        return false;
    }

    const QByteArray& info = asdu.infoObjects();
    const uint8_t* data = reinterpret_cast<const uint8_t*>(info.constData());
    size_t len = info.size();

    if (len < 4) {  // 至少一个信息对象(3字节) + SIN(1字节)
        m_lastError = "ASDU42 info object too short";
        return false;
    }

    // 最后一个字节是SIN
    m_scn = data[len - 1];

    // 解析信息对象 (FUN + INF + DPI = 3字节)
    size_t offset = 0;
    while (offset + 3 <= len - 1) {
        InfoObject obj;
        obj.fun = data[offset];
        obj.inf = data[offset + 1];
        obj.dpi = static_cast<DoublePointValue>(data[offset + 2] & 0x03);
        m_infoObjects.push_back(obj);
        offset += 3;
    }

    return !m_infoObjects.empty();
}

std::vector<DigitalPoint> Asdu42Parser::toDigitalPoints(uint16_t asduAddr) const {
    std::vector<DigitalPoint> points;
    for (const auto& obj : m_infoObjects) {
        DigitalPoint point;
        point.asduAddr = asduAddr;
        point.fun = obj.fun;
        point.inf = obj.inf;
        point.infoAddr = (obj.fun << 8) | obj.inf;
        point.value = obj.dpi;
        point.sin = m_scn;
        points.push_back(point);
    }
    return points;
}

// ========== Asdu42Builder 实现 ==========

Asdu Asdu42Builder::build(uint16_t asduAddr, uint8_t scn,
                          const std::vector<Asdu42Parser::InfoObject>& objects) {
    Asdu asdu;
    asdu.setTi(42);
    asdu.setVsq(false, static_cast<uint8_t>(objects.size()));
    asdu.setCot(static_cast<uint8_t>(COT::GeneralInterrogation));
    asdu.setAddr(asduAddr);

    QByteArray info;
    for (const auto& obj : objects) {
        info.append(static_cast<char>(obj.fun));
        info.append(static_cast<char>(obj.inf));
        info.append(static_cast<char>(static_cast<uint8_t>(obj.dpi)));
    }
    info.append(static_cast<char>(scn));  // SIN
    asdu.infoObjects() = info;

    return asdu;
}

} // namespace IEC103