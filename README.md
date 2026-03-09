# 南网以太网103协议库

> **IEC103以太网通信协议解析与数据采集库**
> 
> 基于南网规范（上海许继），实现遥测、遥信、遥脉三遥数据的实时采集

## 目录

- [项目简介](#项目简介)
- [功能特性](#功能特性)
- [协议说明](#协议说明)
- [项目结构](#项目结构)
- [构建说明](#构建说明)
- [快速开始](#快速开始)
- [配置说明](#配置说明)
- [库API文档](#库api文档)
- [控制台程序](#控制台程序)
- [测试子站](#测试子站)
- [开发状态](#开发状态)

---

## 项目简介

本项目的南网以太网103协议库，用于实现主站与子站之间的通信，完成三遥数据（遥信、遥测、遥脉）的实时采集。

### 协议本质

**IEC 104 的 APCI + IEC 103 的 ASDU**

- 传输层：TCP/IP（端口2404）
- 帧格式：参照IEC 104标准
- 应用层：IEC 103 ASDU + 南网扩展

### 与标准IEC 103的关键差异

| 特性 | 标准 IEC 103 | 南网以太网103 |
|------|-------------|--------------|
| **传输层** | 串口 RS485 | TCP/IP（端口2404） |
| **APCI** | 无 | **4字节控制域**（参照104） |
| **帧格式** | FT1.2固定/可变帧 | 仅可变帧（68H开头） |
| **长度域** | 1字节 | **2字节**（最大2045） |
| **ASDU地址** | 1字节 | **2字节**（设备地址+CPU号） |
| **时标** | CP32Time2a(4字节) | **CP56Time2a(7字节)** |
| **遥测/遥脉传输** | ASDU9/15 | **通用服务ASDU10** |

---

## 功能特性

### 已实现功能

- ✅ TCP连接管理与自动重连
- ✅ STARTDT/STOPDT/TESTFR控制命令
- ✅ I/S/U格式帧编解码
- ✅ 序号管理与流控制（k/w机制）
- ✅ 总召唤（ASDU7/8/42）
- ✅ 通用服务召唤（ASDU10/21）
- ✅ 遥信解析（ASDU1/42）
- ✅ 遥测/遥脉解析（ASDU10）
- ✅ 南网扩展数据类型（213-217）
- ✅ 超时保护（T0/T1/T2/T3）
- ✅ 帧长度校验与缓冲区限制
- ✅ 发送队列与重发机制
- ✅ Python测试子站

### 不实现功能

根据需求，以下功能不在实现范围内：

| 功能 | 说明 |
|------|------|
| 控制命令 | 时钟同步、复归等 |
| 定值操作 | 写定值、切换定值区 |
| 录波文件 | 简要录波、文件传输 |
| 故障历史 | 历史信息召唤 |

---

## 协议说明

### 帧格式

```
┌──────┬──────────┬──────────┬─────────────────────────┐
│ 68H  │ 长度L    │ 长度L    │ APCI (4字节) + ASDU     │
│      │ 低字节   │ 高字节   │                         │
└──────┴──────────┴──────────┴─────────────────────────┘

长度L = APCI长度(4) + ASDU长度
最大长度 = 2045 (APDU_MAX = 2048 - 启动字符(1) - 长度域(2字节))
```

### APCI控制域

**I格式（信息传输）：**
```
┌────────────────────────────────────────────────────┐
│ 字节1: N(S)低8位                              LSB=0 │
│ 字节2: N(S)高7位 + 保留位                           │
│ 字节3: N(R)低8位                              LSB=0 │
│ 字节4: N(R)高7位 + 保留位                           │
└────────────────────────────────────────────────────┘
```

**S格式（监视确认）：**
```
┌────────────────────────────────────────────────────┐
│ 字节1: 00000001                                    │
│ 字节2: 00000000                                    │
│ 字节3: N(R)低8位                              LSB=0 │
│ 字节4: N(R)高7位 + 保留位                           │
└────────────────────────────────────────────────────┘
```

**U格式（控制功能）：**
```
STARTDT(启动): 0x07  STARTDT确认: 0x0B
STOPDT(停止):  0x13  STOPDT确认:  0x23
TESTFR(测试):  0x43  TESTFR确认: 0x83
```

### 流量控制参数

| 参数 | 含义 | 默认值 | 范围 |
|-----|------|-------|------|
| **k** | 未被确认的I格式APDU最大数目 | 12 | 1 ~ 32767 |
| **w** | 最迟确认APDU的最大数目 | 8 | 1 ~ 32767 |

### 超时参数

| 参数 | 含义 | 默认值 |
|-----|------|-------|
| **T0** | 连接建立超时 | 30秒 |
| **T1** | 发送APDU超时 | 15秒 |
| **T2** | 确认超时 | 10秒 |
| **T3** | 空闲测试间隔 | 20秒 |

---

## 项目结构

```
xuji103/
├── lib/                              # 核心协议库 (libiec103)
│   ├── include/iec103/
│   │   ├── IEC103Master.h            # 主接口类
│   │   ├── types/                    # 类型定义
│   │   │   ├── Types.h               # 基础类型
│   │   │   ├── DataPoint.h           # 数据点结构
│   │   │   ├── TimeStamp.h           # 7字节时标
│   │   │   └── Constants.h           # 常量定义
│   │   ├── apci/                     # APCI层
│   │   │   ├── APCI.h                # APCI结构
│   │   │   ├── Frame.h               # 帧结构
│   │   │   ├── SeqManager.h          # 序号管理
│   │   │   └── SendQueue.h           # 发送队列
│   │   ├── asdu/                     # ASDU层
│   │   │   ├── ASDU.h                # ASDU基础结构
│   │   │   ├── ASDU1.h               # 单点遥信
│   │   │   ├── ASDU7_8.h             # 总召唤
│   │   │   ├── ASDU10_21.h           # 通用服务
│   │   │   ├── ASDU11.h              # 通用分类标识
│   │   │   └── ASDU42.h              # 双点遥信
│   │   ├── callback/
│   │   │   └── IDataHandler.h        # 数据回调接口
│   │   └── transport/
│   │       └── TcpTransport.h        # TCP传输
│   └── src/                          # 实现文件
│       ├── apci/
│       ├── asdu/
│       ├── master/
│       ├── transport/
│       └── utils/
├── app/                              # 控制台应用程序
│   ├── main.cpp                      # 主入口
│   ├── Config.h/cpp                  # 配置解析
│   └── DataPrinter.h/cpp             # 数据打印
├── tests/python/                     # Python测试子站
│   ├── slave.py                      # 子站模拟器
│   ├── protocol/                     # 协议实现
│   └── data/                         # 模拟数据
├── docs/                             # 文档
├── IEC60870-5/                       # IEC标准参考
├── 上海许继(IEC103南网)/              # 南网规范文档
├── config.json                       # 配置文件示例
├── CMakeLists.txt                    # CMake配置
└── README.md                         # 本文档
```

---

## 构建说明

### 依赖要求

| 组件 | 版本要求 |
|------|---------|
| C++ | C++17 |
| Qt | Qt5 Core + Network |
| CMake | 3.5+ |
| Python | 3.7+ (测试子站) |

### Linux构建

```bash
# 克隆项目
git clone https://github.com/ljzloser/xuji103.git
cd xuji103

# 创建构建目录
mkdir -p build && cd build

# 配置
cmake .. -G Ninja

# 编译
cmake --build .

# 输出文件
# build/lib/libiec103.a    # 静态库
# build/bin/xuji103        # 控制台程序
```

### Windows构建

```powershell
# 配置 (Visual Studio)
cmake -B build -G "Visual Studio 17 2022"

# 编译
cmake --build build --config Release

# 输出文件
# build/lib/Release/iec103.lib
# build/app/Release/xuji103.exe
```

---

## 快速开始

### 1. 创建配置文件

创建 `config.json`：

```json
{
  "connection": {
    "host": "192.168.1.100",
    "port": 2404,
    "timeout": 15000,
    "reconnect_interval": 5000,
    "test_interval": 20000,
    "max_unconfirmed": 12,
    "max_ack_delay": 8
  },
  "polling": {
    "gi_interval": 60,
    "group_interval": 5,
    "groups": [
      {"group": 1, "name": "装置模拟量"},
      {"group": 2, "name": "装置状态量"}
    ]
  },
  "logging": {
    "level": "info"
  }
}
```

### 2. 启动测试子站

```bash
cd tests/python
python3 slave.py --port 2404
```

### 3. 运行控制台程序

```bash
./build/bin/xuji103
```

---

## 配置说明

### 完整配置项

```json
{
  "connection": {
    "host": "192.168.1.100",      // 子站IP地址
    "port": 2404,                  // 子站端口
    "timeout": 15000,              // T1超时(ms)
    "reconnect_interval": 5000,    // 重连间隔(ms)
    "auto_reconnect": true,        // 自动重连
    "test_interval": 20000,        // T3测试间隔(ms)
    "max_unconfirmed": 12,         // k值：最大未确认I帧数
    "max_ack_delay": 8             // w值：确认阈值
  },
  "polling": {
    "gi_interval": 60,             // 总召唤间隔(秒)
    "group_interval": 5,           // 组召唤间隔(秒)
    "groups": [                    // 要召唤的组列表
      {"group": 1, "name": "装置模拟量"},
      {"group": 2, "name": "装置状态量"}
    ]
  },
  "logging": {
    "level": "info",               // 日志级别: debug/info/warning/error
    "file": "xuji103.log"          // 日志文件(可选)
  }
}
```

### 组号说明

组号由子站配置决定，常见配置：

| 组号 | 说明 | 数据类型 |
|-----|------|---------|
| 1 | 装置模拟量 | 浮点数(遥测) |
| 2 | 装置状态量 | 整数(遥脉/状态) |
| 3 | 装置定值 | 混合类型 |
| 4 | 软压板 | 整数 |
| 5 | ASCII字符串 | 字符串 |

---

## 库API文档

### IEC103Master 主类

```cpp
#include <iec103/IEC103Master.h>

using namespace IEC103;
```

#### 配置结构

```cpp
struct Config {
    QString host;                      // 主机地址
    quint16 port = 2404;               // 端口
    int connectTimeout = 30000;        // T0连接超时(ms)
    int reconnectInterval = 5000;      // 重连间隔(ms)
    int timeout = 15000;               // T1超时(ms)
    int ackTimeout = 10000;            // T2超时(ms)
    int testInterval = 20000;          // T3测试间隔(ms)
    int maxUnconfirmed = 12;           // k值
    int maxAckDelay = 8;               // w值
    int maxReconnectCount = 0;         // 最大重连次数，0=无限
};
```

#### 连接管理

```cpp
// 设置配置
void setConfig(const Config& config);

// 连接服务器
void connectToServer();

// 断开连接
void disconnectFromServer();

// 检查连接状态
bool isConnected() const;

// 获取链路状态
LinkState state() const;
```

#### 数据召唤

```cpp
// 总召唤
void generalInterrogation(uint16_t deviceAddr = 0);

// 读通用组
void readGenericGroup(uint16_t deviceAddr, uint8_t group);

// 周期性总召唤
void startPeriodicGI(int intervalMs);
void stopPeriodicGI();
```

#### 信号

```cpp
signals:
    void connected();                              // 连接成功
    void disconnected();                           // 断开连接
    void errorOccurred(const QString& error);      // 发生错误
    void stateChanged(LinkState state);            // 状态变化
    void giFinished(uint16_t deviceAddr);          // 总召唤完成
```

### ILogHandler 日志回调接口

库内置日志系统，支持自定义日志输出：

```cpp
#include <iec103/callback/ILogHandler.h>

class MyLogHandler : public IEC103::ILogHandler {
public:
    void onLog(const IEC103::LogRecord& record) override {
        // 自定义日志处理
        // 可以输出到文件、数据库、UI等
        
        QString levelStr;
        switch (record.level) {
            case IEC103::LogLevel::Debug:   levelStr = "DEBUG"; break;
            case IEC103::LogLevel::Info:    levelStr = "INFO"; break;
            case IEC103::LogLevel::Warning: levelStr = "WARN"; break;
            case IEC103::LogLevel::Error:   levelStr = "ERROR"; break;
        }
        
        // 示例：输出到自定义日志系统
        myLogger.log(QString("[%1] [%2] %3")
                    .arg(record.timestamp)
                    .arg(levelStr)
                    .arg(record.message));
    }
};

// 设置日志回调（全局设置）
IEC103::IEC103Master::setLogHandler(new MyLogHandler());

// 设置日志级别
IEC103::IEC103Master::setLogLevel(IEC103::LogLevel::Debug);
```

**LogRecord 结构：**

```cpp
struct LogRecord {
    LogLevel level;       // 日志级别
    QString message;      // 日志消息
    QString timestamp;    // 时间戳字符串
    QDateTime dateTime;   // 时间戳对象
};
```

### IDataHandler 数据回调接口

```cpp
#include <iec103/callback/IDataHandler.h>

class MyHandler : public IEC103::IDataHandler {
public:
    // 遥信回调 (ASDU1/42)
    void onDoublePoint(const DigitalPoint& point) override {
        qDebug() << "遥信:" << point.deviceAddr 
                 << "FUN=" << point.fun 
                 << "INF=" << point.inf
                 << (point.isOn() ? "合" : "分");
    }

    // 遥测/遥脉回调 (ASDU10)
    void onGenericValue(const GenericPoint& point) override {
        if (point.isFloat()) {
            qDebug() << "遥测:" << point.toFloat();
        } else if (point.isInteger()) {
            qDebug() << "遥脉:" << point.toUInt32();
        }
    }

    // 原始数据回调
    void onGenericData(uint16_t deviceAddr, 
                       const GenericDataItem& item) override {
        // 自定义解析
    }

    // 连接回调
    void onConnected() override {
        qDebug() << "已连接";
    }

    // 断开回调（带原因）
    void onDisconnected(const QString& reason) override {
        qDebug() << "断开:" << reason;
    }

    // 总召唤完成
    void onGICompleted(uint16_t deviceAddr) override {
        qDebug() << "总召唤完成:" << deviceAddr;
    }
};
```

### 数据结构

#### DigitalPoint 遥信数据

```cpp
struct DigitalPoint {
    uint16_t deviceAddr = 0;      // 设备地址
    uint8_t fun = 0;              // 功能类型
    uint8_t inf = 0;              // 信息序号
    DoublePointValue value;       // 值 (Off=分, On=合)
    Quality quality;              // 质量标志
    QDateTime eventTime;          // 事件时间
    QDateTime recvTime;           // 接收时间

    bool isOn() const;            // 是否合
    bool isOff() const;           // 是否分
    bool isValid() const;         // 是否有效
};
```

#### GenericPoint 通用数据

```cpp
struct GenericPoint {
    uint16_t deviceAddr = 0;      // 设备地址
    uint8_t group = 0;            // 组号
    uint8_t entry = 0;            // 条目号
    uint8_t dataType = 0;         // 数据类型
    QVariant value;               // 值
    Quality quality;              // 质量标志
    QString unit;                 // 单位
    QString description;          // 描述

    bool isFloat() const;         // 是否浮点
    bool isInteger() const;       // 是否整数
    float toFloat() const;        // 转浮点
    uint32_t toUInt32() const;    // 转无符号整数
    int32_t toInt32() const;      // 转有符号整数
    QString toString() const;     // 转字符串
};
```

#### Quality 质量标志

```cpp
struct Quality {
    bool invalid = false;      // IV: 无效
    bool notTopical = false;   // NT: 非当前值
    bool substituted = false;  // SB: 被取代
    bool blocked = false;      // BL: 被封锁
    bool overflow = false;     // OV: 溢出（遥测专用）

    bool isGood() const;       // 是否正常
};
```

### 使用示例

#### 完整示例

```cpp
#include <QCoreApplication>
#include <iec103/IEC103Master.h>

class MyHandler : public IEC103::IDataHandler {
public:
    void onDoublePoint(const IEC103::DigitalPoint& point) override {
        qInfo() << "遥信: 设备" << point.deviceAddr
                << "FUN=" << QString::number(point.fun, 16)
                << "INF=" << QString::number(point.inf, 16)
                << (point.isOn() ? "合" : "分");
    }

    void onGenericValue(const IEC103::GenericPoint& point) override {
        QString typeStr = point.isFloat() ? "遥测" : "遥脉";
        qInfo() << typeStr << ": 设备" << point.deviceAddr
                << "组" << point.group << "条目" << point.entry
                << "=" << point.valueString();
    }

    void onDisconnected(const QString& reason) override {
        qWarning() << "断开连接:" << reason;
    }

    void onGICompleted(uint16_t deviceAddr) override {
        qInfo() << "总召唤完成: 设备" << deviceAddr;
    }
};

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    IEC103::IEC103Master master;
    MyHandler handler;

    master.setDataHandler(&handler);

    IEC103::IEC103Master::Config config;
    config.host = "192.168.1.100";
    config.port = 2404;
    config.maxReconnectCount = 0;  // 无限重连
    master.setConfig(config);

    QObject::connect(&master, &IEC103::IEC103Master::connected, [&]() {
        qInfo() << "已连接，开始总召唤";
        master.generalInterrogation(0);
    });

    master.connectToServer();

    return app.exec();
}
```

---

## 控制台程序

### 命令行参数

```bash
./xuji103 [选项]

选项:
  --config <file>       配置文件路径 (默认: config.json)
  --host <ip>           子站IP地址
  --port <port>         子站端口 (默认: 2404)
  --gi-interval <sec>   总召唤间隔(秒)
  --log-level <level>   日志级别: debug/info/warning/error
  --help                显示帮助
```

### 使用示例

```bash
# 使用配置文件
./xuji103 --config config.json

# 命令行指定参数
./xuji103 --host 192.168.1.100 --port 2404 --gi-interval 60

# 调试模式
./xuji103 --log-level debug
```

### 输出示例

```
[2026-03-09 10:30:15.123] [INFO] 连接到 192.168.1.100:2404
[2026-03-09 10:30:15.156] [STATE] Started
[2026-03-09 10:30:15.200] [DIGITAL] Dev=1 FUN=FF INF=10 Value=ON Quality=OK
[2026-03-09 10:30:15.234] [DIGITAL] Dev=1 FUN=FF INF=11 Value=OFF Quality=OK
[2026-03-09 10:30:15.300] [GI] Completed for device 1
[2026-03-09 10:30:16.100] [GENERIC] Dev=1 Group=1 Entry=1 Value=100.50 Type=浮点
[2026-03-09 10:30:17.100] [GENERIC] Dev=1 Group=2 Entry=1 Value=12345 Type=整数
```

---

## 测试子站

Python测试子站用于验证主站功能。

### 启动方式

```bash
cd tests/python
python3 slave.py [选项]
```

### 命令行参数

```
--host <addr>          绑定地址 (默认: 0.0.0.0)
--port <port>          监听端口 (默认: 2404)
--device <addr>        装置地址 (默认: 1)
--log-level <level>    日志级别: debug/info/warning/error
--test-mode <mode>     测试模式
```

### 测试模式

| 模式 | 说明 | 测试目标 |
|-----|------|---------|
| `normal` | 正常模式 | 完整通信流程 |
| `no_ack` | 不发送S帧确认 | k值阻塞超时 |
| `oversized_frame` | 发送超长帧 | 帧长度校验 |
| `no_response` | 不响应命令 | T1命令超时 |
| `flood` | 快速发送大量帧 | 缓冲区限制 |
| `ext_types` | 南网扩展数据类型 | DataType 213-217 |

### 示例

```bash
# 正常模式
python3 slave.py --port 2404

# 测试帧长度校验
python3 slave.py --test-mode oversized_frame

# 测试南网扩展类型
python3 slave.py --test-mode ext_types

# 调试模式
python3 slave.py --log-level debug
```

---

## 开发状态

### 已完成

- [x] 库基础架构
- [x] APCI基础层（帧编解码、序号管理、I/S/U格式）
- [x] ASDU层（三遥解析）
- [x] 通用服务（ASDU10/21编解码）
- [x] 传输层（TCP传输、STARTDT/STOPDT/TESTFR）
- [x] 库API层（IEC103Master主类）
- [x] 控制台程序
- [x] Python测试子站
- [x] 集成测试

### 安全性与稳定性修复

| 优先级 | 问题 | 状态 |
|-------|------|------|
| P0 | 帧长度校验 | ✅ 已修复 |
| P0 | 接收缓冲区限制 | ✅ 已修复 |
| P1 | T0连接超时 | ✅ 已修复 |
| P1 | T1命令超时 | ✅ 已修复 |
| P1 | DataType 213-217解析 | ✅ 已修复 |
| P1 | KOD 67H属性结构 | ✅ 基本支持 |
| P2 | ASDU11通用分类标识 | ✅ 已修复 |
| P2 | 重连次数限制 | ✅ 已修复 |
| P2 | 链路断开通知 | ✅ 已修复 |
| P2 | 发送队列与重发 | ✅ 已修复 |
| P2 | k值阻塞超时告警 | ✅ 已修复 |
| P2 | 配置参数校验 | ✅ 已修复 |

---

## 参考文档

1. **南网规范**：继电保护信息系统主站-子站以太网103通信规范 (Q/CSG)
2. **IEC 60870-5-103**: 继电保护设备信息接口
3. **IEC 60870-5-104**: 网络访问协议（APCI参考）
4. **DL/T 634.5104-2009**: 104标准中文版

---

## 许可证

MIT License

---

*最后更新: 2026-03-09*
