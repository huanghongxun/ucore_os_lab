#include <defs.h>
#include <mmu.h>
#include <memlayout.h>
#include <clock.h>
#include <trap.h>
#include <x86.h>
#include <stdio.h>
#include <assert.h>
#include <console.h>
#include <kdebug.h>

#define TICK_NUM 100

static void print_ticks() {
    cprintf("%d ticks\n", TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}

#define IDT_SIZE 256

/**
 * Interrupt descriptor table:
 *
 * Must be built at run time because shifted function addresses can't
 * be represented in relocation records.
 */
static struct gatedesc idt[IDT_SIZE] = {{0}};

static struct pseudodesc idt_pd = {
    sizeof(idt) - 1, (uintptr_t)idt
};

void idt_init(void) {
    // LAB1 YOUR CODE : STEP 2
    // (1) Where are the entry addrs of each Interrupt Service Routine (ISR)?
    //     All ISR's entry addrs are stored in __vectors. where is uintptr_t __vectors[] ?
    //     __vectors[] is in kern/trap/vector.S which is produced by tools/vector.c
    //     (try "make" command in lab1, then you will find vector.S in kern/trap DIR)
    //     You can use  "extern uintptr_t __vectors[];" to define this extern variable which will be used later.

    // 表示各个中断处理程序的段内偏移地址
    extern uintptr_t __vectors[]; // defined in kern/trap/vector.S

    // (2) Now you should setup the entries of ISR in Interrupt Description Table (IDT).
    //     Can you see idt[256] in this file? Yes, it's IDT! you can use SETGATE macro to setup each item of IDT

    int i;
    for (i = 0; i < IDT_SIZE; ++i) {
        // 中断处理程序在内核代码段中，特权级为内核级
        SETGATE(idt[i], GATE_INT, GD_KTEXT, __vectors[i], DPL_KERNEL);
    }

    // 系统调用中断（陷入中断）的特权级为用户态
    SETGATE(idt[T_SWITCH_TOK], GATE_INT, GD_KTEXT, __vectors[T_SWITCH_TOK], DPL_USER);

    // (3) After setup the contents of IDT, you will let CPU know where is the IDT by using 'lidt' instruction.
    //     You don't know the meaning of this instruction? just google it! and check the libs/x86.h to know more.
    //     Notice: the argument of lidt is idt_pd. try to find it

    lidt(&idt_pd);
}

/**
 * Find trap display name of trap number.
 * @return trap name if it is software trap; "Hardware Interrupt" if it is hardware interrupt; "(unknown interrupt)" otherwise.
 */
static const char *
trapname(int trapno) {
    static const char * const excnames[] = {
        "Divide error",
        "Debug",
        "Non-Maskable Interrupt",
        "Breakpoint",
        "Overflow",
        "BOUND Range Exceeded",
        "Invalid Opcode",
        "Device Not Available",
        "Double Fault",
        "Coprocessor Segment Overrun",
        "Invalid TSS",
        "Segment Not Present",
        "Stack Fault",
        "General Protection",
        "Page Fault",
        "(unknown trap)",
        "x87 FPU Floating-Point Error",
        "Alignment Check",
        "Machine-Check",
        "SIMD Floating-Point Exception"
    };

    if (trapno < sizeof(excnames)/sizeof(const char * const)) {
        return excnames[trapno];
    }
    if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16) {
        return "Hardware Interrupt";
    }
    return "(unknown trap)";
}

bool
trap_in_kernel(struct trapframe *tf) {
    return (tf->tf_cs == (uint16_t)KERNEL_CS);
}

static const char *IA32flags[] = {
    "CF", NULL, "PF", NULL, "AF", NULL, "ZF", "SF",
    "TF", "IF", "DF", "OF", NULL, NULL, "NT", NULL,
    "RF", "VM", "AC", "VIF", "VIP", "ID", NULL, NULL,
};

void
print_trapframe(struct trapframe *tf) {
    cprintf("trapframe at %p\n", tf);
    print_regs(&tf->tf_regs);
    cprintf("  ds   0x----%04x\n", tf->tf_ds);
    cprintf("  es   0x----%04x\n", tf->tf_es);
    cprintf("  fs   0x----%04x\n", tf->tf_fs);
    cprintf("  gs   0x----%04x\n", tf->tf_gs);
    cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
    cprintf("  err  0x%08x\n", tf->tf_err);
    cprintf("  eip  0x%08x\n", tf->tf_eip);
    cprintf("  cs   0x----%04x\n", tf->tf_cs);
    cprintf("  flag 0x%08x ", tf->tf_eflags);

    int i, j;
    for (i = 0, j = 1; i < sizeof(IA32flags) / sizeof(IA32flags[0]); i ++, j <<= 1) {
        if ((tf->tf_eflags & j) && IA32flags[i] != NULL) {
            cprintf("%s,", IA32flags[i]);
        }
    }
    cprintf("IOPL=%d\n", (tf->tf_eflags & FL_IOPL_MASK) >> 12);

    if (!trap_in_kernel(tf)) {
        cprintf("  esp  0x%08x\n", tf->tf_esp);
        cprintf("  ss   0x----%04x\n", tf->tf_ss);
    }
}

void
print_regs(struct pushregs *regs) {
    cprintf("  edi  0x%08x\n", regs->reg_edi);
    cprintf("  esi  0x%08x\n", regs->reg_esi);
    cprintf("  ebp  0x%08x\n", regs->reg_ebp);
    cprintf("  oesp 0x%08x\n", regs->reg_oesp);
    cprintf("  ebx  0x%08x\n", regs->reg_ebx);
    cprintf("  edx  0x%08x\n", regs->reg_edx);
    cprintf("  ecx  0x%08x\n", regs->reg_ecx);
    cprintf("  eax  0x%08x\n", regs->reg_eax);
}

/* 捕获并处理分发陷入帧 */
static void trap_dispatch(struct trapframe *tf) {
    static struct trapframe tou, tok;
    char c;

    switch (tf->tf_trapno) {
    case IRQ_OFFSET + IRQ_TIMER:
        /* LAB1 YOUR CODE : STEP 3 */
        /* handle the timer interrupt */
        // (1) After a timer interrupt, you should record this event using a global variable (increase it), such as ticks in kern/driver/clock.c
        ++ticks;
        // (2) Every TICK_NUM cycle, you can print some info using a funciton, such as print_ticks().
        if (ticks == TICK_NUM) {
            ticks = 0;
            print_ticks();
        }
        // (3) Too Simple? Yes, I think so!
        break;
    case IRQ_OFFSET + IRQ_COM1:
        c = cons_getc();
        cprintf("serial [%03d] %c\n", c, c);
        break;
    case IRQ_OFFSET + IRQ_KBD:
        c = cons_getc();
        cprintf("kbd [%03d] %c\n", c, c);
        break;
    // LAB1 CHALLENGE 1 : YOUR CODE you should modify below codes.
    // 和 trapentry.S 密切相关
    case T_SWITCH_TOU:
        // 如果当前已经是用户态，不执行操作
        if (tf->tf_cs == USER_CS) break;

        // 返回用户态，通用寄存器值不需要更改
        tf->tf_cs = USER_CS; // 设置代码段，DPL 一定是 3
        tf->tf_gs = tf->tf_fs = tf->tf_es = tf->tf_ds = tf->tf_ss = USER_DS; // 设置数据段
        tf->tf_esp = (uintptr_t)tf + sizeof(struct trapframe) - 8; 
        // IOPL 权限级限制 io 指令，将 IOPL 设为 3（用户态）
        tf->tf_eflags |= FL_IOPL_MASK;

        break;
    case T_SWITCH_TOK:
        // 如果当前已经是内核态，不执行操作
        if (tf->tf_cs == KERNEL_CS) break;

        // 从用户态进入内核态
        // 由于 trapentry 从 tf 中恢复现场，因此我们更改 tf 就可以改变调用完中断服务程序后的各个寄存器值
        tf->tf_cs = KERNEL_CS; // 设置代码段为内核代码段
        tf->tf_gs = tf->tf_fs = tf->tf_es = tf->tf_ds = tf->tf_ss = KERNEL_DS; // 设置数据段为内核数据段
        tf->tf_eflags &= ~FL_IOPL_MASK; // IOPL 权限级限制 io 指令，将 IOPL 设为 0（内核态）

        
        break;
    case IRQ_OFFSET + IRQ_IDE1:
    case IRQ_OFFSET + IRQ_IDE2:
        /* do nothing */
        break;
    default:
        // 内核内部不可能触发陷入，如果发生则报错
        if ((tf->tf_cs & 3) == 0) {
            print_trapframe(tf);
            panic("unexpected trap in kernel.\n");
        }
    }
}

/**
 * 处理中断/陷入异常。
 * 该函数由 trapentry.S 调用，一旦发生中断，trayentry.S 负责收集所有必要的寄存器信息
 * @brief handles or dispatches an exception/interrupt. if and when trap() returns,
 * the code in kern/trap/trapentry.S restores the old CPU state saved in the
 * trapframe and then uses the iret instruction to return from the exception.
 */
void trap(struct trapframe *tf) {
    // dispatch based on what type of trap occurred
    trap_dispatch(tf);
}

