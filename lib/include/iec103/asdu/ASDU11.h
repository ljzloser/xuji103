#ifndef IEC103_ASDU11_H
#define IEC103_ASDU11_H

#include "../types/DataPoint.h"
#include "ASDU.h"
#include <QVector>
#include <cstdint>

namespace IEC103 {

// ASDU11 - 通用分类标识
// 用于响应"读单个条目的目录"请求，返回该条目的所有可用描述类别
// 参考：南网规范 图表17

// 目录条目 - 包含一个KOD对应的描述
struct CatalogEntry {
    uint8_t kod = 0;          // 描述类别 (KOD)
    GDD gdd;                  // 通用分类数据描述
    QByteArray gid;           // 通用分类标识数据
    
    // 便捷方法
    QString kodDescription() const;
};

// ASDU11 数据结构
struct Asdu11Data {
    uint8_t rii = 0;          // 返回信息标识符
    GIN gin;                  // 通用分类标识序号
    uint8_t ngd = 0;          // 通用分类描述元素的数目
    QVector<CatalogEntry> entries;
};

// ASDU11 解析器
class Asdu11Parser : public AsduParser {
public:
    bool parse(const Asdu& asdu) override;
    const Asdu11Data& data() const { return m_data; }
    uint16_t deviceAddr() const { return m_deviceAddr; }
    uint8_t inf() const { return m_inf; }
    
private:
    uint16_t m_deviceAddr = 0;
    uint8_t m_inf = 0;
    Asdu11Data m_data;
};

} // namespace IEC103

#endif // IEC103_ASDU11_H