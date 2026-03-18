#include "iec103/asdu/ASDU.h"
#include <QDebug>

namespace IEC103 {

Asdu::Asdu(const QByteArray& data) : m_header{}, m_infoObjects{} {
    parse(data);
}

bool Asdu::parse(const QByteArray& data) {
    return parse(reinterpret_cast<const uint8_t*>(data.constData()), data.size());
}

bool Asdu::parse(const uint8_t* data, size_t len) {
    if (len < 5) {
        return false;
    }

    // 解析数据单元标识符
    m_header.ti = data[0];
    m_header.vsq = data[1];
    m_header.cot = data[2];
    m_header.addr = data[3] | (data[4] << 8);

    // 解析信息体
    if (len > 5) {
        m_infoObjects = QByteArray(reinterpret_cast<const char*>(data + 5), len - 5);
    }

    return true;
}

QByteArray Asdu::encode() const {
    QByteArray result;
    result.append(static_cast<char>(m_header.ti));
    result.append(static_cast<char>(m_header.vsq));
    result.append(static_cast<char>(m_header.cot));
    result.append(static_cast<char>(m_header.addr & 0xFF));
    result.append(static_cast<char>((m_header.addr >> 8) & 0xFF));
    result.append(m_infoObjects);
    return result;
}

bool Asdu::isValid() const {
    return m_header.ti != 0 || m_infoObjects.size() > 0;
}

QString Asdu::toString() const {
    return QString("ASDU TI=%1 VSQ=%2 COT=%3 Addr=%4 InfoLen=%5")
        .arg(m_header.ti)
        .arg(m_header.vsq, 0, 16)
        .arg(m_header.cot)
        .arg(m_header.addr)
        .arg(m_infoObjects.size());
}

} // namespace IEC103
