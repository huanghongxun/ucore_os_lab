#include <defs.h>
#include <mmu.h>
#include <memlayout.h>
#include <clock.h>
#include <trap.h>
#include <x86.h>
#include <stdio.h>
#include <assert.h>
#include <console.h>
#include <vmm.h>
#include <swap.h>
#include <kdebug.h>
#include <unistd.h>
#include <proc.h>
#include <syscall.h>
#include <error.h>
#include <sched.h>
#include <sync.h>
#include <proc.h>

#define TICK_NUM 100

static void print_ticks() {
    cprintf("%d ticks\n",TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}

/* *
 * Interrupt descriptor table:
 *
 * Must be built at run time because shifted function addresses can't
 * be represented in relocation records.
 * */
static struct gatedesc idt[256] = {{0}};

static struct pseudodesc idt_pd = {
    sizeof(idt) - 1, (uintptr_t)idt
};

void
idt_init(void) {
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
    for (i = 0; i < 256; ++i) {
        // 中断处理程序在内核代码段中，特权级为内核级
        SETGATE(idt[i], GATE_INT, GD_KTEXT, __vectors[i], DPL_KERNEL);
    }

    // LAB1 CHALLENGE：跳转到内核态的特权级为用户态
    // SETGATE(idt[T_SWITCH_TOK], GATE_INT, GD_KTEXT, __vectors[T_SWITCH_TOK], DPL_USER);
    
    /* LAB5 YOUR CODE */ 
    // you should update your lab1 code (just add ONE or TWO lines of code), let user app to use syscall to get the service of ucore
    // so you should setup the syscall interrupt gate in here
    SETGATE(idt[T_SYSCALL], GATE_TRAP, GD_KTEXT, __vectors[T_SYSCALL], DPL_USER);

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

static inline void
print_pgfault(struct trapframe *tf) {
    /* error_code:
     * bit 0 == 0 means no page found, 1 means protection fault
     * bit 1 == 0 means read, 1 means write
     * bit 2 == 0 means kernel, 1 means user
     * */
    cprintf("page fault at 0x%08x: %c/%c [%s].\n", rcr2(),
            (tf->tf_err & 4) ? 'U' : 'K',
            (tf->tf_err & 2) ? 'W' : 'R',
            (tf->tf_err & 1) ? "protection fault" : "no page found");
}

static int
pgfault_handler(struct trapframe *tf) {
    extern struct mm_struct *check_mm_struct;
    if(check_mm_struct !=NULL) { //used for test check_swap
            print_pgfault(tf);
        }
    struct mm_struct *mm;
    if (check_mm_struct != NULL) {
        assert(current == idleproc);
        mm = check_mm_struct;
    }
    else {
        if (current == NULL) {
            print_trapframe(tf);
            print_pgfault(tf);
            panic("unhandled page fault.\n");
        }
        mm = current->mm;
    }
    return do_pgfault(mm, tf->tf_err, rcr2());
}

static volatile int in_swap_tick_event = 0;
extern struct mm_struct *check_mm_struct;
struct trapframe kernel_tf;

/* 捕获并处理分发陷入帧 */
static void
trap_dispatch(struct trapframe *tf) {
    char c;

    int ret=0;

    switch (tf->tf_trapno) {
    case T_PGFLT:  //page fault
        if ((ret = pgfault_handler(tf)) != 0) {
            print_trapframe(tf);
            if (current == NULL) {
                panic("handle pgfault failed. ret=%d\n", ret);
            }
            else {
                if (trap_in_kernel(tf)) {
                    panic("handle pgfault failed in kernel mode. ret=%d\n", ret);
                }
                cprintf("killed by kernel.\n");
                panic("handle user mode pgfault failed. ret=%d\n", ret); 
                do_exit(-E_KILLED);
            }
        }
        break;
    case T_SYSCALL:
        syscall();
        break;
    case IRQ_OFFSET + IRQ_TIMER:
        ++ticks;
        /* LAB6 YOUR CODE */
        assert(current != NULL);
        sched_class_proc_tick(current);        
        /* LAB7 YOUR CODE */
        /* you should upate you lab6 code
         * IMPORTANT FUNCTIONS:
	     * run_timer_list
         */
        break;
    case IRQ_OFFSET + IRQ_COM1:
        c = cons_getc();
        cprintf("serial [%03d] %c\n", c, c);
        break;
    case IRQ_OFFSET + IRQ_KBD:
        c = cons_getc();
        cprintf("kbd [%03d] %c\n", c, c);
        break;
    //LAB1 CHALLENGE 1 : YOUR CODE you should modify below codes.
    // 和 trapentry.S 密切相关
    case T_SWITCH_TOU:
        // 如果当前已经是用户态，不执行操作
        if (tf->tf_cs == USER_CS) break;
        kernel_tf = *tf;
        // 返回用户态，通用寄存器值不需要更改
        kernel_tf.tf_cs = USER_CS; // 设置代码段，DPL 一定是 3
        kernel_tf.tf_ds = kernel_tf.tf_es = kernel_tf.tf_ss = USER_DS; // 设置数据段、堆栈段
        kernel_tf.tf_esp = (uint32_t)tf + sizeof(struct trapframe) - 8;
        // IOPL 权限级限制 io 指令，将 IOPL 设为 3（用户态）
        kernel_tf.tf_eflags |= FL_IOPL_MASK;

        *((uint32_t *)tf - 1) = (uint32_t)&kernel_tf;

        break;
    case T_SWITCH_TOK:
        // 如果当前已经是内核态，不执行操作
        if (tf->tf_cs == KERNEL_CS) break;

        // 从用户态进入内核态
        // 由于 trapentry 从 tf 中恢复现场，因此我们更改 tf 就可以改变调用完中断服务程序后的各个寄存器值
        tf->tf_cs = KERNEL_CS; // 设置代码段为内核代码段
        tf->tf_ds = tf->tf_es = KERNEL_DS; // 设置数据段为内核数据段
        tf->tf_eflags &= ~FL_IOPL_MASK; // IOPL 权限级限制 io 指令，将 IOPL 设为 0（内核态）

        struct trapframe *pkernel_tf = (struct trapframe *)(tf->tf_esp - (sizeof(struct trapframe) - 8));
        memmove(pkernel_tf, tf, sizeof(struct trapframe) - 8);
        *((uint32_t *)tf - 1) = (uint32_t)pkernel_tf;

        break;
    case IRQ_OFFSET + IRQ_IDE1:
    case IRQ_OFFSET + IRQ_IDE2:
        /* do nothing */
        break;
    default:
        print_trapframe(tf);
        if (current != NULL) {
            cprintf("unhandled trap.\n");
            do_exit(-E_KILLED);
        }
        // in kernel, it must be a mistake
        panic("unexpected trap in kernel.\n");

    }
}

/* *
 * 处理中断/陷入异常。
 * 该函数由 trapentry.S 调用，一旦发生中断，trayentry.S 负责收集所有必要的寄存器信息
 * @brief handles or dispatches an exception/interrupt. if and when trap() returns,
 * the code in kern/trap/trapentry.S restores the old CPU state saved in the
 * trapframe and then uses the iret instruction to return from the exception.
 * */
void
trap(struct trapframe *tf) {
    // dispatch based on what type of trap occurred
    // used for previous projects
    if (current == NULL) {
        trap_dispatch(tf);
    }
    else {
        // keep a trapframe chain in stack
        struct trapframe *otf = current->tf;
        current->tf = tf;
    
        bool in_kernel = trap_in_kernel(tf);
    
        trap_dispatch(tf);
    
        current->tf = otf;
        // 由于可能在系统调用或发生其他中断从
        // 用户态进入内核态后再次发生中断，
        // 因此我们需要检查当前状态是不是不是二级中断
        if (!in_kernel) {
            if (current->flags & PF_EXITING) {
                do_exit(-E_KILLED);
            }
            if (current->need_resched) {
                schedule();
            }
        }
    }
}

