#import <Foundation/Foundation.h>

// JIT相关函数的stub实现
// 这些函数在非越狱设备上不可用，提供空实现以避免链接错误

__attribute__((weak))
void jb_enable_ptrace_hack(void) {
    // 空实现 - 仅在越狱设备上有效
}

__attribute__((weak))
BOOL jit_available(void) {
    // 在iOS 14.2+可能可用，否则返回NO
    if (@available(iOS 14.2, *)) {
        // 实际的JIT可用性需要运行时检查
        return NO;
    }
    return NO;
}
