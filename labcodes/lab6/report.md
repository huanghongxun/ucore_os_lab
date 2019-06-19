# 操作系统实验 Lab 6 调度器

## 实验目的

- 理解操作系统的调度管理机制
- 熟悉 ucore 的系统调度器框架，以及缺省的 Round-Robin 调度算法
- 基于调度器框架实现一个 Stride Scheduling 调度算法来替换缺省的调度算法

## 实验内容

- 实验6（ucore lab5）完成了用户进程的管理，可在用户态运行多个进程。但到目前为止，采用的调度策略是很简单的FIFO调度策略。
- 本次实验7 （ucore lab6） ，主要是熟悉 ucore 的系统调度器框架，以及基于此框架的 Round-Robin（RR） 调度算法。然后参考 RR 调度算法的实现，完成 Stride Scheduling 调度算法。

## 练习0：填写已有实验

> 请把你做的实验2/3/4/5的代码填入本实验中代码中有“LAB1”/“LAB2”/“LAB3”/“LAB4”“LAB5”的注释相应部分。 并确保编译通过。注意：为了能够正确执行lab6的测试应用程序，可能需对已完成的实验1/2/3/4/5的代码进行进一步改进。

#### `proc.c:alloc_proc`

根据提示，我们可以写出如下代码：

```cpp
       proc->rq = NULL; // 初始化该进程所属的运行队列
       list_init(&proc->run_link); // 初始化该进程所属队列的指针
       proc->time_slice = 0; // 初始化当前进程的时间片为 0
       skew_heap_init(&proc->lab6_run_pool);
       proc->lab6_stride = 0; // 设置当前进程的
       proc->lab6_priority = 1; // 设置当前进程的优先级为最低
```

#### `trap.c:trap_dispatch`

这里我们选择直接调用调度算法的时间中断处理函数来负责决定此时是否需要进行调度。

```cpp
    case IRQ_OFFSET + IRQ_TIMER:
        ++ticks;
        /* LAB6 YOUR CODE */
        assert(current != NULL);
        sched_class_proc_tick(current);
        break;
```

## 练习**1**：使用 Round Robin 调度算法

> 完成练习 0 后，建议大家比较一下（可用 kdiff3 等文件比较软件）个人完成的 lab5 和练习 0 完成后的刚修改的 lab6 之间的区别，分析了解 lab6 采用 RR 调度算法后的执行过程。执行 make grade，大部分测试用例应该通过。但执行 priority.c 应该过不去 。
> 请在实验报告完成下面要求：
>
> - 请理解并分析 sched_class 中各个函数指针的用法，并接合 Round Robin 调度算法描述 ucore 的调度执行过程

### `sched_class` 各个函数的用法

#### `sched_class`

```cpp
struct sched_class default_sched_class = {
    .name = "RR_scheduler",
    .init = RR_init,
    .enqueue = RR_enqueue,
    .dequeue = RR_dequeue,
    .pick_next = RR_pick_next,
    .proc_tick = RR_proc_tick,
};
```

这个结构体描述了一个调度算法，我们可以通过更换结构体中的函数指针来实现更换调度器。每个函数的用途我会在之后的章节中介绍。

#### `run_queue`

```cpp
/**
 * 存储调度算法维护的进程队列等数据结构
 */
struct run_queue {
    list_entry_t run_list; // 调度队列的头指针
    unsigned int proc_num; // 在调度队列中的进程数
    int max_time_slice; // 队列的最大时间片，进程重新获得时间片的时间为该项值
    // For LAB6 ONLY
    skew_heap_entry_t *lab6_run_pool; // 对于 Stride Scheduling 算法，该项为左偏树的根节点
};
```

#### `RR_init`

```cpp
/**
 * 初始化 Round Rubin 调度算法
 */
static void RR_init(struct run_queue *rq) {
    list_init(&(rq->run_list));
    rq->proc_num = 0;
}
```

#### `RR_enqueue`

```cpp
/**
 * 将进程加入到调度队列中等待下一次调度。
 * 遇到以下两种情况会将进程进入调度队列并（唤醒进程或让出 CPU）
 * 如果当前进程被唤醒（比如拿到了等待的资源，不需要继续等待了，通过 `wake_up` 函数调用；
 * 如果当前进程时间片已到，需要再次调度时
 */
static void
RR_enqueue(struct run_queue *rq, struct proc_struct *proc) {
    assert(list_empty(&(proc->run_link)));
    // 将进程加入到调度队列中
    list_add_before(&(rq->run_list), &(proc->run_link));
    // 如果进程的时间片已到，那么重置该进程的时间片
    if (proc->time_slice == 0 || proc->time_slice > rq->max_time_slice) {
        proc->time_slice = rq->max_time_slice;
    }
    proc->rq = rq; // proc 进程
    rq->proc_num ++; // 待调度的进程数 + 1
}
```

#### `RR_dequeue`

```cpp
/**
 * 将进程从调度队列中删除。
 * 调用此函数后一般会调用 `proc_run` 让进程抢占 CPU。
 */
static void RR_dequeue(struct run_queue *rq, struct proc_struct *proc) {
    assert(!list_empty(&(proc->run_link)) && proc->rq == rq);
    list_del_init(&(proc->run_link)); // 从调度队列中删除
    rq->proc_num --; // 待调度的进程数 - 1
}
```

#### `RR_pick_next`

```cpp
/**
 * 选择一个进程抢占 CPU
 * Round Rubin 算法选择最先失去 CPU 执行权限的进程调度
 */
static struct proc_struct *RR_pick_next(struct run_queue *rq) {
    // 从队列头取出一个进程
    // 由于时间片到期的进程被加入队尾，
    // 因此队列头是最长尚未获得执行权的进程，给予这个进程执行权
    list_entry_t *le = list_next(&(rq->run_list));
    if (le != &(rq->run_list)) {
        return le2proc(le, run_link);
    }
    return NULL;
}
```

#### `RR_proc_tick`

```cpp
/**
 * 时间中断调度处理函数
 * 每次时间中断后维护当前进程的时间片，
 * 如果时间片到期了，执行调度
 */
static void
RR_proc_tick(struct run_queue *rq, struct proc_struct *proc) {
    if (proc->time_slice > 0) {
        proc->time_slice --;
    }
    if (proc->time_slice == 0) {
        // 将当前进程的 need_reschedule 设为 true
        // 这样 trap 函数最后就会调用 schedule 函数以执行调度
        proc->need_resched = 1;
    }
}
```

### Round Robin 调度算法执行过程

Round Robin 算法认为一个时间片到期的进程需要让出 CPU 并重新调度，因此：

1. 算法在每个时钟周期通过 `RR_proc_tick` 维护进程的时间片（减 1）
2. 检查当前执行进程的时间片是否用完，如果用完则标记为需要被调度（`current->need_resched = 1`）
3. ucore 会在用户态跳转到内核态的一级中断结束前（调度算法已经维护过时间片并且知道了当前进程是否需要进行调度），检查是否需要调度并进行调度
4. 调度时将当前被调走的进程通过 `RR_enqueue` 加入到调度队列中，若该进程时间片用完，则恢复其时间片
5. 调度器通过 `RR_pick_next` 挑选下一个获得 CPU 时间的进程，并通过 `RR_dequeue` 将其取出
6. 如果未能找到一个可以执行的用户进程（所有的进程都不是就绪的，比如在等待资源），那么选择 `idle` 进程执行，这个进程将不断检查是否存在可以调度的进程
7. 调用 `proc_run` 切换进程

### 多级反馈队列调度算法



## 练习2：实现 Stride Scheduling 调度算法

> 首先需要换掉 RR 调度器的实现，即用 default_sched_stride_c 覆盖default_sched.c。然后根据此文件和后续文档对 Stride 调度器的相关描述，完成 Stride 调度算法的实现。

### 算法思路

该算法为每一个进程定义了优先级 `priority` 表示其优先级，`stride` 值表示这个进程的优先程度（是优先级倒数的累加和）。

### 算法实现

#### `stride_init`

```cpp
/**
 * @brief 初始化运行队列 rq
 */
static void
stride_init(struct run_queue *rq) {
     /* LAB6: YOUR CODE */
     // (1) init the ready process list: rq->run_list: should be a empty list after initialization.
     list_init(&rq->run_list);
     // (2) init the run pool: rq->lab6_run_pool: NULL
     rq->lab6_run_pool = NULL;
     // (3) set number of process: rq->proc_num to 0
     rq->proc_num = 0;
     // max_time_slice: no need here, the variable would be assigned by the caller.
}
```

#### `stride_enqueue`

```cpp
/**
 * @brief 将 proc 插入到运行队列 rq 中。
 * 本函数将会检查/初始化 proc 的相关成员，并且将 lab6_run_pool 插入到队列中
 * （我们使用左偏树来存储）。本函数还会更新 rq 的 meta date。
 */
static void
stride_enqueue(struct run_queue *rq, struct proc_struct *proc) {
     /* LAB6: YOUR CODE */
     // (1) 将 proc 插入到调度队列中，一种方法是使用队列维护
     // list_add_before(&rq->run_list, &proc->run_link);
     // 另一种方法是使用左偏树来进行维护
     rq->lab6_run_pool = skew_heap_insert(rq->lab6_run_pool, &proc->lab6_run_pool, proc_stride_comp_f);

     // (2) 重新计算进程的时间片
     if (proc->time_slice <= 0 || proc->time_slice > rq->max_time_slice)
          proc->time_slice = rq->max_time_slice;
     // (3) 将当前进程的调度队列 rq 设为当前的 rq
     proc->rq = rq;
     // (4) 当前的调度队列 rq 维护的待调度进程数 + 1
     ++rq->proc_num;
}
```

#### `stride_dequeue`

```cpp
/**
 * @brief 从运行队列 rq 中删除进程 proc。
 */
static void
stride_dequeue(struct run_queue *rq, struct proc_struct *proc) {
     /* LAB6: YOUR CODE */
     // (1) 将进程从调度队列中删除
     // list_del_init(&proc->run_link);
     rq->lab6_run_pool = skew_heap_remove(rq->lab6_run_pool, &proc->lab6_run_pool, proc_stride_comp_f);
     // (2) 标记进程不从属于任意一个调度队列
     proc->rq = NULL;
     // (3) 当前的调度队列 rq 维护的待调度进程数 - 1
     --rq->proc_num;
}
```

#### `stride_proc_tick`

```cpp
/**
 * @brief 该函数在时钟中断时触发，更新当前进程的状态。
 * 该函数会更新当前进程的时间片，如果时间片耗尽，那么该进程就需要被调度。
 */
static void
stride_proc_tick(struct run_queue *rq, struct proc_struct *proc) {
     /* LAB6: YOUR CODE */
     if (--proc->time_slice <= 0) { // 如果时间片耗尽
          proc->time_slice = 0;
          proc->need_resched = true; // 标记当前进程需要被调度
     }
}
```



## 练习3：阅读分析源代码

> 结合中断处理和调度程序，再次理解进程控制块中的 trapframe 和 context 在进程切换时作用。

ucore 实现进程调度的时机是陷入操作系统时完成，具体的调用顺序是：

### 陷入内核

用户程序通过 `int 0x80` 调用系统调用时、或者是发生时钟中断而强制陷入操作系统时，或者其他原因而发生中断，CPU 根据 `trap.c` 中的 `idt_init` 设置的 IDT，跳转到 `vectors.S` 中：

```asm
vectorX:
	pushl $0
	pushl $X
	jmp __alltraps
```

#### `trapentry.S:__alltraps`

代码随即跳转到 `trapentry.S:__alltraps` 中，保存中断调用前的上下文，并进入操作系统的中断处理程序。

```asm
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
```

由于要跳转进内核调用 `trap` 函数，因此我们必须首先保存用户进程的执行状态，以保证回到用户态时可以恢复用户进程状态，或者切换时把用户进程的执行状态从 trapframe 中加载出来。

#### `trap.c:trap(tf)`

调用了 `call trap` 后，我们进入 `trap` 函数。这个函数首先检查是否存在进程，如果不存在则直接进入中断处理程序。否则将汇编代码取出的 trapframe 保存到当前进程的状态中，并进入中断处理程序。

```cpp
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
```

#### `trap.c:trap_dispatch(tf)`

进入了中断处理程序，由于我们可能因为时钟中断进入操作系统，我们此时通知调度器现在可以根据时钟中断来决定是否需要进行调度：通过调用 `sched_class_proc_tick` 来实现。

```cpp
static void
trap_dispatch(struct trapframe *tf) {
    // ...
    switch (tf->tf_trapno) {
    // other cases ...
    case IRQ_OFFSET + IRQ_TIMER:
        ++ticks;
        /* LAB6 YOUR CODE */
        assert(current != NULL);
        sched_class_proc_tick(current);
        break;
    }
    // ...
}
```

#### `sched.c:schedule()`

已经将该函数的解释写到代码注释中了：

```cpp
/**
 * 调度程序，选择一个可以运行的进程抢占 CPU
 */
void schedule(void) {
    bool intr_flag;
    struct proc_struct *next;
    // 调度时中断敏感，我们需要先关中断
    local_intr_save(intr_flag);
    {
        current->need_resched = 0;

        // 如果当前的进程还是可执行的，我们将该进程
        // 标记为待调度的状态
        if (current->state == PROC_RUNNABLE) {
            sched_class_enqueue(current);
        }

        // 选择一个进程抢占 CPU
        if ((next = sched_class_pick_next()) != NULL) {
            sched_class_dequeue(next);
        }

        // 如果不存在可以调度的进程，这说明当前没有进程
        // 可以运行，则执行 idle 进程不断尝试进行调度
        if (next == NULL) {
            next = idleproc;
        }
        next->runs ++;
        // 切换上下文给该进程
        if (next != current) {
            proc_run(next);
        }
    }
    local_intr_restore(intr_flag);
}
```

#### `proc.c:proc_run(struct proc_struct *)`

该函数加载进程的栈指针到 TSS 段中，让 CPU 在调用 `iret` 指令从 TSS 段恢复指针时跳转到该进程的内核栈；然后加载该进程的页表以确保内存访问有效；最后调用 `switch_to` 函数切换上下文。调用完 `switch_to` 之后，会跳转到新进程的 `proc_run` 继续执行，并恢复中断。

```cpp
/**
 * 使 proc 进程抢占 CPU。
 */
void proc_run(struct proc_struct *proc) {
    if (proc != current) {
        bool intr_flag;
        struct proc_struct *prev = current, *next = proc;
        // 调度代码是关键代码，关闭中断以保证数据安全
        local_intr_save(intr_flag);
        {
            current = proc; // 切换到新进程
            // 修改处理器唯一的 TSS 段，以保证发生中断时能正确恢复
            // 堆栈指针寄存器指向这个进程内核栈空间的最高地址
            load_esp0(next->kstack + KSTACKSIZE);
            lcr3(next->cr3); // 加载该进程的页表
            // 切换进程的上下文，保证寄存器正确
            switch_to(&(prev->context), &(next->context));
        }
        local_intr_restore(intr_flag); // 恢复中断
    }
}
```

#### `switch.S`

`switch_to` 所保存的上下文 `context` 和 `trapframe` 的意义不同。`trapframe` 保存的上下文用来进行中断的恢复，也就是从内核态跳转到用户态使用的，此时恢复用户进程的上下文（也就是说，`trapframe` 保存的上下文在进程调度时是指用户进程用户态代码的上下文）；而 `context` 保存的是内核执行状态的上下文，通过 `switch_to` 函数，我们可以从当前用户进程的内核态代码跳转到另一个用户进程的内核态执行状态继续执行，之后再通过 ，因此 `switch_to` 函数执行完成之后会跳转到另一个进程的 `proc_run`（已存在的进程）或者是 `forkret`（新进程）。

```asm
.text
.globl switch_to
switch_to:                      # switch_to(from, to)

    # 保存当前进程的寄存器值到 (struct context) from 中
    movl 4(%esp), %eax          # %eax 寄存器指向 from 进程的上下文结构体
    popl 0(%eax)                # 将返回地址（由 call 压栈）保存到 form 进程中，这里的返回地址指 proc_run
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

    pushl 0(%eax)               # 取出 to 进程的返回地址

    ret # 如果 to 进程是新进程，则跳转到 forkret 函数，否则跳转到 proc_run 函数继续执行

```

#### `trapentry.S:forkrets`

对于新进程，调用 `ret` 命令后因为 `copy_thread` 中设置 `eip` 为 `forkret` 的缘故，直接跳转到 `forkrets` 汇编（这是因为新进程还未执行过，内核栈为空，不存在内核态的上下文，因此不会通过 `switch_to` 来切换内核态上下文，直接通过中断恢复设置好的 `trapframe` 开始执行用户代码即可）：

```asm
.globl forkrets
forkrets:
    # set stack to this new process's trapframe
    movl 4(%esp), %esp
    jmp __trapret
```

对于旧进程，由于旧进程被调度时也必定是通过中断实现，因此切换上下文到这个旧进程时就会跳转到旧进程的 `proc_run` 的执行状态，并最终沿着调用链：从 `schedule`、`trap` 一路返回到 `trapentry.S:__trapret` 结束中断处理。

#### `trapentry.S:__trapret`

不管是新进程，还是旧进程，在获得执行时间片后都会回到本汇编函数，恢复中断前的保存在 `trapframe`  中的用户代码执行状态上下文，并通过 `iret` 跳转到用户态执行用户代码，至此进程切换完成。

```asm
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
```

## 实验分析

由于 Stride Scheduling 算法没有答案可以参考，因此我的实现无法和参考答案进行比较分析。

## 实验心得

### 知识点

通过本次学习：

1. 我了解了缺省的 Round-Robin 调度算法的实现；
2. 我了解了 Stride Scheduling 调度算法的链式实现和斜堆实现；
3. 我了解了 context 和 trapframe 的区别，知道了要同时保存用户态和内核态的上下文；
4. 我了解了如何才能切换到另外一个进程（对于新进程，由于不存在内核栈，直接通过 iret 返回到用户态；对于旧进程，返回到内核态的执行状态并继续完成中断处理）。

### 与理论课的差异

理论课没有讨论 Stride Scheduling 算法。

### 实验中没有对应上的知识点

本实验中没有要求我们实现多级反馈队列的调度算法。

### 心得体会

同时我通过阅读 ucore 源代码，再次认识到了操作系统设计的复杂性：C 语言代码为了和汇编程序代码进行配合，代码写出来会很难懂：设置好了一些变量的值，自动跳转回到汇编代码予以应用，再通过 `iret` 指令跳转到别的地方去。`iret` 指令十分强大，实现了丰富的跳转语义。

我还知道了切换进程时用户态的状态和内核态的状态都需要保存，进程调度的过程混杂了很多汇编代码，跳跃性很大，理解难度也很大，但是经过仔细分析画图还是能够分析出来的。同时我也可以借助 GDB 来了解操作系统的执行过程。