#include "iec103/asdu/ASDU11.h"
#include <QDebug>

namespace IEC103 {

QString CatalogEntry::kodDescription() const
{
    switch (kod) {
    case 0x01: return "实际值";
    case 0x0A: return "描述";
    case 0x67: return "属性结构";
    case 0xA8: return "从装置取实际值";
    case 0xA9: return "从子站数据库读取";
    default: return QString("KOD_%1").arg(kod, 2, 16, QChar('0'));
    }
}

bool Asdu11Parser::parse(const Asdu& asdu)
{
    if (asdu.ti() != 11) {
        m_lastError = QString("Invalid TI: %1, expected 11").arg(asdu.ti());
        return false;
    }
    
    m_deviceAddr = asdu.addr();
    m_inf = 0;  // ASDU11没有INF字段，从FUN获取
    
    const QByteArray& info = asdu.infoObjects();
    int offset = 0;
    
    // 检查最小长度: FUN(1) + INF(1) + RII(1) + GIN(2) + NGD(1) = 6字节
    if (info.size() < 6) {
        m_lastError = QString("Info too short, size=%1").arg(info.size());
        return false;
    }
    
    // FUN (跳过)
    offset++;
    
    // INF
    m_inf = static_cast<uint8_t>(info[offset++]);
    
    // RII
    m_data.rii = static_cast<uint8_t>(info[offset++]);
    
    // GIN (组号 + 条目号)
    m_data.gin.group = static_cast<uint8_t>(info[offset++]);
    m_data.gin.entry = static_cast<uint8_t>(info[offset++]);
    
    // NGD (通用分类描述元素的数目)
    m_data.ngd = static_cast<uint8_t>(info[offset++]);
    
    m_data.entries.clear();
    m_data.entries.reserve(m_data.ngd);
    
    // 解析每个目录条目
    for (int i = 0; i < m_data.ngd && offset < info.size(); ++i) {
        CatalogEntry entry;
        
        // 检查最小长度: KOD(1) + GDD(3) = 4字节
        if (offset + 4 > info.size()) {
            qWarning() << "ASDU11: Incomplete entry" << i;
            break;
        }
        
        // KOD
        entry.kod = static_cast<uint8_t>(info[offset++]);
        
        // GDD
        entry.gdd.dataType = static_cast<uint8_t>(info[offset++]);
        entry.gdd.dataSize = static_cast<uint8_t>(info[offset++]);
        entry.gdd.number = static_cast<uint8_t>(info[offset++]);
        
        // GID
        int gidSize = entry.gdd.dataSize * entry.gdd.number;
        if (offset + gidSize > info.size()) {
            qWarning() << "ASDU11: GID overflow at entry" << i;
            break;
        }
        entry.gid = info.mid(offset, gidSize);
        offset += gidSize;
        
        m_data.entries.append(entry);
    }
    
    return true;
}

} // namespace IEC103