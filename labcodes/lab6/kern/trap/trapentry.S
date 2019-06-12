#include <memlayout.h>

# CPU 会先检查特权级是否变化，如果变化，就需要操作 TSS
# 将新特权级相关的 ss 和 esp 值装载到 ss 和 esp 寄存器，在新的栈中保存 ss 和 esp 以前的值（由 iret 指令中恢复）
# 在栈中保存 eflags、cs、eip 的内容
# 装载 cs 和 eip 寄存器（从 IDT 中读出的，跳转到中断处理程序
# vectors.S sends all traps here.
.text
.globl __alltraps
__alltraps:
    # 以下代码在内核栈中构建 trapframe，trapframe 需要寄存器的数据
    # 因此通过 pushl 和 pushal 来构建 trapframe 的数据
    # trapframe 还包含由 CPU 装载进内核栈中的数据 ss、esp 等内容
    pushl %ds # trapframe.__dsh & ds
    pushl %es # trapframe.__esh & es
    pushl %fs # trapframe.__fsh & fs
    pushl %gs # trapframe.__gsh & gs
    pushal    # trapframe.tf_regs, pushl %esp 指向这里，也就是 tf 指向这里
    # trapframe 到此结束

    # 设置数据段为内核数据段
    movl $GD_KDATA, %eax
    movw %ax, %ds
    movw %ax, %es

    # 此时 esp 指向栈顶，也就是 trapframe 的地址
    # 将 esp 压栈，为 trap 函数的参数，
    pushl %esp     # tf - 1 指向这里

    # 调用 trap(tf), 其中 tf=%esp
    call trap

    # 弹出压入的 %esp
    popl %esp

    # return falls through to trapret...
.globl __trapret
__trapret:
    # 将寄存器从 tf 中恢复出来（tf 可能会被 trap 函数修改）
    popal

    # 恢复段寄存器值
    popl %gs
    popl %fs
    popl %es
    popl %ds

    # 弹出中断服务程序 (vectors.S) 压入的错误码和中断编号
    addl $0x8, %esp
    
    # 中断返回，CPU 恢复现场
    # CPU 弹出压入的 %eip、%cs（32 位）、%eflags
    # 检查 cs 的最低两位的值（DPL），如果不相等，则继续
    # 从栈中装载 ss 和 esp 寄存器，恢复到旧特权级的栈
    # 检查 ds、es、fs、gs 的特权级，如果特权级比 cs 的特权级要高，则清除这些寄存器的值
    # https://c9x.me/x86/html/file_module_x86_id_145.html
    iret

.globl forkrets
forkrets:
    # set stack to this new process's trapframe
    movl 4(%esp), %esp
    jmp __trapret
