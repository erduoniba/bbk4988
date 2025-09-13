# -*- coding:utf-8 -*-

import sys
import mmap
from ctypes import Structure, c_uint8, c_char, memmove, addressof, sizeof, string_at


class FileNode(Structure):
    _fields_ = [("addr_m", c_uint8),
                ("addr_h", c_uint8),
                ("name", c_char*10),
                ("size_l", c_uint8),
                ("size_m", c_uint8),
                ("size_h", c_uint8),
                ("type", c_uint8)]


def gam2flash(gam_path):
    flash_path = r'4988.flash'
    f = open(flash_path, 'rb')
    flash_data = f.read()
    f.close()
    if not flash_data:
        print('flash为空，请检查flash文件大小')
        return
    free_start_addr = 0x0000
    offset = 0x0000
    file_node = FileNode()
    index = 0
    while True:
        memmove(addressof(file_node), flash_data[offset:offset+0x10], sizeof(FileNode))
        if file_node.type == 0xFF:
            break
        offset += 0x10
        if file_node.type == 0x00:
            continue
        file_addr =  (file_node.addr_h << 16) + (file_node.addr_m << 8)
        file_size = (file_node.size_h << 16) + (file_node.size_m << 8) + file_node.size_l
        free_start_addr = file_addr + file_size
        if free_start_addr & 0xFFF:
            free_start_addr &= 0xFFFFF000
            free_start_addr += 0x00001000
        print(index, file_node.name.decode('gbk', errors='ignore'), f'0x{file_addr:04X}', f'0x{file_size:06X}', f'0x{free_start_addr:06X}')
        index += 1
    f = open(gam_path, 'rb')
    gam_data = f.read()
    f.close()
    file_size = len(gam_data)
    file_node.addr_m = free_start_addr >> 8
    file_node.addr_h = free_start_addr >> 16
    file_node.name = gam_data[6:16]
    file_node.size_l = file_size
    file_node.size_m = file_size >> 8
    file_node.size_h = file_size >> 16
    file_node.type = 0x3d
    f = open(flash_path, 'r+b')
    mm = mmap.mmap(f.fileno(), 0)
    mm.seek(offset)
    mm.write(string_at(addressof(file_node), sizeof(FileNode)))
    mm.seek(free_start_addr)
    mm.write(gam_data)
    mm.close()
    print('end')

if __name__ == '__main__':
    for path in sys.argv[1:]:
        gam2flash(path)
