#!/bin/bash

# iOS编译脚本 for gam4980 libretro核心

set -e

echo "=== 编译 gam4980 libretro 核心 for iOS ==="

# 配置
ARCH="arm64"
MIN_IOS="11.0"
OUTPUT="gam4980_libretro.dylib"

# 编译器设置
CC="xcrun -sdk iphoneos clang"
CFLAGS="-arch $ARCH -miphoneos-version-min=$MIN_IOS -O3 -fPIC"
LDFLAGS="-dynamiclib -arch $ARCH -miphoneos-version-min=$MIN_IOS"

# 编译 (注意：只编译libretro.c，s6502.c是被include的)
echo "编译中..."
$CC $CFLAGS $LDFLAGS \
    -o $OUTPUT \
    libretro.c \
    -framework Foundation

# 签名
echo "签名中..."
codesign -s - $OUTPUT

# 验证
echo "验证输出..."
file $OUTPUT
otool -L $OUTPUT | head -5

echo "✅ 编译完成！"
echo "输出文件: $(pwd)/$OUTPUT"
echo ""
echo "下一步："
echo "1. 将 $OUTPUT 复制到 RetroArch/pkg/apple/iOS/modules/"
echo "2. 在Xcode中编译RetroArch"