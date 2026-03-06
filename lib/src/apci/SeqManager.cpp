#include "iec103/apci/SeqManager.h"

namespace IEC103 {

SeqManager::SeqManager()
    : m_sendSeq(0)
    , m_recvSeq(0)
    , m_lastAckSeq(0) {
}

void SeqManager::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sendSeq = 0;
    m_recvSeq = 0;
    m_lastAckSeq = 0;
    m_recvCount = 0;
}

uint16_t SeqManager::sendSeq() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sendSeq;
}

uint16_t SeqManager::nextSendSeq() {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint16_t seq = m_sendSeq;
    m_sendSeq = (m_sendSeq + 1) & SEQ_MASK;
    return seq;
}

uint16_t SeqManager::recvSeq() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_recvSeq;
}

void SeqManager::updateRecvSeq(uint16_t seq) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_recvSeq = seq & SEQ_MASK;
}

uint16_t SeqManager::nextRecvSeq() {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint16_t seq = m_recvSeq;
    m_recvSeq = (m_recvSeq + 1) & SEQ_MASK;
    return seq;
}

uint16_t SeqManager::lastAckSeq() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastAckSeq;
}

void SeqManager::setLastAckSeq(uint16_t seq) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastAckSeq = seq & SEQ_MASK;
}

uint16_t SeqManager::unconfirmedCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    int diff = m_sendSeq - m_lastAckSeq;
    if (diff < 0) {
        diff += SEQ_MAX + 1;
    }
    return static_cast<uint16_t>(diff);
}

bool SeqManager::validateRecvSeq(uint16_t seq) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    // 检查接收序号是否等于期望的序号
    return seq == m_recvSeq;
}

bool SeqManager::validateAckSeq(uint16_t ackSeq) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    // 检查确认序号是否在有效范围内
    // 确认序号应该 <= 当前发送序号，且 > 最后确认序号
    int16_t diffFromSend = static_cast<int16_t>(ackSeq - m_sendSeq);
    int16_t diffFromLast = static_cast<int16_t>(ackSeq - m_lastAckSeq);

    // ackSeq <= sendSeq 且 ackSeq > lastAckSeq
    return diffFromSend <= 0 && diffFromLast > 0;
}

bool SeqManager::needSendAck() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    // 如果接收序号与最后确认序号不同，需要发送确认
    return m_recvSeq != m_lastAckSeq;
}

bool SeqManager::hasUnconfirmed() const {
    return unconfirmedCount() > 0;
}

uint8_t SeqManager::recvCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_recvCount;
}

void SeqManager::incrementRecvCount() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_recvCount++;
}

void SeqManager::resetRecvCount() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_recvCount = 0;
}

} // namespace IEC103
