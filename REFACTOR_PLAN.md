# 代码重构计划

> 基于 AGENTS.md 的修改，遥测和遥脉合并为通用服务

---

## 一、修改概述

### 1.1 核心变更

| 变更项 | 原设计 | 新设计 |
|--------|--------|--------|
| 数据类型 | 遥测(AnalogPoint) + 遥脉(CounterPoint) 分开 | 统一为通用服务数据(GenericPoint) |
| 组号来源 | 代码中硬编码常量 | 从配置文件读取 |
| 数据类型判断 | 根据组号推断 | 根据GDD.DataType动态判断 |
| 回调接口 | onAnalogValue + onIntegratedTotal | 统一为 onGenericValue |
| 配置格式 | analog_groups + counter_groups 分开 | 统一为 groups 数组 |

### 1.2 影响范围

```
lib/include/iec103/types/
├── DataPoint.h      # 合并AnalogPoint和CounterPoint
├── Constants.h      # 移除固定组号常量

lib/include/iec103/callback/
└── IDataHandler.h   # 统一回调接口

lib/src/master/
└── IEC103Master.cpp # 修改解析逻辑

app/
├── Config.h         # 修改配置结构
├── Config.cpp       # 修改配置解析
├── DataPrinter.h    # 修改回调声明
└── DataPrinter.cpp  # 修改回调实现

tests/python/data/
├── analog.py        # 删除
├── counter.py       # 删除
└── generic.py       # 新建，合并遥测和遥脉

tests/python/
└── slave.py         # 修改数据结构
```

---

## 二、修改任务列表

### Phase 1: 数据结构修改

#### 1.1 DataPoint.h
- [x] 合并 `AnalogPoint` 和 `CounterPoint` 为 `GenericPoint`
- [x] 添加 `dataType` 字段用于存储GDD.DataType
- [x] 使用 `QVariant` 存储值，支持多种数据类型

```cpp
// 新结构
struct GenericPoint {
    uint16_t deviceAddr = 0;
    uint8_t group = 0;
    uint8_t entry = 0;
    uint8_t dataType = 0;     // GDD.DataType: 7=浮点, 3=整数
    QVariant value;           // 根据dataType解析
    Quality quality;
    QString unit;
    QString description;
    QDateTime timestamp;

    // 便捷方法
    float toFloat() const;
    uint32_t toUInt32() const;
    bool isFloat() const { return dataType == 7; }
    bool isInteger() const { return dataType == 3; }
};
```

#### 1.2 Constants.h
- [x] 移除 `GROUP_ANALOG` 和 `GROUP_COUNTER` 固定常量
- [x] 保留其他必要的常量

```cpp
// 移除以下常量：
// constexpr uint8_t GROUP_ANALOG = 0x08;
// constexpr uint8_t GROUP_COUNTER = 0x10;

// 添加注释说明：
// 组号由配置文件决定，不再硬编码
```

---

### Phase 2: 回调接口修改

#### 2.1 IDataHandler.h
- [x] 移除 `onAnalogValue` 和 `onIntegratedTotal`
- [x] 添加 `onGenericValue` 统一回调

```cpp
class IDataHandler {
public:
    virtual ~IDataHandler() = default;

    // 遥信（双点）- ASDU1/ASDU42
    virtual void onDoublePoint(quint16 deviceAddr, quint16 infoAddr,
                               DoublePointValue value, const Quality& q,
                               const QDateTime& time) {}

    // 通用服务数据 - ASDU10（遥测/遥脉统一）
    virtual void onGenericValue(quint16 deviceAddr, quint8 group, quint8 entry,
                                quint8 dataType, const QVariant& value,
                                const Quality& q, const QString& unit) {}

    // 通用服务数据（原始格式）
    virtual void onGenericData(quint16 deviceAddr, const GenericDataItem& item) {}

    // 链路状态
    virtual void onLinkStateChanged(LinkState state) {}

    // 总召唤完成
    virtual void onGICompleted(quint16 deviceAddr) {}
};
```

---

### Phase 3: ���置修改

#### 3.1 Config.h
- [x] 移除 `m_analogGroups` 和 `m_counterGroups`
- [x] 添加 `GroupConfig` 结构和 `m_groups` 列表
- [x] 添加 `loadFromFile()` 和 `loadFromJson()` 方法

```cpp
// 组配置结构
struct GroupConfig {
    int group;          // 组号
    QString name;       // 组名称
    int dataType;       // 数据类型提示 (7=浮点, 3=整数)
};

class Config {
public:
    // 移除：
    // QList<int> analogGroups() const;
    // QList<int> counterGroups() const;

    // 新增：
    QList<GroupConfig> groups() const { return m_groups; }

private:
    QList<GroupConfig> m_groups;  // 替代 analogGroups 和 counterGroups
};
```

#### 3.2 Config.cpp
- [x] 实现JSON配置文件解析
- [x] 读取 `groups` 数组
- [x] 支持从 `config.json` 文件加载所有配置

---

### Phase 4: 业务逻辑修改

#### 4.1 DataPrinter.h/cpp
- [ ] 移除 `onAnalogValue` 和 `onCounterValue`
- [ ] 添加 `onGenericValue` 实现

```cpp
void DataPrinter::onGenericValue(quint16 deviceAddr, quint8 group, quint8 entry,
                                  quint8 dataType, const QVariant& value,
                                  const Quality& q, const QString& unit) {
    QString typeStr = (dataType == 7) ? "浮点" : "整数";
    QString valueStr = (dataType == 7) ? 
        QString::number(value.toFloat(), 'f', 2) : 
        QString::number(value.toUInt());
    
    qInfo() << "[GENERIC]" << "Dev=" << deviceAddr 
            << "Group=" << group << "Entry=" << entry
            << "Value=" << valueStr << unit
            << "Type=" << typeStr;
}
```

#### 4.2 IEC103Master.cpp
- [ ] 修改通用服务数据解析，根据GDD.DataType填充GenericPoint
- [ ] 调用 `onGenericValue` 而非分别调用 `onAnalogValue`/`onCounterValue`

```cpp
// 解析通用服务数据
GenericPoint point;
point.deviceAddr = deviceAddr;
point.group = item.gin.group;
point.entry = item.gin.entry;
point.dataType = item.gdd.dataType;  // 关键：从GDD获取

// 根据dataType解析值
if (item.gdd.dataType == 7) {  // 浮点数
    point.value = item.toFloat();
} else if (item.gdd.dataType == 3) {  // 无符号整数
    point.value = item.toUInt32();
}

// 统一回调
if (m_handler) {
    m_handler->onGenericValue(point.deviceAddr, point.group, point.entry,
                               point.dataType, point.value, point.quality, point.unit);
}
```

#### 4.3 main.cpp
- [ ] 修改召唤逻辑，遍历 `Config::instance().groups()`

```cpp
void readGroups(uint16_t deviceAddr) {
    for (const auto& groupConfig : Config::instance().groups()) {
        qInfo() << "Reading group" << groupConfig.group << groupConfig.name;
        master->readGenericGroup(deviceAddr, groupConfig.group);
    }
}
```

---

### Phase 5: Python测试子站修改

#### 5.1 合并数据文件
- [ ] 删除 `tests/python/data/analog.py`
- [ ] 删除 `tests/python/data/counter.py`
- [ ] 创建 `tests/python/data/generic.py`

```python
# generic.py
from dataclasses import dataclass
from typing import List, Union

@dataclass
class GenericPoint:
    group: int
    entry: int
    name: str
    value: Union[float, int]
    unit: str
    data_type: int  # 7=浮点, 3=整数

class GenericDataGenerator:
    def __init__(self):
        self.groups = {}  # {group: [GenericPoint, ...]}
    
    def get_group_points(self, group: int) -> List[GenericPoint]:
        return self.groups.get(group, [])
```

#### 5.2 修改 slave.py
- [ ] 使用新的 `GenericDataGenerator`
- [ ] 修改配置文件读取，支持 `groups` 格式

#### 5.3 修改 __init__.py
- [ ] 更新导出

```python
# data/__init__.py
from .digital import DigitalDataGenerator
from .generic import GenericDataGenerator

__all__ = ['DigitalDataGenerator', 'GenericDataGenerator']
```

---

### Phase 6: 配置文件格式

#### 6.1 主站配置文件 (config.json)

```json
{
  "connection": {
    "host": "192.168.1.100",
    "port": 2404,
    "timeout": 15000,
    "reconnect_interval": 5000
  },
  "polling": {
    "gi_interval": 60000,
    "groups": [
      {"group": 8, "name": "电流电压", "data_type": 7},
      {"group": 9, "name": "功率频率", "data_type": 7},
      {"group": 16, "name": "电能量", "data_type": 3},
      {"group": 17, "name": "运行时间", "data_type": 3}
    ]
  },
  "logging": {
    "level": "info",
    "file": "xuji103.log"
  }
}
```

#### 6.2 Python子站模拟数据 (data/sample.json)

```json
{
  "devices": [
    {
      "address": 1,
      "name": "测试装置1",
      "digitals": [
        {"fun": 200, "inf": 1, "name": "开关1", "value": "on"},
        {"fun": 200, "inf": 2, "name": "开关2", "value": "off"}
      ],
      "groups": [
        {
          "group": 8,
          "name": "电流电压",
          "entries": [
            {"entry": 1, "name": "电流A相", "value": 100.5, "unit": "A", "data_type": 7},
            {"entry": 2, "name": "电压A相", "value": 220.3, "unit": "V", "data_type": 7}
          ]
        },
        {
          "group": 16,
          "name": "电能量",
          "entries": [
            {"entry": 1, "name": "正向有功电能", "value": 12345, "unit": "kWh", "data_type": 3}
          ]
        }
      ]
    }
  ]
}
```

---

## 三、执行顺序

```
Phase 1 (数据结构)
    │
    ├── 1.1 DataPoint.h
    └── 1.2 Constants.h
          │
          ▼
Phase 2 (回调接口)
    │
    └── 2.1 IDataHandler.h
          │
          ▼
Phase 3 (配置)
    │
    ├── 3.1 Config.h
    └── 3.2 Config.cpp
          │
          ▼
Phase 4 (业务逻辑)
    │
    ├── 4.1 DataPrinter.h/cpp
    ├── 4.2 IEC103Master.cpp
    └── 4.3 main.cpp
          │
          ▼
Phase 5 (Python测试)
    │
    ├── 5.1 创建 generic.py
    ├── 5.2 修改 slave.py
    ├── 5.3 修改 __init__.py
    └── 删除 analog.py, counter.py
          │
          ▼
Phase 6 (测试验证)
    │
    └── 编译运行，验证功能正确
```

---

## 四、风险点

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| QVariant性能 | 轻微影响 | 数值类型场景影响极小 |
| 回调接口变更 | 现有代码需修改 | DataPrinter已实现，修改量小 |
| 配置格式不兼容 | 旧配置文件需更新 | 提供迁移脚本或手动更新 |

---

## 五、k/w 流量控制实现

> 基于 IEC 104 标准的滑动窗口机制，防止报文丢失和重复

### 5.1 实现概述

| 功能 | 当前状态 | 目标 |
|-----|---------|------|
| k值发送控制 | ❌ 未实现 | 发送前检查未确认帧数，≥k时暂停 |
| w值确认触发 | ❌ 未实现 | 收到w个I帧后立即发S帧确认 |
| T2超时确认 | ✅ 已实现 | 保持不变，作为兜底机制 |

### 5.2 修改任务列表

#### Phase 1: SeqManager 增强
- [x] 添加 `m_recvCount` 成员变量（接收未确认计数）
- [x] 添加 `recvCount()` 方法
- [x] 添加 `resetRecvCount()` 方法
- [x] 添加 `incrementRecvCount()` 方法

#### Phase 2: IEC103Master 头文件
- [x] 在 Config 结构体中确认 k/w 参数已定义
- [x] 添加 `m_recvCount` 成员变量（用于w计数）

#### Phase 3: IEC103Master 实现
- [x] 实现 k 值发送控制：`sendIFrame()` 前检查未确认数
- [x] 实现 w 值确认触发：`processIFrame()` 中计数并判断
- [x] 重置接收计数：连接断开时、发送确认后

#### Phase 4: 序号错误处理
- [x] 序号验证失败时断开连接重连（符合 IEC 104 规范）
- [x] 打印错误日志：`[I-RX] SEQUENCE ERROR: N(S)=x, expected=y`

#### Phase 5: Python 测试子站
- [x] 移除发送I帧后立即发S帧的错误逻辑
- [x] 添加接收I帧的w值计数确认
- [x] 添加发送方序号管理（等待主站S帧确认）
- [x] 添加k值配置参数支持测试

### 5.4 Python 子站修改详情

#### 5.4.1 当前问题

```python
# 当前错误实现 (send_response)
for frame in response:
    writer.write(frame)
    await writer.drain()
    # 错误：每发一帧就发S帧确认
    ack = FrameCodec.encode_s_format(self.recv_seq)
    writer.write(ack)  # ❌ 不符合协议
```

**问题**：
1. S帧是接收方发送的确认，发送方不应发S帧
2. 子站作为接收方时，未实现w值计数确认

#### 5.4.2 修改后的正确逻辑

```python
class IEC103Slave:
    def __init__(self, ...):
        # 新增：接收计数（用于w值控制）
        self.recv_count = 0
        # 新增：k/w配置
        self.max_unconfirmed = 12  # k值
        self.max_ack_delay = 8     # w值

    async def handle_frame(self, frame, writer):
        if frame.is_i_format:
            self.recv_seq = frame.send_seq + 1
            self.recv_count += 1  # 递增接收计数

            # w值控制：收到w个I帧后立即发S帧确认
            if self.recv_count >= self.max_ack_delay:
                ack = FrameCodec.encode_s_format(self.recv_seq)
                writer.write(ack)
                await writer.drain()
                self.recv_count = 0

            await self.handle_i_format(frame, writer)

    async def send_response(self, response, writer):
        """发送响应（不发送S帧确认）"""
        if isinstance(response, list):
            for frame in response:
                writer.write(frame)
                await writer.drain()
                # 不再发送S帧，等待主站确认
        else:
            writer.write(response)
            await writer.drain()
```

#### 5.4.3 执行顺序

```
1. 添加 recv_count 成员变量和 k/w 配置
       │
       ▼
2. 修改 handle_frame 添加 w 值确认触发
       │
       ▼
3. 修改 send_response 移除错误的S帧发送
       │
       ▼
4. 重置 recv_count（断开连接时、发送确认后）
```

### 5.3 代码修改详情

#### 5.3.1 SeqManager.h 修改

```cpp
class SeqManager {
public:
    // ... 现有方法 ...

    // 新增：接收计数管理（用于w值控制）
    uint8_t recvCount() const;
    void incrementRecvCount();
    void resetRecvCount();

private:
    // ... 现有成员 ...
    uint8_t m_recvCount = 0;  // 接收未确认计数
};
```

#### 5.3.2 IEC103Master.h 修改

```cpp
struct Config {
    // ... 现有配置 ...
    int maxUnconfirmed = 12;       // k值: 发送方暂停阈值
    int maxAckDelay = 8;           // w值: 接收方立即确认阈值
};
```

#### 5.3.3 IEC103Master.cpp 修改

**k值发送控制**:
```cpp
void IEC103Master::sendIFrame(const QByteArray& asdu) {
    // k值控制
    if (m_seqManager.unconfirmedCount() >= m_config.maxUnconfirmed) {
        Logger::warning(QString("Send blocked: unconfirmed=%1 >= k=%2")
                        .arg(m_seqManager.unconfirmedCount())
                        .arg(m_config.maxUnconfirmed));
        return false;  // 或抛出信号等待确认
    }
    // ... 正常发送逻辑 ...
}
```

**w值确认触发**:
```cpp
void IEC103Master::processIFrame(const Frame& frame) {
    // ... 序号验证 ...
    m_seqManager.nextRecvSeq();
    m_seqManager.incrementRecvCount();

    // w值控制：收到w个后立即确认
    if (m_seqManager.recvCount() >= m_config.maxAckDelay) {
        sendAck();
        m_seqManager.resetRecvCount();
        stopAckTimer();
    } else {
        startAckTimer();  // T2兜底
    }

    // ... ASDU处理 ...
}
```

### 5.4 执行顺序

```
Phase 1: SeqManager 增强
    │
    ├── SeqManager.h 添加方法声明
    └── SeqManager.cpp 添加方法实现
          │
          ▼
Phase 2: IEC103Master 修改
    │
    ├── IEC103Master.h 确认配置
    ├── IEC103Master.cpp k值控制
    └── IEC103Master.cpp w值控制
          │
          ▼
Phase 3: 测试验证
    │
    └── 编译运行，验证流量控制正确
```

---

## 六、协议合规性修复

> 基于 IEC 104 标准的错误处理规则

### 6.1 问题清单

| # | 问题 | 当前行为 | 正确行为 | 严重程度 |
|---|------|---------|---------|---------|
| 1 | **I帧接收序号验证** | ✅ 已修复 - 断开连接 | 断开连接 | 🔴 高 |
| 2 | **S帧确认序号验证** | ❌ 不验证，直接接受 | N(R) 不能大于发送序号，否则断开 | 🔴 高 |
| 3 | **TESTFR超时处理** | ❌ 发送后继续等待 | 超时未收到确认应断开 | 🟡 中 |
| 4 | **T1超时处理** | ❌ 仅配置参数 | 发送命令后T1超时应断开 | 🟡 中 |
| 5 | **k值阻塞处理** | ✅ 已实现 - 打印警告 | 暂停发送或等待确认 | 🟢 低 |

### 6.2 修改任务列表

#### Phase 1: S帧确认序号验证
- [x] SeqManager 添加 `validateAckSeq()` 方法（已存在，需验证）
- [x] `processSFrame()` 添加序号验证
- [x] N(R) > sendSeq 时断开连接

```cpp
void IEC103Master::processSFrame(const Frame& frame) {
    uint16_t ackSeq = frame.recvSeq();
    
    // 验证：N(R) 不能大于已发送序号
    if (ackSeq > m_seqManager.sendSeq()) {
        Logger::error(QString("[S-RX] SEQUENCE ERROR: N(R)=%1 > sendSeq=%2 - Disconnecting!")
                      .arg(ackSeq).arg(m_seqManager.sendSeq()));
        disconnectFromServer();
        return;
    }
    
    // 正常处理...
}
```

#### Phase 2: TESTFR超时处理
- [x] 添加 `m_testFrPending` 标志
- [x] 发送 TESTFR_ACT 后设置标志
- [x] 收到 TESTFR_CON 后清除标志
- [x] T1超时时检查标志，若未确认则断开

```cpp
void IEC103Master::onTestTimeout() {
    if (m_testFrPending) {
        // 上次TESTFR未确认，断开连接
        Logger::error("[TESTFR] No confirmation received - Disconnecting!");
        disconnectFromServer();
        return;
    }
    
    Logger::debug("Sending TESTFR");
    Frame testFr = FrameCodec::createUFrame(UControl::TestFrAct);
    sendFrame(testFr);
    m_testFrPending = true;
    startTestTimer();  // T1超时
}
```

#### Phase 3: T1超时处理
- [x] 添加命令超时检测机制（可选，TCP层已处理）
- [x] 发送命令后启动T1定时器
- [x] 收到响应后停止T1定时器
- [x] T1超时时断开连接（可选）

#### Phase 4: Python子站对应修改
- [x] 添加S帧序号验证
- [x] 添加I帧接收序号验证
- [x] 添加断开重连逻辑

### 6.3 执行顺序

```
Phase 1 (S帧验证)
    │
    ├── 修改 processSFrame() 添加验证
    └── 测试验证
          │
          ▼
Phase 2 (TESTFR超时)
    │
    ├── 添加 m_testFrPending 标志
    ├── 修改 onTestTimeout()
    └── 修改 processUFrame() 清除标志
          │
          ▼
Phase 3 (T1超时)
    │
    ├── 添加命令超时检测
    └── 实现超时断开
          │
          ▼
Phase 4 (Python子站)
    │
    └── 对应修改
```

---

## 七、验收标准

- [ ] 编译通过，无错误
- [ ] 主站能正确读取配置文件中的组列表
- [ ] 通用服务响应能正确解析并回调
- [ ] 数据类型正确识别（浮点/整数）
- [ ] Python子站能正确响应新的配置格式
- [ ] 主站+Python子站联调通过

---

*创建日期: 2026-03-06*
