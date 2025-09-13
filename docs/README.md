# 步步高模拟器 iOS 移植文档

## 📱 项目说明

将步步高电子词典模拟器（gam4980核心）移植到iOS平台，通过编译RetroArch实现。

## 📚 文档列表

| 文档 | 说明 |
|------|------|
| [iOS集成指南](./iOS集成指南.md) | RetroArch 1.21.0 完整集成步骤 |
| [编译指南](./编译指南.md) | 详细的编译步骤 |
| [常见问题](./常见问题.md) | 编译错误解决方案 |
| [build_ios.sh](./build_ios.sh) | 自动编译脚本 |

## ⚡ 快速开始

### 针对RetroArch-1.21.0的集成（推荐）

```bash
# 在项目根目录运行集成脚本
./integrate_retroarch.sh

# 然后打开Xcode项目
open ios/RetroArch-1.21.0/pkg/apple/RetroArch_iOS11.xcodeproj
```

详细步骤请查看 [iOS集成指南](./iOS集成指南.md)

### 手动编译核心

```bash
cd gam4980/src
bash ../../docs/build_ios.sh
```

### 方法2：手动编译

```bash
# 1. 编译核心（注意：s6502.c已包含在libretro.c中）
cd gam4980/src
xcrun -sdk iphoneos clang -arch arm64 \
    -miphoneos-version-min=11.0 \
    -O3 -dynamiclib \
    -o gam4980_libretro.dylib \
    libretro.c

# 2. 签名
codesign -s - gam4980_libretro.dylib

# 3. 克隆RetroArch
git clone https://github.com/libretro/RetroArch.git
cd RetroArch

# 4. 添加核心
mkdir -p pkg/apple/iOS/modules
cp ../../gam4980/src/gam4980_libretro.dylib pkg/apple/iOS/modules/

# 5. 在Xcode中编译
open pkg/apple/RetroArch_iOS.xcodeproj
```

## 🔧 系统要求

- Mac电脑 + Xcode 14.0+
- Apple开发者账号（免费或付费）
- iOS 11.0+ 设备

## 📦 产出文件

| 文件 | 说明 |
|------|------|
| `gam4980_libretro.dylib` | 编译的核心文件 |
| `RetroArch.app` | 可直接安装到设备 |
| `RetroArch.ipa` | 可分发的安装包 |

## ⚠️ 重要提示

1. **不要单独编译 s6502.c** - 它通过 `#include` 包含在 libretro.c 中
2. **必须签名** - iOS要求所有动态库都必须签名
3. **检查架构** - 确保编译为 arm64 架构

## 📱 ROM文件

需要以下ROM文件（放在 `Documents/system/gam4980/`）：
- `8.BIN` - 字体ROM
- `E.BIN` - 系统ROM

## 🆘 遇到问题？

查看[常见问题](./常见问题.md)或提交Issue。