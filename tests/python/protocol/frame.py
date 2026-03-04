"""
帧处理模块
"""
from dataclasses import dataclass
from typing import Optional, Tuple
from .apci import APCI, FrameCodec, UFormat


@dataclass
class Frame:
    """完整帧"""
    apci: APCI
    asdu_data: bytes
    
    @staticmethod
    def parse(data: bytes) -> 'Frame':
        """解析帧"""
        apci, asdu_data = FrameCodec.parse(data)
        return Frame(apci, asdu_data)
    
    def encode(self) -> bytes:
        """编码帧"""
        return FrameCodec.encode(self.apci, self.asdu_data)
    
    @property
    def is_i_format(self) -> bool:
        return self.apci.format_type == 'I'
    
    @property
    def is_s_format(self) -> bool:
        return self.apci.format_type == 'S'
    
    @property
    def is_u_format(self) -> bool:
        return self.apci.format_type == 'U'
    
    @property
    def u_format_type(self) -> Optional[UFormat]:
        if self.is_u_format:
            return self.apci.u_format
        return None
    
    @property
    def send_seq(self) -> int:
        return self.apci.send_seq
    
    @property
    def recv_seq(self) -> int:
        return self.apci.recv_seq


class FrameParser:
    """帧解析器 - 处理TCP流"""
    
    def __init__(self):
        self.buffer = bytearray()
    
    def feed(self, data: bytes) -> None:
        """添加数据到缓冲区"""
        self.buffer.extend(data)
    
    def has_frame(self) -> bool:
        """检查是否有完整帧"""
        if len(self.buffer) < 5:
            return False
        
        if self.buffer[0] != 0x68:
            return False
        
        length = self.buffer[1] | (self.buffer[2] << 8)
        # 总长度 = 启动(1) + 长度(2) + APCI+ASDU(length) + 校验(1) + 结束(1) = 5 + length
        return len(self.buffer) >= length + 5
    
    def get_frame(self) -> Optional[Frame]:
        """获取一个完整帧"""
        if not self.has_frame():
            return None
        
        length = self.buffer[1] | (self.buffer[2] << 8)
        frame_data = bytes(self.buffer[:length + 5])
        del self.buffer[:length + 5]
        
        return Frame.parse(frame_data)
    
    def get_all_frames(self) -> list:
        """获取所有完整帧"""
        frames = []
        while self.has_frame():
            frame = self.get_frame()
            if frame:
                frames.append(frame)
        return frames
    
    def clear(self) -> None:
        """清空缓冲区"""
        self.buffer.clear()
