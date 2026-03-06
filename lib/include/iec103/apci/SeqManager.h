#ifndef IEC103_SEQMANAGER_H
#define IEC103_SEQMANAGER_H

#include <QtGlobal>
#include <mutex>

namespace IEC103 {

// 序号管理器
// 管理I格式帧的发送序号N(S)和接收序号N(R)
class SeqManager {
public:
    SeqManager();

    // 重置序号
    void reset();

    // ========== 发送序号 N(S) ==========

    // 获取当前发送序号
    uint16_t sendSeq() const;

    // 递增发送序号 (发送I帧后调用)
    uint16_t nextSendSeq();

    // ========== 接收序号 N(R) ==========

    // 获取当前接收序号 (期望接收的下一个序号)
    uint16_t recvSeq() const;

    // 更新接收序号 (收到I帧后调用)
    void updateRecvSeq(uint16_t seq);

    // 递增接收序号 (收到I帧后调用)
    uint16_t nextRecvSeq();

    // ========== 确认管理 ==========

    // 获取最后确认的序号
    uint16_t lastAckSeq() const;

    // 设置最后确认的序号
    void setLastAckSeq(uint16_t seq);

    // 获取未确认的I帧数量
    uint16_t unconfirmedCount() const;

    // ========== 序号验证 ==========

    // 验证接收到的序号是否正确
    // 返回: true=序号正确, false=序号错误(可能丢帧或重复)
    bool validateRecvSeq(uint16_t seq) const;

    // 验证对方确认的序号是否在有效范围内
    bool validateAckSeq(uint16_t ackSeq) const;

    // ========== 状态查询 ==========

    // 是否需要发送S格式确认
    bool needSendAck() const;

    // 是否有未确认的I帧
    bool hasUnconfirmed() const;

    // ========== 接收计数管理（w值控制）==========

    // 获取接收未确认计数
    uint8_t recvCount() const;

    // 递增接收计数
    void incrementRecvCount();

    // 重置接收计数（发送确认后调用）
    void resetRecvCount();

private:
    mutable std::mutex m_mutex;
    uint16_t m_sendSeq;      // 发送序号 N(S)
    uint16_t m_recvSeq;      // 接收序号 N(R)
    uint16_t m_lastAckSeq;   // 最后确认的序号
    uint8_t m_recvCount = 0; // 接收未确认计数（用于w值控制）

    static constexpr uint16_t SEQ_MASK = 0x7FFF;  // 15位序号掩码
    static constexpr uint16_t SEQ_MAX = 32767;    // 最大序号
};

} // namespace IEC103

#endif // IEC103_SEQMANAGER_H
