"""
ASDU (Application Service Data Unit) 编解码
南网以太网103规范
"""
from dataclasses import dataclass, field
from typing import List, Optional, Any, Tuple
from datetime import datetime
from enum import IntEnum
import struct


# 类型标识 (TI)
class TI(IntEnum):
    """ASDU类型标识"""
    SINGLE_POINT = 0x01          # 单点遥信
    DOUBLE_POINT_TIME = 0x02     # 带相对时间的双点信息
    GI_CMD = 0x07                # 总召唤命令
    GI_END = 0x08                # 总召唤结束
    GENERIC_DATA = 0x0A          # 通用分类数据
    GENERIC_IDENT = 0x0B         # 通用分类标识 (ASDU11)
    GENERIC_READ = 0x15          # 通用分类读命令 (21)
    DOUBLE_POINT = 0x2A          # 双点信息状态 (42)


# 传输原因 (COT)
class COT(IntEnum):
    """传输原因"""
    SPONTANEOUS = 1              # 自发/突发
    CYCLIC = 2                   # 循环
    RESET_FCB = 3                # 复位帧计数位
    RESET_CU = 4                 # 复位通信单元
    STARTUP = 5                  # 启动/重新启动
    POWER_ON = 6                 # 电源合上
    TEST_MODE = 7                # 测试模式
    TIME_SYNC = 8                # 时间同步
    GI = 9                       # 总召唤
    GI_TERMINATE = 10            # 总召唤终止
    LOCAL_OP = 11                # 当地操作
    REMOTE_OP = 12               # 远方操作
    ACK = 20                     # 肯定认可
    NACK = 21                    # 否定认可
    GENERIC_DATA_RESP = 42       # 通用分类数据响应
    GENERIC_DATA_ERR = 43        # 通用分类数据错误响应


# 功能类型 (FUN)
class FUN(IntEnum):
    """功能类型"""
    GLB = 255                    # 全局功能类型
    GEN = 254                    # 通用分类功能类型


# 信息序号 (INF)
class INF(IntEnum):
    """信息序号"""
    GI_START = 0                 # 总召唤启动/结束
    READ_ALL_GROUP_TITLE = 240   # 读全部组标题
    READ_GROUP_ALL = 241         # 读一组全部条目
    READ_SINGLE_CATALOG = 243    # 读单个条目目录
    READ_SINGLE_VALUE = 244      # 读单个条目值
    GENERIC_GI_END = 245         # 通用分类总召唤中止


# 描述类别 (KOD)
class KOD(IntEnum):
    """描述类别"""
    ACTUAL_VALUE = 0x01          # 实际值
    DESCRIPTION = 0x0A           # 描述
    ATTRIBUTE = 0x67             # 属性结构
    ACTUAL_FROM_DEVICE = 0xA8    # 从装置取实际值 (南网扩展)
    ACTUAL_FROM_DB = 0xA9        # 从子站数据库取实际值 (南网扩展)


# 数据类型 (DataType)
class DataType(IntEnum):
    """通用分类数据类型"""
    OS8ASCII = 1                 # ASCII字符串
    UNSIGNED = 3                 # 无符号整数 (UI)
    SIGNED = 4                   # 有符号整数 (I)
    UF = 5                       # 无符号浮点数
    FLOAT = 6                    # 浮点数 (F)
    R32_23 = 7                   # IEEE 754短实数 (R32.23)
    DPI = 9                      # 双点信息
    FAILURE_CODE = 22            # 失败回答码
    DATA_STRUCTURE = 23          # 数据结构
    # 南网扩展类型
    TIME_ABS_7BYTE = 213         # 带绝对时间的七字节时标报文
    TIME_REL_7BYTE = 214         # 带相对时间的七字节时标报文
    FLOAT_WITH_TIME = 215        # 带相对时间七字节时标的浮点值
    INT_WITH_TIME = 216          # 带相对时间七字节时标的整形值
    CHAR_WITH_TIME = 217         # 带相对时间七字节时标的字符值


@dataclass
class CP56Time2a:
    """7字节时标"""
    millisecond: int = 0
    minute: int = 0
    invalid: bool = False
    hour: int = 0
    summer_time: bool = False
    day: int = 1
    weekday: int = 1
    month: int = 1
    year: int = 0
    
    @staticmethod
    def from_datetime(dt: datetime) -> 'CP56Time2a':
        """从datetime创建"""
        return CP56Time2a(
            millisecond=dt.microsecond // 1000,
            minute=dt.minute,
            invalid=False,
            hour=dt.hour,
            summer_time=False,
            day=dt.day,
            weekday=dt.isoweekday(),
            month=dt.month,
            year=dt.year % 100
        )
    
    @staticmethod
    def now() -> 'CP56Time2a':
        """当前时间"""
        return CP56Time2a.from_datetime(datetime.now())
    
    def encode(self) -> bytes:
        """编码为7字节"""
        data = bytearray(7)
        # 字节1-2: 毫秒
        data[0] = self.millisecond & 0xFF
        data[1] = (self.millisecond >> 8) & 0xFF
        # 字节3: 分钟 + IV
        data[2] = (self.minute & 0x3F) | ((1 if self.invalid else 0) << 7)
        # 字节4: 小时 + SU
        data[3] = (self.hour & 0x1F) | ((1 if self.summer_time else 0) << 5)
        # 字节5: 日 + 星期
        data[4] = (self.day & 0x1F) | ((self.weekday & 0x07) << 5)
        # 字节6: 月
        data[5] = self.month & 0x0F
        # 字节7: 年
        data[6] = self.year & 0xFF
        return bytes(data)
    
    def to_datetime(self) -> datetime:
        """转换为datetime"""
        return datetime(
            year=2000 + self.year if self.year < 100 else self.year,
            month=self.month,
            day=self.day,
            hour=self.hour,
            minute=self.minute,
            second=self.millisecond // 1000,
            microsecond=(self.millisecond % 1000) * 1000
        )


@dataclass
class ASDU:
    """ASDU数据单元"""
    ti: int                      # 类型标识
    vsq: int = 1                 # 可变结构限定词
    cot: int = 1                 # 传输原因 (1字节, 与标准103一致)
    asdu_addr: int = 0           # ASDU地址 (2字节, 南网规范: 高字节=设备地址)
    data: bytes = b''            # 信息体数据
    
    def encode(self) -> bytes:
        """编码ASDU"""
        result = bytearray()
        # TI
        result.append(self.ti)
        # VSQ
        result.append(self.vsq)
        # COT (1字节, 与标准103一致)
        result.append(self.cot & 0xFF)
        # ASDU地址 (2字节, 小端, 南网规范)
        result.extend(struct.pack('<H', self.asdu_addr))
        # 信息体
        result.extend(self.data)
        return bytes(result)
    
    @staticmethod
    def parse(data: bytes) -> 'ASDU':
        """解析ASDU"""
        if len(data) < 5:
            raise ValueError("ASDU数据太短")
        
        ti = data[0]
        vsq = data[1]
        cot = data[2]
        asdu_addr = struct.unpack('<H', data[3:5])[0]
        info_data = data[5:]
        
        return ASDU(ti, vsq, cot, asdu_addr, info_data)


@dataclass
class GenericDataItem:
    """通用分类数据项"""
    gin_group: int = 0           # 组号
    gin_entry: int = 0           # 条目号
    kod: int = KOD.ACTUAL_VALUE  # 描述类别
    data_type: int = DataType.R32_23
    data_size: int = 4
    value: Any = 0.0
    data_count: int = 1          # 数据元素数目
    
    def encode(self) -> bytes:
        """编码数据项"""
        result = bytearray()
        # GIN (2字节)
        result.append(self.gin_group)
        result.append(self.gin_entry)
        # KOD
        result.append(self.kod)
        # GDD (3字节)
        result.append(self.data_type)
        result.append(self.data_size)
        result.append(self.data_count)
        # GID - 根据数据类型编码
        if self.data_type == DataType.R32_23 or self.data_type == DataType.FLOAT:
            # 浮点数 (IEEE 754)
            result.extend(struct.pack('<f', float(self.value)))
        elif self.data_type == DataType.UNSIGNED:
            # 无符号整数
            if self.data_size == 1:
                result.append(int(self.value) & 0xFF)
            elif self.data_size == 2:
                result.extend(struct.pack('<H', int(self.value)))
            elif self.data_size == 4:
                result.extend(struct.pack('<I', int(self.value)))
        elif self.data_type == DataType.SIGNED:
            # 有符号整数
            if self.data_size == 2:
                result.extend(struct.pack('<h', int(self.value)))
            elif self.data_size == 4:
                result.extend(struct.pack('<i', int(self.value)))
        elif self.data_type == DataType.OS8ASCII:
            # ASCII字符串
            if isinstance(self.value, str):
                encoded = self.value.encode('ascii')[:self.data_size]
                result.extend(encoded)
                # 补齐到data_size
                if len(encoded) < self.data_size:
                    result.extend(b'\x00' * (self.data_size - len(encoded)))
            else:
                result.extend(str(self.value).encode('ascii')[:self.data_size])
        elif self.data_type == DataType.DPI:
            # 双点信息 (1字节)
            result.append(int(self.value) & 0x03)
        else:
            # 默认: 按浮点数处理
            result.extend(struct.pack('<f', float(self.value)))
        return bytes(result)


class ASDUParser:
    """ASDU解析器"""
    
    @staticmethod
    def parse_gi_cmd(asdu: ASDU) -> Tuple[int, int, int]:
        """解析总召唤命令 (ASDU7)"""
        # FUN + INF + SCN
        fun = asdu.data[0]
        inf = asdu.data[1]
        scn = asdu.data[2] if len(asdu.data) > 2 else 0
        return fun, inf, scn
    
    @staticmethod
    def parse_generic_read(asdu: ASDU) -> Tuple[int, int, int, int, int, int]:
        """解析通用分类读命令 (ASDU21)"""
        # FUN + INF + RII + NGD + GIN(组/条目) + KOD
        fun = asdu.data[0]
        inf = asdu.data[1]
        rii = asdu.data[2]
        ngd = asdu.data[3]  # NGD (包含CONT位)
        gin_group = asdu.data[4]
        gin_entry = asdu.data[5]
        kod = asdu.data[6]
        return fun, inf, rii, gin_group, gin_entry, kod


class ASDUBuilder:
    """ASDU构建器"""
    
    @staticmethod
    def build_asdu_addr(device_addr: int, cpu_no: int = 0, setting_zone: int = 0) -> int:
        """
        构建南网规范ASDU地址 (2字节)
        高字节 = 设备地址 (1-254)
        低字节: D7-D3 = 定值区号, D2-D0 = CPU号
        """
        low_byte = ((setting_zone & 0x1F) << 3) | (cpu_no & 0x07)
        return (device_addr << 8) | low_byte
    
    @staticmethod
    def build_gi_cmd(asdu_addr: int, scn: int = 1) -> ASDU:
        """
        构建总召唤命令 (ASDU7)
        asdu_addr: 完整的2字节ASDU地址
        """
        data = bytes([
            FUN.GLB,    # FUN = 255
            INF.GI_START,  # INF = 0
            scn         # SCN = 扫描序号
        ])
        return ASDU(TI.GI_CMD, 1, COT.GI, asdu_addr, data)
    
    @staticmethod
    def build_gi_cmd(device_addr: int, scn: int = 1) -> ASDU:
        """构建总召唤命令 (ASDU7)"""
        asdu_addr = ASDUBuilder.build_asdu_addr(device_addr)
        data = bytes([
            FUN.GLB,    # FUN
            INF.GI_START,  # INF
            scn         # SCN
        ])
        return ASDU(TI.GI_CMD, 1, COT.GI, asdu_addr, data)
    
    @staticmethod
    def build_gi_end(device_addr: int, scn: int = 0) -> ASDU:
        """构建总召唤结束 (ASDU8)"""
        asdu_addr = ASDUBuilder.build_asdu_addr(device_addr)
        data = bytes([
            FUN.GLB,    # FUN
            INF.GI_START,  # INF
            scn         # SCN
        ])
        return ASDU(TI.GI_END, 1, COT.GI_TERMINATE, asdu_addr, data)
    
    @staticmethod
    def build_read_group(asdu_addr: int, group: int, kod: int = KOD.ACTUAL_VALUE) -> ASDU:
        """构建读一组全部条目命令 (ASDU21 INF=241)"""
        data = bytearray()
        data.append(FUN.GEN)  # FUN
        data.append(INF.READ_GROUP_ALL)  # INF = 241
        data.append(1)  # RII
        data.append(1)  # NGD
        data.append(group)  # GIN group
        data.append(0xFF)  # GIN entry (0xFF表示组标题)
        data.append(kod)  # KOD
        return ASDU(TI.GENERIC_READ, 1, COT.GENERIC_DATA_RESP, asdu_addr, bytes(data))
    
    @staticmethod
    def build_double_point(device_addr: int, points: List[Tuple[int, int, int]], 
                           cot: int = COT.GI, scn: int = 0) -> ASDU:
        """
        构建双点遥信 (ASDU42)
        points: [(fun, inf, dpi), ...]
        """
        asdu_addr = ASDUBuilder.build_asdu_addr(device_addr)
        data = bytearray()
        for fun, inf, dpi in points:
            data.append(fun)
            data.append(inf)
            data.append(dpi & 0x03)  # DPI: 00/01/10/11
        data.append(scn)  # SIN = SCN
        return ASDU(TI.DOUBLE_POINT, len(points), cot, asdu_addr, bytes(data))
    
    @staticmethod
    def build_generic_data(asdu_addr: int, rii: int, items: List[GenericDataItem],
                           cot: int = COT.GENERIC_DATA_RESP, cont: bool = False,
                           fun: int = FUN.GEN, inf: int = INF.READ_GROUP_ALL) -> ASDU:
        """
        构建通用分类数据 (ASDU10)
        asdu_addr: 完整的2字节ASDU地址 (高字节=设备地址)
        """
        data = bytearray()
        # FUN + INF (南网规范)
        data.append(fun)
        data.append(inf)
        # RII
        data.append(rii)
        # NGD (CONT位 + 数目)
        ngd = (0x80 if cont else 0x00) | (len(items) & 0x7F)
        data.append(ngd)
        # 数据项
        for item in items:
            data.extend(item.encode())
        
        return ASDU(TI.GENERIC_DATA, 1, cot, asdu_addr, bytes(data))
    
    @staticmethod
    def build_single_point(device_addr: int, fun: int, inf: int, dpi: int,
                           time1: CP56Time2a = None, time2: CP56Time2a = None,
                           cot: int = COT.SPONTANEOUS) -> ASDU:
        """
        构建单点遥信 (ASDU1) - 南网扩展带双时标
        """
        asdu_addr = ASDUBuilder.build_asdu_addr(device_addr)
        data = bytearray()
        data.append(fun)
        data.append(inf)
        data.append(dpi & 0x03)
        # 时标1 (实际发生时间)
        if time1:
            data.extend(time1.encode())
        else:
            data.extend(CP56Time2a.now().encode())
        # 时标2 (子站接收时间) - 南网扩展
        if time2:
            data.extend(time2.encode())
        else:
            data.extend(CP56Time2a.now().encode())
        # SIN
        data.append(0)
        
        return ASDU(TI.SINGLE_POINT, 1, cot, asdu_addr, bytes(data))
    
    @staticmethod
    def build_catalog_data(asdu_addr: int, rii: int, gin_group: int, gin_entry: int,
                           entries: List[Tuple[int, int, int, bytes]],
                           cot: int = COT.GENERIC_DATA_RESP,
                           fun: int = FUN.GEN, inf: int = INF.READ_SINGLE_CATALOG) -> ASDU:
        """
        构建通用分类标识 (ASDU11) - 读单个条目目录响应
        
        entries: [(kod, data_type, data_size, gid), ...]
        """
        data = bytearray()
        # FUN + INF (南网规范)
        data.append(fun)
        data.append(inf)
        # RII
        data.append(rii)
        # NGD (CONT位 + 数目)
        ngd = len(entries) & 0x7F
        data.append(ngd)
        # GIN (请求的GIN)
        data.append(gin_group)
        data.append(gin_entry)
        # 数据集 (目录条目)
        for kod, data_type, data_size, gid in entries:
            data.append(kod)  # KOD
            data.append(data_type)  # GDD.DataType
            data.append(data_size)  # GDD.DataSize
            data.append(1)  # GDD.Number
            data.extend(gid)  # GID
        
        return ASDU(TI.GENERIC_IDENT, 1, cot, asdu_addr, bytes(data))
    
    @staticmethod
    def build_generic_data_with_time(asdu_addr: int, rii: int, 
                                      gin_group: int, gin_entry: int,
                                      data_type: int, value: Any,
                                      relative_time: int = 0,
                                      fault_num: int = 0,
                                      cot: int = COT.GENERIC_DATA_RESP) -> ASDU:
        """
        构建带时标的通用分类数据 (南网扩展数据类型 215/216/217)
        
        数据格式 (参考南网规范 图表29):
        - DataType 215: VAL(4) + RET(2) + NOF(2) + TIME(7) + RECV_TIME(7) = 22字节
        - DataType 216: VAL(4,FPT+JPT) + RET(2) + NOF(2) + TIME(7) + RECV_TIME(7) = 22字节
        - DataType 217: VAL(40) + RET(2) + NOF(2) + TIME(7) + RECV_TIME(7) = 58字节
        
        asdu_addr: 完整的2字节ASDU地址
        """
        data = bytearray()
        # FUN + INF
        data.append(FUN.GEN)
        data.append(INF.READ_GROUP_ALL)
        # RII
        data.append(rii)
        # NGD
        data.append(1)  # 1个条目
        # GIN
        data.append(gin_group)
        data.append(gin_entry)
        # KOD
        data.append(KOD.ACTUAL_VALUE)
        # GDD
        data.append(data_type)
        
        # 计算数据大小
        if data_type == DataType.FLOAT_WITH_TIME:  # 215
            # VAL(4) + RET(2) + NOF(2) + TIME(7) + RECV_TIME(7) = 22字节
            data_size = 22
        elif data_type == DataType.INT_WITH_TIME:  # 216
            # VAL(4) + RET(2) + NOF(2) + TIME(7) + RECV_TIME(7) = 22字节
            data_size = 22
        elif data_type == DataType.CHAR_WITH_TIME:  # 217
            # VAL(40) + RET(2) + NOF(2) + TIME(7) + RECV_TIME(7) = 58字节
            data_size = 58
        else:
            data_size = 22
        
        data.append(data_size)  # DataSize
        data.append(1)  # Number
        
        # GID - 构建数据
        # 实际数据值 (偏移0-3或0-39)
        if data_type == DataType.FLOAT_WITH_TIME:
            data.extend(struct.pack('<f', float(value)))
        elif data_type == DataType.INT_WITH_TIME:
            # 低字节=FPT故障相别, 高字节=JPT跳闸相别
            data.extend(struct.pack('<I', int(value)))
        elif data_type == DataType.CHAR_WITH_TIME:
            # 40字节ASCII字符串
            char_data = str(value).encode('ascii')[:40]
            data.extend(char_data)
            data.extend(b'\x00' * (40 - len(char_data)))
        
        # 相对时间 RET (2字节) - 偏移4-5
        data.extend(struct.pack('<H', relative_time))
        # 故障序号 NOF (2字节) - 偏移6-7
        data.extend(struct.pack('<H', fault_num))
        # 故障时间 CP56Time2a (7字节) - 偏移8-14
        data.extend(CP56Time2a.now().encode())
        # 子站接收时间 CP56Time2a (7字节) - 偏移15-21
        data.extend(CP56Time2a.now().encode())
        
        return ASDU(TI.GENERIC_DATA, 1, cot, asdu_addr, bytes(data))