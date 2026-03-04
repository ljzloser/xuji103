#ifndef IEC103_ASDU42_H
#define IEC103_ASDU42_H

#include "ASDU.h"
#include "../types/DataPoint.h"
#include <vector>

namespace IEC103 {

// ASDU42解析器 - 双点信息状态 (南网扩展，总召唤响应)
// 南网规范: 多个信息对象，每个包含 FUN + INF + DPI
// 最后一个字节是SIN(扫描序号)
class Asdu42Parser : public AsduParser {
public:
    // 单个信息对象
    struct InfoObject {
        uint8_t fun = 0;
        uint8_t inf = 0;
        DoublePointValue dpi = DoublePointValue::Indeterminate0;
    };

    bool parse(const Asdu& asdu) override;

    // 获取解析结果
    const std::vector<InfoObject>& infoObjects() const { return m_infoObjects; }

    // 获取扫描序号
    uint8_t scn() const { return m_scn; }

    // 转换为DigitalPoint列表
    std::vector<DigitalPoint> toDigitalPoints(uint16_t deviceAddr) const;

private:
    std::vector<InfoObject> m_infoObjects;
    uint8_t m_scn = 0;
};

// ASDU42构建器 (用于测试)
class Asdu42Builder {
public:
    static Asdu build(uint16_t deviceAddr, uint8_t scn,
                      const std::vector<Asdu42Parser::InfoObject>& objects);
};

} // namespace IEC103

#endif // IEC103_ASDU42_H
