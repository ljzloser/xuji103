#!/usr/bin/env python3
"""
南网以太网103协议子站模拟器
用于测试主站程序

支持多装置、多CPU
"""
import asyncio
import argparse
import logging
import struct
import random
from typing import Optional, Dict, List
from dataclasses import dataclass, field

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from protocol import APCI, FrameCodec, Frame, FrameParser
from protocol.asdu import (
    ASDU, ASDUParser, ASDUBuilder, CP56Time2a,
    TI, COT, FUN, INF, KOD, DataType,
    GenericDataItem
)
from data import DigitalDataGenerator, GenericDataGenerator
from data.generic import GenericPoint


logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger(__name__)


@dataclass
class DeviceConfig:
    """装置配置"""
    device_addr: int           # 装置地址 (1-254)
    name: str                  # 装置名称
    cpus: List[int] = field(default_factory=lambda: [0])  # CPU列表
    digital_gen: DigitalDataGenerator = None
    generic_gens: Dict[int, GenericDataGenerator] = field(default_factory=dict)  # CPU -> Generator
    
    def __post_init__(self):
        if self.digital_gen is None:
            self.digital_gen = DigitalDataGenerator()
        for cpu in self.cpus:
            if cpu not in self.generic_gens:
                self.generic_gens[cpu] = GenericDataGenerator()


class MultiDeviceSlave:
    """多装置IEC103子站模拟器"""

    def __init__(self, test_mode: str = 'normal'):
        self.send_seq = 0
        self.recv_seq = 0
        self.last_ack_seq = 0
        self.connected = False
        self.started = False
        self.frame_parser = FrameParser()

        # 多装置配置
        self.devices: Dict[int, DeviceConfig] = {}
        self._init_devices()

        # 当前总召唤序号
        self.current_scn = 0

        # k/w 流量控制参数
        self.max_unconfirmed = 12
        self.max_ack_delay = 8
        self.recv_count = 0

        # 发送队列
        self.send_queue = []
        self.unconfirmed_count = 0
        
        # 测试模式
        self.test_mode = test_mode
        self.test_frame_sent = 0

    def _init_devices(self):
        """初始化多装置配置"""
        # 装置1: 线路保护装置 (CPU 0, 1, 2)
        dev1 = DeviceConfig(
            device_addr=1,
            name="线路保护1号",
            cpus=[0, 1, 2]
        )
        # 为不同CPU设置不同的数据
        self._setup_device_data(dev1)
        self.devices[1] = dev1

        # 装置2: 变压器保护装置 (CPU 0, 1)
        dev2 = DeviceConfig(
            device_addr=2,
            name="变压器保护1号",
            cpus=[0, 1]
        )
        self._setup_device_data(dev2)
        self.devices[2] = dev2

        # 装置3: 母线保护装置 (CPU 0)
        dev3 = DeviceConfig(
            device_addr=3,
            name="母线保护1号",
            cpus=[0]
        )
        self._setup_device_data(dev3)
        self.devices[3] = dev3

        logger.info(f"已初始化 {len(self.devices)} 个装置:")
        for addr, dev in self.devices.items():
            logger.info(f"  装置{addr} ({dev.name}): CPU {dev.cpus}")

    def _setup_device_data(self, device: DeviceConfig):
        """为装置设置数据"""
        # 设置遥信数据
        device.digital_gen = DigitalDataGenerator()
        
        # 为每个CPU设置通用服务数据
        for cpu in device.cpus:
            gen = GenericDataGenerator()
            # 根据CPU号调整数据
            self._customize_generic_data(gen, device.device_addr, cpu)
            device.generic_gens[cpu] = gen

    def _customize_generic_data(self, gen: GenericDataGenerator, device_addr: int, cpu: int):
        """定制通用服务数据"""
        # 清除默认数据
        gen.points.clear()
        
        # 根据装置和CPU设置不同的数据组
        base_value = device_addr * 100 + cpu * 10
        
        # 组1: 模拟量 (浮点数)
        analog_points = [
            GenericPoint(1, 1, f"装置{device_addr}-CPU{cpu}-电流A相", base_value + 100.0, "A", 7, 0, 1000, 2.0),
            GenericPoint(1, 2, f"装置{device_addr}-CPU{cpu}-电流B相", base_value + 100.5, "A", 7, 0, 1000, 2.0),
            GenericPoint(1, 3, f"装置{device_addr}-CPU{cpu}-电流C相", base_value + 101.0, "A", 7, 0, 1000, 2.0),
            GenericPoint(1, 4, f"装置{device_addr}-CPU{cpu}-电压A相", 220.0 + cpu, "V", 7, 0, 500, 1.0),
            GenericPoint(1, 5, f"装置{device_addr}-CPU{cpu}-电压B相", 220.0 + cpu, "V", 7, 0, 500, 1.0),
            GenericPoint(1, 6, f"装置{device_addr}-CPU{cpu}-电压C相", 220.0 + cpu, "V", 7, 0, 500, 1.0),
            GenericPoint(1, 7, f"装置{device_addr}-CPU{cpu}-有功功率", 50.0 + cpu * 10, "MW", 7, 0, 100, 0.5),
            GenericPoint(1, 8, f"装置{device_addr}-CPU{cpu}-无功功率", 20.0 + cpu * 5, "MVar", 7, 0, 50, 0.2),
            GenericPoint(1, 9, f"装置{device_addr}-CPU{cpu}-频率", 50.0, "Hz", 7, 49.5, 50.5, 0.01),
        ]
        gen.points.extend(analog_points)
        
        # 组2: 电能量 (无符号整数)
        energy_points = [
            GenericPoint(2, 1, f"装置{device_addr}-CPU{cpu}-正向有功电能", base_value * 100, "kWh", 3, increment=10),
            GenericPoint(2, 2, f"装置{device_addr}-CPU{cpu}-反向有功电能", base_value * 10, "kWh", 3, increment=1),
            GenericPoint(2, 3, f"装置{device_addr}-CPU{cpu}-正向无功电能", base_value * 50, "kVarh", 3, increment=5),
            GenericPoint(2, 4, f"装置{device_addr}-CPU{cpu}-运行时间", base_value * 1000, "h", 3, increment=1),
        ]
        gen.points.extend(energy_points)
        
        # 组3: 定值 (浮点数)
        setting_points = [
            GenericPoint(3, 1, f"装置{device_addr}-CPU{cpu}-过流I段定值", 5.0 + cpu, "A", 7, 0, 100, 0.1),
            GenericPoint(3, 2, f"装置{device_addr}-CPU{cpu}-过流II段定值", 3.0 + cpu, "A", 7, 0, 100, 0.1),
            GenericPoint(3, 3, f"装置{device_addr}-CPU{cpu}-过流III段定值", 1.5 + cpu, "A", 7, 0, 100, 0.1),
        ]
        gen.points.extend(setting_points)
        
        # 组4: 软压板 (整数)
        board_points = [
            GenericPoint(4, 1, f"装置{device_addr}-CPU{cpu}-过流I段压板", 1, "", 3),
            GenericPoint(4, 2, f"装置{device_addr}-CPU{cpu}-过流II段压板", 1, "", 3),
            GenericPoint(4, 3, f"装置{device_addr}-CPU{cpu}-过流III段压板", 0, "", 3),
        ]
        gen.points.extend(board_points)
        
        # 组5: ASCII字符串
        ascii_points = [
            GenericPoint(5, 1, "装置型号", f"DEV-{device_addr:03d}", "", 1),
            GenericPoint(5, 2, "软件版本", f"V2.{cpu}.0", "", 1),
            GenericPoint(5, 3, "CPU名称", f"CPU-{cpu}", "", 1),
        ]
        gen.points.extend(ascii_points)
        
        # 组6: 有符号整数
        int_points = [
            GenericPoint(6, 1, f"装置{device_addr}-CPU{cpu}-有功功率(整数)", base_value * 10, "kW", 4, -10000, 10000, 100),
            GenericPoint(6, 2, f"装置{device_addr}-CPU{cpu}-无功功率(整数)", -base_value * 2, "kVar", 4, -5000, 5000, 50),
        ]
        gen.points.extend(int_points)
        
        # 组7: 双点信息 (DPI)
        dpi_points = [
            GenericPoint(7, 1, f"装置{device_addr}-CPU{cpu}-开关1状态", 2, "", 9),
            GenericPoint(7, 2, f"装置{device_addr}-CPU{cpu}-开关2状态", 1, "", 9),
            GenericPoint(7, 3, f"装置{device_addr}-CPU{cpu}-隔离开关", 2, "", 9),
        ]
        gen.points.extend(dpi_points)
        
        # 组100: 南网扩展数据类型测试 (DataType 213-217)
        ext_points = [
            GenericPoint(100, 1, f"装置{device_addr}-CPU{cpu}-时标报文213", 0, "", 213),
            GenericPoint(100, 2, f"装置{device_addr}-CPU{cpu}-时标报文214", 0, "", 214),
            GenericPoint(100, 3, f"装置{device_addr}-CPU{cpu}-带时标浮点215", base_value + 123.45, "", 215),
            GenericPoint(100, 4, f"装置{device_addr}-CPU{cpu}-带时标整数216", base_value * 100, "", 216),
            GenericPoint(100, 5, f"装置{device_addr}-CPU{cpu}-带时标字符217", f"FAULT_{device_addr}_{cpu}", "", 217),
        ]
        gen.points.extend(ext_points)

    def reset(self):
        """重置状态"""
        self.send_seq = 0
        self.recv_seq = 0
        self.last_ack_seq = 0
        self.started = False
        self.recv_count = 0
        self.unconfirmed_count = 0
        self.send_queue.clear()

    def unconfirmed_frames(self) -> int:
        """获取未确认的I帧数"""
        diff = self.send_seq - self.last_ack_seq
        if diff < 0:
            diff += 32768
        return diff
    
    def get_device(self, device_addr: int) -> Optional[DeviceConfig]:
        """获取装置配置"""
        return self.devices.get(device_addr)
    
    def parse_asdu_addr(self, asdu_addr: int) -> tuple:
        """
        解析南网规范ASDU地址
        返回: (device_addr, cpu_no)
        """
        device_addr = (asdu_addr >> 8) & 0xFF
        cpu_no = asdu_addr & 0x07
        return device_addr, cpu_no
    
    def build_asdu_addr(self, device_addr: int, cpu_no: int = 0) -> int:
        """构建南网规范ASDU地址"""
        return (device_addr << 8) | (cpu_no & 0x07)

    async def handle_client(self, reader: asyncio.StreamReader, 
                           writer: asyncio.StreamWriter):
        """处理客户端连接"""
        addr = writer.get_extra_info('peername')
        logger.info(f"主站连接: {addr}")

        self.connected = True
        self.reset()

        # 启动突发上送任务
        spontaneous_task = asyncio.create_task(
            self.spontaneous_loop(writer)
        )

        try:
            while self.connected:
                data = await reader.read(4096)
                if not data:
                    logger.info(f"主站断开: {addr}")
                    break

                await self.process_data(data, writer)
        except Exception as e:
            logger.error(f"连接错误: {e}")
        finally:
            self.connected = False
            spontaneous_task.cancel()
            writer.close()
            try:
                await writer.wait_closed()
            except:
                pass

    async def spontaneous_loop(self, writer: asyncio.StreamWriter):
        """定时发送突发事件的循环"""
        event_count = 0
        try:
            while self.connected:
                await asyncio.sleep(10)
                if self.started and self.connected:
                    unconfirmed = self.unconfirmed_frames()
                    if unconfirmed >= self.max_unconfirmed:
                        logger.warning(f"[k控制] 突发发送阻塞: 未确认={unconfirmed} >= k={self.max_unconfirmed}")
                        continue
                    
                    event_count += 1
                    # 随机选择一个装置和CPU发送事件
                    device_addr = random.choice(list(self.devices.keys()))
                    device = self.devices[device_addr]
                    cpu = random.choice(device.cpus)
                    
                    if event_count % 2 == 0:
                        await self.send_spontaneous_event(writer, device_addr, cpu)
                    else:
                        await self.send_protection_event(writer, device_addr, cpu)
        except asyncio.CancelledError:
            pass

    async def process_data(self, data: bytes, writer: asyncio.StreamWriter):
        """处理接收数据"""
        self.frame_parser.feed(data)
        frames = self.frame_parser.get_all_frames()
        for frame in frames:
            await self.handle_frame(frame, writer)

    async def handle_frame(self, frame: Frame, writer: asyncio.StreamWriter):
        """处理帧"""
        if frame.is_u_format:
            await self.handle_u_format(frame, writer)
        elif frame.is_i_format:
            expected_seq = self.recv_seq
            if frame.send_seq != expected_seq:
                logger.error(f"[I-RX] 序号错误: N(S)={frame.send_seq}, 期望={expected_seq} - 断开连接!")
                writer.close()
                await writer.wait_closed()
                self.connected = False
                return
            
            self.recv_seq = (frame.send_seq + 1) & 0x7FFF
            self.recv_count += 1

            logger.info(f"[I-RX] N(S)={frame.send_seq} N(R)={frame.recv_seq}, "
                       f"接收计数={self.recv_count}/{self.max_ack_delay}")

            if self.test_mode == 'no_ack':
                logger.warning(f"[测试] no_ack模式: 不发送S帧确认")
            else:
                if self.recv_count >= self.max_ack_delay:
                    ack = FrameCodec.encode_s_format(self.recv_seq)
                    writer.write(ack)
                    await writer.drain()
                    logger.info(f"[w控制] w阈值达到: {self.recv_count} >= {self.max_ack_delay}")
                    logger.info(f"[S-TX] 发送S帧 N(R)={self.recv_seq}")
                    self.recv_count = 0

            await self.handle_i_format(frame, writer)
        elif frame.is_s_format:
            if frame.recv_seq > self.send_seq:
                logger.error(f"[S-RX] 序号错误: N(R)={frame.recv_seq} > sendSeq={self.send_seq} - 断开连接!")
                writer.close()
                await writer.wait_closed()
                self.connected = False
                return
            
            old_ack = self.last_ack_seq
            self.last_ack_seq = frame.recv_seq
            
            actual_confirmed = self.last_ack_seq - old_ack if self.last_ack_seq >= old_ack else (self.last_ack_seq + 32768 - old_ack)
            
            logger.info(f"[S-RX] 收到确认: N(R)={frame.recv_seq}, "
                       f"确认了 {actual_confirmed} 帧, 未确认={self.unconfirmed_frames()}")

    async def handle_u_format(self, frame: Frame, writer: asyncio.StreamWriter):
        """处理U格式帧"""
        u_type = frame.u_format_type

        if u_type == 0x07:  # STARTDT_ACT
            logger.info("收到 STARTDT 启动命令")
            self.started = True
            response = FrameCodec.encode_u_format(0x0B)
            writer.write(response)
            await writer.drain()
            logger.info("发送 STARTDT 确认")
            
            if self.test_mode == 'oversized_frame':
                await self.send_oversized_frame_test(writer)
            
            if self.test_mode == 'flood':
                await self.send_flood_test(writer)

        elif u_type == 0x13:  # STOPDT_ACT
            logger.info("收到 STOPDT 停止命令")
            self.started = False
            response = FrameCodec.encode_u_format(0x23)
            writer.write(response)
            await writer.drain()

        elif u_type == 0x43:  # TESTFR_ACT
            logger.info("收到 TESTFR 测试帧")
            response = FrameCodec.encode_u_format(0x83)
            writer.write(response)
            await writer.drain()
    
    async def send_oversized_frame_test(self, writer: asyncio.StreamWriter):
        """发送超长帧测试"""
        logger.warning("[测试] 发送超长帧测试...")
        
        asdu_addr = self.build_asdu_addr(1, 0)
        oversized_data = b'\x00' * 3000
        asdu = ASDU(0x01, 1, 1, asdu_addr, oversized_data)
        
        frame_data = bytearray()
        frame_data.append(0x68)
        length = 4 + len(asdu.encode())
        frame_data.extend(struct.pack('<H', length))
        frame_data.extend(struct.pack('<H', self.send_seq << 1))
        frame_data.extend(struct.pack('<H', self.recv_seq << 1))
        frame_data.extend(asdu.encode())
        
        writer.write(bytes(frame_data))
        await writer.drain()
        
        logger.warning(f"[测试] 已发送超长帧: ASDU长度={len(oversized_data)}, 总长度={length}")
        self.send_seq = (self.send_seq + 1) & 0x7FFF
    
    async def send_flood_test(self, writer: asyncio.StreamWriter):
        """发送大量帧测试"""
        logger.warning("[测试] 发送大量帧测试...")
        
        for i in range(50):
            device_addr = (i % 3) + 1
            asdu = ASDUBuilder.build_single_point(
                device_addr, 200, i + 1, 2, cot=COT.SPONTANEOUS
            )
            frame = self.build_i_frame(asdu)
            writer.write(frame)
            
            if (i + 1) % 10 == 0:
                await writer.drain()
                logger.info(f"[测试] 已发送 {i + 1} 帧...")
        
        await writer.drain()
        logger.warning(f"[测试] 完成: 发送了 50 帧")

    async def handle_i_format(self, frame: Frame, writer: asyncio.StreamWriter):
        """处理I格式帧"""
        if not self.started:
            logger.warning("未启动, 忽略I格式帧")
            return

        asdu = ASDU.parse(frame.asdu_data)
        device_addr, cpu_no = self.parse_asdu_addr(asdu.asdu_addr)
        
        logger.info(f"收到ASDU: TI={asdu.ti:02X} COT={asdu.cot} 装置={device_addr} CPU={cpu_no}")

        if self.test_mode == 'no_response':
            logger.warning(f"[测试] no_response模式: 不响应命令 (TI={asdu.ti:02X})")
            return

        response = None

        if asdu.ti == TI.GI_CMD:
            response = await self.handle_gi_cmd(asdu, device_addr)
        elif asdu.ti == TI.GENERIC_READ:
            response = await self.handle_generic_read(asdu, device_addr, cpu_no)

        if response:
            await self.send_response(response, writer)
            
            if self.test_mode == 'ext_types' and self.test_frame_sent == 0:
                await self.send_extended_types_test(writer)

    async def handle_gi_cmd(self, asdu: ASDU, device_addr: int) -> Optional[bytes]:
        """处理总召唤命令"""
        fun, inf, scn = ASDUParser.parse_gi_cmd(asdu)
        self.current_scn = scn
        logger.info(f"总召唤命令: 装置={device_addr} FUN={fun} INF={inf} SCN={scn}")

        frames = []
        
        # 检查装置是否存在
        device = self.get_device(device_addr)
        if not device:
            # 广播地址(255)或未知装置: 响应所有装置
            if device_addr == 255 or device_addr == 0:
                for dev_addr, dev in self.devices.items():
                    frames.extend(self._build_gi_response(dev, scn))
            else:
                logger.warning(f"未知装置地址: {device_addr}")
                return frames
        else:
            frames.extend(self._build_gi_response(device, scn))

        logger.info(f"总召唤响应: {len(frames)} 帧")
        return frames

    def _build_gi_response(self, device: DeviceConfig, scn: int) -> list:
        """构建单个装置的总召唤响应"""
        frames = []
        asdu_addr = self.build_asdu_addr(device.device_addr, 0)
        
        gi_points = device.digital_gen.get_gi_points()
        
        batch_size = 20
        for i in range(0, len(gi_points), batch_size):
            batch = gi_points[i:i+batch_size]
            points_data = [(p.fun, p.inf, int(p.value)) for p in batch]

            asdu_resp = ASDUBuilder.build_double_point(
                device.device_addr, points_data, COT.GI, scn
            )
            frames.append(self.build_i_frame(asdu_resp))

        asdu_end = ASDUBuilder.build_gi_end(device.device_addr, scn)
        frames.append(self.build_i_frame(asdu_end))
        
        return frames

    async def handle_generic_read(self, asdu: ASDU, device_addr: int, cpu_no: int) -> Optional[bytes]:
        """处理通用分类读命令"""
        fun, inf, rii, gin_group, gin_entry, kod = ASDUParser.parse_generic_read(asdu)
        logger.info(f"通用分类读: 装置={device_addr} CPU={cpu_no} FUN={fun:02X} INF={inf:02X} "
                   f"RII={rii} GIN={gin_group}/{gin_entry} KOD={kod:02X}")

        frames = []
        
        device = self.get_device(device_addr)
        if not device:
            logger.warning(f"未知装置: {device_addr}")
            return frames
        
        if cpu_no not in device.generic_gens:
            logger.warning(f"装置{device_addr} 无CPU{cpu_no}")
            return frames
        
        generic_gen = device.generic_gens[cpu_no]
        asdu_addr = self.build_asdu_addr(device_addr, cpu_no)

        if inf == INF.READ_GROUP_ALL:
            frames = self.build_generic_group_response(asdu_addr, generic_gen, rii, gin_group)
        elif inf == INF.READ_SINGLE_CATALOG:
            frames = self.build_catalog_response(asdu_addr, generic_gen, rii, gin_group, gin_entry, kod)
        elif inf == INF.READ_SINGLE_VALUE:
            frames = self.build_generic_single_response(asdu_addr, generic_gen, rii, gin_group, gin_entry)

        return frames
    
    def build_catalog_response(self, asdu_addr: int, gen: GenericDataGenerator, 
                               rii: int, group: int, entry: int, kod: int) -> list:
        """构建ASDU11目录响应"""
        frames = []
        
        point = gen.get_point(group, entry)
        if not point:
            logger.warning(f"未找到数据点: 组{group} 条目{entry}")
            return frames
        
        entries = []
        
        # 实际值 (KOD=0x01)
        if point.data_type == 7:
            entries.append((KOD.ACTUAL_VALUE, DataType.R32_23, 4, struct.pack('<f', float(point.value))))
        elif point.data_type == 3:
            entries.append((KOD.ACTUAL_VALUE, DataType.UNSIGNED, 4, struct.pack('<I', int(point.value))))
        elif point.data_type == 4:
            entries.append((KOD.ACTUAL_VALUE, DataType.SIGNED, 4, struct.pack('<i', int(point.value))))
        elif point.data_type == 1:
            encoded = point.value.encode('ascii')[:20] if isinstance(point.value, str) else str(point.value).encode('ascii')[:20]
            entries.append((KOD.ACTUAL_VALUE, DataType.OS8ASCII, len(encoded), encoded))
        elif point.data_type == 9:
            entries.append((KOD.ACTUAL_VALUE, DataType.DPI, 1, bytes([int(point.value) & 0x03])))
        
        # 描述 (KOD=0x0A)
        desc = point.name.encode('ascii')[:20]
        entries.append((KOD.DESCRIPTION, DataType.OS8ASCII, len(desc), desc))
        
        asdu_resp = ASDUBuilder.build_catalog_data(asdu_addr, rii, group, entry, entries)
        frames.append(self.build_i_frame(asdu_resp))
        
        logger.info(f"ASDU11目录响应: 组{group} 条目{entry} {len(entries)}个KOD")
        
        return frames
    
    async def send_extended_types_test(self, writer: asyncio.StreamWriter):
        """发送南网扩展数据类型测试 (213-217)"""
        logger.info("[测试] 发送南网扩展数据类型测试...")
        
        for device_addr, device in self.devices.items():
            for cpu in device.cpus:
                asdu_addr = self.build_asdu_addr(device_addr, cpu)
                
                # DataType 215: 带时标浮点
                asdu_215 = ASDUBuilder.build_generic_data_with_time(
                    asdu_addr, 1, 100, 1, 
                    DataType.FLOAT_WITH_TIME, device_addr * 100 + cpu + 123.45,
                    relative_time=500, fault_num=device_addr
                )
                writer.write(self.build_i_frame(asdu_215))
                await writer.drain()
                logger.info(f"[测试] 发送 DataType 215: 装置{device_addr} CPU{cpu}")
                
                # DataType 216: 带时标整数
                asdu_216 = ASDUBuilder.build_generic_data_with_time(
                    asdu_addr, 2, 100, 2,
                    DataType.INT_WITH_TIME, device_addr * 1000 + cpu * 100,
                    relative_time=1000, fault_num=device_addr
                )
                writer.write(self.build_i_frame(asdu_216))
                await writer.drain()
                logger.info(f"[测试] 发送 DataType 216: 装置{device_addr} CPU{cpu}")
                
                # DataType 217: 带时标字符
                asdu_217 = ASDUBuilder.build_generic_data_with_time(
                    asdu_addr, 3, 100, 3,
                    DataType.CHAR_WITH_TIME, f"FAULT_D{device_addr}_C{cpu}",
                    relative_time=1500, fault_num=device_addr
                )
                writer.write(self.build_i_frame(asdu_217))
                await writer.drain()
                logger.info(f"[测试] 发送 DataType 217: 装置{device_addr} CPU{cpu}")
        
        self.test_frame_sent = 1
        logger.info("[测试] 南网扩展数据类型测试完成")

    def build_generic_group_response(self, asdu_addr: int, gen: GenericDataGenerator, 
                                     rii: int, group: int) -> list:
        """构建通用分类组数据响应"""
        frames = []

        gen.update()
        
        points = gen.get_group_points(group)
        
        # 分离普通类型和南网扩展类型
        normal_items = []
        ext_type_points = []
        
        for p in points:
            if p.data_type in [213, 214, 215, 216, 217]:
                # 南网扩展类型单独处理
                ext_type_points.append(p)
            elif p.data_type == 1:
                data_size = min(len(str(p.value)), 20)
                item = GenericDataItem(
                    gin_group=p.group, gin_entry=p.entry,
                    kod=KOD.ACTUAL_VALUE,
                    data_type=DataType.OS8ASCII,
                    data_size=data_size, value=p.value
                )
                normal_items.append(item)
            elif p.data_type == 3:
                item = GenericDataItem(
                    gin_group=p.group, gin_entry=p.entry,
                    kod=KOD.ACTUAL_VALUE,
                    data_type=DataType.UNSIGNED,
                    data_size=4, value=p.value
                )
                normal_items.append(item)
            elif p.data_type == 4:
                item = GenericDataItem(
                    gin_group=p.group, gin_entry=p.entry,
                    kod=KOD.ACTUAL_VALUE,
                    data_type=DataType.SIGNED,
                    data_size=4, value=p.value
                )
                normal_items.append(item)
            elif p.data_type == 7:
                item = GenericDataItem(
                    gin_group=p.group, gin_entry=p.entry,
                    kod=KOD.ACTUAL_VALUE,
                    data_type=DataType.R32_23,
                    data_size=4, value=p.value
                )
                normal_items.append(item)
            elif p.data_type == 9:
                item = GenericDataItem(
                    gin_group=p.group, gin_entry=p.entry,
                    kod=KOD.ACTUAL_VALUE,
                    data_type=DataType.DPI,
                    data_size=1, value=p.value
                )
                normal_items.append(item)
            else:
                item = GenericDataItem(
                    gin_group=p.group, gin_entry=p.entry,
                    kod=KOD.ACTUAL_VALUE,
                    data_type=DataType.R32_23,
                    data_size=4, value=p.value
                )
                normal_items.append(item)

        # 发送普通类型数据
        if normal_items:
            asdu_resp = ASDUBuilder.build_generic_data(asdu_addr, rii, normal_items, COT.GENERIC_DATA_RESP)
            frames.append(self.build_i_frame(asdu_resp))
        
        # 发送南网扩展类型数据 (每个条目单独一帧，因为包含时标)
        for p in ext_type_points:
            asdu_resp = ASDUBuilder.build_generic_data_with_time(
                asdu_addr, rii, p.group, p.entry, p.data_type, p.value
            )
            frames.append(self.build_i_frame(asdu_resp))

        if normal_items or ext_type_points:
            type_names = {1: "ASCII", 3: "UINT", 4: "INT", 7: "FLOAT", 9: "DPI",
                         213: "时标213", 214: "时标214", 215: "浮点215", 216: "整数216", 217: "字符217"}
            type_str = type_names.get(points[0].data_type if points else 0, "未知")
            device_addr, cpu_no = self.parse_asdu_addr(asdu_addr)
            logger.info(f"通用服务响应: 装置{device_addr} CPU{cpu_no} 组{group} {len(normal_items)}普通+{len(ext_type_points)}扩展 (类型={type_str})")

        return frames

    def build_generic_single_response(self, asdu_addr: int, gen: GenericDataGenerator,
                                      rii: int, group: int, entry: int) -> list:
        """构建单个条目响应"""
        frames = []

        gen.update()
        
        point = gen.get_point(group, entry)
        if point:
            if point.data_type == 7:
                data_type = DataType.R32_23
            elif point.data_type == 3:
                data_type = DataType.UNSIGNED
            else:
                data_type = DataType.R32_23
            
            item = GenericDataItem(
                gin_group=point.group, gin_entry=point.entry,
                kod=KOD.ACTUAL_VALUE,
                data_type=data_type,
                data_size=4, value=point.value
            )
            asdu_resp = ASDUBuilder.build_generic_data(asdu_addr, rii, [item], COT.GENERIC_DATA_RESP)
            frames.append(self.build_i_frame(asdu_resp))

        return frames

    def build_i_frame(self, asdu: ASDU) -> bytes:
        """构建I格式帧"""
        apci = APCI('I', self.send_seq, self.recv_seq)
        frame = FrameCodec.encode(apci, asdu.encode())
        unconfirmed = self.unconfirmed_frames() + 1
        logger.info(f"[I-TX] N(S)={self.send_seq} N(R)={self.recv_seq}, TI={asdu.ti}, 未确认={unconfirmed}")
        self.send_seq = (self.send_seq + 1) & 0x7FFF
        return frame

    async def send_response(self, response, writer: asyncio.StreamWriter):
        """发送响应"""
        if isinstance(response, list):
            logger.info(f"[I-TX] 发送 {len(response)} 个I帧...")
            for i, frame in enumerate(response):
                # k值控制：未确认数>=k时打印警告（不阻塞）
                unconfirmed = self.unconfirmed_frames()
                if unconfirmed >= self.max_unconfirmed:
                    logger.warning(f"[k控制] 第{i+1}/{len(response)}帧发送: "
                                 f"未确认={unconfirmed} >= k={self.max_unconfirmed}")
                
                writer.write(frame)
                await writer.drain()
        else:
            writer.write(response)
            await writer.drain()

    async def send_spontaneous_event(self, writer: asyncio.StreamWriter, device_addr: int, cpu: int):
        """发送突发遥信事件 (ASDU1)"""
        if not self.started or not self.connected:
            return

        device = self.get_device(device_addr)
        if not device:
            return

        points = device.digital_gen.get_gi_points()
        if points:
            point = random.choice(points)
            point.value = not point.value

            asdu = ASDUBuilder.build_single_point(
                device_addr,
                point.fun,
                point.inf,
                2 if point.value else 1,
                cot=COT.SPONTANEOUS
            )
            frame = self.build_i_frame(asdu)
            writer.write(frame)
            await writer.drain()

            logger.info(f"突发遥信(ASDU1): 装置{device_addr} CPU{cpu} FUN={point.fun:02X} INF={point.inf:02X}")

    async def send_protection_event(self, writer: asyncio.StreamWriter, device_addr: int, cpu: int):
        """发送保护动作事件 (ASDU2)"""
        if not self.started or not self.connected:
            return
        
        asdu_addr = self.build_asdu_addr(device_addr, cpu)

        data = bytearray()
        data.append(0x80)  # FUN
        data.append(0x01)  # INF
        data.append(0x02)  # DPI = ON
        data.extend(struct.pack('<H', 100))  # RET
        data.extend(struct.pack('<H', device_addr))  # FAN
        data.extend(CP56Time2a.now().encode())  # 事件时间
        data.extend(CP56Time2a.now().encode())  # 接收时间
        data.append(0x89)  # SIN (A相接地故障)

        asdu = ASDU(TI.DOUBLE_POINT_TIME, 1, COT.SPONTANEOUS, asdu_addr, bytes(data))
        frame = self.build_i_frame(asdu)
        writer.write(frame)
        await writer.drain()

        logger.info(f"保护事件(ASDU2): 装置{device_addr} CPU{cpu} 故障号={device_addr}")


async def main():
    parser = argparse.ArgumentParser(description='IEC103子站模拟器 (多装置多CPU)')
    parser.add_argument('--host', default='0.0.0.0', help='监听地址')
    parser.add_argument('--port', type=int, default=2404, help='监听端口')
    parser.add_argument('--log-level', default='info', help='日志级别')
    parser.add_argument('--test-mode', default='normal',
                       choices=['normal', 'no_ack', 'oversized_frame', 'no_response', 'flood', 'ext_types'],
                       help='测试模式')
    
    args = parser.parse_args()
    
    logging.getLogger().setLevel(getattr(logging, args.log_level.upper()))
    
    slave = MultiDeviceSlave(args.test_mode)
    
    server = await asyncio.start_server(
        slave.handle_client,
        args.host,
        args.port
    )
    
    addr = server.sockets[0].getsockname()
    logger.info(f"IEC103子站模拟器启动: {addr[0]}:{addr[1]}")
    logger.info(f"测试模式: {args.test_mode}")
    logger.info(f"k值(最大未确认帧数): {slave.max_unconfirmed}")
    logger.info(f"w值(确认阈值): {slave.max_ack_delay}")
    
    # 打印装置配置
    logger.info("=" * 50)
    logger.info("装置配置:")
    for device_addr, device in slave.devices.items():
        logger.info(f"  装置{device_addr} ({device.name}):")
        for cpu in device.cpus:
            asdu_addr = slave.build_asdu_addr(device_addr, cpu)
            logger.info(f"    CPU{cpu}: ASDU地址=0x{asdu_addr:04X}")
            gen = device.generic_gens[cpu]
            groups = sorted(set(p.group for p in gen.points))
            logger.info(f"           数据组: {groups}")
    logger.info("=" * 50)
    
    if args.test_mode != 'normal':
        logger.warning(f"========== 测试模式: {args.test_mode} ==========")
    
    async with server:
        await server.serve_forever()


if __name__ == '__main__':
    asyncio.run(main())
