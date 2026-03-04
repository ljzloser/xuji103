#ifndef IEC103_TYPES_H
#define IEC103_TYPES_H

#include <QtGlobal>
#include <QString>
#include <QStringList>

namespace IEC103 {

// 双点信息值
enum class DoublePointValue : uint8_t {
    Indeterminate0 = 0,  // 不确定
    Off = 1,             // 分/开
    On = 2,              // 合/关
    Indeterminate3 = 3   // 不确定
};

// 单点信息值
enum class SinglePointValue : uint8_t {
    Off = 0,  // 分/开
    On = 1    // 合/关
};

// 数据质量标志
struct Quality {
    bool invalid = false;      // IV: 无效
    bool notTopical = false;   // NT: 非当前值
    bool substituted = false;  // SB: 被取代
    bool blocked = false;      // BL: 被封锁
    bool overflow = false;     // OV: 溢出（遥测专用）

    static Quality fromByte(uint8_t byte) {
        Quality q;
        q.invalid = (byte & 0x80) != 0;
        q.notTopical = (byte & 0x40) != 0;
        q.substituted = (byte & 0x20) != 0;
        q.blocked = (byte & 0x10) != 0;
        q.overflow = (byte & 0x01) != 0;
        return q;
    }

    uint8_t toByte() const {
        uint8_t byte = 0;
        if (invalid) byte |= 0x80;
        if (notTopical) byte |= 0x40;
        if (substituted) byte |= 0x20;
        if (blocked) byte |= 0x10;
        if (overflow) byte |= 0x01;
        return byte;
    }

    bool isGood() const {
        return !invalid && !notTopical && !substituted && !blocked;
    }

    QString toString() const {
        QStringList flags;
        if (invalid) flags << "IV";
        if (notTopical) flags << "NT";
        if (substituted) flags << "SB";
        if (blocked) flags << "BL";
        if (overflow) flags << "OV";
        return flags.isEmpty() ? "OK" : flags.join("|");
    }
};

// 链路状态
enum class LinkState {
    Disconnected,   // 未连接
    Connecting,     // 连接中
    Connected,      // TCP已连接
    Started,        // STARTDT已启动
    Stopped,        // STOPDT已停止
    Error           // 错误
};

// 传送原因
enum class COT : uint16_t {
    Spontaneous = 1,              // 自发(突发)
    Cyclic = 2,                   // 循环
    ResetFCB = 3,                 // 复位帧计数位
    ResetCU = 4,                  // 复位通信单元
    StartRestart = 5,             // 启动/重新启动
    PowerOn = 6,                  // 电源合上
    TestMode = 7,                 // 测试模式
    TimeSync = 8,                 // 时间同步
    GeneralInterrogation = 9,     // 总召唤
    GITermination = 10,           // 总召唤终止
    LocalOperation = 11,          // 当地操作
    RemoteOperation = 12,         // 远方操作
    ActCon = 20,                  // 命令的肯定认可
    ActNeg = 21,                  // 命令的否定认可
    DisturbanceData = 31,         // 扰动数据传送
    GenWriteAck = 40,             // 通用分类写命令肯定认可
    GenWriteNeg = 41,             // 通用分类写命令否定认可
    GenReadValid = 42,            // 对通用分类读命令有效数据响应
    GenReadInvalid = 43           // 对通用分类读命令无效数据响应
};

// 功能类型
enum class FUN : uint8_t {
    DistanceProtection = 128,     // t(Z) 距离保护
    OvercurrentProtection = 160,  // I>> 过流保护
    TransformerDiff = 176,        // IT 变压器差动保护
    LineDiff = 192,               // IL 线路差动保护
    Generic = 254,                // GEN 通用分类功能类型
    Global = 255                  // GLB 全局功能类型
};

// 类型标识 (TI)
enum class TI : uint8_t {
    M_TM_TA_3 = 1,    // 带时标的报文 (遥信突发)
    M_TMR_TA_3 = 2,   // 带相对时间的时标报文
    M_MEI_NA_3 = 3,   // 被测值I
    M_TME_TA_3 = 4,   // 带相对时间的时标被测值
    M_SYN_TA_3 = 6,   // 时间同步
    C_IGI_NA_3 = 7,   // 总召唤命令
    M_TGI_NA_3 = 8,   // 总召唤终止
    M_MEII_NA_3 = 9,  // 被测值II
    M_GD_NA_3 = 10,   // 通用分类数据 (监视方向)
    C_GD_NA_3 = 10,   // 通用分类数据 (控制方向)
    M_GI_NA_3 = 11,   // 通用分类标识
    C_GRC_NA_3 = 20,  // 一般命令
    C_GC_NA_3 = 21,   // 通用分类命令
    M_DP_TA_3 = 42,   // 双点信息状态 (南网扩展ASDU42)
    M_LRD_TA_3 = 23,  // 被记录扰动表
    M_RTD_TA_3 = 26,  // 扰动数据传输准备就绪
    M_RTC_NA_3 = 27,  // 被记录通道传输准备就绪
    M_RTT_NA_3 = 28,  // 带标志的状态变位传输准备就绪
    M_TDT_TA_3 = 29,  // 传送带标志的状态变位
    M_TDN_NA_3 = 30,  // 传送扰动值
    M_EOT_NA_3 = 31   // 传送结束
};

// KOD 描述类别
enum class KOD : uint8_t {
    None = 0,           // 无指定描述类别
    ActualValue = 1,    // 实际值
    DefaultValue = 2,   // 缺省值
    Range = 3,          // 量程
    Precision = 5,      // 精度
    Factor = 6,         // 因子
    Percent = 7,        // %参比
    Enumeration = 8,    // 列表
    Dimension = 9,      // 量纲
    Description = 10,   // 描述
    Password = 12,      // 口令条目
    ReadOnly = 13,      // 只读
    WriteOnly = 14,     // 只写
    CorrespondingFUNINF = 19,  // 相应的功能类型和信息序号
    CorrespondingEvents = 20,  // 相应的事件
    EnumTextArray = 21,        // 列表文本阵列
    EnumValueArray = 22,       // 列表值阵列
    RelatedEntries = 23,       // 相关联的条目
    // 南网扩展
    FromDevice = 0x01,         // 从装置取实际值
    AttributeStruct = 0x67,    // 属性结构
    FromSubstationDB = 0xA8,   // 从子站数据库取实际值
    FromSubstationDBRead = 0xA9 // 从子站数据库读取
};

// 数据类型 (通用服务)
enum class DataType : uint8_t {
    None = 0,
    OS8ASCII = 1,       // ASCII字符串
    BS1 = 2,            // 成组8位串
    UI = 3,             // 无符号整数
    I = 4,              // 有符号整数
    UF = 5,             // 无符号浮点数
    F = 6,              // 浮点数
    R32_23 = 7,         // IEEE 754短实数
    R64_53 = 8,         // IEEE 754实数
    DPI = 9,            // 双点信息
    SPI = 10,           // 单点信息
    DPI_TE = 11,        // 带瞬变和差错的双点信息
    MEA = 12,           // 带品质描述词的被测值
    BinaryTime = 14,    // 二进制时间
    GIN = 15,           // 通用分类标识序号
    RelativeTime = 16,  // 相对时间
    FUN_INF = 18,       // 功能类型和信息序号
    TimeTagMsg = 19,    // 带时标的报文
    TimeTagMsgRel = 20, // 带相对时间的时标报文
    MeasTimeTag = 21,   // 带相对时间的时标被测值
    ExtTextNo = 22,     // 外部文本序号
    GRC = 23,           // 通用分类回答码
    DataStructure = 24, // 数据结构
    Index = 25,
    // 南网扩展数据类型
    TimeTagMsg7 = 213,      // 带绝对时间的七字节时标报文
    TimeTagMsgRel7 = 214,   // 带相对时间的七字节时标报文
    FloatTimeTag7 = 215,    // 带相对时间七字节时标的浮点值
    IntTimeTag7 = 216,      // 带相对时间七字节时标的整形值
    CharTimeTag7 = 217      // 带相对时间七字节时标的字符值
};

// 通用分类回答码
enum class GRC : uint8_t {
    Acknowledge = 0,           // 认可
    InvalidGIN = 1,            // 无效的GIN
    NoSuchData = 2,            // 不存在所请求的数据
    DataNotAvailable = 3,      // 数据不能用
    ChangeSettingError = 4,    // 改变设定时确认出错
    ChangeSettingOutOfRange = 5, // 改变设定时超出量程
    EntryTooLarge = 6,         // 条目范围太大
    TooManyCommands = 7,       // 太多命令
    EntryReadOnly = 8,         // 条目只读
    SettingPasswordProtected = 9, // 设定受口令保护
    LocalSettingInProgress = 10, // 当地设定在进行中
    ErrorWithDescription = 11  // 带有差错
};

}

#endif // IEC103_TYPES_H
