#ifndef IEC103_ASDU7_8_H
#define IEC103_ASDU7_8_H

#include "ASDU.h"
#include "../types/Types.h"
#include "../types/Constants.h"

namespace IEC103 {

// ASDU7 - 总召唤命令 (控制方向)
class Asdu7Builder {
public:
    // 构建总召唤命令
    static Asdu build(uint16_t deviceAddr, uint8_t scn);

    // 构建广播总召唤命令
    static Asdu buildBroadcast(uint8_t scn);
};

// ASDU8 - 总召唤终止 (监视方向)
class Asdu8Parser : public AsduParser {
public:
    struct Result {
        uint16_t deviceAddr = 0;
        uint8_t scn = 0;
    };

    bool parse(const Asdu& asdu) override;
    const Result& result() const { return m_result; }

private:
    Result m_result;
};

// ASDU8构建器
class Asdu8Builder {
public:
    static Asdu build(uint16_t deviceAddr, uint8_t scn);
};

} // namespace IEC103

#endif // IEC103_ASDU7_8_H
