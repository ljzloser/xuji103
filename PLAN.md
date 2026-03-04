# 南网以太网103协议库开发计划

> 创建时间: 2026-03-04
> 最后更新: 2026-03-04

---

## 开发进度总览

| 阶段 | 任务 | 状态 | 完成时间 |
|------|------|------|----------|
| Phase 1 | 项目结构搭建 | ✅ 已完成 | 2026-03-04 |
| Phase 2 | APCI基础层 | ✅ 已完成 | 2026-03-04 |
| Phase 3 | ASDU层 | ✅ 已完成 | 2026-03-04 |
| Phase 4 | 通用服务 | ✅ 已完成 | 2026-03-04 |
| Phase 5 | 传输层 | ✅ 已完成 | 2026-03-04 |
| Phase 6 | 库API层 | ✅ 已完成 | 2026-03-04 |
| Phase 7 | 控制台程序 | ✅ 已完成 | 2026-03-04 |
| Phase 8 | Python测试子站 | ✅ 已完成 | 2026-03-04 |

---

## Phase 1: 项目结构搭建 ✅

### 1.1 创建目录结构
- [x] 创建 `lib/` 目录结构
- [x] 创建 `app/` 目录结构
- [x] 创建 `tests/python/` 目录结构
- [x] 创建 `docs/` 目录

### 1.2 CMake配置
- [x] 根目录 CMakeLists.txt
- [x] lib/CMakeLists.txt (静态库)
- [x] app/CMakeLists.txt (控制台程序)

### 1.3 基础类型定义
- [x] lib/include/iec103/types/Types.h
- [x] lib/include/iec103/types/Constants.h
- [x] lib/include/iec103/types/TimeStamp.h
- [x] lib/include/iec103/types/DataPoint.h
- [x] lib/include/iec103/callback/IDataHandler.h

---

## Phase 2: APCI基础层 ✅

### 2.1 帧结构定义
- [x] lib/include/iec103/apci/APCI.h
- [x] lib/include/iec103/apci/Frame.h

### 2.2 帧编解码
- [x] lib/src/apci/FrameCodec.cpp

### 2.3 序号管理
- [x] lib/include/iec103/apci/SeqManager.h
- [x] lib/src/apci/SeqManager.cpp

---

## Phase 3: ASDU层 ✅

### 3.1 ASDU基础结构
- [x] lib/include/iec103/asdu/ASDU.h
- [x] lib/src/asdu/ASDU.cpp

### 3.2 遥信解析
- [x] lib/include/iec103/asdu/ASDU1.h (单点突发)
- [x] lib/include/iec103/asdu/ASDU42.h (双点总召唤)
- [x] lib/src/asdu/DigitalParser.cpp

### 3.3 总召唤
- [x] lib/include/iec103/asdu/ASDU7_8.h
- [x] lib/src/asdu/GIParser.cpp

---

## Phase 4: 通用服务 ✅

### 4.1 通用服务结构
- [x] lib/include/iec103/asdu/ASDU10_21.h
- [x] lib/src/asdu/GenericService.cpp

### 4.2 数据解析
- [x] lib/src/asdu/DataParser.cpp
- [x] 数据类型213-217解析 (在DataPoint.h中定义)

---

## Phase 5: 传输层 ✅

### 5.1 TCP传输
- [x] lib/include/iec103/transport/TcpTransport.h
- [x] lib/src/transport/TcpTransport.cpp

### 5.2 重连机制
- [x] lib/src/transport/Reconnector.h
- [x] lib/src/transport/Reconnector.cpp

---

## Phase 6: 库API层 ✅

### 6.1 主接口类
- [x] lib/include/iec103/IEC103Master.h
- [x] lib/src/master/IEC103Master.cpp

### 6.2 日志
- [x] lib/src/utils/Logger.h
- [x] lib/src/utils/Logger.cpp

---

## Phase 7: 控制台程序 ✅

### 7.1 主程序
- [x] app/main.cpp

### 7.2 配置解析
- [x] app/Config.h
- [x] app/Config.cpp

### 7.3 数据输出
- [x] app/DataPrinter.h
- [x] app/DataPrinter.cpp

---

## Phase 8: Python测试子站 ✅

### 8.1 TCP服务器
- [x] tests/python/slave.py

### 8.2 协议实现
- [x] tests/python/protocol/apci.py
- [x] tests/python/protocol/asdu.py
- [x] tests/python/protocol/frame.py

### 8.3 模拟数据
- [x] tests/python/data/analog.py
- [x] tests/python/data/digital.py
- [x] tests/python/data/counter.py

### 8.4 依赖
- [x] tests/python/requirements.txt

---

## 构建产物

```
build/
├── lib/iec103.lib          # 静态库
└── bin/xuji103.exe         # 控制台程序
```

---

## 运行方式

### 构建C++项目
```powershell
.\build.bat
```

### 运行Python子站模拟器
```powershell
.\venv\Scripts\python.exe tests\python\slave.py --port 2404
```

### 运行主站程序
```powershell
.\build\bin\xuji103.exe --host 127.0.0.1 --port 2404
```

---

## 状态说明

- ⏳ 待开始
- 🔄 进行中
- ✅ 已完成
- ❌ 已阻塞

---

*本计划文档随开发进度实时更新*