#include <asm.h>

# bootloader:
# 0. BIOS 从磁盘第一扇区（主引导记录）中获取 bootloader 指令到 0:7C00 处
# 1. 进入 32 位寻址模式
# 2. 调用 BIOS 中断获取物理内存布局
# 3. 进入 32 位模式
# 4. 进入 C 程序 bootmain

.set PROT_MODE_CSEG,        0x8                     # 内核代码段选择子
.set PROT_MODE_DSEG,        0x10                    # 内核数据段选择子
.set CR0_PE_ON,             0x1                     # protected mode enable flag
.set SMAP,                  0x534d4150

# start 标签的指令 (cli 指令) 位于 0:7C00
.globl start
start:
.code16                                             # Assemble for 16-bit mode
    cli                                             # Disable interrupts
    cld                                             # String operations increment

    # 初始化数据段寄存器（xor 优势是指令字节数少，参数只需要寄存器，而不需要额外的字节存放立即数）
    xorw %ax, %ax                                   # Segment number zero
    movw %ax, %ds                                   # -> Data Segment
    movw %ax, %es                                   # -> Extra Segment
    movw %ax, %ss                                   # -> Stack Segment

    # 启用 A20 地址线。8086 CPU 只支持 20 位地址线寻址，80286、80386
    # 为了向下兼容，强制将第 20 位地址线置为低电平（超出的地址就取模）。
    # 为了能寻址整块内存空间，需要启用 A20 地址线。
    # A20地址线由键盘控制器 8042 进行控制
    # 将P21引脚置1的操作：查手册知，首先要先向64h发送0xd1的指令，然后向60h发送0xdf的指令
seta20.1: # 向 64H 发送 0xD1 指令
    inb $0x64, %al                                  # Wait for not busy(8042 input buffer empty).
    testb $0x2, %al
    jnz seta20.1

    movb $0xd1, %al                                 # 0xd1 -> port 0x64
    outb %al, $0x64                                 # 0xd1 means: write data to 8042's P2 port

seta20.2:
    inb $0x64, %al                                  # Wait for not busy(8042 input buffer empty).
    testb $0x2, %al
    jnz seta20.2

    movb $0xdf, %al                                 # 0xdf -> port 0x60
    outb %al, $0x60                                 # 0xdf = 11011111, means set P2's A20 bit(the 1 bit) to 1

# 通过 BIOS 中断调用来帮助了解计算机系统中的物理内存分布（哪些地址可用、哪些地址不可用）
# 由于 BIOS 中断调用必须在实模式下完成，因此由 bootloader 完成该任务比较方便。
# 0x15 号 BIOS 中断将内存布局通过系统内存映射地址描述符（Address Range Descriptor）的
# 形式返回给调用者。

probe_memory:
    movl $0, 0x8000                                 # 0x8000: 该内存地址用于记录已读取的 ARD 的字节数
    xorl %ebx, %ebx                                 # %ebx: 如果是第一次调用或内存区域扫描完毕，则为 0；否则存放上次调用之后的计数值
    movw $0x8004, %di                               # %edi: 指向保存 ARD 结构的缓冲区，BIOS 从这里开始写入信息
start_probe:
    movl $0xE820, %eax                              # %eax: 0x15 中断获取内存可调用参数
    movl $20, %ecx                                  # $ecx: 保存 ARD 的内存大小，至少为 20 个字节
    movl $SMAP, %edx                                # $edx: 4 个 ASCII 字符 "SMAP"，只是一个签名
    int $0x15                                       # 调用中断
    jnc cont                                        # 中断成功返回时跳转 cont 读取数据
    movw $12345, 0x8000                             # 否则将 0x8000 设为特殊值
    jmp finish_probe
cont:
    addw $20, %di
    incl 0x8000                                     # 记录已读取的 ARD 个数
    cmpl $0, %ebx                                   # %ebx: 中断返回后保存下一个 ARD 的计数地址
    jnz start_probe                                 # 如果不存在下一个 ARD 的计数地址，则表示已经读取完成
finish_probe:

    # 切换到保护模式，这里使用主引导记录的最简单的 GDT 来进行虚拟地址索引。
    # 更加复杂的 GDT 将由内核完成设置。
    lgdt gdtdesc
    # 设置 cr0 寄存器的保护模式位来进入保护模式
    movl %cr0, %eax
    orl $CR0_PE_ON, %eax
    movl %eax, %cr0

    # 跳转到 32 位指令将使 CPU 进入 32 位模式
    ljmp $PROT_MODE_CSEG, $protcseg

.code32                                             # Assemble for 32-bit mode
protcseg:
    # 初始化数据段寄存器
    movw $PROT_MODE_DSEG, %ax                       # Our data segment selector
    movw %ax, %ds                                   # -> DS: Data Segment
    movw %ax, %es                                   # -> ES: Extra Segment
    movw %ax, %fs                                   # -> FS
    movw %ax, %gs                                   # -> GS
    movw %ax, %ss                                   # -> SS: Stack Segment

    # 初始化 C 程序所需的 %ebp 和 %esp 寄存器的值。
    # %ebp 设为 0 对堆栈调用追踪非常有用，0 表示这里的调用是最深层的根函数
    # %esp 设为 0x7C00，即栈空间为 [0, 0x7C00)。
    movl $0x0, %ebp
    movl $start, %esp
    call bootmain

    # 操作系统内核引导程序启动后不应该退出。若退出，则待机
spin:
    jmp spin

.data
# 主引导记录的 GDT
.p2align 2                                          # force 4 byte alignment
gdt:
    SEG_NULLASM                                     # null seg
    SEG_ASM(STA_X|STA_R, 0x0, 0xffffffff)           # code seg for bootloader and kernel
    SEG_ASM(STA_W, 0x0, 0xffffffff)                 # data seg for bootloader and kernel

gdtdesc:
    .word 0x17                                      # sizeof(gdt) - 1
    .long gdt                                       # address gdt
