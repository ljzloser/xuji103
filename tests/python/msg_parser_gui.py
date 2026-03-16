#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
南网以太网103协议报文解析器 - GUI版本

功能:
- 解析报文并显示每个字段对应的字节位置
- 点击字段时高亮对应的原始字节
- 可视化报文结构
"""

import re
import struct
import tkinter as tk
from tkinter import ttk, filedialog
from datetime import datetime
from dataclasses import dataclass, field
from typing import List, Optional, Tuple, Dict


# ==================== 常量定义 ====================

APCI_TYPE_I = 0
APCI_TYPE_S = 1
APCI_TYPE_U = 3

U_FORMAT = {
    0x07: "STARTDT_act", 0x0B: "STARTDT_con",
    0x13: "STOPDT_act", 0x23: "STOPDT_con",
    0x43: "TESTFR_act", 0x83: "TESTFR_con",
}

ASDU_TYPES = {
    0x01: "ASDU1-带时标报文(双点遥信)", 
    0x02: "ASDU2-带相对时间时标报文",
    0x07: "ASDU7-总召唤命令", 
    0x08: "ASDU8-总召唤结束",
    0x0A: "ASDU10-通用分类数据", 
    0x0B: "ASDU11-通用分类标识",
    0x15: "ASDU21-通用分类读命令", 
    0x2A: "ASDU42-双点信息状态",
}

COT_NAMES = {
    1: "自发/突发", 2: "循环", 3: "复位FCB", 4: "复位CU",
    5: "启动/重启", 6: "电源合上", 7: "测试模式", 8: "时间同步",
    9: "总召唤", 10: "总召唤终止", 11: "当地操作", 12: "远方操作",
    20: "命令肯定认可", 21: "命令否定认可", 31: "扰动数据传送",
    40: "写命令肯定", 41: "写命令否定", 42: "读命令有效响应", 43: "读命令无效响应",
}

FUN_NAMES = {
    0xFE: "GEN(通用分类)", 
    0xFF: "GLB(全局)", 
    128: "t(Z)距离保护", 
    160: "I>>过流保护", 
    176: "IT变压器差动", 
    192: "IL线路差动"
}

INF_GENERIC = {
    0xF0: "读全部组标题", 
    0xF1: "读一组全部条目", 
    0xF3: "读单条目录", 
    0xF4: "读单条值",
    0xF5: "总召唤中止",
}

DATA_TYPES = {
    1: "OS8ASCII字符串", 
    2: "BS16位串", 
    3: "UINT无符号整数", 
    5: "INT有符号整数", 
    7: "R32.23浮点数",
    8: "CP56Time2a时标",
    22: "失败回答码", 
    23: "数据结构",
    213: "带绝对时标报文", 
    214: "带相对时标报文", 
    215: "带相对时标浮点", 
    216: "带相对时标整数", 
    217: "带相对时标字符",
}

KOD_NAMES = {
    0x01: "实际值(从装置取)", 
    0x0A: "描述字符串", 
    0x67: "属性结构(南网扩展)", 
    0xA8: "装置实际值(南网)", 
    0xA9: "数据库值(南网)",
    0x00: "未用",
}


# ==================== 数据结构 ====================

@dataclass
class FieldInfo:
    """字段信息：包含名称、值、字节位置"""
    name: str
    value: str
    start: int  # 字节起始位置(0-based)
    end: int    # 字节结束位置(不包含)
    children: List['FieldInfo'] = field(default_factory=list)


@dataclass
class Frame:
    raw: bytes
    length: int
    frame_type: int
    timestamp: str = ""
    direction: str = ""
    fields: List[FieldInfo] = field(default_factory=list)


# ==================== 解析函数 ====================

def parse_log_line(line: str):
    match = re.match(r'\[(.*?)\].*?\[info\]\s+(recv|send):\s*(.*)', line)
    if match:
        try:
            hex_str = match.group(3).replace(' ', '')
            ascii_bytes = bytes.fromhex(hex_str)
            real_hex = ascii_bytes.decode('ascii').replace(' ', '')
            return (match.group(1), match.group(2), bytes.fromhex(real_hex))
        except:
            pass
    return None


def format_cp56time2a(data: bytes) -> str:
    if len(data) < 7:
        return "无效"
    ms = data[0] | (data[1] << 8)
    minute = data[2] & 0x3F
    hour = data[3] & 0x1F
    day = data[4] & 0x1F
    month = data[5] & 0x0F
    year = data[6]
    try:
        return f"{2000+year:04d}-{month:02d}-{day:02d} {hour:02d}:{minute:02d}:{ms//1000:02d}.{ms%1000:03d}"
    except:
        return f"无效({year}-{month}-{day})"


def format_dpi(val: int) -> str:
    return {0: "不确定", 1: "分/OFF", 2: "合/ON", 3: "不确定"}.get(val & 0x03, "??")


def parse_frame(data: bytes, timestamp: str = "", direction: str = "") -> Optional[Frame]:
    if len(data) < 6 or data[0] != 0x68:
        return None
    
    length = data[1] | (data[2] << 8)
    if len(data) < length + 3:
        return None
    
    fields = []
    
    # 启动字节
    fields.append(FieldInfo("启动字符", "68H", 0, 1))
    
    # 长度域
    fields.append(FieldInfo("长度L", f"{length} (0x{length:04X})", 1, 3))
    
    # APCI
    apci_start = 3
    byte1 = data[3]
    
    if byte1 & 0x01 == 0:  # I帧
        send_seq = (byte1 >> 1) | ((data[4] & 0xFE) << 7)
        recv_seq = (data[5] >> 1) | ((data[6] & 0xFE) << 7)
        
        apci = FieldInfo("APCI (I格式)", "", apci_start, 7)
        apci.children = [
            FieldInfo("控制域[0]", f"N(S)={send_seq}", 3, 5),
            FieldInfo("控制域[1]", f"N(R)={recv_seq}", 5, 7),
        ]
        fields.append(apci)
        frame_type = APCI_TYPE_I
        
        # ASDU
        if length > 4:
            asdu_start = 7
            asdu = FieldInfo("ASDU", "", asdu_start, 3 + length)
            
            type_id = data[7]
            vsq = data[8]
            cot = data[9]
            oa = data[10] & 0x3F
            pn = (data[10] >> 7) & 1
            test = bool((data[10] >> 6) & 1)
            asdu_addr = data[11] | (data[12] << 8)
            
            asdu.children = [
                FieldInfo("类型标识TI", f"{type_id:02X} ({ASDU_TYPES.get(type_id, f'未知类型{type_id}')})", 7, 8),
                FieldInfo("VSQ", f"{vsq:02X} (SQ={bool(vsq&0x80)}, 数目={vsq&0x7F})", 8, 9),
                FieldInfo("传输原因COT", f"{cot} ({COT_NAMES.get(cot, f'未知原因')})", 9, 10),
                FieldInfo("P/N/Test/OA", f"PN={pn} Test={test} OA={oa}", 10, 11),
                FieldInfo("ASDU地址", f"{asdu_addr} (0x{asdu_addr:04X})", 11, 13),
            ]
            
            # 信息对象
            info_start = 13
            info_data = data[13:3+length]
            
            if type_id == 0x07:  # ASDU7
                if len(info_data) >= 3:
                    fun = info_data[0]
                    inf = info_data[1]
                    scn = info_data[2]
                    info = FieldInfo("信息对象", "", info_start, info_start + 3)
                    info.children = [
                        FieldInfo("FUN", f"{fun:02X} ({FUN_NAMES.get(fun, f'功能类型{fun}')})", info_start, info_start+1),
                        FieldInfo("INF", f"{inf:02X}", info_start+1, info_start+2),
                        FieldInfo("SCN", f"{scn:02X} (扫描序号)", info_start+2, info_start+3),
                    ]
                    asdu.children.append(info)
            
            elif type_id == 0x08:  # ASDU8
                if len(info_data) >= 3:
                    fun = info_data[0]
                    inf = info_data[1]
                    scn = info_data[2]
                    info = FieldInfo("信息对象", "", info_start, info_start + 3)
                    info.children = [
                        FieldInfo("FUN", f"{fun:02X} ({FUN_NAMES.get(fun, f'功能类型{fun}')})", info_start, info_start+1),
                        FieldInfo("INF", f"{inf:02X}", info_start+1, info_start+2),
                        FieldInfo("SCN", f"{scn:02X} (总召唤结束标志)", info_start+2, info_start+3),
                    ]
                    asdu.children.append(info)
            
            elif type_id == 0x0A:  # ASDU10 通用分类数据
                # 结构: FUN(1) + INF(1) + RII(1) + NGD(1) + 数据项...
                if len(info_data) >= 4:
                    fun = info_data[0]
                    inf = info_data[1]
                    rii = info_data[2]
                    ngd = info_data[3]
                    num = ngd & 0x7F
                    cont = bool(ngd & 0x80)
                    
                    inf_name = INF_GENERIC.get(inf, f"INF=0x{inf:02X}")
                    
                    info = FieldInfo("信息对象", "", info_start, 3 + length)
                    info.children = [
                        FieldInfo("FUN", f"{fun:02X} ({FUN_NAMES.get(fun, f'功能类型{fun}')})", info_start, info_start+1),
                        FieldInfo("INF", f"{inf:02X} ({inf_name})", info_start+1, info_start+2),
                        FieldInfo("RII", f"{rii:02X} (返回信息标识)", info_start+2, info_start+3),
                        FieldInfo("NGD", f"数目={num} CONT={cont}", info_start+3, info_start+4),
                    ]
                    
                    offset = 4
                    for i in range(min(num, 10)):
                        if offset + 6 > len(info_data):
                            break
                        g_start = info_start + offset
                        
                        group = info_data[offset]
                        entry = info_data[offset+1]
                        kod = info_data[offset+2]
                        dt = info_data[offset+3]
                        ds = info_data[offset+4]
                        dn = info_data[offset+5]
                        offset += 6
                        
                        gid_len = ds * dn
                        if offset + gid_len > len(info_data):
                            break
                        
                        item = FieldInfo(f"条目[{i+1}]", "", g_start, g_start + 6 + gid_len)
                        dt_name = DATA_TYPES.get(dt, f"类型{dt}")
                        kod_name = KOD_NAMES.get(kod, f"KOD=0x{kod:02X}")
                        
                        item.children = [
                            FieldInfo("GIN组号", f"{group}", g_start, g_start+1),
                            FieldInfo("GIN条目", f"{entry}", g_start+1, g_start+2),
                            FieldInfo("KOD", f"{kod:02X} ({kod_name})", g_start+2, g_start+3),
                            FieldInfo("GDD类型", f"{dt} ({dt_name})", g_start+3, g_start+4),
                            FieldInfo("GDD长度", f"{ds} 字节", g_start+4, g_start+5),
                            FieldInfo("GDD数目", f"{dn}", g_start+5, g_start+6),
                        ]
                        
                        # 解析值
                        gid = info_data[offset:offset+gid_len]
                        val_str = ""
                        if dt == 7 and len(gid) >= 4:  # 浮点数
                            val = struct.unpack('<f', gid[:4])[0]
                            val_str = f"{val:.6f}"
                        elif dt == 3 and len(gid) >= 4:  # 无符号整数
                            val = struct.unpack('<I', gid[:4])[0]
                            val_str = f"{val}"
                        elif dt == 5 and len(gid) >= 4:  # 有符号整数
                            val = struct.unpack('<i', gid[:4])[0]
                            val_str = f"{val}"
                        elif dt == 1:  # ASCII字符串
                            try:
                                # 过滤掉非打印字符
                                s = gid.decode('ascii', errors='replace')
                                s = ''.join(c if c.isprintable() or c in '\t\n\r' else '.' for c in s)
                                s = s.strip('\x00')
                                val_str = f'"{s}"'
                            except:
                                val_str = gid.hex()
                        elif dt == 8 and len(gid) >= 7:  # CP56Time2a时标
                            val_str = format_cp56time2a(gid[:7])
                        elif dt in [213, 214] and len(gid) >= 7:  # 带时标报文
                            val_str = f"时标: {format_cp56time2a(gid[:7])}"
                        elif dt == 215 and len(gid) >= 11:  # 带相对时标浮点
                            val = struct.unpack('<f', gid[:4])[0]
                            ts = format_cp56time2a(gid[4:11])
                            val_str = f"{val:.4f} @ {ts}"
                        elif dt == 216 and len(gid) >= 11:  # 带相对时标整数
                            val = struct.unpack('<i', gid[:4])[0]
                            ts = format_cp56time2a(gid[4:11])
                            val_str = f"{val} @ {ts}"
                        else:
                            # 显示十六进制
                            hex_str = gid.hex()
                            if len(hex_str) > 32:
                                hex_str = hex_str[:32] + "..."
                            val_str = f"0x{hex_str}"
                        
                        item.children.append(FieldInfo("GID值", val_str, g_start+6, g_start+6+gid_len))
                        info.children.append(item)
                        offset += gid_len
                    
                    asdu.children.append(info)
            
            elif type_id == 0x15:  # ASDU21 通用分类读命令
                # 结构: FUN(1) + INF(1) + RII(1) + NGD(1) + 数据项...
                if len(info_data) >= 4:
                    fun = info_data[0]
                    inf = info_data[1]
                    rii = info_data[2]
                    ngd = info_data[3]
                    num = ngd & 0x7F
                    cont = bool(ngd & 0x80)
                    
                    inf_name = INF_GENERIC.get(inf, f"INF=0x{inf:02X}")
                    
                    info = FieldInfo("信息对象", "", info_start, 3 + length)
                    info.children = [
                        FieldInfo("FUN", f"{fun:02X} ({FUN_NAMES.get(fun, f'功能类型{fun}')})", info_start, info_start+1),
                        FieldInfo("INF", f"{inf:02X} ({inf_name})", info_start+1, info_start+2),
                        FieldInfo("RII", f"{rii:02X} (返回信息标识)", info_start+2, info_start+3),
                        FieldInfo("NGD", f"数目={num} CONT={cont}", info_start+3, info_start+4),
                    ]
                    
                    # 解析数据项
                    offset = 4
                    for i in range(min(num, 10)):
                        if offset + 7 > len(info_data):
                            break
                        g_start = info_start + offset
                        
                        group = info_data[offset]
                        entry = info_data[offset+1]
                        kod = info_data[offset+2]
                        dt = info_data[offset+3]
                        ds = info_data[offset+4]
                        dn = info_data[offset+5] & 0x7F
                        offset += 6
                        
                        gid_len = ds * dn
                        if offset + gid_len > len(info_data):
                            break
                        
                        item = FieldInfo(f"数据项[{i+1}]", "", g_start, g_start + 6 + gid_len)
                        dt_name = DATA_TYPES.get(dt, f"类型{dt}")
                        kod_name = KOD_NAMES.get(kod, f"KOD=0x{kod:02X}")
                        
                        item.children = [
                            FieldInfo("GIN组号", f"{group}", g_start, g_start+1),
                            FieldInfo("GIN条目", f"{entry}", g_start+1, g_start+2),
                            FieldInfo("KOD", f"{kod:02X} ({kod_name})", g_start+2, g_start+3),
                            FieldInfo("GDD类型", f"{dt} ({dt_name})", g_start+3, g_start+4),
                            FieldInfo("GDD长度", f"{ds} 字节", g_start+4, g_start+5),
                            FieldInfo("GDD数目", f"{dn}", g_start+5, g_start+6),
                        ]
                        
                        # 解析GID值
                        gid = info_data[offset:offset+gid_len]
                        val_str = self._parse_gid_value(dt, gid)
                        item.children.append(FieldInfo("GID值", val_str, g_start+6, g_start+6+gid_len))
                        info.children.append(item)
                        offset += gid_len
                    
                    asdu.children.append(info)
            
            elif type_id == 0x2A:  # ASDU42 双点信息状态
                offset = 0
                idx = 1
                items = []
                while offset + 3 <= len(info_data):
                    pos = info_start + offset
                    fun = info_data[offset]
                    inf = info_data[offset+1]
                    dpi = info_data[offset+2] & 0x03
                    
                    if offset + 3 == len(info_data) and fun == 0xFF:
                        items.append(FieldInfo("SIN", f"{inf:02X} (扫描序号)", pos, pos+3))
                        break
                    
                    dpi_name = {0: "不确定(00)", 1: "分/OFF", 2: "合/ON", 3: "不确定(11)"}.get(dpi, f"未知{dpi}")
                    item = FieldInfo(f"双点[{idx}]", "", pos, pos+3)
                    item.children = [
                        FieldInfo("FUN", f"{fun:02X} ({FUN_NAMES.get(fun, f'功能类型{fun}')})", pos, pos+1),
                        FieldInfo("INF", f"{inf:02X}", pos+1, pos+2),
                        FieldInfo("DPI", f"{dpi:02X} ({dpi_name})", pos+2, pos+3),
                    ]
                    items.append(item)
                    offset += 3
                    idx += 1
                
                if items:
                    info = FieldInfo("信息对象", "", info_start, info_start + offset)
                    info.children = items
                    asdu.children.append(info)
            
            elif type_id == 0x01:  # ASDU1 带时标报文
                if len(info_data) >= 17:
                    fun = info_data[0]
                    inf = info_data[1]
                    dpi = info_data[2] & 0x03
                    dpi_name = {0: "不确定(00)", 1: "分/OFF", 2: "合/ON", 3: "不确定(11)"}.get(dpi, f"未知{dpi}")
                    info = FieldInfo("信息对象", "", info_start, info_start + 17)
                    info.children = [
                        FieldInfo("FUN", f"{fun:02X} ({FUN_NAMES.get(fun, f'功能类型{fun}')})", info_start, info_start+1),
                        FieldInfo("INF", f"{inf:02X}", info_start+1, info_start+2),
                        FieldInfo("DPI", f"{dpi:02X} ({dpi_name})", info_start+2, info_start+3),
                        FieldInfo("事件时间", f"{format_cp56time2a(info_data[3:10])}", info_start+3, info_start+10),
                        FieldInfo("接收时间", f"{format_cp56time2a(info_data[10:17])}", info_start+10, info_start+17),
                    ]
                    asdu.children.append(info)
                else:
                    info = FieldInfo("信息对象", f"(原始) {info_data.hex()}", info_start, 3 + length)
                    asdu.children.append(info)
            else:
                # 其他类型显示原始数据
                info = FieldInfo("信息对象", f"(原始) {info_data.hex()}", info_start, 3 + length)
                asdu.children.append(info)
            
            fields.append(asdu)
    
    elif byte1 == 0x01:  # S帧
        recv_seq = (data[5] >> 1) | ((data[6] & 0xFE) << 7)
        apci = FieldInfo("APCI (S格式)", "", apci_start, 7)
        apci.children = [
            FieldInfo("控制域[0]", "01 00 (S格式标识)", 3, 5),
            FieldInfo("控制域[1]", f"N(R)={recv_seq}", 5, 7),
        ]
        fields.append(apci)
        frame_type = APCI_TYPE_S
    
    else:  # U帧
        u_name = U_FORMAT.get(byte1, f"未知({byte1:02X})")
        apci = FieldInfo("APCI (U格式)", "", apci_start, 7)
        apci.children = [
            FieldInfo("控制域", f"{byte1:02X} ({u_name})", 3, 4),
            FieldInfo("保留", "00 00 00", 4, 7),
        ]
        fields.append(apci)
        frame_type = APCI_TYPE_U
    
    return Frame(raw=data, length=length, frame_type=frame_type, 
                 timestamp=timestamp, direction=direction, fields=fields)


# ==================== GUI应用 ====================

class MsgParserApp:
    def __init__(self, root):
        self.root = root
        self.root.title("南网以太网103协议报文解析器")
        self.root.geometry("1500x900")
        
        self.frames: List[Frame] = []
        self.current_frame: Optional[Frame] = None
        self.highlight_tags: List[str] = []
        
        self.setup_ui()
    
    def setup_ui(self):
        # 主框架
        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # 左侧 - 帧列表
        left_frame = ttk.LabelFrame(main_frame, text="帧列表")
        left_frame.pack(side=tk.LEFT, fill=tk.Y, padx=(0, 5))
        left_frame.configure(width=400)
        
        # 工具栏
        toolbar = ttk.Frame(left_frame)
        toolbar.pack(fill=tk.X, padx=5, pady=5)
        ttk.Button(toolbar, text="打开文件", command=self.open_file).pack(side=tk.LEFT, padx=2)
        
        # 过滤
        filter_frame = ttk.Frame(left_frame)
        filter_frame.pack(fill=tk.X, padx=5, pady=2)
        ttk.Label(filter_frame, text="过滤:").pack(side=tk.LEFT)
        self.filter_var = tk.StringVar(value="all")
        ttk.Radiobutton(filter_frame, text="全部", variable=self.filter_var, value="all", command=self.refresh_list).pack(side=tk.LEFT)
        ttk.Radiobutton(filter_frame, text="接收", variable=self.filter_var, value="recv", command=self.refresh_list).pack(side=tk.LEFT)
        ttk.Radiobutton(filter_frame, text="发送", variable=self.filter_var, value="send", command=self.refresh_list).pack(side=tk.LEFT)
        
        # 帧列表
        list_frame = ttk.Frame(left_frame)
        list_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        columns = ("序号", "方向", "类型", "ASDU", "时间")
        self.frame_tree = ttk.Treeview(list_frame, columns=columns, show="headings", height=35)
        for c in columns:
            self.frame_tree.heading(c, text=c)
        self.frame_tree.column("序号", width=40)
        self.frame_tree.column("方向", width=40)
        self.frame_tree.column("类型", width=50)
        self.frame_tree.column("ASDU", width=160)
        self.frame_tree.column("时间", width=120)
        
        scrollbar = ttk.Scrollbar(list_frame, orient=tk.VERTICAL, command=self.frame_tree.yview)
        self.frame_tree.configure(yscrollcommand=scrollbar.set)
        self.frame_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.frame_tree.bind("<<TreeviewSelect>>", self.on_frame_select)
        
        # 右侧 - 详情
        right_frame = ttk.Frame(main_frame)
        right_frame.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True)
        
        # 原始数据 - 带字节索引
        raw_frame = ttk.LabelFrame(right_frame, text="原始数据 (点击字段高亮对应字节)")
        raw_frame.pack(fill=tk.X, padx=5, pady=5)
        
        self.raw_text = tk.Text(raw_frame, height=6, font=("Consolas", 11), wrap=tk.WORD)
        self.raw_text.pack(fill=tk.X, padx=5, pady=5)
        
        # 配置高亮颜色
        self.raw_text.tag_configure("highlight", background="#FFEB3B")
        self.raw_text.tag_configure("byte", foreground="gray")
        
        # 字段树
        field_frame = ttk.LabelFrame(right_frame, text="报文结构 (点击字段查看对应字节)")
        field_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        self.field_tree = ttk.Treeview(field_frame, columns=("value", "bytes"), show="tree headings", height=25)
        self.field_tree.heading("#0", text="字段名称")
        self.field_tree.heading("value", text="值")
        self.field_tree.heading("bytes", text="字节位置")
        self.field_tree.column("#0", width=200)
        self.field_tree.column("value", width=300)
        self.field_tree.column("bytes", width=120)
        
        scrollbar2 = ttk.Scrollbar(field_frame, orient=tk.VERTICAL, command=self.field_tree.yview)
        self.field_tree.configure(yscrollcommand=scrollbar2.set)
        self.field_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar2.pack(side=tk.RIGHT, fill=tk.Y)
        self.field_tree.bind("<<TreeviewSelect>>", self.on_field_select)
    
    def open_file(self):
        filepath = filedialog.askopenfilename(
            title="选择报文文件",
            filetypes=[("文本文件", "*.txt"), ("所有文件", "*.*")],
            initialdir="."
        )
        if filepath:
            self.load_file(filepath)
    
    def load_file(self, filepath: str):
        with open(filepath, 'r', encoding='utf-8') as f:
            lines = f.readlines()
        
        self.frames = []
        for line in lines:
            parsed = parse_log_line(line.strip())
            if parsed:
                timestamp, direction, data = parsed
                frame = parse_frame(data, timestamp, direction)
                if frame:
                    self.frames.append(frame)
        
        self.refresh_list()
        children = self.frame_tree.get_children()
        if children:
            self.frame_tree.selection_set(children[0])
            self.on_frame_select(None)
    
    def refresh_list(self):
        for item in self.frame_tree.get_children():
            self.frame_tree.delete(item)
        
        filter_val = self.filter_var.get()
        for idx, frame in enumerate(self.frames):
            if filter_val != "all" and frame.direction != filter_val:
                continue
            
            ftype = {APCI_TYPE_I: "I帧", APCI_TYPE_S: "S帧", APCI_TYPE_U: "U帧"}.get(frame.frame_type, "?")
            asdu = ""
            # 从fields中找ASDU类型
            for f in frame.fields:
                if "ASDU" in f.name and f.children:
                    for c in f.children:
                        if "TI" in c.name:
                            asdu = c.value.split("(")[1].rstrip(")") if "(" in c.value else ""
                            break
            
            self.frame_tree.insert("", tk.END, values=(
                idx + 1,
                "←" if frame.direction == "recv" else "→",
                ftype,
                asdu,
                frame.timestamp
            ))
    
    def on_frame_select(self, event):
        selection = self.frame_tree.selection()
        if not selection:
            return
        
        idx = int(self.frame_tree.item(selection[0], "values")[0]) - 1
        if 0 <= idx < len(self.frames):
            self.current_frame = self.frames[idx]
            self.show_frame_detail(self.current_frame)
    
    def show_frame_detail(self, frame: Frame):
        # 清空
        self.raw_text.delete("1.0", tk.END)
        self.field_tree.delete(*self.field_tree.get_children())
        
        # 显示原始数据，带字节索引
        data = frame.raw
        hex_str = data.hex()
        
        # 每行显示16字节
        line = ""
        for i, b in enumerate(data):
            if i > 0 and i % 16 == 0:
                line += "\n"
            line += f"{b:02X} "
        
        self.raw_text.insert(tk.END, line.strip())
        
        # 显示字段树
        direction = "← 接收" if frame.direction == "recv" else "→ 发送"
        root_item = self.field_tree.insert("", tk.END, text=f"[{frame.timestamp}] {direction}", values=("", ""), open=True)
        
        for field in frame.fields:
            self.add_field_to_tree(root_item, field)
        
        self.field_tree.item(root_item, open=True)
    
    def add_field_to_tree(self, parent, field: FieldInfo):
        byte_range = f"[{field.start}:{field.end}]" if field.end > field.start else ""
        item = self.field_tree.insert(parent, tk.END, text=field.name, values=(field.value, byte_range), 
                                       tags=(f"pos_{field.start}_{field.end}",))
        
        for child in field.children:
            self.add_field_to_tree(item, child)
        
        self.field_tree.item(item, open=True)
    
    def on_field_select(self, event):
        selection = self.field_tree.selection()
        if not selection:
            return
        
        item = selection[0]
        tags = self.field_tree.item(item, "tags")
        
        # 清除之前的高亮
        self.raw_text.tag_remove("highlight", "1.0", tk.END)
        
        if tags:
            for tag in tags:
                if tag.startswith("pos_"):
                    parts = tag.split("_")
                    if len(parts) == 3:
                        start = int(parts[1])
                        end = int(parts[2])
                        self.highlight_bytes(start, end)
                        break
    
    def highlight_bytes(self, start: int, end: int):
        """高亮指定字节范围"""
        self.raw_text.tag_remove("highlight", "1.0", tk.END)
        
        # 计算在Text中的位置
        # 每个字节占3个字符(2位hex+空格)，每16字节换行
        def get_position(byte_idx: int) -> str:
            line = byte_idx // 16
            col = (byte_idx % 16) * 3
            # 行号从1开始
            return f"{line + 1}.{col}"
        
        start_pos = get_position(start)
        end_pos = get_position(end)
        
        self.raw_text.tag_add("highlight", start_pos, end_pos)
        self.raw_text.see(start_pos)


def main():
    root = tk.Tk()
    app = MsgParserApp(root)
    
    import sys
    if len(sys.argv) > 1:
        app.load_file(sys.argv[1])
    
    root.mainloop()


if __name__ == '__main__':
    main()