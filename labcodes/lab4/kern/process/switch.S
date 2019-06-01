.text
.globl switch_to
switch_to:                      # switch_to(from, to)

    # 保存当前进程的寄存器值到 (struct context) from 中
    movl 4(%esp), %eax          # %eax 寄存器指向 from 进程的上下文结构体
    popl 0(%eax)                # save eip !popl
    movl %esp, 4(%eax)          # 保存 form 进程的 esp 寄存器到上下文中
    movl %ebx, 8(%eax)          # 保存 form 进程的 ebx 寄存器到上下文中
    movl %ecx, 12(%eax)         # 保存 form 进程的 ecx 寄存器到上下文中
    movl %edx, 16(%eax)         # 保存 form 进程的 edx 寄存器到上下文中
    movl %esi, 20(%eax)         # 保存 form 进程的 esi 寄存器到上下文中
    movl %edi, 24(%eax)         # 保存 form 进程的 edi 寄存器到上下文中
    movl %ebp, 28(%eax)         # 保存 form 进程的 ebp 寄存器到上下文中

    # 恢复 to 进程的寄存器值
    movl 4(%esp), %eax          # not 8(%esp): popped return address already
                                # eax now points to to
    movl 28(%eax), %ebp         # 从上下文中恢复 to 进程的 ebp 寄存器
    movl 24(%eax), %edi         # 从上下文中恢复 to 进程的 edi 寄存器
    movl 20(%eax), %esi         # 从上下文中恢复 to 进程的 esi 寄存器
    movl 16(%eax), %edx         # 从上下文中恢复 to 进程的 edx 寄存器
    movl 12(%eax), %ecx         # 从上下文中恢复 to 进程的 ecx 寄存器
    movl 8(%eax), %ebx          # 从上下文中恢复 to 进程的 ebx 寄存器
    movl 4(%eax), %esp          # 从上下文中恢复 to 进程的 esp 寄存器

    pushl 0(%eax)               # push eip

    ret # 该指令从栈中弹出 eip 并跳转到 forkret 函数

