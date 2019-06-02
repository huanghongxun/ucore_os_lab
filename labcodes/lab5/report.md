# 操作系统实验 Lab 5 内核线程管理
## 实验目的

1. 了解第一个用户进程创建过程
2. 了解系统调用框架的实现机制
3. 了解ucore如何实现系统调用 sys_fork/sys_exec/sys_exit/sys_wait 来进行进程管理

## 实验内容

实验5（ucore lab4）完成了内核线程，但到目前为止，所有的运行都在内核态执行。本实验 6（ucore lab5）将创建用户进程，让用户进程在用户态执行，且在需要ucore支持时，可通过系统调用来让ucore提供服务。为此需要构造出第一个用户进程，并通过系统调用sys_fork/sys_exec/sys_exit/sys_wait来支持运行不同的应用程序，完成对用户进程的执行过程的基本管理。

## 练习1：加载应用程序并执行

>do_execve 函数调用load_icode（位于 kern/process/proc.c 中）来加载并解析一个处于内存中的ELF执行文件格式的应用程序，建立相应的用户内存空间来放置应用程序的代码段、数据段等，且要设置好proc_struct结构中的成员变量trapframe中的内容，确保在执行此进程后，能够从应用程序设定的起始执行地址开始执行。需设置正确的trapframe内容。

### 设计实现过程

#### `do_execve`

我们调用外部程序的过程如下：

* 系统进程 `initproc` 在 `init_main` 中会启动新内核线程并调用 `KERNEL_EXECVE` 宏

* `kernel.sym` 是编译完的用户程序，其中包含所有用户程序的入口地址，比如 `_binary_obj___user_yield_out_start`；和用户程序代码长度，比如 `_binary_obj___user_yield_out_size`

* `KERNEL_EXECVE` 宏负责链接并找到这些外部程序的内核虚拟地址

* 这个地址传给 `kernel_execve`

* 触发 `SYS_exec` 系统调用，进入操作系统

* 调用 `do_execve` 函数，并释放当期进程原有的页面（因为外部程序要抢占这个进程的资源，当前进程原来的资源都没有用了，因此需要释放）

* 调用 `load_icode` 函数加载外部程序代码（在内存中）到当前进程

  * (1) 为当期进程分配内存管理器（由于 `do_execve` 函数中已经释放了父进程 `fork` 过来的内存管理器和页面，当前进程是完全没有内存管理器以及内存的，我们需要为这个进程重新分配一次）

    ```cpp
    if ((mm = mm_create()) == NULL) {
        goto bad_mm;
    }
    ```

  * (2) 为当期进程分配页目录表

    ```cpp
    if (setup_pgdir(mm) != 0) {
        goto bad_pgdir_cleanup_mm;
    }
    ```

  * (3.1~3.4) 解析当期程序的 ELF 头，并按照 ELF 头分配内存空间

    ```cpp
    struct Page *page;
    //(3.1) get the file header of the bianry program (ELF format)
    struct elfhdr *elf = (struct elfhdr *)binary;
    //(3.2) get the entry of the program section headers of the bianry program (ELF format)
    struct proghdr *ph = (struct proghdr *)(binary + elf->e_phoff);
    //(3.3) This program is valid?
    if (elf->e_magic != ELF_MAGIC) {
        ret = -E_INVAL_ELF;
        goto bad_elf_cleanup_pgdir;
    }
    
    uint32_t vm_flags, perm;
    struct proghdr *ph_end = ph + elf->e_phnum;
    for (; ph < ph_end; ph ++) {
        //(3.4) find every program section headers
        if (ph->p_type != ELF_PT_LOAD) {
            continue ;
        }
        if (ph->p_filesz > ph->p_memsz) {
            ret = -E_INVAL_ELF;
            goto bad_cleanup_mmap;
        }
        if (ph->p_filesz == 0) {
            continue ;
        }
    ```

  * (3.5~2.6) 分配新的连续虚拟内存空间 (VMA)，并分配用户代码段/数据段/BSS段

    ```cpp
    vm_flags = 0, perm = PTE_U;
    if (ph->p_flags & ELF_PF_X) vm_flags |= VM_EXEC;
    if (ph->p_flags & ELF_PF_W) vm_flags |= VM_WRITE;
    if (ph->p_flags & ELF_PF_R) vm_flags |= VM_READ;
    if (vm_flags & VM_WRITE) perm |= PTE_W;
    if ((ret = mm_map(mm, ph->p_va, ph->p_memsz, vm_flags, NULL)) != 0) {
        goto bad_cleanup_mmap;
    }
    unsigned char *from = binary + ph->p_offset;
    size_t off, size;
    uintptr_t start = ph->p_va, end, la = ROUNDDOWN(start, PGSIZE);
    
    ret = -E_NO_MEM;
    ```

  * (4) 分配用户栈空间

    我们通过 `mm_map` 函数来通过 `kmalloc` 申请内存并映射到指定的虚拟内存地址：

    ```cpp
    vm_flags = VM_READ | VM_WRITE | VM_STACK;
    if ((ret = mm_map(mm, USTACKTOP - USTACKSIZE, USTACKSIZE, vm_flags, NULL)) != 0) {
        goto bad_cleanup_mmap;
    }
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP-PGSIZE , PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP-2*PGSIZE , PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP-3*PGSIZE , PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP-4*PGSIZE , PTE_USER) != NULL);
    ```

  * (5) 设置当期进程的 cr3 寄存器来加载页表

    ```cpp
    mm_count_inc(mm);
    current->mm = mm;
    current->cr3 = PADDR(mm->pgdir);
    lcr3(PADDR(mm->pgdir));
    ```

  * (6) 设置中断帧以保证中断的正确性，能正确地从内核态调回用户态（在 `trapentry.S` 中）

    ```cpp
    struct trapframe *tf = current->tf;
    memset(tf, 0, sizeof(struct trapframe));
    /* LAB5:EXERCISE1 YOUR CODE */
    // If we set trapframe correctly, then the user level process can return to USER MODE from kernel. So
    tf->tf_cs = USER_CS; // see memlayout.h
    tf->tf_ds = tf->tf_es = tf->tf_ss = USER_DS;
    tf->tf_esp = USTACKTOP; // top addr of user stack
    tf->tf_eip = elf->e_entry; // entry point of this binary program
    tf->tf_eflags |= FL_IF; // enable interrupts
    ```

* `do_execve` 函数执行完成，通过 `trapentry.S` 的汇编代码中的 `iret` 返回原特权级，根据第 6 步的代码跳转到新进程的代码

### 代码实现

#### `alloc_proc`

LAB 5 要求对 `alloc_proc` 函数的内容进行补充，补充结果如下：

```cpp
// alloc_proc - alloc a proc_struct and init all fields of proc_struct
static struct proc_struct *
alloc_proc(void) {
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL) {
        // LAB4:EXERCISE1 YOUR CODE
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
        // LAB5 YOUR CODE : (update LAB4 steps)
        proc->wait_state = 0;
        proc->cptr = proc->yptr = proc->optr = NULL;
    }
    return proc;
}
```

完成对以下 4 个变量的初始化：

* `wait_state`：表示当前进程的等待状态，可以为 `WT_CHILD` 表示在等在子进程，`WT_INTERRUPTED` 表示当前正在等待
* `cptr`：第一个子进程的结构体地址
* `yptr`：当前进程的右（年轻）兄弟进程（与当前进程的父进程相同）的结构体地址
* `optr`：当前进程的左（年长）兄弟进程（与当前进程的父进程相同）的结构体地址
* 这三个结构体指针可以完成对进程树的遍历，方便我们对进程进行管理。

#### `do_fork`

LAB 5 要求对 `do_fork` 函数的内容进行补充，补充结果如下：

```cpp
/**
 * 根据父进程状态拷贝出一个状态相同的子进程。
 * 
 * @param clone_flags 标记子进程资源是共享还是拷贝，若 clone_flags & CLONE_VM，则共享，否则拷贝
 * @param stack 父进程的用户栈指针，如果为 0，表示该进程是内核线程
 * @param tf 进程的中断帧，将被拷贝给子进程，保证状态一致
 */
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) {
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;
    if (nr_process >= MAX_PROCESS) {
        goto fork_out;
    }
    ret = -E_NO_MEM;
    //LAB4:EXERCISE2 YOUR CODE
	//LAB5 YOUR CODE : (update LAB4 steps)
    /* Some Functions
     *    set_links:  set the relation links of process.  ALSO SEE: remove_links:  lean the relation links of process 
     *    -------------------
     *    update step 1: set child proc's parent to current process, make sure current process's wait_state is 0
     *    update step 5: insert proc_struct into hash_list && proc_list, set the relation links of process
     */

    //    1. call alloc_proc to allocate a proc_struct
    // 新建 proc 结构体并初始化为空
    if ((proc = alloc_proc()) == NULL) {
        goto fork_out;
    }
    // !!! 将子进程的父亲设置为当前进程
    proc->parent = current;
    assert(current->wait_state == 0); // !!! 调用 fork 的进程必定在运行中

    //    2. call setup_kstack to allocate a kernel stack for child process
    // 分配页帧给子进程内核栈
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
        hash_proc(proc); // 将进程加入到哈希表中
        set_links(proc); // !!! 将进程加入到进程列表中
    }
    local_intr_restore(intr_flag); // 恢复中断
    //    6. call wakeup_proc to make the new child process RUNNABLE
    wakeup_proc(proc); // 令子进程的状态为运行中：PROC_RUNNABLE
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

这个函数调用了 `set_links` 函数

```cpp
/**
 * 设置进程间的关系指针 optr、cptr、yptr
 */
static void
set_links(struct proc_struct *proc) {
    // 将 proc 进程加入到进程列表中
    list_add(&proc_list, &(proc->list_link));
    // 当前进程是最新的，显然没有更年轻的兄弟进程
    proc->yptr = NULL;
    // 如果当前进程存在更年长的兄弟进程
    if ((proc->optr = proc->parent->cptr) != NULL) {
        // 维护指针关系，确保多叉树的正确性
        proc->optr->yptr = proc;
    }
    // 父进程的最新子进程设为 proc
    proc->parent->cptr = proc;
    // 我们新建了一个父进程
    nr_process ++;
}
```

#### `idt_init`

LAB 5 要求对 `idt_init` 函数的内容进行补充，补充结果如下：

补充方法和 LAB1 的 Challenge 很像，我们需要添加一个用户中断 SYSCALL 给用户用来进行系统调用

```cpp
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
```

#### `trap_dispatch`

LAB 5 要求对 `trap_dispatch` 函数的内容进行补充，补充结果如下：

我们在定时中断内添加内核抢占 CPU 进行调度的代码：

```cpp
    case IRQ_OFFSET + IRQ_TIMER:
#if 0
    LAB3 : If some page replacement algorithm(such as CLOCK PRA) need tick to change the priority of pages,
    then you can add code here. 
#endif
        /* LAB1 YOUR CODE : STEP 3 */
        /* handle the timer interrupt */
        // (1) After a timer interrupt, you should record this event using a global variable (increase it), such as ticks in kern/driver/clock.c
        ++ticks;
        // (2) Every TICK_NUM cycle, you can print some info using a funciton, such as print_ticks().
        assert(ticks <= TICK_NUM);
        if (ticks == TICK_NUM) {
            ticks = 0;
            // print_ticks();

            /* LAB5 YOUR CODE */
            /* you should update you lab1 code (just add ONE or TWO lines of code):
             *    Every TICK_NUM cycle, you should set current process's current->need_resched = 1
             */
            assert(current != NULL); // 无论如何当前都必存在进程，无论是 initproc、idleproc 还是其他用户进程
            current->need_resched = 1; // 系统通过定时时钟中断来抢占 CPU 来进行调度
        }
        // (3) Too Simple? Yes, I think so!
        break;
```

#### 设置 `trapframe` 的代码如下

```cpp
/* LAB5:EXERCISE1 YOUR CODE */
// If we set trapframe correctly, then the user level process can return to USER MODE from kernel. So
tf->tf_cs = USER_CS; // see memlayout.h
tf->tf_ds = tf->tf_es = tf->tf_ss = USER_DS;
tf->tf_esp = USTACKTOP; // top addr of user stack
tf->tf_eip = elf->e_entry; // entry point of this binary program
tf->tf_eflags |= FL_IF; // enable interrupts
```

我们根据已有的提示很容易就能完成这里的代码填写。

### 问题回答

> 描述当创建一个用户态进程并加载了应用程序后，CPU是如何让这个应用程序最终在用户态执行起来的。即这个用户态进程被ucore选择占用CPU执行（RUNNING态）到具体执行应用程序第一条指令的整个经过。

1. `proc_init` 函数创建唯一的内核进程 `initproc`
2. `initproc` 进程创建内核线程 `A`，调用 `do_wait` 等待子进程结束
3. `do_wait` 和 `init_main` 寻找是否存在子进程会调用 `schedule` 函数开始调度
4. 调度找到了可以执行 `initproc` 刚新建的子进程，并通过 `proc_run` 函数切换上下文到这个进程 `A`
5. 进程 `A` 进入 `user_main` 函数，并调用 `KERNEL_EXECVE` 加载用户程序，调用 `SYS_exec` 中断进入操作系统
6. 操作系统调用 `do_execve` 完成用户程序的加载，并设置 `trapframe` ，使得进入 `trapentry.S` 后将 `trapframe` 内容导出，使得中断跳出后进入 `trapframe` 设置好的状态，进入用户态开始执行外部用户程序代码
7. 每 100 个 tick 调用一次时钟中断，设置当期进程为需要调度的状态。下次中断时调用 `schedule` 函数进行调度



## 练习**2**：父进程复制自己的内存空间给子进程

>  创建子进程的函数do_fork在执行中将拷贝当前进程（即父进程）的用户内存地址空间中的合法内容到新进程中（子进程），完成内存资源的复制。具体是通过copy_range函数（位于kern/mm/pmm.c中）实现的，请补充copy_range的实现，确保能够正确执行。

### `copy_range`

该函数负责将父进程的指定页面复制给进程 B，实现过程是：

1. 遍历进程 A 的指定地址对应的页面
2. 如果页面所属页表不存在，则跳过这个页表
3. 否则为进程 B 新建一个页表项
4. 如果实现了 CopyOnWrite，则将进程 A 的页面设置为只读，并将这个页面直接送给进程 B
5. 如果没有实现，则分配一个新页面，调用 `memcpy` 来复制页面的 4K 字节的数据，再将这个页面送给进程 B

#### 代码实现

```cpp
/**
 * @brief 将进程 A 中的用户内存段 [start,end) 复制给进程 B。
 * @param to 目标进程 B 的页目录表
 * @param from 源进程 A 的页目录表
 * @param start 进程 A 的待复制用户内存段起始地址，必须页对齐
 * @param end 进程 A 的待复制用户内存段结束地址，必须页对齐
 * @param share 为真时将共享页面，否则新建页面并复制内存内容。
 * @call_chain copy_mm-->dup_mmap-->copy_range
 */
int copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share) {
    assert(start % PGSIZE == 0 && end % PGSIZE == 0);
    assert(USER_ACCESS(start, end));
    // copy content by page unit.
    do {
        // 我们遍历 [start,end) 区间内的所有页表
        pte_t *ptep = get_pte(from, start, 0), *nptep;
        // 由于内存地址对应页表不存在，我们跳过整个页表段
        if (ptep == NULL) {
            start = ROUNDDOWN(start + PTSIZE, PTSIZE);
            continue;
        }
        // 如果 start 对应的页表项存在，我们完成这个页面的复制
        if (*ptep & PTE_P) {
            // 获取进程 B 中对应虚拟地址页面
            if ((nptep = get_pte(to, start, 1)) == NULL) {
                return -E_NO_MEM;
            }
            uint32_t perm = (*ptep & PTE_USER);
            // 获取 start 所属页面的结构体
            struct Page *page = pte2page(*ptep);
            struct Page *npage;
            assert(page != NULL);
            int ret = 0;
            // LAB5: EXERCISE2 YOUR CODE
            if (share) {
                if (perm & PTE_W) {
                    // 设置页面为不可读写的，更新页表并刷新 TLB
                    ret = page_insert(from, page, start, perm &= ~PTE_W);
                    assert(ret == 0);
                }
                npage = page;
            } else {
                // 为进程 B 分配页面
                npage = alloc_page();
                assert(npage != NULL);
                // replicate content of page to npage, build the map of phy addr of nage with the linear addr start
                // (1) find src_kvaddr: the kernel virtual address of page
                void *src_kvaddr = page2kva(page);
                // (2) find dst_kvaddr: the kernel virtual address of npage
                void *dst_kvaddr = page2kva(npage);
                // (3) memory copy from src_kvaddr to dst_kvaddr, size is PGSIZE
                memcpy(dst_kvaddr, src_kvaddr, PGSIZE); 
            }
            // (4) build the map of phy addr of  nage with the linear addr start
            ret = page_insert(to, npage, start, perm);
            // 我们已经通过 nptep 检查了 get_pte 的合法性，因此 page_insert 必正常结束
            assert(ret == 0);
        }
        start += PGSIZE;
    } while (start != 0 && start < end);
    return 0;
}
```

### Copy On Write

>  请在实验报告中简要说明如何设计实现 ”Copy on Write 机制“，给出概要设计，鼓励给出详细设计。

1. fork 之后，我们在 `copy_range` 函数中进行页面复制时，不执行复制，而是共享只读页面。即将父进程的页面设置为不可读，并将这个页面直接插入到子进程的页表中，这样就节省了一次复制的时间
2. 由于页面已经被设置为只读，因此对这些页面进行读写时会产生页访问异常，此时我们可以判断页面是否存在，如果页面存在，且 VMA 中记录页面可写，则一定是 CopyOnWrite 产生的只读页面。此时我们需要复制新的页面（或直接使用原页面，如果这个页面的引用数为 1）。由于创建了新页面，原页面的引用数就减一。将新的页面替换原页表中的页面。

```cpp
/*LAB3 EXERCISE 1: YOUR CODE*/
// (1) try to find a pte, if pte's PT(Page Table) isn't existed, then create a PT.
if (!(ptep = get_pte(mm->pgdir, addr, true))) {
    cprintf("do_pgfault failed: no enough space for allocating a page for page table in get_pte");
    goto failed;
}
if (!*ptep) {
    // (2) if the phy addr isn't exist (No page table entry exists),
    // then alloc a page & map the phy addr with logical addr
    if (!pgdir_alloc_page(mm->pgdir, addr, perm)) {
        cprintf("do_pgfault failed: no enough space for allocating a page for user");
        goto failed;
    }
}
else {
    struct Page *page = NULL;
    /*
         * LAB5 CHALLENGE (the implmentation Copy on Write)
         */
    if (*ptep & PTE_P) {
        // 如果我们写入一个已经在内存中的页面而导致缺页异常，
        // 一定是用户程序没有该页的写入权限（页面写保护/只读），
        // 页面只读是因为我们采取了页面共享的策略以减少内存占用，
        // 因此我们需要实现 copy-on-write 算法来保证用户仍能
        // 对这些页面进行写入操作。
        // 若查询 VMA 已知该页面是真的可写的，我们创建一个新的
        // 页面，并修改 *ptep
        if (vma->vm_flags & VM_WRITE) {
            struct Page *opage = pte2page(*ptep);
            if (page_ref(opage) == 1) {
                cprintf("do_pgfault: COW");
                // 如果我们写入的页面引用数刚好为 1，
                // 那么当前页面就可以直接设置为可写
                page = opage;
            } else if (page_ref(opage) > 1) {
                // 否则复制出来一个新页面
                cprintf("do_pgfault: COW: allocating a new page");
                page_ref_dec(opage);
                if ((page = alloc_page()) == NULL) {
                    cprintf("do_pgfault failed: no enough space for allocating a page for user");
                    goto failed;
                }
                void *src_kvaddr = page2kva(opage);
                void *dst_kvaddr = page2kva(page);
                memcpy(dst_kvaddr, src_kvaddr, PGSIZE);
            } else {
                panic("error writing to a non-ref page");
            }
        } else {
            panic("error writing to a non-writable page");
        }
    } else {
        // 若页面不存在，表明该页被交换进磁盘了，我们需要执行页交换操作
        /*
            * LAB3 EXERCISE 2: YOUR CODE
            * Now we think this pte is a swap entry, we should load data from disk to a page with physical address,
            * and map the phy addr with logical addr, trigger swap manager to record the access situation of this page.
            */
        if (swap_init_ok) {
            if (swap_in(mm, addr, &page) != 0) {        // (1) According to the mm AND addr, try to load the content of right disk page
                cprintf("do_pgfault failed: swap_in");  //     into the memory which page managed.
                goto failed;
            }
        }
        else {
            cprintf("no swap_init_ok but ptep is %x, failed\n",*ptep);
            goto failed;
        }
    }
    page_insert(mm->pgdir, page, addr, perm);   // (2) According to the mm, addr AND page, setup the map of phy addr <---> logical addr
    swap_map_swappable(mm, addr, page, true);   // (3) make the page swappable.
    page->pra_vaddr = addr;
}
```



## 练习**3**：分析代码: fork/exec/wait/exit 函数，以及系统调用的实现

> 请在实验报告中简要说明你对 fork/exec/wait/exit  函数的分析。并回答如下问题

### 问题回答

> 请分析fork/exec/wait/exit在实现中是如何影响进程的执行状态的？

1. fork

   通过 `SYS_fork` 系统调用，调用 `do_fork` 函数实现

2. exec

   通过 `SYS_exec` 系统调用，调用 `do_execve` 函数实现。该函数释放当前进程的内存，并调用 `load_icode` 为新程序分配空间。

3. wait

   通过 `SYS_wait` 系统调用，调用 `do_wait` 函数实现。

4. exit

   通过 `SYS_exit` 系统调用，调用 `do_exit` 函数实现



> 请给出ucore中一个用户态进程的执行状态生命周期图（包执行状态，执行状态之间的变换关系，以及产生变换的事件或函数调用）。（字符方式画即可）

![1559491005110](E:\sources\homework\ucore_os_lab\labcodes\lab5\assets\1559491005110.png)

## 实验心得

通过本次学习：

1. 我了解了进程的生命周期，知道了操作系统如何通过调度完成进程的生命周期的变换；
2. 我了解了操作系统是如何启动一个用户程序的：通过创建子进程 + `do_execve` 来实现；
3. 知道了 `execve` 实现跳转的复杂步骤；
4. 知道了如何通过设置 `trapframe` 来跳转到新的用户程序；
5. 知道了 `wait` 函数是如何导致系统调度的。

我的代码与答案的区别在于部分实现了 CopyOnWrite 的功能，但是这部分功能仍然不能很好地工作，所以只能放弃。

同时我通过阅读 ucore 源代码，再次认识到了操作系统设计的复杂性：C 语言代码为了和汇编程序代码进行配合，代码写出来会很难懂：设置好了一些变量的值，自动跳转回到汇编代码予以应用，再通过 `iret` 指令跳转到别的地方去。`iret` 指令十分强大，实现了丰富的跳转语义。