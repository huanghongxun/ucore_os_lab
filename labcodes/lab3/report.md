# 操作系统实验 Lab3

## 练习1：给未被映射的地址映射上物理页

我们首先获取页表项，通过 `get_pte` 函数即可完成页表项的获取，同时还帮助我们完成了页表页面的自动创建。

接着我们检查地址是否未被映射，如果未映射，分配一个物理页面并且予以绑定。

```cpp
    pte_t *ptep = NULL;
    /* LAB3 EXERCISE 1: YOUR CODE */
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
```



### 请描述页目录项（Page Directory Entry）和页表项（Page Table Entry）中组成部分对 ucore 实现页替换算法的潜在用处

`mmu.h` 中详细描述了线性地址各部分的含义：

```
+-------10-------+-------10-------+---------12----------+
| Page Directory |   Page Table   | Offset within Page  |
|      Index     |     Index      |                     |
+----------------+----------------+---------------------+
 \--- PDX(la) --/ \--- PTX(la) --/ \---- PGOFF(la) ----/
  \---------- PPN(la) ----------/
```

`memlayout.h` 中描述了 PDE 和 PTE 的类型为 `uintptr_t`，是个 32 位整数，通过 `pmm.c` 中的处理可知：

```cpp
// 页表项类型，高 20 位为页面编号 (Address of 4KB page frame; Address of page table)
//           低 12 位为页表项标记，见下图以及 mmu.h 中的 PTE 系列宏。
// 由于二级页表项只能指向某个页面的首地址（物理地址），这个首地址的低 12 位必全零，因此我们可以利用
// 低 12 位来存储一些标记。如果需要获得页面地址，只需要 PTE_ADDR 宏即可（就是把低 12
// 位清零，就可以得到页面地址了）
typedef uintptr_t pte_t;

// 页表目录项类型，高 20 位为二级页表的页面编号，低 12 位为页表项标记，见 mmu.h 中的
// PTE 系列宏。
// 由于页表目录项也是特殊的页表项，因此 pde_t 的结构和 pte_t 一致。
typedef uintptr_t pde_t;
```

PDE 和 PTE 的结构在 *Intel® 64 and IA-32 Architectures Software Developer ’s Manual – Volume 3A* 中有详细说明（因为页表项需要由 CPU 直接访问，因此 ucore 的页表项结构必须符合 Intel 官方手册）：

![1556090153792](E:\sources\homework\ucore_os_lab\labcodes\lab2\assets\1556090153792.png)

与上图一致的页表标记在 `mmu.h` 中也描述的很清楚了：

```c
#define PTE_P     0x001 // 页面是否存在（是否分配）
#define PTE_W     0x002 // 页面是否可写 R/W
#define PTE_U     0x004 // 页面是否可以在用户态 (CPL=3) 操作 U/S
#define PTE_PWT   0x008 // 页面是否写穿透
#define PTE_PCD   0x010 // 页面是否禁止载入高速缓存
#define PTE_A     0x020 // 页面是否被访问过
#define PTE_D     0x040 // 页面是否被写入过
#define PTE_PS    0x080 // 页面大小（0 表示二级页表是 4KB 的页）
#define PTE_MBZ   0x180 // PAT&G
#define PTE_AVAIL 0xE00 // 9~11 位提供给操作系统或用户程序进行自由发挥
```

下表是 PDE 各位的功能：

![1556099305977](E:\sources\homework\ucore_os_lab\labcodes\lab2\assets\1556099305977.png)

下表是 PTE 的各位功能：

![1556099159432](E:\sources\homework\ucore_os_lab\labcodes\lab2\assets\1556099159432.png)

其中，这些标记用于 Lab 3 的开发（实现页交换）：

1. `PTE_PWT` 标记页面是否写穿透，如果页面要求写穿透，那么磁盘上的相应页面交换进内存并发生了写入后，必须同时写入硬盘，这样可以提升页面数据的可靠性，在断电后数据可以恢复。
2. `PTE_D` 项标记页面是否被写入过，如果被写入过，交换进磁盘时就需要一次回写。

### 如果ucore的缺页服务例程在执行过程中访问内存，出现了页访问异常，请问硬件要做哪些事情

硬件将会触发页访问异常中断，进入 PGFLT 中断服务程序，最终调用 `do_pgfault` 函数进行中断处理。

CPU 将产生缺页异常错误码并保存在栈顶，对应 `trapframe->tf_err`。将引发缺页异常的线性地址保存在 CR2 寄存器中。

## 练习2：补充完成基于FIFO的页面替换算法

我们已经知道 `pte` 对应的页面在磁盘中，因此在可交换的情况下，我们调用 `swap_in` 函数先交换一个页面进入内存，然后将这个页面加入到内存管理器中予以管理，并将该页标记为可交换的（也就是加入页交换管理器的托管列表中），同时还需要标记该页面的线性地址。

```cpp
/**
 * 处理缺页异常的中断处理程序.
 * 
 * @param mm the control struct for a set of vma using the same PDT
 * @param error_code CPU 产生的缺页异常错误码，记录在 trapframe->tf_err 中.
 *                   P 标记 (bit 0) P=0 表示访问不存在的页面 (PTE_P = 0)；P=1 表示访问权限不足或访问保留页面.
 *                   W/R 标记 (bit 1) R=0 表示执行读取操作时失败；W=1 表示执行写入操作时失败.
 *                   U/S 标记 (bit 2) S=0 表示在内核态访问内存时失败；U=1 表示在用户态访问内存时失败.
 * @param addr 引发缺页异常的线性地址 (记录在 CR2 寄存器中)
 * @note 调用链: trap --> trap_dispatch --> pgfault_handler --> do_pgfault
 */
int
do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr) {
    int ret = -E_INVAL;
    //try to find a vma which include addr
    struct vma_struct *vma = find_vma(mm, addr);

    pgfault_num++;
    //If the addr is in the range of a mm's vma?
    if (vma == NULL || vma->vm_start > addr) {
        cprintf("not valid addr %x, and  can not find it in vma\n", addr);
        goto failed;
    }
    //check the error_code
    switch (error_code & 3) {
    default:
            /* error code flag : default is 3 ( W/R=1, P=1): write, present */
    case 2: /* error code flag : (W/R=1, P=0): write, not present */
        if (!(vma->vm_flags & VM_WRITE)) {
            cprintf("do_pgfault failed: error code flag = write AND not present, but the addr's vma cannot write\n");
            goto failed;
        }
        break;
    case 1: /* error code flag : (W/R=0, P=1): read, present */
        cprintf("do_pgfault failed: error code flag = read AND present\n");
        goto failed;
    case 0: /* error code flag : (W/R=0, P=0): read, not present */
        if (!(vma->vm_flags & (VM_READ | VM_EXEC))) {
            cprintf("do_pgfault failed: error code flag = read AND not present, but the addr's vma cannot read or exec\n");
            goto failed;
        }
    }
    /* IF (write an existed addr ) OR
     *    (write an non_existed addr && addr is writable) OR
     *    (read  an non_existed addr && addr is readable)
     * THEN
     *    continue process
     */
    uint32_t perm = PTE_U;
    if (vma->vm_flags & VM_WRITE) {
        perm |= PTE_W;
    }
    addr = ROUNDDOWN(addr, PGSIZE);

    ret = -E_NO_MEM;

    pte_t *ptep=NULL;
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
        /*
         * LAB3 EXERCISE 2: YOUR CODE
         * Now we think this pte is a  swap entry, we should load data from disk to a page with phy addr,
         * and map the phy addr with logical addr, trigger swap manager to record the access situation of this page.
         */
        if(swap_init_ok) {
            struct Page *page=NULL;
            swap_in(mm, addr, &page);                   // (1) According to the mm AND addr, try to load the content of right disk page
                                                        //     into the memory which page managed.
            page_insert(mm->pgdir, page, addr, perm);   // (2) According to the mm, addr AND page, setup the map of phy addr <---> logical addr
            swap_map_swappable(mm, addr, page, true);   // (3) make the page swappable.
            page->pra_vaddr = addr;
        }
        else {
            cprintf("no swap_init_ok but ptep is %x, failed\n",*ptep);
            goto failed;
        }
   }
   ret = 0;
failed:
    return ret;
}
```



`fifo_map_swappable` 实现很简单：只需要将页面加入到托管的链表中即可。

```cpp
/*
 * (3)_fifo_map_swappable: According FIFO PRA, we should link the most recent arrival page at the back of pra_list_head queue
 */
static int
_fifo_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
    list_entry_t *head=(list_entry_t*) mm->sm_priv;
    list_entry_t *entry=&(page->pra_page_link);
 
    assert(entry != NULL && head != NULL);
    //record the page access situlation
    /*LAB3 EXERCISE 2: YOUR CODE*/
    list_add_before(head, entry); //(1)link the most recent arrival page at the back of the pra_list_head queue.
    return 0;
}
```

`fifo_swap_out_victim` 函数从链表中取出头元素，并得到该元素对应的页面地址。

```cpp
/*
 *  (4)_fifo_swap_out_victim: According FIFO PRA, we should unlink the  earliest arrival page in front of pra_list_head queue,
 *                            then assign the value of *ptr_page to the addr of this page.
 */
static int
_fifo_swap_out_victim(struct mm_struct *mm, struct Page ** ptr_page, int in_tick)
{
     list_entry_t *head=(list_entry_t*) mm->sm_priv;
         assert(head != NULL);
     assert(in_tick==0);
     /* Select the victim */
     /* LAB3 EXERCISE 2: YOUR CODE */ 
     list_entry_t *entry = list_next(head);
     assert(entry != NULL);
     *ptr_page = le2page(entry, pra_page_link);
     list_del(entry);
     //(1)  unlink the  earliest arrival page in front of pra_list_head queue
     //(2)  assign the value of *ptr_page to the addr of this page
     return 0;
}
```