"""
模拟遥信数据生成器
"""
import random
from typing import List, Dict, Any
from dataclasses import dataclass
from enum import IntEnum


class DoublePointValue(IntEnum):
    """双点遥信值"""
    INDETERMINATE0 = 0   # 不确定
    OFF = 1              # 分/开
    ON = 2               # 合/关
    INDETERMINATE1 = 3   # 不确定


@dataclass
class DigitalPoint:
    """遥信点"""
    fun: int            # 功能类型
    inf: int            # 信息序号
    name: str           # 名称
    value: DoublePointValue  # 当前值
    gi_include: bool = True  # 是否包含在总召唤中


class DigitalDataGenerator:
    """遥信数据生成器"""
    
    def __init__(self):
        self.points: List[DigitalPoint] = []
        self._init_default_points()
    
    def _init_default_points(self):
        """初始化默认遥信点"""
        # FUN=255 (GLB) 系统信号
        # FUN=128-254 保护信号
        
        default_points = [
            # 开关位置 (FUN=255, INF=16-31)
            DigitalPoint(255, 16, "开关1位置", DoublePointValue.ON),
            DigitalPoint(255, 17, "开关2位置", DoublePointValue.OFF),
            DigitalPoint(255, 18, "开关3位置", DoublePointValue.ON),
            DigitalPoint(255, 19, "开关4位置", DoublePointValue.OFF),
            # 压板状态
            DigitalPoint(255, 20, "保护压板1", DoublePointValue.ON),
            DigitalPoint(255, 21, "保护压板2", DoublePointValue.ON),
            DigitalPoint(255, 22, "保护压板3", DoublePointValue.OFF),
            # 告警信号
            DigitalPoint(255, 32, "装置告警", DoublePointValue.OFF),
            DigitalPoint(255, 33, "通信异常", DoublePointValue.OFF),
            # 保护动作信号 (FUN=128 距离保护)
            DigitalPoint(128, 1, "距离I段动作", DoublePointValue.OFF),
            DigitalPoint(128, 2, "距离II段动作", DoublePointValue.OFF),
            DigitalPoint(128, 3, "距离III段动作", DoublePointValue.OFF),
        ]
        self.points.extend(default_points)
    
    def random_flip(self, probability: float = 0.01):
        """随机翻转状态 (模拟变位)"""
        for point in self.points:
            if random.random() < probability:
                if point.value == DoublePointValue.ON:
                    point.value = DoublePointValue.OFF
                elif point.value == DoublePointValue.OFF:
                    point.value = DoublePointValue.ON
    
    def get_gi_points(self) -> List[DigitalPoint]:
        """获取总召唤点"""
        return [p for p in self.points if p.gi_include]
    
    def get_point(self, fun: int, inf: int) -> DigitalPoint:
        """获取指定遥信点"""
        for p in self.points:
            if p.fun == fun and p.inf == inf:
                return p
        return None
    
    def add_point(self, point: DigitalPoint):
        """添加遥信点"""
        self.points.append(point)
    
    def to_dict_list(self) -> List[Dict[str, Any]]:
        """转换为字典列表"""
        return [
            {
                "fun": p.fun,
                "inf": p.inf,
                "name": p.name,
                "value": int(p.value),
                "value_str": "ON" if p.value == DoublePointValue.ON else "OFF"
            }
            for p in self.points
        ]
