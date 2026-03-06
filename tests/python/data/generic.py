"""
通用服务数据生成器 (遥测/遥脉统一)
数据类型由 data_type 决定: 7=浮点数(遥测), 3=无符号整数(遥脉)

组号说明（根据南网规范，组号由保信子站配置决定）:
- 组1: 装置模拟量（遥测数据，浮点数）
- 组2: 装置状态量（状态信息，整数）
- 组3: 装置定值（定值数据，混合类型）
- 组4: 软压板（布尔状态，整数）
"""
import random
from typing import List, Dict, Any, Union
from dataclasses import dataclass


@dataclass
class GenericPoint:
    """通用服务数据点"""
    group: int              # 组号
    entry: int              # 条目号
    name: str               # 名称
    value: Union[float, int]  # 当前值
    unit: str               # 单位
    data_type: int          # 数据类型: 7=浮点, 3=整数
    min_val: float = 0.0    # 最小值 (遥测用)
    max_val: float = 0.0    # 最大值 (遥测用)
    noise: float = 0.0      # 噪声幅度 (遥测用)
    increment: int = 0      # 每次增量 (遥脉用)


class GenericDataGenerator:
    """通用服务数据生成器 (遥测/遥脉统一)"""
    
    # 数据类型常量
    DATA_TYPE_ASCII = 1     # OS8ASCII字符串
    DATA_TYPE_UINT = 3      # 无符号整数 (遥脉/状态)
    DATA_TYPE_INT = 4       # 有符号整数
    DATA_TYPE_FLOAT = 7     # R32.23浮点数 (遥测)
    DATA_TYPE_DPI = 9       # 双点信息
    
    def __init__(self):
        self.points: List[GenericPoint] = []
        self._init_default_points()
    
    def _init_default_points(self):
        """初始化默认数据点"""
        # ========== 组1: 装置模拟量 (浮点数) ==========
        analog_points = [
            GenericPoint(1, 1, "电流A相", 100.0, "A", self.DATA_TYPE_FLOAT, 0, 1000, 2.0),
            GenericPoint(1, 2, "电流B相", 100.0, "A", self.DATA_TYPE_FLOAT, 0, 1000, 2.0),
            GenericPoint(1, 3, "电流C相", 100.0, "A", self.DATA_TYPE_FLOAT, 0, 1000, 2.0),
            GenericPoint(1, 4, "电压A相", 220.0, "V", self.DATA_TYPE_FLOAT, 0, 500, 1.0),
            GenericPoint(1, 5, "电压B相", 220.0, "V", self.DATA_TYPE_FLOAT, 0, 500, 1.0),
            GenericPoint(1, 6, "电压C相", 220.0, "V", self.DATA_TYPE_FLOAT, 0, 500, 1.0),
            GenericPoint(1, 7, "有功功率", 50.0, "MW", self.DATA_TYPE_FLOAT, 0, 100, 0.5),
            GenericPoint(1, 8, "无功功率", 20.0, "MVar", self.DATA_TYPE_FLOAT, 0, 50, 0.2),
            GenericPoint(1, 9, "频率", 50.0, "Hz", self.DATA_TYPE_FLOAT, 49.5, 50.5, 0.01),
        ]
        self.points.extend(analog_points)
        
        # ========== 组2: 装置状态量 (整数) ==========
        status_points = [
            GenericPoint(2, 1, "正向有功电能", 12345, "kWh", self.DATA_TYPE_UINT, increment=10),
            GenericPoint(2, 2, "反向有功电能", 100, "kWh", self.DATA_TYPE_UINT, increment=1),
            GenericPoint(2, 3, "正向无功电能", 5000, "kVarh", self.DATA_TYPE_UINT, increment=5),
            GenericPoint(2, 4, "反向无功电能", 50, "kVarh", self.DATA_TYPE_UINT, increment=1),
            GenericPoint(2, 5, "运行时间", 10000, "h", self.DATA_TYPE_UINT, increment=1),
            GenericPoint(2, 6, "动作次数", 50, "次", self.DATA_TYPE_UINT, increment=0),
        ]
        self.points.extend(status_points)
        
        # ========== 组3: 装置定值 (混合类型) ==========
        setting_points = [
            GenericPoint(3, 1, "过流一段定值", 5.0, "A", self.DATA_TYPE_FLOAT, 0, 100, 0.1),
            GenericPoint(3, 2, "过流二段定值", 3.0, "A", self.DATA_TYPE_FLOAT, 0, 100, 0.1),
            GenericPoint(3, 3, "过流三段定值", 1.5, "A", self.DATA_TYPE_FLOAT, 0, 100, 0.1),
            GenericPoint(3, 4, "一段延时", 0.5, "s", self.DATA_TYPE_FLOAT, 0, 10, 0.01),
            GenericPoint(3, 5, "二段延时", 1.0, "s", self.DATA_TYPE_FLOAT, 0, 10, 0.01),
            GenericPoint(3, 6, "三段延时", 2.0, "s", self.DATA_TYPE_FLOAT, 0, 10, 0.01),
        ]
        self.points.extend(setting_points)
        
        # ========== 组4: 软压板 (整数，0=退出，1=投入) ==========
        soft_board_points = [
            GenericPoint(4, 1, "过流一段压板", 1, "", self.DATA_TYPE_UINT),
            GenericPoint(4, 2, "过流二段压板", 1, "", self.DATA_TYPE_UINT),
            GenericPoint(4, 3, "过流三段压板", 0, "", self.DATA_TYPE_UINT),
            GenericPoint(4, 4, "零序一段压板", 1, "", self.DATA_TYPE_UINT),
            GenericPoint(4, 5, "零序二段压板", 0, "", self.DATA_TYPE_UINT),
        ]
        self.points.extend(soft_board_points)
        
        # ========== 组5: ASCII字符串测试 ==========
        ascii_points = [
            GenericPoint(5, 1, "装置型号", "PCS-9000", "", self.DATA_TYPE_ASCII),
            GenericPoint(5, 2, "软件版本", "V2.1.0", "", self.DATA_TYPE_ASCII),
            GenericPoint(5, 3, "厂家名称", "XJ-RELAY", "", self.DATA_TYPE_ASCII),
            GenericPoint(5, 4, "装置地址", "DEV-001", "", self.DATA_TYPE_ASCII),
        ]
        self.points.extend(ascii_points)
        
        # ========== 组6: 有符号整数测试 ==========
        int_points = [
            GenericPoint(6, 1, "有功功率(整数)", 5000, "kW", self.DATA_TYPE_INT, min_val=-10000, max_val=10000, noise=100),
            GenericPoint(6, 2, "无功功率(整数)", -1200, "kVar", self.DATA_TYPE_INT, min_val=-5000, max_val=5000, noise=50),
            GenericPoint(6, 3, "功率因数(千分)", 850, "", self.DATA_TYPE_INT, min_val=-1000, max_val=1000, noise=5),
            GenericPoint(6, 4, "温度偏差", -15, "℃", self.DATA_TYPE_INT, min_val=-50, max_val=50, noise=2),
        ]
        self.points.extend(int_points)
        
        # ========== 组7: 双点信息测试 (DPI) ==========
        # DPI: 0=不确定, 1=分/OFF, 2=合/ON, 3=不确定
        dpi_points = [
            GenericPoint(7, 1, "开关1状态", 2, "", self.DATA_TYPE_DPI),  # 合
            GenericPoint(7, 2, "开关2状态", 1, "", self.DATA_TYPE_DPI),  # 分
            GenericPoint(7, 3, "开关3状态", 2, "", self.DATA_TYPE_DPI),  # 合
            GenericPoint(7, 4, "隔离开关1", 1, "", self.DATA_TYPE_DPI),  # 分
            GenericPoint(7, 5, "接地开关", 0, "", self.DATA_TYPE_DPI),  # 不确定
        ]
        self.points.extend(dpi_points)
    
    def update(self):
        """更新数据 (遥测添加随机噪声, 遥脉累加, 状态变化)"""
        for point in self.points:
            if point.data_type == self.DATA_TYPE_FLOAT:
                # 浮点数: 添加随机噪声
                if point.noise > 0:
                    noise = random.uniform(-point.noise, point.noise)
                    point.value = max(point.min_val, 
                                     min(point.max_val, point.value + noise))
            elif point.data_type == self.DATA_TYPE_UINT:
                # 无符号整数: 累加
                if point.increment > 0:
                    inc = random.randint(0, point.increment * 2)
                    point.value = point.value + inc
            elif point.data_type == self.DATA_TYPE_INT:
                # 有符号整数: 随机波动
                if point.noise > 0:
                    noise = random.randint(-int(point.noise), int(point.noise))
                    point.value = max(int(point.min_val),
                                     min(int(point.max_val), point.value + noise))
            elif point.data_type == self.DATA_TYPE_DPI:
                # 双点信息: 小概率随机变化 (5%概率)
                if random.random() < 0.05:
                    # 在1(分)和2(合)之间切换
                    point.value = 3 - point.value if point.value in [1, 2] else random.choice([1, 2])
            elif point.data_type == self.DATA_TYPE_ASCII:
                # ASCII字符串: 不变化
                pass
    
    def get_group_points(self, group: int) -> List[GenericPoint]:
        """获取指定组的数据点"""
        return [p for p in self.points if p.group == group]
    
    def get_point(self, group: int, entry: int) -> GenericPoint:
        """获取指定数据点"""
        for p in self.points:
            if p.group == group and p.entry == entry:
                return p
        return None
    
    def add_point(self, point: GenericPoint):
        """添加数据点"""
        self.points.append(point)
    
    def is_float_group(self, group: int) -> bool:
        """判断组是否为浮点类型"""
        points = self.get_group_points(group)
        return any(p.data_type == self.DATA_TYPE_FLOAT for p in points)
    
    def is_integer_group(self, group: int) -> bool:
        """判断组是否为整数类型"""
        points = self.get_group_points(group)
        return any(p.data_type == self.DATA_TYPE_UINT for p in points)
    
    def to_dict_list(self) -> List[Dict[str, Any]]:
        """转换为字典列表"""
        return [
            {
                "group": p.group,
                "entry": p.entry,
                "name": p.name,
                "value": round(p.value, 2) if p.data_type == self.DATA_TYPE_FLOAT else p.value,
                "unit": p.unit,
                "data_type": p.data_type
            }
            for p in self.points
        ]