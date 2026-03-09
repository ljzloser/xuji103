#ifndef IEC103_SENDQUEUE_H
#define IEC103_SENDQUEUE_H

#include "../types/Types.h"
#include "../apci/Frame.h"
#include <QMap>
#include <QVector>
#include <cstdint>

namespace IEC103 {

// 发送队列 - 缓存未确认的I帧
// 用于在连接恢复后重发
class SendQueue {
public:
    // 入队一个待确认的帧
    void enqueue(uint16_t sendSeq, const Frame& frame);
    
    // 确认序号之前的所有帧（从队列中移除）
    void confirm(uint16_t ackSeq);
    
    // 获取所有未确认的帧
    QVector<Frame> getUnconfirmed() const;
    
    // 获取未确认帧数量
    int count() const { return m_queue.size(); }
    
    // 清空队列
    void clear();
    
    // 检查是否为空
    bool isEmpty() const { return m_queue.isEmpty(); }

private:
    QMap<uint16_t, Frame> m_queue;
};

} // namespace IEC103

#endif // IEC103_SENDQUEUE_H
