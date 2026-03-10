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
    // 南网规范统一使用双点遥信，单点由子站转换为双点
    virtual void onDoublePoint(const DigitalPoint& point) {
        (void)point; // 默认忽略
    }

    // ========== 通用服务回调 (遥测/遥脉统一) ==========

    // 通用服务数据 (ASDU10)
    // dataType由GDD.DataType决定: 7=浮点数(遥测), 3=无符号整数(遥脉)
    virtual void onGenericValue(const GenericPoint& point) {
        (void)point;
    }

    // 通用分类数据 (原始格式)
    // asduAddr: 完整ASDU地址 (高字节=设备编号)
    virtual void onGenericData(uint16_t asduAddr, const GenericDataItem& item) {
        (void)asduAddr; (void)item;
    }

    // 通用分类数据集
    virtual void onGenericDataSet(uint16_t asduAddr, const GenericDataSet& dataset) {
        (void)asduAddr; (void)dataset;
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
    
    // 带原因的断开连接通知
    virtual void onDisconnected(const QString& reason) {
        (void)reason;
        onDisconnected();  // 默认调用无参版本
    }

    // 发生错误
    virtual void onError(const QString& error) {
        (void)error;
    }

    // ========== 总召唤回调 ==========

    // 总召唤开始
    // deviceAddr: 设备编号 (0=子站本身, 1-254=装置编号)
    virtual void onGIStarted(uint8_t deviceAddr) {
        (void)deviceAddr;
    }

    // 总召唤完成
    virtual void onGICompleted(uint8_t deviceAddr) {
        (void)deviceAddr;
    }

    // ========== 通用分类召唤回调 ==========

    // 通用分类召唤完成
    virtual void onGenericGICompleted(uint8_t deviceAddr, uint8_t group) {
        (void)deviceAddr; (void)group;
    }
};

} // namespace IEC103

#endif // IEC103_IDATAHANDLER_H
