#ifndef __KERN_TRAP_TRAP_H__
#define __KERN_TRAP_TRAP_H__

#include <defs.h>

/* Trap Numbers */

/* Processor-defined: */
#define T_DIVIDE                0    // divide error
#define T_DEBUG                 1    // debug exception
#define T_NMI                   2    // non-maskable interrupt
#define T_BRKPT                 3    // breakpoint
#define T_OFLOW                 4    // overflow
#define T_BOUND                 5    // bounds check
#define T_ILLOP                 6    // illegal opcode
#define T_DEVICE                7    // device not available
#define T_DBLFLT                8    // double fault
// #define T_COPROC             9    // reserved (not used since 486)
#define T_TSS                  10    // invalid task switch segment
#define T_SEGNP                11    // segment not present
#define T_STACK                12    // stack exception
#define T_GPFLT                13    // general protection fault
#define T_PGFLT                14    // page fault
// #define T_RES               15    // reserved
#define T_FPERR                16    // floating point error
#define T_ALIGN                17    // aligment check
#define T_MCHK                 18    // machine check
#define T_SIMDERR              19    // SIMD floating point error

#define T_SYSCALL               0x80 // SYSCALL, ONLY FOR THIS PROJ

/* Hardware IRQ numbers. We receive these as (IRQ_OFFSET + IRQ_xx) */
#define IRQ_OFFSET             32    // IRQ 0 corresponds to int IRQ_OFFSET

#define IRQ_TIMER               0
#define IRQ_KBD                 1
#define IRQ_COM1                4
#define IRQ_IDE1               14
#define IRQ_IDE2               15
#define IRQ_ERROR              19
#define IRQ_SPURIOUS           31

/**
 * These are arbitrarily chosen, but with care not to overlap
 * processor defined exceptions or interrupt vectors.
 */

#define T_SWITCH_TOU           120   // switch to user mode
#define T_SWITCH_TOK           121   // switch to kernel mode

/**
 * 顺序必须和 pushal 指令的顺序一致
 * 
 * https://c9x.me/x86/html/file_module_x86_id_270.html
 */
struct pushregs {
    uint32_t reg_edi;
    uint32_t reg_esi;
    uint32_t reg_ebp;
    uint32_t reg_oesp;            /* 调用 pushal 指令的指令地址 */
    uint32_t reg_ebx;
    uint32_t reg_edx;
    uint32_t reg_ecx;
    uint32_t reg_eax;
};

/**
 * 保存调用中断时的所有必要的寄存器值，以便恢复现场。
 * 顺序必须和 trapentry.S 中代码保持一致
 */
struct trapframe {
    struct pushregs tf_regs;
    uint16_t tf_gs;
    uint16_t __gsh;
    uint16_t tf_fs;
    uint16_t __fsh;
    uint16_t tf_es;
    uint16_t __esh;
    uint16_t tf_ds;
    uint16_t __dsh;
    uint32_t tf_trapno; // 在 vectors.S 中压栈
    uint32_t tf_err;    // 在 vectors.S 中压栈

    /*
     * 以下由 CPU 在中断触发时压栈，由 iret 指令弹出
     * https://c9x.me/x86/html/file_module_x86_id_145.html
     */

    uintptr_t tf_eip;
    uint16_t tf_cs;
    uint16_t __csh;
    uint32_t tf_eflags;

    /* 变换特权级时由 CPU 压入 */

    uintptr_t tf_esp;
    uint16_t tf_ss;
    uint16_t __ssh;
} __attribute__((packed)); // We insert padding by ourself

/* 初始化 IDT，入口点定义在 kern/trap/vectors.S 中 */
void idt_init(void);

/* 输出陷入帧 */
void print_trapframe(struct trapframe *tf);

/* 输出寄存器值 */
void print_regs(struct pushregs *regs);

/* 检查陷入异常是否是由内核本身触发的 */
bool trap_in_kernel(struct trapframe *tf);

#endif /* !__KERN_TRAP_TRAP_H__ */

