#!/usr/bin/env python3
"""测试帧格式是否正确"""
import sys
sys.path.insert(0, '/root/work/C++/xuji103/tests/python')

from protocol.apci import FrameCodec, APCI, UFormat

def test_u_frame():
    """测试U格式帧"""
    print("=== U格式帧测试 ===")
    
    # STARTDT_ACT
    frame = FrameCodec.encode_u_format(UFormat.STARTDT_ACT)
    print(f"STARTDT_ACT: {frame.hex()}")
    
    # 预期: 68 04 00 07 00 00 00 (启动+长度+APCI)
    # 长度=4 (APCI长度)
    # APCI=07 00 00 00
    expected = bytes([0x68, 0x04, 0x00, 0x07, 0x00, 0x00, 0x00])
    if frame == expected:
        print(f"  ✓ 正确! 期望: {expected.hex()}")
    else:
        print(f"  ✗ 错误! 期望: {expected.hex()}")
        return False
    
    # STARTDT_CON
    frame = FrameCodec.encode_u_format(UFormat.STARTDT_CON)
    print(f"STARTDT_CON: {frame.hex()}")
    expected = bytes([0x68, 0x04, 0x00, 0x0B, 0x00, 0x00, 0x00])
    if frame == expected:
        print(f"  ✓ 正确!")
    else:
        print(f"  ✗ 错误! 期望: {expected.hex()}")
        return False
    
    return True

def test_s_frame():
    """测试S格式帧"""
    print("\n=== S格式帧测试 ===")
    
    frame = FrameCodec.encode_s_format(5)
    print(f"S-frame N(R)=5: {frame.hex()}")
    
    # 预期: 68 04 00 01 00 0A 00
    # 长度=4
    # APCI=01 00 (S格式) + N(R)=5 -> 0A 00
    expected = bytes([0x68, 0x04, 0x00, 0x01, 0x00, 0x0A, 0x00])
    if frame == expected:
        print(f"  ✓ 正确! 期望: {expected.hex()}")
    else:
        print(f"  ✗ 错误! 期望: {expected.hex()}")
        return False
    
    return True

def test_i_frame():
    """测试I格式帧"""
    print("\n=== I格式帧测试 ===")
    
    asdu = bytes([0x07, 0x01, 0x09, 0x00, 0x01, 0x00, 0xFF, 0x00, 0x01])  # GI命令
    apci = APCI('I', send_seq=0, recv_seq=0)
    frame = FrameCodec.encode(apci, asdu)
    print(f"I-frame N(S)=0 N(R)=0 ASDU=GI: {frame.hex()}")
    
    # 预期: 68 0D 00 00 00 00 00 07 01 09 00 01 00 FF 00 01
    # 长度=13 (4 + 9)
    # APCI=00 00 00 00 (N(S)=0, N(R)=0)
    # ASDU=07 01 09 00 01 00 FF 00 01
    expected = bytes([0x68, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 
                      0x07, 0x01, 0x09, 0x00, 0x01, 0x00, 0xFF, 0x00, 0x01])
    if frame == expected:
        print(f"  ✓ 正确!")
    else:
        print(f"  ✗ 错误! 期望: {expected.hex()}")
        return False
    
    return True

def test_parse():
    """测试解析"""
    print("\n=== 帧解析测试 ===")
    
    # U格式帧
    frame = bytes([0x68, 0x04, 0x00, 0x07, 0x00, 0x00, 0x00])
    apci, asdu = FrameCodec.parse(frame)
    print(f"解析 STARTDT_ACT: type={apci.format_type}, u_format={apci.u_format}")
    if apci.format_type == 'U' and apci.u_format == UFormat.STARTDT_ACT:
        print("  ✓ 正确!")
    else:
        print("  ✗ 错误!")
        return False
    
    # I格式帧
    frame = bytes([0x68, 0x0D, 0x00, 0x00, 0x00, 0x0A, 0x00,
                   0x07, 0x01, 0x09, 0x00, 0x01, 0x00, 0xFF, 0x00, 0x01])
    apci, asdu = FrameCodec.parse(frame)
    print(f"解析 I-frame: type={apci.format_type}, N(S)={apci.send_seq}, N(R)={apci.recv_seq}")
    print(f"  ASDU: {asdu.hex()}")
    if apci.format_type == 'I' and apci.send_seq == 0 and apci.recv_seq == 5:
        print("  ✓ 正确!")
    else:
        print("  ✗ 错误!")
        return False
    
    return True

if __name__ == '__main__':
    all_pass = True
    all_pass &= test_u_frame()
    all_pass &= test_s_frame()
    all_pass &= test_i_frame()
    all_pass &= test_parse()
    
    print("\n" + "="*50)
    if all_pass:
        print("所有测试通过!")
    else:
        print("有测试失败!")
        sys.exit(1)
