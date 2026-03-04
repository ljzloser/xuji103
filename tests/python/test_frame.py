#!/usr/bin/env python3
"""Test frame encoding"""
import sys
sys.path.insert(0, '.')

from protocol.apci import FrameCodec, APCI, UFormat

# 测试U格式帧编码
frame = FrameCodec.encode_u_format(0x07)  # STARTDT_ACT
print('STARTDT_ACT帧:', frame.hex())
print('长度:', len(frame))

# 解析
apci, asdu = FrameCodec.parse(frame)
print('APCI type:', apci.format_type, 'U format:', hex(apci.u_format))
