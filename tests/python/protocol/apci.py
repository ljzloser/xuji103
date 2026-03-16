"""
APCI (Application Protocol Control Information) 编解码
南网以太网103规范 - 4字节控制域
"""
from dataclasses import dataclass
from enum import IntEnum
from typing import Tuple, Optional


class UFormat(IntEnum):
    """U格式控制域"""
    STARTDT_ACT = 0x07      # 启动数据传输
    STARTDT_CON = 0x0B      # 启动确认
    STOPDT_ACT = 0x13       # 停止数据传输
    STOPDT_CON = 0x23       # 停止确认
    TESTFR_ACT = 0x43       # 测试帧
    TESTFR_CON = 0x83       # 测试确认


@dataclass
class APCI:
    """APCI控制域"""
    format_type: str  # 'I', 'S', 'U'
    send_seq: int = 0     # N(S) 发送序号
    recv_seq: int = 0     # N(R) 接收序号
    u_format: Optional[UFormat] = None
    
    @staticmethod
    def parse(data: bytes) -> 'APCI':
        """解析4字节APCI控制域"""
        if len(data) < 4:
            raise ValueError("APCI数据长度不足")
        
        byte1 = data[0]
        
        # 判断帧格式
        if byte1 & 0x01 == 0:
            # I格式: N(S)低7位在byte1高7位, N(S)高8位在byte2
            format_type = 'I'
            send_seq = (data[1] << 7) | (byte1 >> 1)
            recv_seq = (data[3] << 7) | (data[2] >> 1)
            return APCI(format_type, send_seq, recv_seq)
        
        elif byte1 == 0x01:
            # S格式: 00000001
            format_type = 'S'
            recv_seq = (data[3] << 7) | (data[2] >> 1)
            return APCI(format_type, 0, recv_seq)
        
        else:
            # U格式
            format_type = 'U'
            u_val = UFormat(byte1)
            return APCI(format_type, 0, 0, u_val)
    
    def encode(self) -> bytes:
        """编码为4字节"""
        if self.format_type == 'I':
            # I格式 - IEC 104 序号编码
            # byte1: N(S)低7位左移1位，bit0=0
            # byte2: N(S)高8位
            byte1 = (self.send_seq << 1) & 0xFF
            byte2 = (self.send_seq >> 7) & 0xFF
            byte3 = (self.recv_seq << 1) & 0xFF
            byte4 = (self.recv_seq >> 7) & 0xFF
            return bytes([byte1, byte2, byte3, byte4])
        
        elif self.format_type == 'S':
            # S格式 - IEC 104 序号编码
            byte3 = (self.recv_seq << 1) & 0xFF
            byte4 = (self.recv_seq >> 7) & 0xFF
            return bytes([0x01, 0x00, byte3, byte4])
        
        else:
            # U格式
            return bytes([self.u_format, 0x00, 0x00, 0x00])


class FrameCodec:
    """帧编解码器"""
    START_BYTE = 0x68
    
    @staticmethod
    def parse(data: bytes) -> Tuple[APCI, bytes]:
        """
        解析完整APDU帧
        南网规范帧格式 (参照104标准):
        68H | 长度L(2字节) | APCI(4字节) | ASDU
        无校验和，无结束字节
        返回: (APCI, ASDU数据)
        """
        if len(data) < 7:
            raise ValueError("帧数据太短")
        
        if data[0] != FrameCodec.START_BYTE:
            raise ValueError(f"无效启动字节: {data[0]:02X}")
        
        # 长度域 (2字节) - APCI + ASDU的总长度
        length = data[1] | (data[2] << 8)
        
        # 南网规范: 总帧长 = 启动(1) + 长度(2) + APCI+ASDU(length) = 3 + length
        if len(data) < length + 3:
            raise ValueError(f"帧数据不完整: 期望{length + 3}字节, 实际{len(data)}字节")
        
        # 解析APCI
        apci = APCI.parse(data[3:7])
        
        # ASDU数据
        asdu_data = data[7:length + 3] if length > 4 else b''
        
        return apci, asdu_data
    
    @staticmethod
    def encode(apci: APCI, asdu: bytes = b'') -> bytes:
        """
        编码完整APDU帧
        南网规范帧格式 (参照104标准):
        68H | 长度L(2字节) | APCI(4字节) | ASDU
        无校验和，无结束字节
        """
        apci_data = apci.encode()
        length = len(apci_data) + len(asdu)
        
        # 启动字节 + 长度域(2字节) + APCI + ASDU
        frame = bytearray()
        frame.append(FrameCodec.START_BYTE)
        frame.append(length & 0xFF)
        frame.append((length >> 8) & 0xFF)
        frame.extend(apci_data)
        frame.extend(asdu)
        
        return bytes(frame)
    
    @staticmethod
    def encode_u_format(u_format: UFormat) -> bytes:
        """编码U格式帧"""
        return FrameCodec.encode(APCI('U', u_format=u_format))
    
    @staticmethod
    def encode_s_format(recv_seq: int) -> bytes:
        """编码S格式帧"""
        return FrameCodec.encode(APCI('S', recv_seq=recv_seq))
