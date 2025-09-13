# RetroArch iOS 集成指南

## 📋 前置条件

- ✅ 已下载 RetroArch-1.21.0 源码到 `ios/RetroArch-1.21.0`
- ✅ Mac电脑 + Xcode 14.0+
- ✅ Apple开发者账号（免费或付费）
- ✅ iOS 11.0+ 设备

## 🚀 一键集成

运行集成脚本，自动完成核心编译和文件配置：

```bash
# 在项目根目录运行
./integrate_retroarch.sh
```

脚本将自动：
1. 编译 gam4980_libretro.dylib
2. 创建 gam4980_libretro.info
3. 复制文件到 RetroArch 源码
4. 生成 Xcode 配置说明

## 📱 Xcode编译步骤

### 1. 打开项目

```bash
open ios/RetroArch-1.21.0/pkg/apple/RetroArch_iOS11.xcodeproj
```

> 💡 推荐使用 RetroArch_iOS11.xcodeproj（支持iOS 11+）

### 2. 配置签名

在Xcode中：

1. **选择Target**
   - 左侧导航栏选择 RetroArch_iOS11 项目
   - 在TARGETS列表中选择 "RetroArch iOS"

2. **配置Team和Bundle ID**
   - 点击 "Signing & Capabilities" 标签
   - Team → 选择您的开发团队
   - Bundle Identifier → 修改为唯一值
     - 例如：`com.yourname.retroarch`
   - ✅ 勾选 "Automatically manage signing"

### 3. 添加核心文件到项目

1. **找到Resources文件夹**
   - 在左侧项目导航器中找到 Resources 文件夹

2. **添加文件**
   - 右键点击 Resources
   - 选择 "Add Files to RetroArch iOS..."

3. **选择核心文件**
   - 导航到：`pkg/apple/iOS/modules/`
   - 选择以下文件：
     - `gam4980_libretro.dylib`
     - `gam4980_libretro.info`

4. **配置添加选项**
   - ✅ Copy items if needed
   - ✅ Create groups
   - ✅ Add to targets: RetroArch iOS
   - 点击 "Add"

### 4. 验证Build Phases

1. 选择 "Build Phases" 标签
2. 展开 "Copy Bundle Resources"
3. 确认包含：
   - gam4980_libretro.dylib
   - gam4980_libretro.info

如果缺失，点击 "+" 手动添加。

### 5. 编译设置确认

在 "Build Settings" 中确认：
- Architectures: Standard Architectures (arm64)
- iOS Deployment Target: 11.0
- Valid Architectures: arm64

### 6. 编译运行

#### 方法A：直接运行到设备（推荐）
1. 连接iOS设备到Mac
2. 顶部工具栏选择您的设备
3. 点击运行按钮或按 ⌘R
4. 首次运行需要在设备上信任开发者证书：
   - 设置 → 通用 → VPN与设备管理 → 开发者APP → 信任

#### 方法B：生成IPA文件
1. Product → Archive
2. 等待编译完成（约5-10分钟）
3. Window → Organizer
4. 选择刚创建的Archive
5. Distribute App → Development → Next
6. App Thinning: None
7. Export → 选择保存位置

## 📂 ROM文件配置

### 1. 准备ROM文件

需要以下两个文件：
- `8.BIN` - 字体ROM（必需）
- `E.BIN` - 系统ROM（必需）

### 2. 创建目录结构

在iOS设备上的RetroArch文档目录：
```
Documents/
└── system/
    └── gam4980/
        ├── 8.BIN
        └── E.BIN
```

### 3. 传输ROM文件

#### 方法A：iTunes/Finder文件共享
1. 连接设备到电脑
2. 打开iTunes（macOS Mojave及更早）或Finder（macOS Catalina及更新）
3. 选择您的设备
4. 点击"文件共享"
5. 选择RetroArch
6. 创建文件夹结构并拖入ROM文件

#### 方法B：通过Xcode
1. Window → Devices and Simulators
2. 选择您的设备
3. Installed Apps → RetroArch
4. 点击齿轮图标 → Download Container
5. 在下载的容器中添加ROM文件
6. Replace Container上传回设备

## 🎮 使用步骤

1. **启动RetroArch**

2. **配置目录**
   - Settings → Directory → System/BIOS
   - 设置为 Documents 目录

3. **加载核心**
   - Main Menu → Load Core
   - 选择 "BBK Electronic Dictionary (GAM4980)"

4. **加载内容**
   - Main Menu → Load Content
   - 浏览到 system/gam4980/
   - 选择 E.BIN

5. **开始使用**
   - 模拟器将自动启动
   - 使用屏幕虚拟按键操作

## ❓ 常见问题

### 问题1：'GCDWebDAVServer.h' file not found

**原因**：WebServer组件头文件路径问题

**解决方案**：

方法A：运行修复脚本（推荐）
```bash
./fix_xcode_build.sh
```

方法B：在Xcode中手动配置
1. Build Settings → Header Search Paths
2. 添加以下路径：
   - `$(SRCROOT)/pkg/apple/WebServer`
   - `$(SRCROOT)/pkg/apple/WebServer/GCDWebServer`
   - `$(SRCROOT)/pkg/apple/WebServer/GCDWebDAVServer`
   - `$(SRCROOT)/pkg/apple/WebServer/GCDWebUploader`
3. 设置为 "recursive"
4. Clean Build Folder (⇧⌘K) 后重新编译

### 问题2：'JITSupport.h' file not found

**原因**：JIT支持组件路径问题

**解决方案**：

运行完整修复脚本：
```bash
./fix_all_dependencies.sh
```

这个脚本会修复所有依赖问题，包括：
- WebServer组件（GCDWebDAVServer等）
- JITSupport组件
- 创建xcconfig配置文件
- 生成详细的手动配置指南

### 问题3：编译失败 "Code signing required"

**解决方案**：
- 确保已选择开发团队
- 确保Bundle ID唯一
- 清理项目：Product → Clean Build Folder (⇧⌘K)

### 问题4：核心无法加载

**检查步骤**：
```bash
# 验证dylib架构
lipo -info gam4980_libretro.dylib
# 应显示: arm64

# 检查签名
codesign -dvvv gam4980_libretro.dylib
```

### 问题5：ROM文件找不到

**确认路径**：
- 必须在 `Documents/system/gam4980/` 目录
- 文件名必须完全匹配（区分大小写）

### 问题6：Undefined symbol链接错误

**错误示例**：
```
Undefined symbol: _CHHapticDynamicParameterIDHapticIntensityControl
Undefined symbol: _OBJC_CLASS_$_GCDWebDAVServer
Undefined symbol: _jb_enable_ptrace_hack
```

**解决方案**：

1. **运行修复脚本**：
   ```bash
   ./fix_linking_errors.sh
   ```

2. **在Xcode中添加框架**：
   - Build Phases → Link Binary With Libraries
   - 添加 CoreHaptics.framework (设为Optional)
   - 添加 MetricKit.framework (设为Optional)

3. **添加JITStub.m文件**：
   - 右键 pkg/apple → Add Files
   - 选择 JITStub.m
   - 确保勾选目标target

4. **设置链接标志**：
   - Build Settings → Other Linker Flags
   - 添加: `-ObjC -weak_framework CoreHaptics -weak_framework MetricKit`

5. **清理并重新编译**

### 问题7：设备信任问题

**解决方案**：
1. 设置 → 通用 → VPN与设备管理
2. 开发者APP → 选择您的证书
3. 点击"信任"

## 📊 性能优化

### 编译优化选项

在Build Settings中可调整：
- Optimization Level: 
  - Debug: None [-O0]
  - Release: Fastest, Smallest [-Os]
- Enable Bitcode: No（libretro核心不支持）

### 运行时设置

在RetroArch设置中：
- Settings → Video → Threaded Video: ON
- Settings → Audio → Audio Latency: 64ms
- Settings → Input → Input Latency: 0

## 🔧 调试技巧

### 查看日志
1. Xcode → Window → Devices and Simulators
2. 选择设备 → View Device Logs
3. 过滤 "RetroArch"

### 核心调试
如需调试核心代码：
1. 重新编译核心with调试符号：
   ```bash
   CFLAGS="-g -O0" ./build_ios.sh
   ```
2. 在Xcode中启用调试

## ✅ 完成检查清单

- [ ] 运行 integrate_retroarch.sh 脚本
- [ ] Xcode中配置签名和Bundle ID
- [ ] 添加核心文件到项目
- [ ] 成功编译运行到设备
- [ ] ROM文件传输到正确位置
- [ ] RetroArch中成功加载核心
- [ ] 模拟器正常运行

## 📚 相关文档

- [编译指南](./编译指南.md) - 手动编译步骤
- [常见问题](./常见问题.md) - 编译错误解决
- [build_ios.sh](./build_ios.sh) - 核心编译脚本