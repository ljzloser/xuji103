#!/usr/bin/env python3
"""
南网以太网103协议子站模拟器
用于测试主站程序
"""
import asyncio
import argparse
import logging
import struct
from typing import Optional

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


logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger(__name__)


class IEC103Slave:
    """IEC103子站模拟器"""

    def __init__(self, device_addr: int = 1, test_mode: str = 'normal'):
        self.device_addr = device_addr
        self.send_seq = 0
        self.recv_seq = 0
        self.last_ack_seq = 0  # 最后确认的序号（主站确认）
        self.connected = False
        self.started = False
        self.frame_parser = FrameParser()

        # 数据生成器
        self.digital_gen = DigitalDataGenerator()
        self.generic_gen = GenericDataGenerator()  # 遥测/遥脉统一

        # 当前总召唤序号
        self.current_scn = 0

        # k/w 流量控制参数
        self.max_unconfirmed = 12  # k值: 最大未确认I帧数
        self.max_ack_delay = 8     # w值: 收到w个I帧后立即确认
        self.recv_count = 0        # 接收未确认计数

        # 发送队列（用于k值控制）
        self.send_queue = []
        self.unconfirmed_count = 0  # 未确认的I帧数
        
        # ========== 测试模式 ==========
        # normal: 正常模式
        # no_ack: 不发送S帧确认（测试k值阻塞超时）
        # oversized_frame: 发送超长帧（测试帧长度校验）
        # no_response: 不响应命令（测试T1超时）
        # flood: 快速发送大量帧（测试接收缓冲区限制）
        # ext_types: 发送南网扩展数据类型(213-217)
        self.test_mode = test_mode
        self.test_frame_sent = 0  # 测试帧计数

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
            diff += 32768  # 序号回绕
        return diff

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
        import struct
        event_count = 0
        try:
            while self.connected:
                await asyncio.sleep(10)  # 每10秒发送一次
                if self.started and self.connected:
                    # k值控制：未确认帧数超过k时暂停发送
                    unconfirmed = self.unconfirmed_frames()
                    if unconfirmed >= self.max_unconfirmed:
                        logger.warning(f"[k-Control] Spontaneous blocked: unconfirmed={unconfirmed} >= k={self.max_unconfirmed}")
                        continue
                    
                    event_count += 1
                    if event_count % 2 == 0:
                        # 发送ASDU1突发遥信
                        await self.send_spontaneous_event(writer)
                    else:
                        # 发送ASDU2保护动作事件
                        await self.send_protection_event(writer)
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
            # 序号验证：N(S) 必须等于期望值
            expected_seq = self.recv_seq
            if frame.send_seq != expected_seq:
                logger.error(f"[I-RX] SEQUENCE ERROR: N(S)={frame.send_seq}, expected={expected_seq} - Disconnecting!")
                writer.close()
                await writer.wait_closed()
                self.connected = False
                return
            
            # 更新接收序号
            self.recv_seq = (frame.send_seq + 1) & 0x7FFF
            self.recv_count += 1  # 递增接收计数

            logger.info(f"[I-RX] N(S)={frame.send_seq} N(R)={frame.recv_seq}, "
                       f"recv_count={self.recv_count}/{self.max_ack_delay}")

            # ========== 测试模式: no_ack ==========
            if self.test_mode == 'no_ack':
                logger.warning(f"[TEST] no_ack模式: 不发送S帧确认")
            else:
                # w值控制：收到w个I帧后立即发S帧确认
                if self.recv_count >= self.max_ack_delay:
                    ack = FrameCodec.encode_s_format(self.recv_seq)
                    writer.write(ack)
                    await writer.drain()
                    logger.info(f"[w-Control] w threshold reached: {self.recv_count} >= {self.max_ack_delay}")
                    logger.info(f"[S-TX] Sent S-Frame N(R)={self.recv_seq} (acknowledged)")
                    self.recv_count = 0
                else:
                    logger.debug(f"[w-Control] w threshold not reached, waiting...")

            await self.handle_i_format(frame, writer)
        elif frame.is_s_format:
            # S格式确认序号验证：N(R) 不能大于已发送序号
            # N(R) 表示期望接收的下一个序号，所以 N(R) 应该 <= send_seq
            if frame.recv_seq > self.send_seq:
                logger.error(f"[S-RX] SEQUENCE ERROR: N(R)={frame.recv_seq} > send_seq={self.send_seq} - Disconnecting!")
                writer.close()
                await writer.wait_closed()
                self.connected = False
                return
            
            # 更新最后确认序号
            old_ack = self.last_ack_seq
            self.last_ack_seq = frame.recv_seq
            
            # 计算确认的帧数
            confirmed = self.unconfirmed_frames()
            was_unconfirmed = (old_ack - self.last_ack_seq) if old_ack >= self.last_ack_seq else (old_ack + 32768 - self.last_ack_seq)
            actual_confirmed = self.last_ack_seq - old_ack if self.last_ack_seq >= old_ack else (self.last_ack_seq + 32768 - old_ack)
            
            logger.info(f"[S-RX] Peer acknowledged: N(R)={frame.recv_seq}, "
                       f"send_seq={self.send_seq}, confirmed={actual_confirmed} frames, "
                       f"unconfirmed={self.unconfirmed_frames()}")

    async def handle_u_format(self, frame: Frame, writer: asyncio.StreamWriter):
        """处理U格式帧"""
        u_type = frame.u_format_type

        if u_type == 0x07:  # STARTDT_ACT
            logger.info("收到STARTDT启动命令")
            self.started = True
            # 响应STARTDT_CON
            response = FrameCodec.encode_u_format(0x0B)
            writer.write(response)
            await writer.drain()
            logger.info("发送STARTDT确认")
            
            # ========== 测试模式: oversized_frame ==========
            if self.test_mode == 'oversized_frame':
                await self.send_oversized_frame_test(writer)
            
            # ========== 测试模式: flood ==========
            if self.test_mode == 'flood':
                await self.send_flood_test(writer)

        elif u_type == 0x13:  # STOPDT_ACT
            logger.info("收到STOPDT停止命令")
            self.started = False
            # 响应STOPDT_CON
            response = FrameCodec.encode_u_format(0x23)
            writer.write(response)
            await writer.drain()

        elif u_type == 0x43:  # TESTFR_ACT
            logger.info("收到TESTFR测试帧")
            # 响应TESTFR_CON
            response = FrameCodec.encode_u_format(0x83)
            writer.write(response)
            await writer.drain()
    
    async def send_oversized_frame_test(self, writer: asyncio.StreamWriter):
        """
        发送超长帧测试 (P0: 帧长度校验)
        
        发送超过最大允许长度(2045字节)的帧，测试主站是否会断开连接
        """
        logger.warning("[TEST] 发送超长帧测试...")
        
        # 构建超长帧 (3000字节ASDU)
        oversized_data = b'\x00' * 3000
        asdu = ASDU(0x01, 1, 1, 0, self.device_addr, oversized_data)
        
        # 手动构建帧头
        frame_data = bytearray()
        frame_data.append(0x68)  # 启动字符
        # 长度 = APCI(4) + ASDU长度 (3006 > 2045)
        length = 4 + len(asdu.encode())
        frame_data.extend(struct.pack('<H', length))  # 2字节长度
        # APCI (I格式)
        frame_data.extend(struct.pack('<H', self.send_seq << 1))  # N(S)
        frame_data.extend(struct.pack('<H', self.recv_seq << 1))  # N(R)
        # ASDU
        frame_data.extend(asdu.encode())
        
        writer.write(bytes(frame_data))
        await writer.drain()
        
        logger.warning(f"[TEST] 已发送超长帧: ASDU长度={len(oversized_data)}, 总长度={length}")
        self.send_seq = (self.send_seq + 1) & 0x7FFF
    
    async def send_flood_test(self, writer: asyncio.StreamWriter):
        """
        发送大量帧测试 (P0: 接收缓冲区限制)
        
        快速连续发送大量I帧，测试主站接收缓冲区处理
        """
        logger.warning("[TEST] 发送大量帧测试...")
        
        for i in range(50):
            # 构建简单的遥信数据
            asdu = ASDUBuilder.build_single_point(
                self.device_addr, 200, i + 1, 2, cot=COT.SPONTANEOUS
            )
            frame = self.build_i_frame(asdu)
            writer.write(frame)
            
            # 不等待drain，快速发送
            if (i + 1) % 10 == 0:
                await writer.drain()
                logger.info(f"[TEST] 已发送 {i + 1} 帧...")
        
        await writer.drain()
        logger.warning(f"[TEST] 完成: 发送了 50 帧")

    async def handle_i_format(self, frame: Frame, writer: asyncio.StreamWriter):
        """处理I格式帧"""
        if not self.started:
            logger.warning("未启动,忽略I格式帧")
            return

        asdu = ASDU.parse(frame.asdu_data)
        logger.info(f"收到ASDU: TI={asdu.ti:02X} COT={asdu.cot} ADDR={asdu.asdu_addr}")

        # ========== 测试模式: no_response ==========
        if self.test_mode == 'no_response':
            logger.warning(f"[TEST] no_response模式: 不响应命令 (TI={asdu.ti:02X})")
            return

        response = None

        if asdu.ti == TI.GI_CMD:
            # 总召唤命令
            response = await self.handle_gi_cmd(asdu)
        elif asdu.ti == TI.GENERIC_READ:
            # 通用分类读命令
            response = await self.handle_generic_read(asdu)

        if response:
            await self.send_response(response, writer)
            
            # ========== 测试模式: ext_types ==========
            if self.test_mode == 'ext_types' and self.test_frame_sent == 0:
                await self.send_extended_types_test(writer)

    async def handle_gi_cmd(self, asdu: ASDU) -> Optional[bytes]:
        """处理总召唤命令"""
        fun, inf, scn = ASDUParser.parse_gi_cmd(asdu)
        self.current_scn = scn
        logger.info(f"总召唤命令: FUN={fun} INF={inf} SCN={scn}")

        # 发送S格式确认
        # (在send_response中处理)

        # 生成遥信数据响应
        frames = []

        # 获取总召唤遥信点
        gi_points = self.digital_gen.get_gi_points()

        # 分帧发送 (每帧最多20个点)
        batch_size = 20
        for i in range(0, len(gi_points), batch_size):
            batch = gi_points[i:i+batch_size]
            points_data = [(p.fun, p.inf, int(p.value)) for p in batch]

            asdu_resp = ASDUBuilder.build_double_point(
                self.device_addr, points_data, COT.GI, self.current_scn
            )
            frames.append(self.build_i_frame(asdu_resp))

        # 发送总召唤结束
        asdu_end = ASDUBuilder.build_gi_end(self.device_addr, self.current_scn)
        frames.append(self.build_i_frame(asdu_end))

        logger.info(f"总召唤响应: {len(gi_points)}个遥信点, {len(frames)}帧")

        return frames

    async def handle_generic_read(self, asdu: ASDU) -> Optional[bytes]:
        """处理通用分类读命令"""
        fun, inf, rii, gin_group, gin_entry, kod = ASDUParser.parse_generic_read(asdu)
        logger.info(f"通用分类读: FUN={fun:02X} INF={inf:02X} RII={rii} GIN={gin_group}/{gin_entry} KOD={kod:02X}")

        frames = []

        if inf == INF.READ_GROUP_ALL:
            # 读一组全部条目
            frames = self.build_generic_group_response(rii, gin_group, gin_entry)
        elif inf == INF.READ_SINGLE_CATALOG:
            # 读单个条目目录 (ASDU11响应)
            frames = self.build_catalog_response(rii, gin_group, gin_entry, kod)
        elif inf == INF.READ_SINGLE_VALUE:
            # 读单个条目
            frames = self.build_generic_single_response(rii, gin_group, gin_entry)

        return frames
    
    def build_catalog_response(self, rii: int, group: int, entry: int, kod: int) -> list:
        """
        构建ASDU11目录响应
        
        目录包含多个KOD的描述:
        - KOD 0x01: 实际值
        - KOD 0x0A: 描述
        - KOD 0x67: 属性结构
        """
        frames = []
        
        # 获取数据点
        point = self.generic_gen.get_point(group, entry)
        if not point:
            logger.warning(f"未找到数据点: 组{group} 条目{entry}")
            return frames
        
        # 构建目录条目
        entries = []
        
        # 实际值 (KOD=0x01)
        if point.data_type == GenericDataGenerator.DATA_TYPE_FLOAT:
            entries.append((KOD.ACTUAL_VALUE, DataType.R32_23, 4, struct.pack('<f', float(point.value))))
        elif point.data_type == GenericDataGenerator.DATA_TYPE_UINT:
            entries.append((KOD.ACTUAL_VALUE, DataType.UNSIGNED, 4, struct.pack('<I', int(point.value))))
        elif point.data_type == GenericDataGenerator.DATA_TYPE_INT:
            entries.append((KOD.SIGNED, DataType.SIGNED, 4, struct.pack('<i', int(point.value))))
        elif point.data_type == GenericDataGenerator.DATA_TYPE_ASCII:
            encoded = point.value.encode('ascii')[:20] if isinstance(point.value, str) else str(point.value).encode('ascii')[:20]
            entries.append((KOD.ACTUAL_VALUE, DataType.OS8ASCII, len(encoded), encoded))
        elif point.data_type == GenericDataGenerator.DATA_TYPE_DPI:
            entries.append((KOD.ACTUAL_VALUE, DataType.DPI, 1, bytes([int(point.value) & 0x03])))
        
        # 描述 (KOD=0x0A)
        desc = point.name.encode('ascii')[:20]
        entries.append((KOD.DESCRIPTION, DataType.OS8ASCII, len(desc), desc))
        
        # 属性结构 (KOD=0x67) - 南网扩展
        # 简化版本：包含数据类型、量程等
        attr_data = bytearray()
        attr_data.append(point.data_type)  # 数据类型
        attr_data.extend(struct.pack('<f', float(point.min_val) if point.min_val else 0))  # 最小值
        attr_data.extend(struct.pack('<f', float(point.max_val) if point.max_val else 0))  # 最大值
        attr_data.extend(point.unit.encode('ascii')[:8].ljust(8, b'\x00'))  # 单位
        entries.append((KOD.ATTRIBUTE, DataType.DATA_STRUCTURE, len(attr_data), bytes(attr_data)))
        
        asdu_resp = ASDUBuilder.build_catalog_data(
            self.device_addr, rii, group, entry, entries
        )
        frames.append(self.build_i_frame(asdu_resp))
        
        logger.info(f"ASDU11目录响应: 组{group} 条目{entry} {len(entries)}个KOD")
        
        return frames
    
    async def send_extended_types_test(self, writer: asyncio.StreamWriter):
        """
        发送南网扩展数据类型测试 (213-217)
        
        DataType 213: 带绝对时间的七字节时标报文
        DataType 214: 带相对时间的七字节时标报文
        DataType 215: 带相对时间七字节时标的浮点值
        DataType 216: 带相对时间七字节时标的整形值
        DataType 217: 带相对时间七字节时标的字符值
        """
        logger.info("[TEST] 发送南网扩展数据类型测试...")
        
        # DataType 215: 带相对时间七字节时标的浮点值
        asdu_215 = ASDUBuilder.build_generic_data_with_time(
            self.device_addr, 1, 100, 1, 
            DataType.FLOAT_WITH_TIME, 123.45,
            relative_time=500, fault_num=1
        )
        frame_215 = self.build_i_frame(asdu_215)
        writer.write(frame_215)
        await writer.drain()
        logger.info(f"[TEST] 发送 DataType 215 (带时标浮点): 123.45")
        
        # DataType 216: 带相对时间七字节时标的整形值
        asdu_216 = ASDUBuilder.build_generic_data_with_time(
            self.device_addr, 2, 100, 2,
            DataType.INT_WITH_TIME, 65535,
            relative_time=1000, fault_num=2
        )
        frame_216 = self.build_i_frame(asdu_216)
        writer.write(frame_216)
        await writer.drain()
        logger.info(f"[TEST] 发送 DataType 216 (带时标整数): 65535")
        
        # DataType 217: 带相对时间七字节时标的字符值
        asdu_217 = ASDUBuilder.build_generic_data_with_time(
            self.device_addr, 3, 100, 3,
            DataType.CHAR_WITH_TIME, 0x41,  # 'A'
            relative_time=1500, fault_num=3
        )
        frame_217 = self.build_i_frame(asdu_217)
        writer.write(frame_217)
        await writer.drain()
        logger.info(f"[TEST] 发送 DataType 217 (带时标字符): A")
        
        self.test_frame_sent = 1
        logger.info("[TEST] 南网扩展数据类型测试完成")

    def build_generic_group_response(self, rii: int, group: int, entry: int) -> list:
        """构建通用分类组数据响应 (遥测/遥脉统一)"""
        frames = []

        # 更新数据（模拟数据变化）
        self.generic_gen.update()
        
        # 获取指定组的数据点
        points = self.generic_gen.get_group_points(group)
        items = []
        
        for p in points:
            # 根据数据类型设置参数
            data_type = p.data_type  # 直接使用定义的数据类型
            
            if data_type == GenericDataGenerator.DATA_TYPE_ASCII:
                # ASCII字符串
                data_size = min(len(str(p.value)), 20)  # 最大20字节
                item = GenericDataItem(
                    gin_group=p.group,
                    gin_entry=p.entry,
                    kod=KOD.ACTUAL_VALUE,
                    data_type=DataType.OS8ASCII,
                    data_size=data_size,
                    value=p.value
                )
            elif data_type == GenericDataGenerator.DATA_TYPE_UINT:
                # 无符号整数
                item = GenericDataItem(
                    gin_group=p.group,
                    gin_entry=p.entry,
                    kod=KOD.ACTUAL_VALUE,
                    data_type=DataType.UNSIGNED,
                    data_size=4,
                    value=p.value
                )
            elif data_type == GenericDataGenerator.DATA_TYPE_INT:
                # 有符号整数
                item = GenericDataItem(
                    gin_group=p.group,
                    gin_entry=p.entry,
                    kod=KOD.ACTUAL_VALUE,
                    data_type=DataType.SIGNED,
                    data_size=4,
                    value=p.value
                )
            elif data_type == GenericDataGenerator.DATA_TYPE_FLOAT:
                # 浮点数 (R32.23)
                item = GenericDataItem(
                    gin_group=p.group,
                    gin_entry=p.entry,
                    kod=KOD.ACTUAL_VALUE,
                    data_type=DataType.R32_23,
                    data_size=4,
                    value=p.value
                )
            elif data_type == GenericDataGenerator.DATA_TYPE_DPI:
                # 双点信息
                item = GenericDataItem(
                    gin_group=p.group,
                    gin_entry=p.entry,
                    kod=KOD.ACTUAL_VALUE,
                    data_type=DataType.DPI,
                    data_size=1,
                    value=p.value
                )
            else:
                # 默认浮点
                item = GenericDataItem(
                    gin_group=p.group,
                    gin_entry=p.entry,
                    kod=KOD.ACTUAL_VALUE,
                    data_type=DataType.R32_23,
                    data_size=4,
                    value=p.value
                )
            items.append(item)

        if items:
            asdu_resp = ASDUBuilder.build_generic_data(
                self.device_addr, rii, items, COT.GENERIC_DATA_RESP
            )
            frames.append(self.build_i_frame(asdu_resp))
            type_names = {1: "ASCII", 3: "UINT", 4: "INT", 7: "FLOAT", 9: "DPI"}
            type_str = type_names.get(points[0].data_type if points else 0, "未知")
            logger.info(f"通用服务响应: 组{group} {len(items)}个点 (类型={type_str})")

        return frames

    def build_generic_single_response(self, rii: int, group: int, entry: int) -> list:
        """构建单个条目响应 (遥测/遥脉统一)"""
        frames = []

        # 更新数据
        self.generic_gen.update()
        
        # 查找数据点
        point = self.generic_gen.get_point(group, entry)
        if point:
            # 根据 data_type 判断数据类型
            if point.data_type == GenericDataGenerator.DATA_TYPE_FLOAT:
                data_type = DataType.FLOAT
            elif point.data_type == GenericDataGenerator.DATA_TYPE_UINT:
                data_type = DataType.UNSIGNED
            else:
                data_type = DataType.FLOAT
            
            item = GenericDataItem(
                gin_group=point.group,
                gin_entry=point.entry,
                kod=KOD.ACTUAL_VALUE,
                data_type=data_type,
                data_size=4,
                value=point.value
            )
            asdu_resp = ASDUBuilder.build_generic_data(
                self.device_addr, rii, [item], COT.GENERIC_DATA_RESP
            )
            frames.append(self.build_i_frame(asdu_resp))

        return frames

    def build_i_frame(self, asdu: ASDU) -> bytes:
        """构建I格式帧"""
        apci = APCI('I', self.send_seq, self.recv_seq)
        frame = FrameCodec.encode(apci, asdu.encode())
        unconfirmed = self.unconfirmed_frames() + 1  # 发送后未确认数
        logger.info(f"[I-TX] N(S)={self.send_seq} N(R)={self.recv_seq}, TI={asdu.ti}, unconfirmed={unconfirmed}")
        self.send_seq = (self.send_seq + 1) & 0x7FFF
        return frame

    async def send_response(self, response, writer: asyncio.StreamWriter):
        """发送响应（实现k值控制）"""
        if isinstance(response, list):
            logger.info(f"[I-TX] Sending {len(response)} I-frames...")
            sent_count = 0
            for i, frame in enumerate(response):
                # k值控制：检查未确认帧数
                unconfirmed = self.unconfirmed_frames()
                if unconfirmed >= self.max_unconfirmed:
                    logger.warning(f"[k-Control] Send blocked at frame {i+1}/{len(response)}: "
                                 f"unconfirmed={unconfirmed} >= k={self.max_unconfirmed}")
                    logger.info(f"[k-Control] Waiting for master's S-Frame acknowledgment...")
                    # 在实际实现中，这里应该等待主站的S帧确认
                    # 为了简化，我们继续发送（但会记录警告）
                
                writer.write(frame)
                await writer.drain()
                sent_count += 1
                logger.debug(f"[I-TX] Frame {i+1}/{len(response)} sent")
        else:
            writer.write(response)
            await writer.drain()

    async def send_spontaneous_event(self, writer: asyncio.StreamWriter):
        """发送突发遥信事件 (ASDU1)"""
        if not self.started or not self.connected:
            return

        # 随机选择一个遥信点发送
        import random
        points = self.digital_gen.get_gi_points()
        if points:
            point = random.choice(points)
            # 翻转状态
            point.value = not point.value

            # 构建ASDU1
            asdu = ASDUBuilder.build_single_point(
                self.device_addr,
                point.fun,
                point.inf,
                2 if point.value else 1,  # DPI: 1=OFF, 2=ON
                cot=COT.SPONTANEOUS
            )
            frame = self.build_i_frame(asdu)
            writer.write(frame)
            await writer.drain()

            logger.info(f"突发遥信 (ASDU1): FUN={point.fun:02X} INF={point.inf:02X} DPI={2 if point.value else 1}")

    async def send_protection_event(self, writer: asyncio.StreamWriter):
        """发送保护动作事件 (ASDU2)"""
        if not self.started or not self.connected:
            return

        # 构建ASDU2 - 保护动作事件
        data = bytearray()
        data.append(0x80)  # FUN = 距离保护
        data.append(0x01)  # INF = 保护动作
        data.append(0x02)  # DPI = ON (动作)
        data.extend(struct.pack('<H', 100))  # RET = 相对时间 100ms
        data.extend(struct.pack('<H', 1))    # FAN = 故障序号 1
        # 时标1 (实际发生时间)
        data.extend(CP56Time2a.now().encode())
        # 时标2 (子站接收时间)
        data.extend(CP56Time2a.now().encode())
        # SIN (故障相别)
        data.append(0x89)  # A相接地故障

        asdu = ASDU(TI.DOUBLE_POINT_TIME, 1, COT.SPONTANEOUS, 0, self.device_addr, bytes(data))
        frame = self.build_i_frame(asdu)
        writer.write(frame)
        await writer.drain()

        logger.info(f"保护动作事件 (ASDU2): FUN=80 INF=01 DPI=02 FAN=1")


async def main():
    parser = argparse.ArgumentParser(description='IEC103子站模拟器')
    parser.add_argument('--host', default='0.0.0.0', help='监听地址')
    parser.add_argument('--port', type=int, default=2404, help='监听端口')
    parser.add_argument('--device', type=int, default=1, help='装置地址')
    parser.add_argument('--log-level', default='info', help='日志级别')
    parser.add_argument('--test-mode', default='normal',
                       choices=['normal', 'no_ack', 'oversized_frame', 'no_response', 'flood', 'ext_types'],
                       help='''测试模式:
                         normal: 正常模式
                         no_ack: 不发送S帧确认(测试k值阻塞超时)
                         oversized_frame: 发送超长帧(测试帧长度校验)
                         no_response: 不响应命令(测试T1超时)
                         flood: 快速发送大量帧(测试缓冲区限制)
                         ext_types: 发送南网扩展数据类型(213-217)''')
    
    args = parser.parse_args()
    
    logging.getLogger().setLevel(getattr(logging, args.log_level.upper()))
    
    slave = IEC103Slave(args.device, args.test_mode)
    
    server = await asyncio.start_server(
        slave.handle_client,
        args.host,
        args.port
    )
    
    addr = server.sockets[0].getsockname()
    logger.info(f"IEC103子站模拟器启动: {addr[0]}:{addr[1]}")
    logger.info(f"装置地址: {args.device}")
    logger.info(f"测试模式: {args.test_mode}")
    logger.info(f"k值(最大未确认帧数): {slave.max_unconfirmed}")
    logger.info(f"w值(确认阈值): {slave.max_ack_delay}")
    
    if args.test_mode != 'normal':
        logger.warning(f"========== 测试模式: {args.test_mode} ==========")
        if args.test_mode == 'no_ack':
            logger.warning("[TEST] 将不发送S帧确认，测试主站k值阻塞超时")
        elif args.test_mode == 'oversized_frame':
            logger.warning("[TEST] 连接后发送超长帧，测试主站帧长度校验")
        elif args.test_mode == 'no_response':
            logger.warning("[TEST] 不响应任何I格式命令，测试主站T1超时")
        elif args.test_mode == 'flood':
            logger.warning("[TEST] 连接后快速发送大量帧，测试缓冲区限制")
        elif args.test_mode == 'ext_types':
            logger.warning("[TEST] 发送南网扩展数据类型(213-217)")
    
    async with server:
        await server.serve_forever()


if __name__ == '__main__':
    asyncio.run(main())
