# 🚨 快速修复链接错误

## 最新错误：libxml2
**Undefined symbol: _xmlFreeDoc, _xmlReadMemory, _xmlStrcmp**

### 修复方法
1. Build Phases → Link Binary With Libraries → +
2. 搜索 "xml"
3. 选择 **libxml2.tbd**
4. Build Settings → Header Search Paths
5. 添加：`$(SDKROOT)/usr/include/libxml2`

---

## 之前的错误
WebDAVServer和JIT相关文件没有加入Xcode编译

## 立即修复（3步）

### 1️⃣ 添加WebDAVServer到编译
在Xcode中：
- Build Phases → Compile Sources
- 点击 "+" 按钮
- 导航到 `pkg/apple/WebServer/GCDWebDAVServer/`
- 选择 **GCDWebDAVServer.m**
- 点击 Add

### 2️⃣ 添加GCDWebServer核心文件
继续在Compile Sources中添加：
- `pkg/apple/WebServer/GCDWebServer/Core/GCDWebServer.m`
- `pkg/apple/WebServer/GCDWebServer/Core/GCDWebServerConnection.m`
- `pkg/apple/WebServer/GCDWebServer/Core/GCDWebServerFunctions.m`

### 3️⃣ 添加JIT Stub
- 点击 "+" 
- 选择 `pkg/apple/JITStub.c`
- 点击 Add

## 编译
- Clean: ⇧⌘K
- Build: ⌘B

## ✅ 完成！

---

💡 **提示**：如果找不到JITStub.c，先运行：
```bash
./fix_final_linking.sh
```