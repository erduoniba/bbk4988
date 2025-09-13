# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

这是一个步步高电子词典模拟器项目，包含两个主要组件：

1. **BA4988模拟器** (`bbk_c/BA4988/`) - 基于反汇编的BA4988电子词典Windows模拟器
2. **gam4980 libretro核心** (`gam4980/`) - 步步高朗文4980电子词典的libretro核心实现

## 构建命令

### gam4980 libretro核心

**使用Zig构建（推荐）：**
```bash
cd gam4980/build
zig build -Doptimize=ReleaseFast
```

**针对Miyoo设备构建：**
```bash
cd gam4980/build
./build-miyoo.sh
```

**针对ARM v7设备构建（使用Docker）：**
```powershell
cd gam4980/build
./build-armv7-dockcross.ps1
```

**手动编译：**
```bash
cd gam4980/src
gcc -std=c11 -Wall -O3 -fpic -shared -o gam4980_libretro.so libretro.c
```

## 项目架构

### gam4980 libretro核心
- `gam4980/src/libretro.c` - libretro接口实现，包含模拟器主逻辑
- `gam4980/src/s6502.c` - 6502 CPU模拟器实现
- `gam4980/src/libretro.h` - libretro API定义
- `gam4980/retroarch/` - RetroArch相关文件和ROM存放目录

### BA4988模拟器
- `bbk_c/BA4988/BA4988.cpp` - Windows平台模拟器主程序
- `bbk_c/BA4988/*.DAT` - ROM文件和数据文件
- `bbk_c/BA4988/4988.ini` - 模拟器配置文件

## 关键技术细节

### ROM文件要求
使用gam4980核心需要以下ROM文件：
- `8.BIN` - 从设备地址0x800000-0x9fffff导出
- `E.BIN` - 从设备地址0xe00000-0xffffff导出

文件需放置在RetroArch的`system/gam4980/`目录下。

### 内存映射
- Flash区域：0x00200000 - 0x003fffff
- 字体区域：0x00800000 - 0x009fffff  
- 系统区域：0x00e00000 - 0x00ffffff

### 编译选项
- `SWAP_LCD_WIDTH_HEIGHT` - 条件编译符，用于交换LCD宽高以适配某些设备（如RG28XX）

## 开发注意事项

- libretro核心基于6502 CPU模拟
- 使用computed goto和宏优化了CPU模拟性能
- 支持多种ARM设备的交叉编译
- ROM文件必须从实际设备导出，项目不包含受版权保护的ROM数据