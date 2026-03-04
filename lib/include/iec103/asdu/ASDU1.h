#ifndef IEC103_ASDU1_H
#define IEC103_ASDU1_H

#include "ASDU.h"
#include "../types/DataPoint.h"
#include <vector>
#include <functional>

namespace IEC103 {

// ASDU1解析器 - 带时标的报文 (遥信突发)
// 南网规范: 信息体包含 FUN + INF + DPI + CP56Time2a(实际时间) + CP56Time2a(接收时间) + SIN
class Asdu1Parser : public AsduParser {
public:
    // 解析结果
    struct Result {
        uint8_t fun = 0;
        uint8_t inf = 0;
        DoublePointValue dpi = DoublePointValue::Indeterminate0;
        CP56Time2a eventTime;
        CP56Time2a recvTime;
        uint8_t sin = 0;
    };

    bool parse(const Asdu& asdu) override;

    // 获取解析结果
    const std::vector<Result>& results() const { return m_results; }

    // 转换为DigitalPoint
    DigitalPoint toDigitalPoint(const Result& result, uint16_t deviceAddr) const;

private:
    std::vector<Result> m_results;
};

// ASDU2解析器 - 带相对时间的时标报文 (保护动作事件)
// 南网规范: FUN + INF + DPI + RET + FAN + CP56Time2a + CP56Time2a + SIN(FPT)
class Asdu2Parser : public AsduParser {
public:
    struct Result {
        uint8_t fun = 0;
        uint8_t inf = 0;
        DoublePointValue dpi = DoublePointValue::Indeterminate0;
        uint16_t relativeTime = 0;  // 相对时间(ms)
        uint16_t faultNo = 0;       // 故障序号
        CP56Time2a eventTime;
        CP56Time2a recvTime;
        uint8_t fpt = 0;            // 故障相别及类型
    };

    bool parse(const Asdu& asdu) override;

    const std::vector<Result>& results() const { return m_results; }

    // 解析FPT
    bool isFptValid(const Result& r) const { return (r.fpt & 0x80) != 0; }
    bool isInternalFault(const Result& r) const { return (r.fpt & 0x10) != 0; }
    bool isGroundFault(const Result& r) const { return (r.fpt & 0x08) != 0; }
    bool isPhaseCFault(const Result& r) const { return (r.fpt & 0x04) != 0; }
    bool isPhaseBFault(const Result& r) const { return (r.fpt & 0x02) != 0; }
    bool isPhaseAFault(const Result& r) const { return (r.fpt & 0x01) != 0; }

private:
    std::vector<Result> m_results;
};

} // namespace IEC103

#endif // IEC103_ASDU1_H
