#ifndef __KERN_DEBUG_KDEBUG_H__
#define __KERN_DEBUG_KDEBUG_H__

#include <defs.h>

/**
 * 打印内核相关信息：内核入口地址、数据段/代码段起始地址、空闲内存起始地址，内核内存使用情况
 */
void print_kerninfo(void);

/**
 * 输出调用堆栈
 *
 * In print_debuginfo(), the function debuginfo_eip() can get enough information about
 * calling-chain. Finally print_stackframe() will trace and print them for debugging.
 *
 */
void print_stackframe(void);

/**
 * 从当前指令地址 eip 中读取当前状态信息并输出
 * info.eip_fn_addr should be the first address of the related function.
 */
void print_debuginfo(uintptr_t eip);

#endif /* !__KERN_DEBUG_KDEBUG_H__ */

