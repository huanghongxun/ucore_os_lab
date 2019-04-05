#ifndef __KERN_MM_MEMLAYOUT_H__
#define __KERN_MM_MEMLAYOUT_H__

/**
 * 定义操作系统内存布局的一些常量
 */

/* 全局段 (Global Segment) */

#define SEG_KTEXT    1 // 内核代码段
#define SEG_KDATA    2 // 内核数据段
#define SEG_UTEXT    3 // 用户代码段
#define SEG_UDATA    4 // 用户数据段
#define SEG_TSS      5 // 任务选择段（TSS）

/* 全局描述符编号 (Global Descriptor)，低三位 */

#define GD_KTEXT    ((SEG_KTEXT) << 3) // 内核代码段描述符编号
#define GD_KDATA    ((SEG_KDATA) << 3) // 内核数据段描述符编号
#define GD_UTEXT    ((SEG_UTEXT) << 3) // 用户代码段描述符编号
#define GD_UDATA    ((SEG_UDATA) << 3) // 用户数据段描述符编号
#define GD_TSS      ((SEG_TSS  ) << 3) // 任务状态段描述符编号

/* 权限级别 (Descriptor Privilege Level) */

#define DPL_KERNEL  (0)  // 内核权限级
#define DPL_USER    (3)  // 用户权限级

/* 代码段描述符 */

#define KERNEL_CS    ((GD_KTEXT) | DPL_KERNEL) // 内核代码段描述符
#define KERNEL_DS    ((GD_KDATA) | DPL_KERNEL) // 内核数据段描述符
#define USER_CS      ((GD_UTEXT) | DPL_USER  ) // 用户代码段描述符
#define USER_DS      ((GD_UDATA) | DPL_USER  ) // 用户数据段描述符

#endif /* !__KERN_MM_MEMLAYOUT_H__ */

