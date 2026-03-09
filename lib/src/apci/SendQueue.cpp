#include "iec103/apci/SendQueue.h"

namespace IEC103 {

void SendQueue::enqueue(uint16_t sendSeq, const Frame& frame)
{
    m_queue.insert(sendSeq, frame);
}

void SendQueue::confirm(uint16_t ackSeq)
{
    // 移除序号小于ackSeq的所有帧
    // 注意：序号是模32768循环的，需要处理回绕
    
    QVector<uint16_t> toRemove;
    for (auto it = m_queue.begin(); it != m_queue.end(); ++it) {
        uint16_t seq = it.key();
        
        // 比较序号：考虑回绕情况
        // 如果 ackSeq >= seq，或者 (ackSeq很小 && seq很大) 表示回绕
        int16_t diff = static_cast<int16_t>(ackSeq - seq);
        if (diff >= 0) {
            toRemove.append(seq);
        }
    }
    
    for (uint16_t seq : toRemove) {
        m_queue.remove(seq);
    }
}

QVector<Frame> SendQueue::getUnconfirmed() const
{
    return m_queue.values().toVector();
}

void SendQueue::clear()
{
    m_queue.clear();
}

} // namespace IEC103
