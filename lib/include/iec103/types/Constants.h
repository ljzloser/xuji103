#ifndef IEC103_CONSTANTS_H
#define IEC103_CONSTANTS_H

#include <QtGlobal>

namespace IEC103 {

// APCI常量
constexpr uint8_t FRAME_START_BYTE = 0x68;      // 帧起始字节
constexpr uint16_t APDU_MAX_LENGTH = 2048;      // APDU最大长度
constexpr uint16_t ASDU_MAX_LENGTH = 2045;      // ASDU最大长度 (2048-3)
constexpr uint8_t APCI_LENGTH = 4;              // APCI长度
constexpr uint16_t FRAME_LENGTH_MAX = 2045;     // 帧长度域最大值 (南网规范附录A)
constexpr int MAX_RECEIVE_BUFFER = 4096;        // 接收缓冲区最大大小 (允许缓存2个最大帧)

// I格式控制域最低位
constexpr uint8_t I_FORMAT_MASK = 0x01;         // I格式: LSB=0
constexpr uint8_t S_FORMAT_BYTE1 = 0x01;        // S格式: 字节1=0x01
constexpr uint8_t S_FORMAT_BYTE2 = 0x00;        // S格式: 字节2=0x00

// U格式控制域
constexpr uint8_t U_STARTDT_ACT = 0x07;         // STARTDT激活
constexpr uint8_t U_STARTDT_CON = 0x0B;         // STARTDT确认
constexpr uint8_t U_STOPDT_ACT = 0x13;          // STOPDT激活
constexpr uint8_t U_STOPDT_CON = 0x23;          // STOPDT确认
constexpr uint8_t U_TESTFR_ACT = 0x43;          // TESTFR激活
constexpr uint8_t U_TESTFR_CON = 0x83;          // TESTFR确认

// 默认端口
constexpr uint16_t DEFAULT_PORT = 2404;

// 超时常量 (毫秒)
constexpr int DEFAULT_T1_TIMEOUT = 15000;       // T1: 发送或测试APDU的超时
constexpr int DEFAULT_T2_TIMEOUT = 10000;       // T2: 未确认I格式APDU确认超时
constexpr int DEFAULT_T3_TIMEOUT = 20000;       // T3: 空闲时发送TESTFR的间隔
constexpr int DEFAULT_RECONNECT_INTERVAL = 5000; // 重连间隔

// 序号参数
constexpr uint16_t MAX_SEQ_NUMBER = 32767;      // 最大序号 (15位)
constexpr uint8_t MAX_UNCONFIRMED_I = 12;       // k值: 最大未确认I格式APDU
constexpr uint8_t MAX_ACK_DELAY_W = 8;          // w值: 最大确认延迟

// ASDU地址
constexpr uint16_t ASDU_ADDR_SUBSTATION = 0x0000;  // 子站本身
constexpr uint16_t ASDU_ADDR_BROADCAST = 0x00FF;   // 广播地址
constexpr uint16_t ASDU_ADDR_MIN = 0x0001;         // 最小装置地址
constexpr uint16_t ASDU_ADDR_MAX = 0x00FE;         // 最大装置地址

// 通用服务 INF (控制方向)
constexpr uint8_t INF_READ_ALL_GROUPS = 0xF0;      // 读全部组的标题
constexpr uint8_t INF_READ_GROUP_ENTRIES = 0xF1;   // 读一组的全部条目
constexpr uint8_t INF_READ_ENTRY_DIR = 0xF3;       // 读单个条目的目录
constexpr uint8_t INF_READ_ENTRY_VALUE = 0xF4;     // 读单个条目的值
constexpr uint8_t INF_GI_GENERIC = 0xF5;           // 通用分类数据总召唤

// 通用服务 INF (监视方向)
constexpr uint8_t INF_READ_ALL_GROUPS_RESP = 0xF0;
constexpr uint8_t INF_READ_GROUP_ENTRIES_RESP = 0xF1;
constexpr uint8_t INF_READ_ENTRY_DIR_RESP = 0xF3;
constexpr uint8_t INF_READ_ENTRY_VALUE_RESP = 0xF4;
constexpr uint8_t INF_GI_GENERIC_TERM = 0xF5;      // 通用分类数据总召唤中止

// 通用服务数据类型 (GDD.DataType)
// 数据类型由响应报文中的GDD.DataType决定
constexpr uint8_t DATA_TYPE_FLOAT = 7;       // R32.23浮点数 (遥测)
constexpr uint8_t DATA_TYPE_UINT = 3;        // 无符号整数 (遥脉)
constexpr uint8_t DATA_TYPE_INT = 5;         // 有符号整数
constexpr uint8_t DATA_TYPE_ASCII = 1;       // ASCII字符串

// 组号说明：
// 组号由子站配置决定，不再硬编码
// 通过配置文件或读取子站配置获取

// 总召唤扫描序号
constexpr uint8_t GI_SCN_DEFAULT = 0x01;

}

#endif // IEC103_CONSTANTS_H
