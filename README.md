# 步步高电子词典模拟器

本项目提供步步高电子词典系列产品的模拟器实现，包括BA4988模拟器和gam4980 libretro核心。

## 🎯 项目简介

步步高电子词典在中国教育电子产品历史上占有重要地位，本项目通过逆向工程和模拟器技术，让这些经典设备能够在现代计算机和游戏机上重新运行。

### 主要组件

1. **BA4988模拟器** - Windows平台的步步高A系列电子词典模拟器
2. **gam4980 libretro核心** - 跨平台的步步高朗文4980电子词典libretro核心

## ✨ 功能特性

- 完整的6502 CPU模拟
- 支持原版ROM文件运行
- libretro核心支持多平台（RetroArch）
- 优化的性能（使用computed goto和宏优化）
- 支持ARM设备交叉编译

## 📦 快速开始

### 系统要求

- **BA4988模拟器**：Windows系统，Visual Studio 2019或更高版本
- **gam4980核心**：支持C11标准的编译器，或Zig构建工具

### 获取ROM文件

#### gam4980核心所需文件

从实际设备导出以下ROM文件：
- `8.BIN` - 字体ROM（地址范围：0x800000-0x9fffff）
- `E.BIN` - 系统ROM（地址范围：0xe00000-0xffffff）

将文件放置在RetroArch的系统目录下：
```
~/.config/retroarch/system/gam4980/
├── 8.BIN
└── E.BIN
```

#### BA4988模拟器所需文件

BA4988模拟器需要以下数据文件（已包含在`bbk_c/BA4988/`目录中）：
- `4988.flash` - Flash ROM
- `4988.font` - 字体文件
- 多个`.DAT`文件 - 游戏和应用数据

## 🔨 构建说明

### 构建gam4980 libretro核心

#### 方法1：使用Zig（推荐）
```bash
cd gam4980/build
zig build -Doptimize=ReleaseFast
```

#### 方法2：直接使用GCC
```bash
cd gam4980/src
gcc -std=c11 -Wall -O3 -fpic -shared -o gam4980_libretro.so libretro.c
```

#### 方法3：为特定平台构建

**Miyoo设备：**
```bash
cd gam4980/build
./build-miyoo.sh
```

**ARM v7设备（使用Docker）：**
```powershell
cd gam4980/build
./build-armv7-dockcross.ps1
```

### 构建BA4988模拟器

使用Visual Studio打开`bbk_c/BA4988/BA4988.sln`，选择Release配置进行构建。

## 🚀 使用方法

### 在RetroArch中使用gam4980核心

1. 将编译好的`gam4980_libretro.so`（或`.dll`）复制到RetroArch的核心目录
2. 确保ROM文件已放置在正确的系统目录
3. 在RetroArch中加载核心并选择游戏ROM

### 运行BA4988模拟器

1. 编译生成`BA4988.exe`
2. 确保所有`.DAT`文件和配置文件在同一目录
3. 运行可执行文件

## 📂 项目结构

```
bbk4988/
├── bbk_c/                    # BA4988模拟器
│   └── BA4988/              # Windows平台模拟器源码和资源
│       ├── BA4988.cpp       # 主程序
│       ├── *.DAT            # ROM和数据文件
│       └── 4988.ini         # 配置文件
│
├── gam4980/                  # gam4980 libretro核心
│   ├── src/                 # 源代码
│   │   ├── libretro.c      # libretro接口实现
│   │   ├── s6502.c         # 6502 CPU模拟器
│   │   └── libretro.h      # libretro API定义
│   ├── build/              # 构建脚本
│   └── retroarch/          # RetroArch相关文件
│
└── CLAUDE.md               # AI辅助开发指南
```

## 🛠️ 开发指南

### 技术架构

#### CPU模拟
- 基于6502处理器架构
- 使用vrEmu6502库，通过computed goto和宏进行优化
- 支持完整的6502指令集

#### 内存映射
```
0x00200000 - 0x003fffff  Flash区域
0x00800000 - 0x009fffff  字体区域
0x00e00000 - 0x00ffffff  系统区域
```

### 编译选项

- `SWAP_LCD_WIDTH_HEIGHT` - 交换LCD宽高以适配特定设备（如RG28XX）

### 调试

BA4988模拟器包含调试功能，可通过配置文件`4988.ini`设置：
```ini
[DEBUG]
BaseAdr   = 00F80000
NumOfBank = 4
```

## 🤝 贡献指南

欢迎贡献代码、报告问题或提出建议！

### 如何贡献

1. Fork本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启Pull Request

### 开发规范

- 遵循现有代码风格
- 提交前进行充分测试
- 更新相关文档
- 不要提交受版权保护的ROM文件

## 📚 相关资源

### 项目依赖

- [vrEmu6502](https://github.com/visrealm/vrEmu6502) - 6502 CPU模拟器
- [sph-sc](https://github.com/sph-mn/sph-sc) - Scheme到C代码生成器
- [libretro](https://www.libretro.com/) - 跨平台游戏和模拟器API

### 原始项目

- gam4980原始仓库：https://codeberg.org/iyzsong/gam4980.git
- BA4988模拟器：https://gitee.com/BA4988/BBK-simulator（作者：无云）

## ⚖️ 许可证

- bbk_c（BA4988模拟器）：GPL v2
- gam4980 libretro核心：GPL v3

详见各目录下的LICENSE文件。

## ⚠️ 免责声明

1. 本项目仅供学习和研究使用
2. 使用本模拟器需要合法获得的ROM文件
3. 请勿用于商业用途
4. 步步高是步步高电子工业有限公司的注册商标

## 📮 联系方式

如有问题或建议，请通过以下方式联系：

- 提交Issue到本仓库
- Pull Request欢迎

---

*本项目致力于保存和研究中国电子词典的技术历史*