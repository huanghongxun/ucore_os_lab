#ifndef __KERN_MM_MEMLAYOUT_H__
#define __KERN_MM_MEMLAYOUT_H__

/**
 * 定义操作系统内存布局的一些常量
 */

/* 全局段 (Global Segment) */

#define SEG_KTEXT    1 // 内核代码段
#define SEG_KDATA    2 // 内核数据段
#define SEG_UTEXT    3 // 用户代码段
#define SEG_UDATA    4 // 用户数据段
#define SEG_TSS      5 // 任务选择段（TSS）

/* 全局描述符编号 (Global Descriptor)，低三位 */

#define GD_KTEXT    ((SEG_KTEXT) << 3) // 内核代码段描述符编号
#define GD_KDATA    ((SEG_KDATA) << 3) // 内核数据段描述符编号
#define GD_UTEXT    ((SEG_UTEXT) << 3) // 用户代码段描述符编号
#define GD_UDATA    ((SEG_UDATA) << 3) // 用户数据段描述符编号
#define GD_TSS      ((SEG_TSS  ) << 3) // 任务状态段描述符编号

/* 权限级别 (Descriptor Privilege Level) */

#define DPL_KERNEL  (0)  // 内核权限级
#define DPL_USER    (3)  // 用户权限级

/* 代码段描述符 */

#define KERNEL_CS    ((GD_KTEXT) | DPL_KERNEL) // 内核代码段描述符
#define KERNEL_DS    ((GD_KDATA) | DPL_KERNEL) // 内核数据段描述符
#define USER_CS      ((GD_UTEXT) | DPL_USER  ) // 用户代码段描述符
#define USER_DS      ((GD_UDATA) | DPL_USER  ) // 用户数据段描述符

/**
 * Physical memory map:
 * 
 * +-------------------------------+ 0xFFFFFFFF (4GB)
 * |  32-bit device mapping space  |
 * +-------------------------------+
 * |                               |
 * +-------------------------------+ 实际物理内存空间结束地址 
 * |       free memory space       |
 * +-------------------------------+ 空闲物理内存空间起始地址（pmm.c:freemem）
 * |    n * sizeof(struct Page)    | 管理空闲空间的区域（Page Directory Table）
 * +-------------------------------+ BSS 段结束处（end）
 * |    ucore kernel BSS segment   |
 * +-------------------------------+ 基于 ucore 的数据大小
 * |   ucore kernel DATA segment   |
 * +-------------------------------+ 基于 ucore 的代码大小
 * |   ucore kernel TEXT segment   |
 * +-------------------------------+ 0x00100000 (1MB) 因此 kernel.ld 中的虚拟地址必须从 0xC0100000 开始
 * |            BIOS ROM           |
 * +-------------------------------+ 0x000F0000 (960KB)
 * |   16-bit device extended ROM  |
 * +-------------------------------+ 0x000C0000 (768KB)
 * |   CGA graphical memory space  |
 * +-------------------------------+ 0x000B8000
 * |        free memory space      |
 * +-------------------------------+ 0x00011000
 * |      ucore's ELF header       | ELF 头：4KB
 * +-------------------------------+ 0x00010000 (见 bootmain.c:ELFHDR)
 * |        free memory space      |
 * +-------------------------------+ 基于 bootloader 大小
 * | bootloader TEXT and DATA seg  |
 * +-------------------------------+ 0x00007C00 (栈顶)
 * |   bootloader and ucore stack  |
 * +-------------------------------+ (内核 esp) 基于对堆栈的使用情况
 * |    low-address free space     |
 * +-------------------------------+ 0x00000000
 */

/* *
 * Virtual memory map:                                          Permissions
 *                                                              kernel/user
 *
 *     4G ------------------> +---------------------------------+
 *                            |                                 |
 *                            |         Empty Memory (*)        |
 *                            |                                 |
 *                            +---------------------------------+ 0xFB000000
 *                            |   Cur. Page Table (Kern, RW)    | RW/-- PTSIZE
 *     VPT -----------------> +---------------------------------+ 0xFAC00000
 *                            |        Invalid Memory (*)       | --/--
 *     KERNTOP -------------> +---------------------------------+ 0xF8000000
 *                            |                                 |
 *                            |    Remapped Physical Memory     | RW/-- KMEMSIZE
 *                            |                                 |
 *     KERNBASE ------------> +---------------------------------+ 0xC0000000
 *                            |        Invalid Memory (*)       | --/--
 *     USERTOP -------------> +---------------------------------+ 0xB0000000
 *                            |           User stack            |
 *                            +---------------------------------+
 *                            |                                 |
 *                            :                                 :
 *                            |         ~~~~~~~~~~~~~~~~        |
 *                            :                                 :
 *                            |                                 |
 *                            ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                            |       User Program & Heap       |
 *     UTEXT ---------------> +---------------------------------+ 0x00800000
 *                            |        Invalid Memory (*)       | --/--
 *                            |  - - - - - - - - - - - - - - -  |
 *                            |    User STAB Data (optional)    |
 *     USERBASE, USTAB------> +---------------------------------+ 0x00200000
 *                            |        Invalid Memory (*)       | --/--
 *     0 -------------------> +---------------------------------+ 0x00000000
 * (*) Note: The kernel ensures that "Invalid Memory" is *never* mapped.
 *     "Empty Memory" is normally unmapped, but user programs may map pages
 *     there if desired.
 *
 * */

/* All physical memory mapped at this address */
#define KERNBASE            0xC0000000
#define KMEMSIZE            0x38000000                  // the maximum amount of physical memory
#define KERNTOP             (KERNBASE + KMEMSIZE)

/* *
 * Virtual page table. Entry PDX[VPT] in the PD (Page Directory) contains
 * a pointer to the page directory itself, thereby turning the PD into a page
 * table, which maps all the PTEs (Page Table Entry) containing the page mappings
 * for the entire virtual address space into that 4 Meg region starting at VPT.
 * */
#define VPT                 0xFAC00000

#define KSTACKPAGE          2                           // # of pages in kernel stack
#define KSTACKSIZE          (KSTACKPAGE * PGSIZE)       // sizeof kernel stack

#define USERTOP             0xB0000000
#define USTACKTOP           USERTOP
#define USTACKPAGE          256                         // # of pages in user stack
#define USTACKSIZE          (USTACKPAGE * PGSIZE)       // sizeof user stack

#define USERBASE            0x00200000
#define UTEXT               0x00800000                  // where user programs generally begin
#define USTAB               USERBASE                    // the location of the user STABS data structure

#define USER_ACCESS(start, end)                     \
(USERBASE <= (start) && (start) < (end) && (end) <= USERTOP)

#define KERN_ACCESS(start, end)                     \
(KERNBASE <= (start) && (start) < (end) && (end) <= KERNTOP)

#ifndef __ASSEMBLER__

#include <defs.h>
#include <atomic.h>
#include <list.h>

// 页表项类型，高 20 位为页面编号，低 12 位为页表项标记，见 mmu.h 中的 PTE 系列宏。
// 由于二级页表项只能指向某个页面的首地址，这个首地址的低 12 位必全零，因此我们可以利用
// 低 12 位来存储一些标记。如果需要获得页面地址，只需要 PTE_ADDR 宏即可（就是把低 12
// 位清零，就可以得到页面地址了）
typedef uintptr_t pte_t;

// 页表目录项类型，高 20 位为页表索引，低 12 位为页表项标记，见 mmu.h 中的 PTE 系列宏
// 由于页表目录项也是特殊的页表项，因此 pde_t 的结构和 pte_t 一致。
typedef uintptr_t pde_t;
typedef pte_t swap_entry_t; //the pte can also be a swap entry

/**
 * BIOS 15H 中断的一些相关常量
 * see bootasm.S
 */
#define E820MAX             20      // number of entries in E820MAP
#define E820_ARM            1       // address range memory
#define E820_ARR            2       // address range reserved

struct e820map {
    int nr_map;
    struct {
        uint64_t addr;
        uint64_t size;
        uint32_t type;
    } __attribute__((packed)) map[E820MAX];
};

/* *
 * 页描述符。每个页描述一个页帧。
 * 
 * 在 kern/mm/pmm.h 中包含了很多页管理的工具函数。
 * */
struct Page {
    int ref;                        // 页帧的引用计数器，若被页表引用的次数为 0，那么这个页帧将被释放
    uint32_t flags;                 // array of flags that describe the status of the page frame
    unsigned int property;          // 不同内存管理算法用作不同用处
    int zone_num;                   // used in buddy system, the No. of zone which the page belongs to
    list_entry_t page_link;         // 空闲块列表 free_list 的链表项
    list_entry_t pra_page_link;     // 先进先出队列列表，used for pra (page replace algorithm)
    uintptr_t pra_vaddr;            // 页面的虚拟地址，used for pra (page replace algorithm)
};

/* Flags describing the status of a page frame */
#define PG_reserved                 0       // if this bit=1: the Page is reserved for kernel, cannot be used in alloc/free_pages; otherwise, this bit=0 
#define PG_property                 1       // if this bit=1: the Page is the head page of a free memory block(contains some continuous_addrress pages), and can be used in alloc_pages; if this bit=0: if the Page is the the head page of a free memory block, then this Page and the memory block is alloced. Or this Page isn't the head page.

// 将页面设置为保留页面。供给内存分配器管理
#define SetPageReserved(page)       set_bit(PG_reserved, &((page)->flags))
#define ClearPageReserved(page)     clear_bit(PG_reserved, &((page)->flags))

// 判断页面是否保留。保留页面不可用于分配，保留的页面给 pmm_manager 进行处理
#define PageReserved(page)          test_bit(PG_reserved, &((page)->flags))

// 将该页面标记为空闲内存块的头页面
#define SetPageProperty(page)       set_bit(PG_property, &((page)->flags))
#define ClearPageProperty(page)     clear_bit(PG_property, &((page)->flags))
#define PageProperty(page)          test_bit(PG_property, &((page)->flags))

// convert list entry to page
#define le2page(le, member)                 \
    to_struct((le), struct Page, member)

/**
 * 维护记录空闲页的双向链表
 * 
 * 在初始情况下，也许这个物理内存的空闲页帧都是连续的，这样就形成了一个大的连续
 * 内存空闲块。但随着页帧的分配与释放，这个大的连续内存空闲块会分裂为一系列地址
 * 不连续的多个小连续内存空闲块，且每个连续内存空闲块内部的页帧是连续的。那么为
 * 了有效地管理这些小连续内存空闲块。所有的连续内存空闲块可用一个双向链表管理起
 * 来，便于分配和释放，为此定义了一个 free_area_t 数据结构，包含了一个
 * list_entry 结构的双向链表指针和记录当前空闲页的个数的无符号整型变量 nr_free。
 * 其中的链表指针指向了空闲的页帧。
 */
typedef struct {
    list_entry_t free_list;         // 空闲块双向链表的虚拟头节点
    unsigned int nr_free;           // 空闲块的总数（以页为单位）
} free_area_t;

/* for slab style kmalloc */
#define PG_slab                     2       // page frame is included in a slab
#define SetPageSlab(page)           set_bit(PG_slab, &((page)->flags))
#define ClearPageSlab(page)         clear_bit(PG_slab, &((page)->flags))
#define PageSlab(page)              test_bit(PG_slab, &((page)->flags))

#endif /* !__ASSEMBLER__ */

#endif /* !__KERN_MM_MEMLAYOUT_H__ */

