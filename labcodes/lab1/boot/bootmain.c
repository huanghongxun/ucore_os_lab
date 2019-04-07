#include <defs.h>
#include <x86.h>
#include <elf.h>

/* *********************************************************************
 * This a dirt simple boot loader, whose sole job is to boot
 * an ELF kernel image from the first IDE hard disk.
 *
 * DISK LAYOUT
 *  * This program(bootasm.S and bootmain.c) is the bootloader.
 *    It should be stored in the first sector of the disk.
 *
 *  * The 2nd sector onward holds the kernel image.
 *
 *  * The kernel image must be in ELF format.
 *
 * BOOT UP STEPS
 *  * when the CPU boots it loads the BIOS into memory and executes it
 *
 *  * the BIOS intializes devices, sets of the interrupt routines, and
 *    reads the first sector of the boot device(e.g., hard-drive)
 *    into memory and jumps to it.
 *
 *  * Assuming this boot loader is stored in the first sector of the
 *    hard-drive, this code takes over...
 *
 *  * control starts in bootasm.S -- which sets up protected mode,
 *    and a stack so C code then run, then calls bootmain()
 *
 *  * bootmain() in this file takes over, reads in the kernel and jumps to it.
 * */

#define SECTSIZE        512
#define ELFHDR          ((struct elfhdr *)0x10000)      // scratch space

/* waitdisk - wait for disk ready */
static void
waitdisk(void) {
    while ((inb(0x1F7) & 0xC0) != 0x40)
        /* do nothing */;
}

/**
 * 读取扇区编号为 secno 的数据到内存 dst 中，操作端口的方式满足 LBA28.
 * @param dst 存放磁盘数据的内存区域，由于现在是 bootloader，内存空间不需要分配即可使用
 * @param secno 要读取数据的扇区编号
 */
static void readsect(void *dst, uint32_t secno) {
    waitdisk();                             // 等待操作完成

    outb(0x1F2, 1);                         // 这个函数每次只读取一个扇区的数据
    outb(0x1F3, secno & 0xFF);              // 扇区编号的 0~7 位
    outb(0x1F4, (secno >> 8) & 0xFF);       // 扇区编号的 8~15 位
    outb(0x1F5, (secno >> 16) & 0xFF);      // 扇区编号的 16~23 位
    outb(0x1F6, ((secno >> 24) & 0xF) | 0xE0); // 7~4 位位 1110，表示主盘（操作系统安装盘）
                                            // 3~0 位为扇区编号的 24~27 位
    outb(0x1F7, 0x20);                      // 0x20 表示读取，0x30 表示写入

    waitdisk();                             // 等待操作完成
    insl(0x1F0, dst, SECTSIZE / 4);         // 执行读取操作
}

/**
 * 读取从磁盘地址 offset 开始，count 字节的数据。
 * 可能会读取超过范围的数据到内存中（将多出来的部分予以覆盖）
 */
static void readseg(uintptr_t va, uint32_t count, uint32_t offset) {
    uintptr_t end_va = va + count; // 内存读取的结束地址
    va -= offset % SECTSIZE; // 我们要将数据存放在 va 开始的内存区域，但是数据读取必须扇区对齐
                             // 因此我们将 va 之前向前移动到扇区开始处以便将内存区域与扇区对齐
    uint32_t secno = (offset / SECTSIZE) + 1; // 扇区号从 1 开始

    // If this is too slow, we could read lots of sectors at a time.
    // We'd write more to memory than asked, but it doesn't matter --
    // we load in increasing order.
    for (; va < end_va; va += SECTSIZE, secno ++) {
        // 每个扇区都进行一次读取操作
        readsect((void *)va, secno);
    }
}

/* 操作系统加载入口 */
void bootmain(void) {
    // 读入磁盘的 1 号扇区（ELF 可执行文件头地址）
    readseg((uintptr_t)ELFHDR, SECTSIZE * 8, 0);

    // 检查魔数是否匹配，否则当前启动盘内并没有存放 ELF 格式的可执行程序（可启动的操作系统）
    if (ELFHDR->e_magic != ELF_MAGIC) {
        goto bad;
    }

    struct proghdr *ph, *eph;

    // 根据偏移地址计算程序段头记录表
    ph = (struct proghdr *)((uintptr_t)ELFHDR + ELFHDR->e_phoff);
    eph = ph + ELFHDR->e_phnum;
    for (; ph < eph; ph ++) { // ph 指向程序段表的每一项
        // 由于编译后各个段（代码、常量）地址都已经被固定，因此我们从 ph 中读取出代码段的地址。
        // 由于 p_offset 是相对于文件头的偏移值（相对于 elf 本身），ELF 本身存放在 1 号扇区中，
        // 因此 ph->p_offset 就是该程序段的磁盘地址，一次性将操作系统的数据读到对应的内存中。
        readseg(ph->p_va & 0xFFFFFF, ph->p_memsz, ph->p_offset);
    }

    // 从 ELF 头中读取出操作系统入口函数地址并予以调用，将 CPU 控制权转交给操作系统
    // 这个函数将执行到操作系统关闭或故障
    ((void (*)(void))(ELFHDR->e_entry & 0xFFFFFF))();

bad: // 由于操作系统未能正确地写入磁盘，因此报错并等待
    outw(0x8A00, 0x8A00);
    outw(0x8A00, 0x8E00);

    /* do nothing */
    while (1);
}

