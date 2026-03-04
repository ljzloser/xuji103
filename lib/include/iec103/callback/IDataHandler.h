#ifndef IEC103_IDATAHANDLER_H
#define IEC103_IDATAHANDLER_H

#include "../types/DataPoint.h"
#include "../types/Types.h"
#include <QtGlobal>

namespace IEC103 {

// 数据回调接口
// 用户需要继承此接口并实现需要的回调方法
class IDataHandler {
public:
    virtual ~IDataHandler() = default;

    // ========== 遥信回调 ==========

    // 双点遥信 (总召唤响应ASDU42或突发ASDU1)
    virtual void onDoublePoint(const DigitalPoint& point) {
        (void)point; // 默认忽略
    }

    // 单点遥信 (突发ASDU1，如果子站支持)
    virtual void onSinglePoint(uint16_t deviceAddr, uint16_t infoAddr,
                               bool value, const Quality& quality,
                               const QDateTime& time) {
        (void)deviceAddr; (void)infoAddr; (void)value; (void)quality; (void)time;
    }

    // ========== 遥测回调 ==========

    // 遥测值 (通用服务ASDU10)
    virtual void onAnalogValue(const AnalogPoint& point) {
        (void)point;
    }

    // ========== 遥脉回调 ==========

    // 遥脉值 (通用服务ASDU10)
    virtual void onCounterValue(const CounterPoint& point) {
        (void)point;
    }

    // ========== 通用服务回调 ==========

    // 通用分类数据 (原始格式)
    virtual void onGenericData(uint16_t deviceAddr, const GenericDataItem& item) {
        (void)deviceAddr; (void)item;
    }

    // 通用分类数据集
    virtual void onGenericDataSet(uint16_t deviceAddr, const GenericDataSet& dataset) {
        (void)deviceAddr; (void)dataset;
    }

    // ========== 链路状态回调 ==========

    // 链路状态变化
    virtual void onLinkStateChanged(LinkState state) {
        (void)state;
    }

    // 连接成功
    virtual void onConnected() {}

    // 断开连接
    virtual void onDisconnected() {}

    // 发生错误
    virtual void onError(const QString& error) {
        (void)error;
    }

    // ========== 总召唤回调 ==========

    // 总召唤开始
    virtual void onGIStarted(uint16_t deviceAddr) {
        (void)deviceAddr;
    }

    // 总召唤完成
    virtual void onGICompleted(uint16_t deviceAddr) {
        (void)deviceAddr;
    }

    // ========== 通用分类召唤回调 ==========

    // 通用分类召唤完成
    virtual void onGenericGICompleted(uint16_t deviceAddr, uint8_t group) {
        (void)deviceAddr; (void)group;
    }
};

} // namespace IEC103

#endif // IEC103_IDATAHANDLER_H
