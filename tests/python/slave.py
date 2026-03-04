#!/usr/bin/env python3
"""
南网以太网103协议子站模拟器
用于测试主站程序
"""
import asyncio
import argparse
import logging
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
from data import AnalogDataGenerator, DigitalDataGenerator, CounterDataGenerator


logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s'
)
logger = logging.getLogger(__name__)


class IEC103Slave:
    """IEC103子站模拟器"""
    
    def __init__(self, device_addr: int = 1):
        self.device_addr = device_addr
        self.send_seq = 0
        self.recv_seq = 0
        self.connected = False
        self.started = False
        self.frame_parser = FrameParser()
        
        # 数据生成器
        self.analog_gen = AnalogDataGenerator()
        self.digital_gen = DigitalDataGenerator()
        self.counter_gen = CounterDataGenerator()
        
        # 当前总召唤序号
        self.current_scn = 0
    
    def reset(self):
        """重置状态"""
        self.send_seq = 0
        self.recv_seq = 0
        self.started = False
    
    async def handle_client(self, reader: asyncio.StreamReader, 
                           writer: asyncio.StreamWriter):
        """处理客户端连接"""
        addr = writer.get_extra_info('peername')
        logger.info(f"主站连接: {addr}")
        
        self.connected = True
        self.reset()
        
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
            writer.close()
            try:
                await writer.wait_closed()
            except:
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
            self.recv_seq = frame.send_seq + 1
            await self.handle_i_format(frame, writer)
        elif frame.is_s_format:
            # S格式仅确认,无需处理
            pass
    
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
        
        elif u_type == 0x13:  # STOPDT_ACT
            logger.info("收到STOPDT停止命令")
            self.started = False
            # 响应STOPDT_CON
            response = FrameCodec.encode_u_format(0x23)
            writer.write(response)
            await writer.drain()
        
        elif u_type == 0x43:  # TESTFR_ACT
            logger.debug("收到TESTFR测试帧")
            # 响应TESTFR_CON
            response = FrameCodec.encode_u_format(0x83)
            writer.write(response)
            await writer.drain()
    
    async def handle_i_format(self, frame: Frame, writer: asyncio.StreamWriter):
        """处理I格式帧"""
        if not self.started:
            logger.warning("未启动,忽略I格式帧")
            return
        
        asdu = ASDU.parse(frame.asdu_data)
        logger.debug(f"收到ASDU: TI={asdu.ti:02X} COT={asdu.cot} ADDR={asdu.asdu_addr}")
        
        response = None
        
        if asdu.ti == TI.GI_CMD:
            # 总召唤命令
            response = await self.handle_gi_cmd(asdu)
        elif asdu.ti == TI.GENERIC_READ:
            # 通用分类读命令
            response = await self.handle_generic_read(asdu)
        
        if response:
            await self.send_response(response, writer)
    
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
        elif inf == INF.READ_SINGLE_VALUE:
            # 读单个条目
            frames = self.build_generic_single_response(rii, gin_group, gin_entry)
        
        return frames
    
    def build_generic_group_response(self, rii: int, group: int, entry: int) -> list:
        """构建通用分类组数据响应"""
        frames = []
        
        if group in (8, 9, 10):
            # 遥测组
            points = self.analog_gen.get_group_points(group)
            items = []
            for p in points:
                item = GenericDataItem(
                    gin_group=p.group,
                    gin_entry=p.entry,
                    kod=KOD.ACTUAL_VALUE,
                    data_type=DataType.FLOAT,
                    data_size=4,
                    value=p.value
                )
                items.append(item)
            
            if items:
                asdu_resp = ASDUBuilder.build_generic_data(
                    self.device_addr, rii, items, COT.GENERIC_DATA_RESP
                )
                frames.append(self.build_i_frame(asdu_resp))
                logger.info(f"遥测响应: 组{group} {len(items)}个点")
        
        elif group in (16, 17):
            # 遥脉组
            points = self.counter_gen.get_group_points(group)
            items = []
            for p in points:
                item = GenericDataItem(
                    gin_group=p.group,
                    gin_entry=p.entry,
                    kod=KOD.ACTUAL_VALUE,
                    data_type=DataType.UNSIGNED,
                    data_size=4,
                    value=p.value
                )
                items.append(item)
            
            if items:
                asdu_resp = ASDUBuilder.build_generic_data(
                    self.device_addr, rii, items, COT.GENERIC_DATA_RESP
                )
                frames.append(self.build_i_frame(asdu_resp))
                logger.info(f"遥脉响应: 组{group} {len(items)}个点")
        
        return frames
    
    def build_generic_single_response(self, rii: int, group: int, entry: int) -> list:
        """构建单个条目响应"""
        frames = []
        
        # 查找遥测点
        point = self.analog_gen.get_point(group, entry)
        if point:
            item = GenericDataItem(
                gin_group=point.group,
                gin_entry=point.entry,
                kod=KOD.ACTUAL_VALUE,
                data_type=DataType.FLOAT,
                data_size=4,
                value=point.value
            )
            asdu_resp = ASDUBuilder.build_generic_data(
                self.device_addr, rii, [item], COT.GENERIC_DATA_RESP
            )
            frames.append(self.build_i_frame(asdu_resp))
            return frames
        
        # 查找遥脉点
        point = self.counter_gen.get_point(group, entry)
        if point:
            item = GenericDataItem(
                gin_group=point.group,
                gin_entry=point.entry,
                kod=KOD.ACTUAL_VALUE,
                data_type=DataType.UNSIGNED,
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
        self.send_seq = (self.send_seq + 1) & 0x7FFF
        return FrameCodec.encode(apci, asdu.encode())
    
    async def send_response(self, response, writer: asyncio.StreamWriter):
        """发送响应"""
        if isinstance(response, list):
            for frame in response:
                writer.write(frame)
                await writer.drain()
                # 发送S格式确认
                ack = FrameCodec.encode_s_format(self.recv_seq)
                writer.write(ack)
                await writer.drain()
        else:
            writer.write(response)
            await writer.drain()
            ack = FrameCodec.encode_s_format(self.recv_seq)
            writer.write(ack)
            await writer.drain()


async def main():
    parser = argparse.ArgumentParser(description='IEC103子站模拟器')
    parser.add_argument('--host', default='0.0.0.0', help='监听地址')
    parser.add_argument('--port', type=int, default=2404, help='监听端口')
    parser.add_argument('--device', type=int, default=1, help='装置地址')
    parser.add_argument('--log-level', default='info', help='日志级别')
    
    args = parser.parse_args()
    
    logging.getLogger().setLevel(getattr(logging, args.log_level.upper()))
    
    slave = IEC103Slave(args.device)
    
    server = await asyncio.start_server(
        slave.handle_client,
        args.host,
        args.port
    )
    
    addr = server.sockets[0].getsockname()
    logger.info(f"IEC103子站模拟器启动: {addr[0]}:{addr[1]}")
    logger.info(f"装置地址: {args.device}")
    
    async with server:
        await server.serve_forever()


if __name__ == '__main__':
    asyncio.run(main())
