# 操作系统实验 Lab 4 内核线程管理

## 实验目的
1. 了解内核线程创建/执行的管理过程
2. 了解内核线程的切换和基本调度过程

## 练习1：分配并初始化一个进程控制块

> alloc_proc函数（位于kern/process/proc.c中）负责分配并返回一个新的struct proc_struct结构，用于存储新建立的内核线程的管理信息。ucore需要对这个结构进行最基本的初始化，你需要完成这个初始化过程。	【提示】在alloc_proc函数的实现中，需要初始化的proc_struct结构中的成员变量至少包括：state/pid/runs/kstack/need_resched/parent/mm/context/tf/cr3/flags/name。

### 设计实现过程

我们先分析 `struct proc_struct` 各成员的含义：

```cpp
/**
 * 进程控制块
 * 由于我们把线程看作特殊的进程，我们就可以同时方便地管理进程和线程，因此线程和进程
 * 都使用该结构体来存放相关信息。
 * 在 alloc_proc 中初始化该结构体
 */
struct proc_struct {
    enum proc_state state;                      // 进程状态
    int pid;                                    // 进程 ID
    int runs;                                   // 进程运行的次数
    uintptr_t kstack;                           // 进程内核栈基地址
    volatile bool need_resched;                 // 是否需要对当前进程进行调度
    struct proc_struct *parent;                 // 父进程的结构体地址
    struct mm_struct *mm;                       // 进程的内存管理器相关信息
    struct context context;                     // 进程运行的寄存器上下文
    struct trapframe *tf;                       // 当前中断的 trapframe
    uintptr_t cr3;                              // CR3 寄存器: 进程自己的页目录表页基址
    uint32_t flags;                             // Process flag
    char name[PROC_NAME_LEN + 1];               // 进程名
    list_entry_t list_link;                     // 进程集合列表的链表指针
    list_entry_t hash_link;                     // 进程哈希表的链表指针
};
```

* `enum proc_state state`
  记录进程状态，初始化为 PROC_UNINIT。状态的可能情况存储在 `enum proc_state` 中：
  状态的修改由 `sched.c:wakeup_proc(struct proc_struct *proc)` 函数来完成。

  ```cpp
  /**
   * 表示进程生命周期中的各个状态。
   * 此处使用 RUNNABLE 来同时表示正在运行和等待时间片的进程。
   */
  enum proc_state {
      // 未初始化：新建进程时 (alloc_proc 函数) 将其状态设为该项
      PROC_UNINIT = 0,
      // 阻塞：一般是在等待资源
      PROC_SLEEPING,
      /**
       * 就绪：运行中或等待运行中
       * 进程已经可以运行了（可能已经在运行，或者未获得 CPU 的时间片，被其他进程抢占）。
       * 我们进行调度时选择 RUNNABLE 的进程进行调度。
       */
      PROC_RUNNABLE,
      // 僵尸：进程执行结束，但未被操作系统或父进程回收
      PROC_ZOMBIE,
  };
  ```

* `int pid`
  记录进程的 pid。在 `alloc_proc` 中初始化为 `-1`，表示尚未分配（分配好的 pid 都是正数，因此取 -1 来表示未分配）。该项在 `do_fork` 函数中通过调用 `get_pid` 函数来获取分配。
* `int runs`
  记录进程的运行次数，用于调度算法。在 `alloc_proc` 中初始化为 0。
* `uintptr_t kstack`
  记录内核堆栈的基址，页对齐。初始化为 0。通过 `setup_kstack` 来直接分配页帧用于存储内核栈。

* `volatile bool need_resched`
  记录当前进程是否需要进行调度。目前只有 `idleproc` 是需要进行调度的。

* `struct proc_struct *parent`
  记录当前进程的父进程。可以知道父进程的进程信息。`alloc_proc` 时不知道进程的父进程是谁，因此初始化为 `NULL` 即可。

* `struct mm_struct *mm`
  记录该进程的内存管理器。

* `struct context context`
  保存切换进程时（之前）的现场。因为这里都是内核线程，内核线程的其他段寄存器值都相同，我们只需要维护通用寄存器即可。在 `proc.c:copy_thread(proc, esp, tf)` 中进行维护。这样我们在进行进程调度时就可以根据该结构体进行保护现场和恢复现场的工作。我们没有使用 TSS 来进行维护，是因为 CPU 硬件实现的进程切换速度太慢了。我们在 `switch.S` 中进行这些操作。

  ```cpp
  /**
   * 内核态切换时来保存通用寄存器的结构体。
   * 
   * 由于上下文是内核的，内核态的段寄存器都是一样的（操作系统只用一个），
   * 因此不需要保存段寄存器值。
   * %eax 寄存器的值我们这里不予以进行维护。
   * @note context 结构体的顺序必须和 switch.S 中的代码完全一致
   */
  struct context {
      uint32_t eip;
      uint32_t esp;
      uint32_t ebx;
      uint32_t ecx;
      uint32_t edx;
      uint32_t esi;
      uint32_t edi;
      uint32_t ebp;
  };
  ```

  

* `struct trapframe *tf`
  中断帧用于保存中断时的现场信息。

  ```cpp
  /**
   * 保存调用中断时的所有必要的寄存器值，以便恢复现场。
   * 顺序必须和 trapentry.S 中代码保持一致。
   * 
   * 中断帧保存在内核栈中，记录中断前的状态。离开中断时恢复保存的通用寄存器和段寄存器的值。
   * 一部分由 CPU 压栈填充，一部分由 trapentry.S 填写。
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
      uint32_t tf_err;    // 在异常中断时由 CPU 压栈，否则我们压一个 0 来对齐
  
      /*
       * 以下由 CPU 在中断触发时压栈，由 iret 指令弹出
       * https://c9x.me/x86/html/file_module_x86_id_145.html
       */
  
      uintptr_t tf_eip;
      uint16_t tf_cs;
      uint16_t __csh;
      uint32_t tf_eflags;
  
      /* 下面两个变量由变换特权级时由 CPU 压入 */
  
      uintptr_t tf_esp;
      uint16_t tf_ss;
      uint16_t __ssh;
  } __attribute__((packed)); // We insert padding by ourself
  ```

  发生中断时，CPU 会先检查特权级是否变化，如果变化，就需要操作 TSS。将新特权级相关的 ss 和 esp 值装载到 ss 和 esp 寄存器，在新的栈中保存 ss 和 esp 以前的值（由 iret 指令中恢复）。在栈中保存 eflags、cs、eip 的内容。装载 cs 和 eip 寄存器（从 IDT 中读出的），跳转到中断处理程序。

* `uintptr_t cr3`
  记录当前进程的页目录表的页帧基地址。由于我们只创建内核线程，因此这些线程的页目录表都是内核的页目录表 `boot_cr3`。

* `uint32_t flags`
  记录一些进程的标记位。

* `char name[PROC_NAME_LEN + 1]`
  记录该进程的进程名。

* `list_entry_t list_link`
  记录系统所有进程的列表中该项的链表项。
* `list_entry_t hash_link`
  记录系统哈希表中该项的链表项。哈希表可以加快查找 pid 对应进程的速度。

### 代码实现

```cpp
// alloc_proc - alloc a proc_struct and init all fields of proc_struct
static struct proc_struct *
alloc_proc(void) {
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL) {
    //LAB4:EXERCISE1 YOUR CODE
        proc->state = PROC_UNINIT;
        proc->pid = -1;
        proc->runs = 0;
        proc->kstack = 0;
        proc->need_resched = 0;
        proc->parent = NULL;
        proc->mm = NULL;
        memset(&(proc->context), 0, sizeof(struct context));
        proc->tf = NULL;
        proc->cr3 = boot_cr3;
        proc->flags = 0;
        memset(proc->name, 0, PROC_NAME_LEN);
    }
    return proc;
}
```

### 问题回答

> 请说明 `proc_struct` 中 `struct context context` 和 `struct trapframe *tf` 成员变量含义和在本实验中的作用是啥？（提示通过看代码和编程调试可以判断出来）

`context` 保存切换进程时（之前）的现场。我们在进行进程调度时就可以根据该结构体进行保护现场和恢复现场的工作。

`trapframe` 中断帧用于保存中断时的现场信息。中断帧保存在内核栈中，记录中断前的状态。离开中断时恢复保存的通用寄存器和段寄存器的值。

## 练习2：为新创建的内核线程分配资源

> 创建一个内核线程需要分配和设置好很多资源。kernel_thread 函数通过调用 **do_fork** 函数完成具体内核线程的创建工作。do_kernel 函数会调用 alloc_proc 函数来分配并初始化一个进程控制块，但 alloc_proc 只是找到了一小块内存用以记录进程的必要信息，并没有实际分配这些资源。ucore 一般通过 do_fork 实际创建新的内核线程。do_fork 的作用是，创建当前内核线程的一个副本，它们的执行上下文、代码、数据都一样，但是存储位置不同。在这个过程中，需要给新内核线程分配资源，并且复制原进程的状态。你需要完成在kern/process/proc.c 中的 do_fork 函数中的处理过程。

do_fork的大致执行步骤包括：

* 调用alloc_proc，首先获得一块用户信息块。

* 为进程分配一个内核栈。

* 复制原进程的内存管理信息到新进程（但内核线程不必做此事）

* 复制原进程上下文到新进程

* 将新进程添加到进程列表

* 唤醒新进程

* 返回新进程号

### 设计实现过程

设计实现过程在代码中有所体现。

```cpp
/**
 * 根据父进程状态拷贝出一个状态相同的子进程。
 * 
 * @param clone_flags 标记子进程资源是共享还是拷贝，若 clone_flags & CLONE_VM，则共享，否则拷贝
 * @param stack 父进程的用户栈指针，如果为 0，表示该进程是内核线程
 * @param tf 进程的中断帧，将被拷贝给子进程，保证状态一致
 */
int
do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) {
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;
    if (nr_process >= MAX_PROCESS) {
        goto fork_out;
    }
    ret = -E_NO_MEM;
    //LAB4:EXERCISE2 YOUR CODE

    //    1. call alloc_proc to allocate a proc_struct
    // 新建 proc 结构体并初始化为空
    if (!(proc = alloc_proc())) {
        goto fork_out;
    }
    // 将子进程的父亲设置为当前进程
    proc->parent = current;
    //    2. call setup_kstack to allocate a kernel stack for child process
    // 分配一个物理页帧给子进程内核栈使用
    if (setup_kstack(proc) != 0) {
        goto bad_fork_cleanup_proc;
    }
    //    3. call copy_mm to dup OR share mm according clone_flag
    // 根据 clone_flags 来共享或复制出一个新的内存管理器给子进程
    if (copy_mm(clone_flags, proc) != 0) {
        goto bad_fork_cleanup_kstack;
    }

    //    4. call copy_thread to setup tf & context in proc_struct
    // 初始化进程内核栈中的 trapframe 结构体，并保存进程的内核入口和内核栈
    copy_thread(proc, stack, tf);
    //    5. insert proc_struct into hash_list && proc_list
    bool intr_flag;
    local_intr_save(intr_flag); // 关闭中断确保安全
    {
        proc->pid = get_pid(); // 为进程分配唯一的 pid
        ++nr_process; // 进程数
        hash_proc(proc); // 将进程加入到哈希表中
        list_add(&proc_list, &(proc->list_link)); // 将进程加入到进程列表中
    }
    local_intr_restore(intr_flag); // 恢复中断
    //    6. call wakeup_proc to make the new child process RUNNABLE
    wakeup_proc(proc); // 令子进程的状态为运行中：PROC_RUNNABLE，即唤醒子进程
    //    7. set ret vaule using child proc's pid
    ret = proc->pid; // 返回子进程的 pid
fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}
```



### 问题回答

> 请说明 ucore 是否做到给每个新 fork 的线程一个唯一的 id？请说明你的分析和理由。

是的。`get_pid` 函数保证返回一个不在使用中的 pid，同时我们在 `do_fork` 函数中通过关闭中断的方式来确保 `get_pid` 函数运行时的不会发生中断导致内存的修改而出错。

## 练习**3**：分析代码  `proc_run` 函数
> 阅读代码，理解 `proc_run` 函数和它调用的函数如何完成进程切换的。

### 函数分析

#### 1. 进程初始化



#### `cpu_idle`

```cpp
/**
 * 系统空闲进程 idle_proc 执行的代码
 */
void
cpu_idle(void) {
    // 不断查找是否存在进程可以抢占 CPU。
    // 如果当前进程需要调度，则调用 `schedule` 函数进行调度。
    // 由于 idleproc->need_resched 一开始就被设置为 true，
    // 因此空闲进程一开始就会尝试进行调度。
    while (1) {
        if (current->need_resched) {
            schedule();
        }
    }
}
```

#### `schedule`

```cpp
/**
 * 调度程序，选择一个可以运行的进程抢占 CPU
 */
void schedule(void) {
    bool intr_flag;
    list_entry_t *le, *last;
    struct proc_struct *next = NULL;
    // 调度代码是关键代码，一旦发生中断会导致数据错乱，因此关闭中断
    local_intr_save(intr_flag);
    {
        current->need_resched = 0;
        last = (current == idleproc) ? &proc_list : &(current->list_link);
        le = last;
        // 在进程列表中寻找一个可执行的进程
        do {
            if ((le = list_next(le)) != &proc_list) {
                next = le2proc(le, list_link);
                if (next->state == PROC_RUNNABLE) {
                    break;
                }
            }
        } while (le != last);
        // 如果不存在可执行的进程，则执行系统空闲进程等待下一次调度
        if (next == NULL || next->state != PROC_RUNNABLE) {
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

#### `proc_run`

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

```asm
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

    ret # 该指令从栈中弹出 eip 并跳转到 forkret 函数（见 copy_thread）
```
#### `forkret`

```cpp
/**
 * 该函数是进程的内核入口，由 copy_thread 函数设置为入口
 * 并由 switch_to 函数跳转到该函数，进入该进程
 */
static void
forkret(void) {
    forkrets(current->tf);
}
```

#### `forkrets`

```asm
.globl forkrets
forkrets:
	# %esp 指向当前的中断帧，然后跳转到 __trapret 恢复中断帧的寄存器值来恢复现场。
    movl 4(%esp), %esp
    jmp __trapret
```



### 问题回答

#### 1

> 在本实验的执行过程中，创建且运行了几个内核线程？

创建并运行了 `idleproc` 和 `initproc` 两个进程。

#### 2

> 语句 local_intr_save(intr_flag);....local_intr_restore(intr_flag); 在这里有何作用?请说明理由。

关闭中断和恢复中断。避免进入关键区域时被切换导致数据错乱。

## 实验总结

通过这次实验，我再次了解了操作系统编写的难度。可以看到很多地方都在汇编代码和 C 语言代码之间来回跳跃，非常难懂。需要通读代码并画出代码的跳转示意图才能理解。

这次的实验让我了解了汇编和 C 混编的方式。