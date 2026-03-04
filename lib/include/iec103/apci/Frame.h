#ifndef IEC103_FRAME_H
#define IEC103_FRAME_H

#include "APCI.h"
#include "../types/Constants.h"
#include <QtGlobal>
#include <vector>
#include <QByteArray>
#include <QString>

namespace IEC103 {

// APDU帧结构
class Frame {
public:
    Frame() = default;
    explicit Frame(const QByteArray& data);
    Frame(const APCI& apci, const QByteArray& asdu = {});

    // 帧类型
    ApduType type() const;

    // 是否有效
    bool isValid() const;

    // 获取APCI
    const APCI& apci() const { return m_apci; }
    APCI& apci() { return m_apci; }

    // 获取ASDU数据
    const QByteArray& asdu() const { return m_asdu; }
    QByteArray& asdu() { return m_asdu; }

    // I格式方法
    uint16_t sendSeq() const;
    uint16_t recvSeq() const;
    void setSendSeq(uint16_t seq);
    void setRecvSeq(uint16_t seq);

    // S格式方法
    void setSFormat(uint16_t recvSeq);

    // U格式方法
    UControl uControl() const;
    void setUFormat(UControl ctrl);
    bool isStartDtAct() const;
    bool isStartDtCon() const;
    bool isStopDtAct() const;
    bool isStopDtCon() const;
    bool isTestFrAct() const;
    bool isTestFrCon() const;

    // 编码为字节数组
    QByteArray encode() const;

    // 获取总长度
    uint16_t length() const;

    // 调试输出
    QString toString() const;

private:
    APCI m_apci;
    QByteArray m_asdu;
};

// 帧编解码器
class FrameCodec {
public:
    // 解码帧
    // 返回: 解码成功返回Frame, 失败返回无效Frame
    static Frame decode(const uint8_t* data, size_t len);
    static Frame decode(const QByteArray& data);

    // 编码帧
    static QByteArray encode(const Frame& frame);

    // 计算校验和
    static uint8_t calculateChecksum(const uint8_t* data, size_t len);
    static uint8_t calculateChecksum(const QByteArray& data);

    // 验证校验和
    static bool verifyChecksum(const uint8_t* data, size_t len);
    static bool verifyChecksum(const QByteArray& data);

    // 获取帧长度 (从长度域)
    static uint16_t getFrameLength(const uint8_t* data);
    static uint16_t getFrameLength(const QByteArray& data);

    // 检查是否是完整帧
    static bool isCompleteFrame(const uint8_t* data, size_t len);
    static bool isCompleteFrame(const QByteArray& data);

    // 创建I格式帧
    static Frame createIFrame(uint16_t sendSeq, uint16_t recvSeq, const QByteArray& asduData);

    // 创建S格式帧
    static Frame createSFrame(uint16_t recvSeq);

    // 创建U格式帧
    static Frame createUFrame(UControl ctrl);
};

} // namespace IEC103

#endif // IEC103_FRAME_H
