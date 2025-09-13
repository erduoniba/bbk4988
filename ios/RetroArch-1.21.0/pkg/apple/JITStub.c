// JIT函数的stub实现
// 提供这些函数以避免链接错误

#include <stdbool.h>

// 越狱检测相关
void jb_enable_ptrace_hack(void) {
    // 空实现 - 仅在越狱设备上有效
}

// JIT可用性检查
bool jit_available(void) {
    // 在非越狱设备上JIT不可用
    return false;
}
