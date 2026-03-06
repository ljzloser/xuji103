#include "iec103/apci/Frame.h"
#include "iec103/types/Constants.h"
#include <QDebug>

namespace IEC103 {

// ========== Frame 实现 ==========

Frame::Frame(const QByteArray& data) : m_apci{}, m_asdu{} {
    // 南网规范帧格式 (参照104标准):
    // 68H | 长度L(2字节) | APCI(4字节) | ASDU
    // 无校验和，无结束字节
    if (data.size() >= 7 && static_cast<uint8_t>(data[0]) == FRAME_START_BYTE) {
        uint16_t len = static_cast<uint8_t>(data[1]) | (static_cast<uint8_t>(data[2]) << 8);
        // 总长度 = 3 + len (启动1 + 长度2 + APCI+ASDU)
        if (data.size() >= static_cast<int>(3 + len)) {
            // 解析APCI (索引3-6)
            m_apci.byte1 = static_cast<uint8_t>(data[3]);
            m_apci.byte2 = static_cast<uint8_t>(data[4]);
            m_apci.byte3 = static_cast<uint8_t>(data[5]);
            m_apci.byte4 = static_cast<uint8_t>(data[6]);

            // 解析ASDU (索引7开始，长度 = len - 4)
            if (len > APCI_LENGTH) {
                m_asdu = data.mid(7, len - APCI_LENGTH);
            }
        }
    }
}

Frame::Frame(const APCI& apci, const QByteArray& asdu)
    : m_apci(apci), m_asdu(asdu) {}

ApduType Frame::type() const {
    if ((m_apci.byte1 & 0x01) == 0) {
        return ApduType::I_FORMAT;
    } else if (m_apci.byte1 == 0x01) {
        return ApduType::S_FORMAT;
    } else {
        return ApduType::U_FORMAT;
    }
}

bool Frame::isValid() const {
    return m_apci.byte1 != 0 || m_apci.byte2 != 0 ||
           m_apci.byte3 != 0 || m_apci.byte4 != 0;
}

uint16_t Frame::sendSeq() const {
    if (type() == ApduType::I_FORMAT) {
        return m_apci.iFormat.sendSeq >> 1;
    }
    return 0;
}

uint16_t Frame::recvSeq() const {
    if (type() == ApduType::I_FORMAT) {
        return m_apci.iFormat.recvSeq >> 1;
    }
    if (type() == ApduType::S_FORMAT) {
        return m_apci.sFormat.recvSeq >> 1;
    }
    return 0;
}

void Frame::setSendSeq(uint16_t seq) {
    m_apci.iFormat.sendSeq = static_cast<uint16_t>((seq << 1) & 0xFFFE);
}

void Frame::setRecvSeq(uint16_t seq) {
    m_apci.iFormat.recvSeq = static_cast<uint16_t>((seq << 1) & 0xFFFE);
}

void Frame::setSFormat(uint16_t recvSeqVal) {
    m_apci.sFormat.reserved1 = 0x01;
    m_apci.sFormat.reserved2 = 0x00;
    m_apci.sFormat.recvSeq = static_cast<uint16_t>((recvSeqVal << 1) & 0xFFFE);
}

UControl Frame::uControl() const {
    return static_cast<UControl>(m_apci.uFormat.control);
}

void Frame::setUFormat(UControl ctrl) {
    m_apci.uFormat.control = static_cast<uint8_t>(ctrl);
    m_apci.uFormat.reserved1 = 0x00;
    m_apci.uFormat.reserved2 = 0x00;
    m_apci.uFormat.reserved3 = 0x00;
}

bool Frame::isStartDtAct() const {
    return uControl() == UControl::StartDtAct;
}

bool Frame::isStartDtCon() const {
    return uControl() == UControl::StartDtCon;
}

bool Frame::isStopDtAct() const {
    return uControl() == UControl::StopDtAct;
}

bool Frame::isStopDtCon() const {
    return uControl() == UControl::StopDtCon;
}

bool Frame::isTestFrAct() const {
    return uControl() == UControl::TestFrAct;
}

bool Frame::isTestFrCon() const {
    return uControl() == UControl::TestFrCon;
}

QByteArray Frame::encode() const {
    QByteArray result;
    uint16_t len = APCI_LENGTH + static_cast<uint16_t>(m_asdu.size());

    // 南网规范帧格式 (参照104标准):
    // 68H | 长度L(2字节) | APCI(4字节) | ASDU
    // 无校验和，无结束字节
    result.append(static_cast<char>(FRAME_START_BYTE));
    result.append(static_cast<char>(len & 0xFF));
    result.append(static_cast<char>((len >> 8) & 0xFF));

    result.append(static_cast<char>(m_apci.byte1));
    result.append(static_cast<char>(m_apci.byte2));
    result.append(static_cast<char>(m_apci.byte3));
    result.append(static_cast<char>(m_apci.byte4));

    result.append(m_asdu);

    return result;
}

uint16_t Frame::length() const {
    return APCI_LENGTH + static_cast<uint16_t>(m_asdu.size());
}

QString Frame::toString() const {
    QString str;
    switch (type()) {
        case ApduType::I_FORMAT:
            str = QString("I-Frame N(S)=%1 N(R)=%2 ASDU=%3B")
                .arg(sendSeq()).arg(recvSeq()).arg(m_asdu.size());
            break;
        case ApduType::S_FORMAT:
            str = QString("S-Frame N(R)=%1").arg(recvSeq());
            break;
        case ApduType::U_FORMAT:
            str = QString("U-Frame Ctrl=0x%1").arg(static_cast<int>(uControl()), 2, 16, QChar('0'));
            break;
    }
    return str;
}

// ========== FrameCodec 实现 ==========

Frame FrameCodec::decode(const uint8_t* data, size_t len) {
    if (!isCompleteFrame(data, len)) {
        return Frame();
    }

    // 南网规范帧格式 (参照104标准):
    // 68H | 长度L(2字节) | APCI(4字节) | ASDU
    // 无校验和，无结束字节
    uint16_t frameLen = getFrameLength(data);
    size_t totalLen = 3 + frameLen;  // 启动(1) + 长度(2) + APCI+ASDU(frameLen)

    QByteArray frameData(reinterpret_cast<const char*>(data), static_cast<int>(totalLen));
    return Frame(frameData);
}

Frame FrameCodec::decode(const QByteArray& data) {
    return decode(reinterpret_cast<const uint8_t*>(data.constData()), data.size());
}

QByteArray FrameCodec::encode(const Frame& frame) {
    return frame.encode();
}

uint8_t FrameCodec::calculateChecksum(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; ++i) {
        sum += data[i];
    }
    return sum;
}

uint8_t FrameCodec::calculateChecksum(const QByteArray& data) {
    return calculateChecksum(reinterpret_cast<const uint8_t*>(data.constData()), data.size());
}

bool FrameCodec::verifyChecksum(const uint8_t* data, size_t len) {
    if (len < 7) return false;

    // 获取APCI+ASDU长度
    uint16_t frameLen = getFrameLength(data);
    
    // 校验和位置 = 3 + frameLen (启动1 + 长度2 + APCI+ASDU)
    // 结束字节位置 = 4 + frameLen
    if (len < static_cast<size_t>(5 + frameLen)) return false;
    
    // 计算从APCI开始到ASDU结束的校验和 (索引3开始)
    uint8_t calculated = calculateChecksum(data + 3, frameLen);
    uint8_t received = data[3 + frameLen];

    return calculated == received;
}

bool FrameCodec::verifyChecksum(const QByteArray& data) {
    return verifyChecksum(reinterpret_cast<const uint8_t*>(data.constData()), data.size());
}

uint16_t FrameCodec::getFrameLength(const uint8_t* data) {
    if (!data) return 0;
    return data[1] | (data[2] << 8);
}

uint16_t FrameCodec::getFrameLength(const QByteArray& data) {
    if (data.size() < 3) return 0;
    return getFrameLength(reinterpret_cast<const uint8_t*>(data.constData()));
}

bool FrameCodec::isCompleteFrame(const uint8_t* data, size_t len) {
    if (!data || len < 7) return false;  // 最小帧: 68H + L(2) + APCI(4) = 7字节
    if (data[0] != FRAME_START_BYTE) return false;

    uint16_t frameLen = getFrameLength(data);
    // 南网规范帧格式 (参照104标准):
    // 总长度 = 启动(1) + 长度(2) + APCI+ASDU(frameLen) = 3 + frameLen
    size_t totalLen = 3 + frameLen;

    return len >= totalLen;
}

bool FrameCodec::isCompleteFrame(const QByteArray& data) {
    return isCompleteFrame(reinterpret_cast<const uint8_t*>(data.constData()), data.size());
}

Frame FrameCodec::createIFrame(uint16_t sendSeqNo, uint16_t recvSeqNo, const QByteArray& asduData) {
    Frame frame;
    frame.setSendSeq(sendSeqNo);
    frame.setRecvSeq(recvSeqNo);
    frame.asdu() = asduData;
    return frame;
}

Frame FrameCodec::createSFrame(uint16_t recvSeqNo) {
    Frame frame;
    frame.setSFormat(recvSeqNo);
    frame.asdu() = QByteArray();  // S格式帧没有ASDU
    return frame;
}

Frame FrameCodec::createUFrame(UControl ctrl) {
    Frame frame;
    frame.setUFormat(ctrl);
    return frame;
}

} // namespace IEC103