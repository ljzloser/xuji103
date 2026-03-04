"""
模拟遥测数据生成器
"""
import random
from typing import List, Dict, Any
from dataclasses import dataclass


@dataclass
class AnalogPoint:
    """遥测点"""
    group: int          # 组号
    entry: int          # 条目号
    name: str           # 名称
    value: float        # 当前值
    unit: str           # 单位
    min_val: float      # 最小值
    max_val: float      # 最大值
    noise: float = 0.0  # 噪声幅度


class AnalogDataGenerator:
    """遥测数据生成器"""
    
    def __init__(self):
        self.points: List[AnalogPoint] = []
        self._init_default_points()
    
    def _init_default_points(self):
        """初始化默认遥测点"""
        # 组8: 电流电压
        default_points = [
            # 电流
            AnalogPoint(8, 1, "电流A相", 100.0, "A", 0, 1000, 2.0),
            AnalogPoint(8, 2, "电流B相", 100.0, "A", 0, 1000, 2.0),
            AnalogPoint(8, 3, "电流C相", 100.0, "A", 0, 1000, 2.0),
            # 电压
            AnalogPoint(8, 4, "电压A相", 220.0, "V", 0, 500, 1.0),
            AnalogPoint(8, 5, "电压B相", 220.0, "V", 0, 500, 1.0),
            AnalogPoint(8, 6, "电压C相", 220.0, "V", 0, 500, 1.0),
            # 功率
            AnalogPoint(8, 7, "有功功率", 50.0, "MW", 0, 100, 0.5),
            AnalogPoint(8, 8, "无功功率", 20.0, "MVar", 0, 50, 0.2),
            # 频率
            AnalogPoint(8, 9, "频率", 50.0, "Hz", 49.5, 50.5, 0.01),
        ]
        self.points.extend(default_points)
    
    def update(self):
        """更新数据 (添加随机噪声)"""
        for point in self.points:
            if point.noise > 0:
                noise = random.uniform(-point.noise, point.noise)
                point.value = max(point.min_val, 
                                 min(point.max_val, point.value + noise))
    
    def get_group_points(self, group: int) -> List[AnalogPoint]:
        """获取指定组的遥测点"""
        return [p for p in self.points if p.group == group]
    
    def get_point(self, group: int, entry: int) -> AnalogPoint:
        """获取指定遥测点"""
        for p in self.points:
            if p.group == group and p.entry == entry:
                return p
        return None
    
    def add_point(self, point: AnalogPoint):
        """添加遥测点"""
        self.points.append(point)
    
    def to_dict_list(self) -> List[Dict[str, Any]]:
        """转换为字典列表"""
        return [
            {
                "group": p.group,
                "entry": p.entry,
                "name": p.name,
                "value": round(p.value, 2),
                "unit": p.unit
            }
            for p in self.points
        ]
