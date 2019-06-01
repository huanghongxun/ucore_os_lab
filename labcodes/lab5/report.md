# 操作系统实验 Lab 5 内核线程管理
## 实验目的
1. 了解第一个用户进程创建过程
2. 了解系统调用框架的实现机制
3. 了解ucore如何实现系统调用 sys_fork/sys_exec/sys_exit/sys_wait 来进行进程管理

## 实验内容

实验5（ucore lab4）完成了内核线程，但到目前为止，所有的运行都在内核态执行。本实验 6（ucore lab5）将创建用户进程，让用户进程在用户态执行，且在需要ucore支持时，可通过系统调用来让ucore提供服务。为此需要构造出第一个用户进程，并通过系统调用sys_fork/sys_exec/sys_exit/sys_wait来支持运行不同的应用程序，完成对用户进程的执行过程的基本管理。

## 练习1：加载应用程序并执行

>do_execv 函数调用load_icode（位于 kern/process/proc.c 中）来加载并解析一个处于内存中的ELF执行文件格式的应用程序，建立相应的用户内存空间来放置应用程序的代码段、数据段等，且要设置好proc_struct结构中的成员变量trapframe中的内容，确保在执行此进程后，能够从应用程序设定的起始执行地址开始执行。需设置正确的trapframe内容。

### 设计实现过程



### 代码实现



### 问题回答

> 描述当创建一个用户态进程并加载了应用程序后，CPU是如何让这个应用程序最终在用户态执行起来的。即这个用户态进程被ucore选择占用CPU执行（RUNNING态）到具体执行应用程序第一条指令的整个经过。





## 练习**2**：父进程复制自己的内存空间给子进程

>  创建子进程的函数do_fork在执行中将拷贝当前进程（即父进程）的用户内存地址空间中的合法内容到新进程中（子进程），完成内存资源的复制。具体是通过copy_range函数（位于kern/mm/pmm.c中）实现的，请补充copy_range的实现，确保能够正确执行。

### `copy_range`



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

>  请在实验报告中简要说明如何设计实现”Copy on Write 机制“，给出概要设计，鼓励给出详细设计。



## 练习**3**：分析代码: fork/exec/wait/exit 函数，以及系统调用的实现

> 请在实验报告中简要说明你对 fork/exec/wait/exit  函数的分析。并回答如下问题

### 问题回答

> 请分析fork/exec/wait/exit在实现中是如何影响进程的执行状态的？





> 请给出ucore中一个用户态进程的执行状态生命周期图（包执行状态，执行状态之间的变换关系，以及产生变换的事件或函数调用）。（字符方式画即可）





## 扩展练习**Challenge**：实现 Copy On Write 机制
> 给出实现源码,测试用例和设计报告（包括在cow情况下的各种状态转换（类似有限状态自动机）的说明）。
>
> 这个扩展练习涉及到本实验和上一个实验“虚拟内存管理”。在ucore操作系统中，当一个用户父进程创建自己的子进程时，父进程会把其申请的用户空间设置为只读，子进程可共享父进程占用的用户内存空间中的页面（这就是一个共享的资源）。当其中任何一个进程修改此用户内存空间中的某页面时，ucore会通过page fault异常获知该操作，并完成拷贝内存页面，使得两个进程都有各自的内存页面。这样一个进程所做的修改不会被另外一个进程可见了。请在ucore中实现这样的COW机制。
>
> 由于COW实现比较复杂，容易引入bug，请参考 https://dirtycow.ninja/ 看看能否在ucore的COW实现中模拟这个错误和解决方案。需要有解释。
>
> 这是一个big challenge.