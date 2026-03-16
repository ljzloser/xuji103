#ifndef IEC103_IDATAHANDLER_H
#define IEC103_IDATAHANDLER_H

#include "../types/DataPoint.h"
#include "../types/Types.h"
#include <QtGlobal>
#include <QByteArray>

namespace IEC103 {

// 数据回调接口
// 用户需要继承此接口并实现需要的回调方法
class IDataHandler {
public:
    virtual ~IDataHandler() = default;

    // ========== 原始报文回调 ==========

    // 接收到原始报文 (APDU完整帧，包含68H开头)
    // data: 完整APDU帧数据
    // direction: true=接收, false=发送
    virtual void onRawFrame(const QByteArray &data, bool direction)
    {
        (void)data;
        (void)direction;
    }

    // 接收到I格式帧 (解析后的ASDU)
    // asduData: ASDU原始数据 (不含APCI)
    // sendSeq: 发送序号 N(S)
    // recvSeq: 接收序号 N(R)
    virtual void onIFrame(const QByteArray &asduData, uint16_t sendSeq, uint16_t recvSeq)
    {
        (void)asduData;
        (void)sendSeq;
        (void)recvSeq;
    }

    // 接收到S格式帧
    // recvSeq: 接收序号 N(R)
    virtual void onSFrame(uint16_t recvSeq)
    {
        (void)recvSeq;
    }

    // 接收到U格式帧
    // uType: U格式类型 (0x07=STARTDT_ACT, 0x0B=STARTDT_CON, etc.)
    virtual void onUFrame(uint8_t uType)
    {
        (void)uType;
    }

    // ========== 遥信回调 ==========

    // 双点遥信 (总召唤响应ASDU42或突发ASDU1)
    // 南网规范统一使用双点遥信，单点由子站转换为双点
    virtual void onDoublePoint(const DigitalPoint& point) {
        (void)point; // 默认忽略
    }

    // 保护动作事件 (ASDU2 - 带相对时间的时标报文)
    // 包含故障序号、故障相别、相对时间等信息
    virtual void onProtectionEvent(const ProtectionEvent &event)
    {
        (void)event; // 默认忽略
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
