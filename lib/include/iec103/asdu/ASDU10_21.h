#ifndef IEC103_ASDU10_21_H
#define IEC103_ASDU10_21_H

#include "ASDU.h"
#include "../types/DataPoint.h"
#include "../types/Types.h"
#include <vector>

namespace IEC103 {

// ASDU10 - 通用分类数据 (监视方向)
// ASDU21 - 通用分类命令 (控制方向)
// 两者结构类似，只是TI和方向不同

// 通用分类数据结构
struct GenericDataStruct {
    uint8_t rii = 0;              // 返回信息标识符
    uint8_t ngd = 0;              // 数据集数目
    bool cont = false;            // 后续状态位

    std::vector<GenericDataItem> items;
};

// ASDU10解析器
class Asdu10Parser : public AsduParser {
public:
    bool parse(const Asdu& asdu) override;

    // 获取解析结果
    const GenericDataStruct& data() const { return m_data; }

    // 获取设备地址
    uint16_t deviceAddr() const { return m_deviceAddr; }

    // 获取INF
    uint8_t inf() const { return m_inf; }

private:
    uint16_t m_deviceAddr = 0;
    uint8_t m_inf = 0;
    GenericDataStruct m_data;

    bool parseItem(const uint8_t* data, size_t& offset, size_t maxLen, GenericDataItem& item);
};

// ASDU10构建器
class Asdu10Builder {
public:
    static Asdu build(uint16_t deviceAddr, uint8_t inf, const GenericDataStruct& data);
};

// ASDU21解析器
class Asdu21Parser : public AsduParser {
public:
    bool parse(const Asdu& asdu) override;

    const GenericDataStruct& data() const { return m_data; }
    uint16_t deviceAddr() const { return m_deviceAddr; }
    uint8_t inf() const { return m_inf; }

private:
    uint16_t m_deviceAddr = 0;
    uint8_t m_inf = 0;
    GenericDataStruct m_data;
};

// ASDU21构建器
class Asdu21Builder {
public:
    // 读一组全部条目 (INF=F1)
    static Asdu buildReadGroup(uint16_t deviceAddr, uint8_t group, KOD kod);

    // 读单个条目 (INF=F4)
    static Asdu buildReadEntry(uint16_t deviceAddr, const GIN& gin, KOD kod);

    // 读全部组标题 (INF=F0)
    static Asdu buildReadAllGroups(uint16_t deviceAddr);

    // 通用分类总召唤 (INF=F5)
    static Asdu buildGenericGI(uint16_t deviceAddr);

private:
    static Asdu build(uint16_t deviceAddr, uint8_t inf, const GenericDataStruct& data);
};

} // namespace IEC103

#endif // IEC103_ASDU10_21_H
