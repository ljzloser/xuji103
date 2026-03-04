"""
模拟遥脉数据生成器
"""
import random
from typing import List, Dict, Any
from dataclasses import dataclass


@dataclass
class CounterPoint:
    """遥脉点"""
    group: int          # 组号
    entry: int          # 条目号
    name: str           # 名称
    value: int          # 当前值 (积分总量)
    unit: str           # 单位
    increment: int = 1  # 每次增量


class CounterDataGenerator:
    """遥脉数据生成器"""
    
    def __init__(self):
        self.points: List[CounterPoint] = []
        self._init_default_points()
    
    def _init_default_points(self):
        """初始化默认遥脉点"""
        # 组16: 电能量
        default_points = [
            CounterPoint(16, 1, "正向有功电能", 12345, "kWh", 10),
            CounterPoint(16, 2, "反向有功电能", 100, "kWh", 1),
            CounterPoint(16, 3, "正向无功电能", 5000, "kVarh", 5),
            CounterPoint(16, 4, "反向无功电能", 50, "kVarh", 1),
            # 组17: 其他积分量
            CounterPoint(17, 1, "运行时间", 10000, "h", 1),
            CounterPoint(17, 2, "动作次数", 50, "次", 0),
        ]
        self.points.extend(default_points)
    
    def update(self):
        """更新数据 (累加)"""
        for point in self.points:
            if point.increment > 0:
                # 随机增量
                inc = random.randint(0, point.increment * 2)
                point.value += inc
    
    def get_group_points(self, group: int) -> List[CounterPoint]:
        """获取指定组的遥脉点"""
        return [p for p in self.points if p.group == group]
    
    def get_point(self, group: int, entry: int) -> CounterPoint:
        """获取指定遥脉点"""
        for p in self.points:
            if p.group == group and p.entry == entry:
                return p
        return None
    
    def add_point(self, point: CounterPoint):
        """添加遥脉点"""
        self.points.append(point)
    
    def to_dict_list(self) -> List[Dict[str, Any]]:
        """转换为字典列表"""
        return [
            {
                "group": p.group,
                "entry": p.entry,
                "name": p.name,
                "value": p.value,
                "unit": p.unit
            }
            for p in self.points
        ]
