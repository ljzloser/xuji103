#ifndef IEC103_APCI_H
#define IEC103_APCI_H

#include <QtGlobal>
#include <vector>

namespace IEC103 {

// APDU类型
enum class ApduType {
    I_FORMAT,   // I格式 - 信息传输
    S_FORMAT,   // S格式 - 监视确认
    U_FORMAT    // U格式 - 控制功能
};

// U格式控制功能
enum class UControl : uint8_t {
    None = 0,
    StartDtAct = 0x07,   // STARTDT激活
    StartDtCon = 0x0B,   // STARTDT确认
    StopDtAct = 0x13,    // STOPDT激活
    StopDtCon = 0x23,    // STOPDT确认
    TestFrAct = 0x43,    // TESTFR激活
    TestFrCon = 0x83     // TESTFR确认
};

// APCI控制域 (4字节)
#pragma pack(push, 1)
union APCI {
    struct {
        uint8_t byte1;
        uint8_t byte2;
        uint8_t byte3;
        uint8_t byte4;
    };

    // I格式: 发送序号N(S)和接收序号N(R)
    struct {
        uint16_t sendSeq;    // 发送序号 N(S)
        uint16_t recvSeq;    // 接收序号 N(R)
    } iFormat;

    // S格式: 仅接收序号N(R)
    struct {
        uint8_t reserved1;   // 0x01
        uint8_t reserved2;   // 0x00
        uint16_t recvSeq;    // 接收序号 N(R)
    } sFormat;

    // U格式: 控制功能
    struct {
        uint8_t control;     // 控制字节
        uint8_t reserved1;   // 0x00
        uint8_t reserved2;   // 0x00
        uint8_t reserved3;   // 0x00
    } uFormat;

    uint8_t bytes[4];

    // 判断APDU类型
    ApduType type() const {
        if ((byte1 & 0x01) == 0) {
            return ApduType::I_FORMAT;
        } else if (byte1 == 0x01) {
            return ApduType::S_FORMAT;
        } else {
            return ApduType::U_FORMAT;
        }
    }

    // 获取I格式发送序号
    uint16_t sendSequence() const {
        return iFormat.sendSeq >> 1;  // 低1位是0，需要右移
    }

    // 获取I格式接收序号
    uint16_t recvSequence() const {
        return iFormat.recvSeq >> 1;
    }

    // 获取U格式控制功能
    UControl uControl() const {
        return static_cast<UControl>(uFormat.control);
    }

    // 设置I格式序号
    void setIFormat(uint16_t sendSeqNo, uint16_t recvSeqNo) {
        iFormat.sendSeq = (sendSeqNo << 1);  // LSB=0
        iFormat.recvSeq = (recvSeqNo << 1);
    }

    // 设置S格式
    void setSFormat(uint16_t recvSeqNo) {
        sFormat.reserved1 = 0x01;
        sFormat.reserved2 = 0x00;
        sFormat.recvSeq = (recvSeqNo << 1);
    }

    // 设置U格式
    void setUFormat(UControl ctrl) {
        uFormat.control = static_cast<uint8_t>(ctrl);
        uFormat.reserved1 = 0x00;
        uFormat.reserved2 = 0x00;
        uFormat.reserved3 = 0x00;
    }
};
#pragma pack(pop)

static_assert(sizeof(APCI) == 4, "APCI must be 4 bytes");

} // namespace IEC103

#endif // IEC103_APCI_H
