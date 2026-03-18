#ifndef IEC103_ASDU_H
#define IEC103_ASDU_H

#include "../types/Types.h"
#include "../types/TimeStamp.h"
#include <QtGlobal>
#include <QByteArray>
#include <QString>

namespace IEC103 {

// ASDU数据单元标识符 (5字节)
#pragma pack(push, 1)
struct AsduHeader {
    uint8_t ti;       // 类型标识
    uint8_t vsq;      // 可变结构限定词
    uint8_t cot;      // 传送原因 (1字节, 与标准103一致)
    uint16_t addr;    // ASDU公共地址 (2字节, 南网扩展)
    // 高字节: 设备地址 (0x00=子站, 0x01-0xFE=装置编号, 0xFF=广播)
    // 低字节: D7-D3=定值区号, D2-D0=CPU号

    // 获取SQ位
    bool sq() const { return (vsq & 0x80) != 0; }

    // ========== ASDU地址解析方法 (南网规范) ==========

    // 设备地址 (高字节)
    uint8_t deviceAddr() const { return (addr >> 8) & 0xFF; }

    // CPU号 (低3位)
    uint8_t cpuNo() const { return addr & 0x07; }

    // 定值区号 (D7-D3, 仅定值处理时有效)
    uint8_t settingZone() const { return (addr >> 3) & 0x1F; }

    // 是否广播地址
    bool isBroadcast() const { return deviceAddr() == 0xFF; }

    // 是否子站本身
    bool isStationItself() const { return deviceAddr() == 0x00; }

    // 设置设备地址
    void setDeviceAddr(uint8_t dev) {
        addr = (addr & 0x00FF) | (static_cast<uint16_t>(dev) << 8);
    }

    // 设置CPU号
    void setCpuNo(uint8_t cpu) {
        addr = (addr & 0xFFF8) | (cpu & 0x07);
    }

    // 设置完整地址 (设备地址 + CPU号)
    void setAddrFields(uint8_t device, uint8_t cpu) {
        addr = (static_cast<uint16_t>(device) << 8) | (cpu & 0x07);
    }

    // 获取信息对象数目
    uint8_t count() const { return vsq & 0x7F; }
};
#pragma pack(pop)

static_assert(sizeof(AsduHeader) == 5, "AsduHeader must be 5 bytes");

// ASDU完整结构
class Asdu {
public:
    Asdu() = default;
    explicit Asdu(const QByteArray& data);

    // 解析ASDU
    bool parse(const QByteArray& data);
    bool parse(const uint8_t* data, size_t len);

    // 编码ASDU
    QByteArray encode() const;

    // 获取/设置数据单元标识符
    const AsduHeader& header() const { return m_header; }
    AsduHeader& header() { return m_header; }

    // 获取信息体数据
    const QByteArray& infoObjects() const { return m_infoObjects; }
    QByteArray& infoObjects() { return m_infoObjects; }

    // 便捷方法
    uint8_t ti() const { return m_header.ti; }
    void setTi(uint8_t ti) { m_header.ti = ti; }

    uint8_t vsq() const { return m_header.vsq; }
    void setVsq(uint8_t vsq) { m_header.vsq = vsq; }

    uint8_t cot() const { return m_header.cot; }
    void setCot(uint8_t cot) { m_header.cot = cot; }

    uint16_t addr() const { return m_header.addr; }
    void setAddr(uint16_t addr) { m_header.addr = addr; }

    bool sq() const { return m_header.sq(); }
    uint8_t count() const { return m_header.count(); }

    // 设置VSQ
    void setVsq(bool sq, uint8_t count) {
        m_header.vsq = (sq ? 0x80 : 0x00) | (count & 0x7F);
    }

    // 获取类型标识枚举
    TI tiEnum() const { return static_cast<TI>(m_header.ti); }

    // 获取传送原因枚举
    COT cotEnum() const { return static_cast<COT>(m_header.cot); }

    // 是否有效
    bool isValid() const;

    // 调试输出
    QString toString() const;

private:
    AsduHeader m_header;
    QByteArray m_infoObjects;
};

// ASDU解析器基类
class AsduParser {
public:
    virtual ~AsduParser() = default;

    // 解析ASDU信息体
    // 返回: 解析成功返回true
    virtual bool parse(const Asdu& asdu) = 0;

    // 获取解析结果
    virtual QString lastError() const { return m_lastError; }

protected:
    QString m_lastError;
};

} // namespace IEC103

#endif // IEC103_ASDU_H
