#include <defs.h>
#include <x86.h>
#include <mmu.h>
#include <memlayout.h>
#include <pmm.h>

/**
 * 任务状态段（Task State Segment）:
 *
 * TSS 在内存中的位置不定（因此这里就直接定义了 ts 而没有规定其地址。
 * 会有一个任务寄存器 (TR) 来记录 TSS 结构体的位置（段选择子，因此具体位置在 GDT 中）。
 * gdt_init 函数需要：
 *   1. 在 GDT 中创建 TSS 段描述符
 *   2. 在内存中创建 TSS 结构体并进行初始化
 *   3. 设置 TR 寄存器
 *
 * TSS 中的一些属性存储切换到新特权级时的堆栈指针值。但是本内核只使用内核态（CPL=0）和用户态
 * （CPL=3）。因此只使用 SS0 和 ESP0。
 *
 * TSS.SS0 存储当前特权级 CPL=0 时的堆栈段寄存器值，ESP0 包含 CPL=0 时的堆栈指针寄存器的值。
 * 在保护模式下，如果发生中断，x86 CPU 将会从 TSS.{SS0,ESP0} 加载对应值到寄存器中（因为中断
 * 导致跳转到内核态，内核态的特权级为 0），并将旧值
 * 载入堆栈。
 */
static struct taskstate ts = {0};

/**
 * Global Descriptor Table:
 *
 * The kernel and user segments are identical (except for the DPL). To load
 * the %ss register, the CPL must equal the DPL. Thus, we must duplicate the
 * segments for the user and the kernel. Defined as follows:
 *   - 0x0 :  unused (always faults -- for trapping NULL far pointers)
 *   - 0x8 :  kernel code segment
 *   - 0x10:  kernel data segment
 *   - 0x18:  user code segment
 *   - 0x20:  user data segment
 *   - 0x28:  defined for tss, initialized in gdt_init
 * */
static struct segdesc gdt[] = {
    SEG_NULL,
    [SEG_KTEXT] = SEG(STA_X | STA_R, 0x0, 0xFFFFFFFF, DPL_KERNEL),
    [SEG_KDATA] = SEG(STA_W, 0x0, 0xFFFFFFFF, DPL_KERNEL),
    [SEG_UTEXT] = SEG(STA_X | STA_R, 0x0, 0xFFFFFFFF, DPL_USER),
    [SEG_UDATA] = SEG(STA_W, 0x0, 0xFFFFFFFF, DPL_USER),
    [SEG_TSS]   = SEG_NULL,
};

static struct pseudodesc gdt_pd = {
    sizeof(gdt) - 1, (uint32_t)gdt
};

/**
 * 加载 GDT 寄存器，并为内核初始化数据段寄存器、代码段寄存器
 * */
static inline void
lgdt(struct pseudodesc *pd) {
    asm volatile ("lgdt (%0)" :: "r" (pd));
    asm volatile ("movw %%ax, %%gs" :: "a" (USER_DS));
    asm volatile ("movw %%ax, %%fs" :: "a" (USER_DS));
    asm volatile ("movw %%ax, %%es" :: "a" (KERNEL_DS));
    asm volatile ("movw %%ax, %%ds" :: "a" (KERNEL_DS));
    asm volatile ("movw %%ax, %%ss" :: "a" (KERNEL_DS));
    // reload cs
    asm volatile ("ljmp %0, $1f\n 1:\n" :: "i" (KERNEL_CS));
}

/* 临时的内核堆栈（lab2 中将进行修改） */
uint8_t stack0[1024];

/* 初始化默认的 GDT、TSS */
static void
gdt_init(void) {
    // 初始化 TSS，允许用户程序进行系统调用。
    // 这里的实现还不安全，我们将内核态的 esp 设为我们临时的内核堆栈区，
    // 堆栈段寄存器设置为内核数据区（内核堆栈段区lab2 中将会改成 KSTACKTOP。
    ts.ts_esp0 = (uint32_t)&stack0 + sizeof(stack0);
    ts.ts_ss0 = KERNEL_DS;

    // initialize the TSS filed of the gdt
    gdt[SEG_TSS] = SEG16(STS_T32A, (uint32_t)&ts, sizeof(ts), DPL_KERNEL);
    gdt[SEG_TSS].sd_s = 0;

    // 重置所有的段寄存器
    lgdt(&gdt_pd);

    // load the TSS
    ltr(GD_TSS);
}

void
pmm_init(void) {
    gdt_init();
}

