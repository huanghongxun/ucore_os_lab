#ifndef __KERN_MM_MMU_H__
#define __KERN_MM_MMU_H__

/**
 * Linux 相关源代码参见 include/asm-i386/processor.h
 * https://elixir.bootlin.com/linux/v2.6.18-rc6/source/include/asm-i386/processor.h
 */

/* Eflags register */
#define FL_CF            0x00000001    // Carry Flag
#define FL_PF            0x00000004    // Parity Flag
#define FL_AF            0x00000010    // Auxiliary carry Flag
#define FL_ZF            0x00000040    // Zero Flag
#define FL_SF            0x00000080    // Sign Flag
#define FL_TF            0x00000100    // Trap Flag
#define FL_IF            0x00000200    // Interrupt Flag
#define FL_DF            0x00000400    // Direction Flag
#define FL_OF            0x00000800    // Overflow Flag
#define FL_IOPL_MASK     0x00003000    // I/O Privilege Level bitmask
#define FL_IOPL_0        0x00000000    //   IOPL == 0
#define FL_IOPL_1        0x00001000    //   IOPL == 1
#define FL_IOPL_2        0x00002000    //   IOPL == 2
#define FL_IOPL_3        0x00003000    //   IOPL == 3
#define FL_NT            0x00004000    // Nested Task
#define FL_RF            0x00010000    // Resume Flag
#define FL_VM            0x00020000    // Virtual 8086 mode
#define FL_AC            0x00040000    // Alignment Check
#define FL_VIF           0x00080000    // Virtual Interrupt Flag
#define FL_VIP           0x00100000    // Virtual Interrupt Pending
#define FL_ID            0x00200000    // CPUID detection flag

/* Application segment type bits */
#define STA_X            0x8            // Executable segment
#define STA_E            0x4            // Expand down (non-executable segments)
#define STA_C            0x4            // Conforming code segment (executable only)
#define STA_W            0x2            // Writeable (non-executable segments)
#define STA_R            0x2            // Readable (executable segments)
#define STA_A            0x1            // Accessed

/* System segment type bits */
#define STS_T16A         0x1            // Available 16-bit TSS
#define STS_LDT          0x2            // Local Descriptor Table
#define STS_T16B         0x3            // Busy 16-bit TSS
#define STS_CG16         0x4            // 16-bit Call Gate
#define STS_TG           0x5            // Task Gate / Coum Transmitions
#define STS_IG16         0x6            // 16-bit Interrupt Gate
#define STS_TG16         0x7            // 16-bit Trap Gate
#define STS_T32A         0x9            // Available 32-bit TSS
#define STS_T32B         0xB            // Busy 32-bit TSS
#define STS_CG32         0xC            // 32-bit Call Gate
#define STS_IG32         0xE            // 32-bit Interrupt Gate
#define STS_TG32         0xF            // 32-bit Trap Gate

/* Gate descriptors for interrupts and traps */
struct gatedesc {
    unsigned gd_off_15_0 : 16;        // low 16 bits of offset in segment
    unsigned gd_ss : 16;              // segment selector
    unsigned gd_args : 5;             // # args, 0 for interrupt/trap gates
    unsigned gd_rsv1 : 3;             // reserved(should be zero I guess)
    unsigned gd_type : 4;             // type(STS_{TG,IG32,TG32})
    unsigned gd_s : 1;                // must be 0 (system)
    unsigned gd_dpl : 2;              // descriptor(meaning new) privilege level
    unsigned gd_p : 1;                // Present
    unsigned gd_off_31_16 : 16;       // high bits of offset in segment
};

#define GATE_TRAP 1
#define GATE_INT 0

/**
 * @brief 初始化中断/陷阱门描述符
 * @param istrap 0 表示中断门，1 表示陷阱（异常）门
 * @param sel 中断处理程序的代码段选择子
 * @param off 中断处理程序的代码段内偏移
 * @param dpl 描述符特权级
 */
#define SETGATE(gate, istrap, sel, off, dpl) {            \
    (gate).gd_off_15_0 = (uint32_t)(off) & 0xffff;        \
    (gate).gd_ss = (sel);                                 \
    (gate).gd_args = 0;                                   \
    (gate).gd_rsv1 = 0;                                   \
    (gate).gd_type = (istrap) ? STS_TG32 : STS_IG32;      \
    (gate).gd_s = 0;                                      \
    (gate).gd_dpl = (dpl);                                \
    (gate).gd_p = 1;                                      \
    (gate).gd_off_31_16 = (uint32_t)(off) >> 16;          \
}

/* 初始化函数调用门描述符 */
#define SETCALLGATE(gate, ss, off, dpl) {                 \
    (gate).gd_off_15_0 = (uint32_t)(off) & 0xffff;        \
    (gate).gd_ss = (ss);                                  \
    (gate).gd_args = 0;                                   \
    (gate).gd_rsv1 = 0;                                   \
    (gate).gd_type = STS_CG32;                            \
    (gate).gd_s = 0;                                      \
    (gate).gd_dpl = (dpl);                                \
    (gate).gd_p = 1;                                      \
    (gate).gd_off_31_16 = (uint32_t)(off) >> 16;          \
}

/* 段描述符 */
struct segdesc {
    unsigned sd_lim_15_0 : 16;        // low bits of segment limit
    unsigned sd_base_15_0 : 16;       // low bits of segment base address
    unsigned sd_base_23_16 : 8;       // middle bits of segment base address
    unsigned sd_type : 4;             // segment type (see STS_ constants)
    unsigned sd_s : 1;                // 0 = system, 1 = application
    unsigned sd_dpl : 2;              // descriptor Privilege Level
    unsigned sd_p : 1;                // present
    unsigned sd_lim_19_16 : 4;        // high bits of segment limit
    unsigned sd_avl : 1;              // unused (available for software use)
    unsigned sd_rsv1 : 1;             // reserved
    unsigned sd_db : 1;               // 0 = 16-bit segment, 1 = 32-bit segment
    unsigned sd_g : 1;                // granularity: limit scaled by 4K when set
    unsigned sd_base_31_24 : 8;       // high bits of segment base address
};

// 空段
#define SEG_NULL                                           \
    (struct segdesc){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}

// 32 位 GPT 描述符，段基地址为 20 位，因此丢弃低 12 位
// SEG(type, base, lim, dpl)
#define SEG(type, base, lim, dpl)                          \
    (struct segdesc){                                      \
        ((lim) >> 12) & 0xffff, (base) & 0xffff,           \
        ((base) >> 16) & 0xff, type, 1, dpl, 1,            \
        (unsigned)(lim) >> 28, 0, 0, 1, 1,                 \
        (unsigned) (base) >> 24                            \
    }

// 16 位段寄存器
#define SEG16(type, base, lim, dpl)                        \
    (struct segdesc){                                      \
        (lim) & 0xffff, (base) & 0xffff,                   \
        ((base) >> 16) & 0xff, type, 1, dpl, 1,            \
        (unsigned) (lim) >> 16, 0, 0, 1, 0,                \
        (unsigned) (base) >> 24                            \
    }

/**
 * TSS 段结构体
 * @see tss_struct in include/asm-i386/processor.h
 */
struct taskstate {
    uint32_t ts_link;        // 指向前一个任务的选择子

    // ss0~3, esp0~3 由任务的创建者填写
    uintptr_t ts_esp0;       // ring0 级堆栈指针寄存器
    uint16_t ts_ss0;         // ring0 级堆栈段寄存器
    uint16_t __ss0h;         // 保留位
    // 我们只使用 ring0（内核态）和 ring3（用户态），因此 ss1,ss2,esp1,esp2 没有用
    uintptr_t ts_esp1;       // ring1 级堆栈指针寄存器
    uint16_t ts_ss1;         // ss1 is used to cache MSR_IA32_SYSENTER_CS
    uint16_t __ss1h;         // 保留位
    uintptr_t ts_esp2;       // ring2 级堆栈指针寄存器
    uint16_t ts_ss2;         // ring2 级堆栈段寄存器
    uint16_t __ss2h;         // 保留位
    uintptr_t ts_cr3;        // 页目录基地址寄存器 CR3 (PDBR)

    // 寄存器保存区域，用于任务切换时保存现场。
    // 当任务首次执行时，CPU 从这些寄存器中加载初始执行环境，
    // 从 CS:EIP 处开始执行任务的第一条指令
    uintptr_t ts_eip;
    uint32_t ts_eflags;      // IOPL 位决定当前任务的 IO 特权级别
    uint32_t ts_eax;         // 通用寄存器
    uint32_t ts_ecx;
    uint32_t ts_edx;
    uint32_t ts_ebx;
    uintptr_t ts_esp;
    uintptr_t ts_ebp;
    uint32_t ts_esi;
    uint32_t ts_edi;
    uint16_t ts_es;          // 段寄存器
    uint16_t __esh;          // 保留位
    uint16_t ts_cs;
    uint16_t __csh;          // 保留位
    uint16_t ts_ss;
    uint16_t __ssh;          // 保留位
    uint16_t ts_ds;
    uint16_t __dsh;          // 保留位
    uint16_t ts_fs;
    uint16_t __fsh;          // 保留位
    uint16_t ts_gs;
    uint16_t __gsh;          // 保留位
    // 寄存器保存区域

    uint16_t ts_ldt;         // 当前任务的LDT选择子，由内核填写，以指向当前任务的LDT。该信息由处理器在任务切换时使用，在任务运行期间保持不变。
    uint16_t __ldth;         // 保留位
    uint16_t ts_t;           // Debug trap，用于软件调试。在多任务环境中，如果 T=1，则每次切换到该任务的时候，会引发一个调试异常中断（int 1）
    uint16_t ts_iomb;        // i/o bitmap base address，用来决定当前的任务是否可以访问特定的硬件端口，需要设置为段界限之外

    // 这里可以定义 io_bitmap，用于规定特权级不足时可以对哪些端口执行 IO 操作，ts_iomb 需要指向 io_bitmap
    // 如果我们将 ts_iomb 设置超过了 TSS 段界限（段寄存器的长度限制），表示我们不使用 io_bitmap
    // io_bitmap 以 0xFF 结尾，因此其长度 = 实际长度+1，且这个 0xFF 终止符需要在 TSS 段界限之内
    // CPU 最多能访问 65536 个端口，因此 io_bitmap 大小需要为 65536 字节 (8KB)
};

#endif /* !__KERN_MM_MMU_H__ */

